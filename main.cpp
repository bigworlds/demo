
#include <iostream>
#include <fstream>
#include <float.h>
#include <stdio.h> //__DATE__
#include "utils.h"

//#include "ray.h"
#include "camera.h"
#include "sphere.h"
#include "hitableList.h"
#include "timer.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;
using namespace glm;

#define WIDTH 1200
#define HEIGHT 800
#define NUM_CHANNEL 3 //RGB
//#define SINGLE_THREAD
#define PARALLEL

class material
{
public:
	virtual bool scatter(const ray& r_in, const hit_record& rec, glm::vec3& attenuation, ray& scattered) const = 0;
};

glm::vec3 random_in_unit_sphere()
{
	glm::vec3 p;
	do {
		p = 2.f * glm::vec3(double(rand()) / RAND_MAX, double(rand()) / RAND_MAX, double(rand()) / RAND_MAX) - glm::vec3(1, 1, 1);
	} while (glm::dot(p, p) >= 1.f);

	return p;
}

glm::vec3 color(const ray& r, hitable* world, int depth)
{
	ray bounce = r;
	vec3 colors = vec3(1);
	hit_record rec;
	for (int i = 0; i<50; ++i)
	{
		if (world->hit(bounce, 0.001, FLT_MAX, rec))
		{
			ray scattered;
			vec3 attenuation;
			if (rec.pMat->scatter(bounce, rec, attenuation, scattered))
			{
				colors *= attenuation;
				bounce = scattered;
			}
			else
			{
				return vec3(0);
			}
		}
		else
		{
			//background블렌딩
			vec3 unit_direction = glm::normalize(r.direction());
			float t = 0.5f * (unit_direction.y + 1.f);
			colors *= mix(vec3(1), vec3(0.5, 0.7, 1.), t);
			return colors;
		}
	}
	return vec3(0);
}

class lambertian : public material
{
	glm::vec3 albedo;
public:
	lambertian(const glm::vec3& a)
	{
		albedo = a;
	}
	bool scatter(const ray& r_in, const hit_record& rec, glm::vec3& attenuation, ray& scattered) const //override
	{
		glm::vec3 target = rec.p + rec.normal + random_in_unit_sphere();
		scattered = ray(rec.p, target - rec.p);
		attenuation = albedo;
		return true;
	}
};

glm::vec3 reflect(const glm::vec3& v, const glm::vec3& n)
{
	return v - 2 * dot(v, n)*n;
}

class metal : public material
{
	glm::vec3 albedo;
public:
	metal(const glm::vec3& a/* ,float f*/) : albedo(a)
	{
		//if( f<1) fuzz = f; else fuzz = 1;
	}
	bool scatter(const ray& r_in, const hit_record& rec, glm::vec3& attenuation, ray& scattered) const //override
	{
		glm::vec3 reflected = reflect(normalize(r_in.direction()), rec.normal);
		scattered = ray(rec.p, reflected);
		attenuation = albedo;
		return(dot(scattered.direction(), rec.normal) > 0);
	}
};

bool refract(const glm::vec3& v, const glm::vec3& n, float ni_over_nt, glm::vec3& refracted)
{
	glm::vec3 uv = normalize(v);
	//vec3 un = normalize(n);
	float dt = glm::dot(uv, n);
	float discriminant = 1 - ni_over_nt * ni_over_nt*(1 - dt * dt);
	if (discriminant > 0)
	{
		refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
		return true;
	}
	else
	{
		return false;
	}
}

float schlick(float cosine, float ref_idx)
{
	float r0 = (ref_idx - 1) / (ref_idx + 1);
	r0 = r0 * r0;
	float c = 1 - cosine;
	return r0 + (1 - r0) * (c * c * c * c * c);
}

class dielectric : public material
{
	float ref_idx;
public:
	dielectric(float ri)
	{
		ref_idx = ri;
	}
	bool scatter(const ray& r_in, const hit_record& rec, glm::vec3& attenuation, ray& scattered) const //override
	{
		glm::vec3 outward_normal;
		glm::vec3 reflected = reflect(r_in.direction(), rec.normal);
		float ni_over_nt;
		attenuation = glm::vec3(1.f, 1.f, 1.f);
		glm::vec3 refracted;
		float reflect_prob;
		float cosine;
		if (glm::dot(r_in.direction(), rec.normal) > 0)
		{
			outward_normal = -rec.normal;
			ni_over_nt = ref_idx;

			//cosine = ref_idx* dot(r_in.direction(), rec.normal)/r_in.direction().length();
			cosine = glm::dot(r_in.direction(), rec.normal) / r_in.direction().length();
			cosine = sqrt(1 - ref_idx * ref_idx*(1 - cosine * cosine));
		}
		else
		{
			outward_normal = rec.normal;
			ni_over_nt = 1.f / ref_idx;

			cosine = -dot(r_in.direction(), rec.normal) / r_in.direction().length();
		}

		if (refract(r_in.direction(), outward_normal, ni_over_nt, refracted))
		{
			reflect_prob = schlick(cosine, ref_idx);
		}
		else
		{
			reflect_prob = 1.f;
		}

		if (double(rand()) / RAND_MAX < reflect_prob)
		{
			scattered = ray(rec.p, reflected);
		}
		else
		{

			scattered = ray(rec.p, refracted); //refracted
		}

		return true;
	}
};

hitable* random_scene()
{
	int n = 500;
	hitable** list = new hitable*[n + 1];
	list[0] = new sphere(glm::vec3(0, -1000, 0), 1000, new lambertian(glm::vec3(0.5, 0.5, 0.5)));
	int i = 1;

	//for (int a = -11; a < 11; a++)
	for (int a = -1; a<1; a++)
	{
		//for (int b = -11; b < 11; b++)
		for (int b = -1; b<1; b++)
		{
			float choose_mat = (double(rand()) / RAND_MAX);
			glm::vec3 center(a + 0.9*(double(rand()) / RAND_MAX), 0.2, b + 0.9*(double(rand()) / RAND_MAX));
			if (glm::length(center - glm::vec3(4, 0.2, 0)) > 0.9)
			{
				if (choose_mat < 0.8)
				{
					list[i++] = new sphere(center, 0.2, new metal(glm::vec3(0.5 *(1 + (double(rand()) / RAND_MAX)), 0.5 *(1 + (double(rand()) / RAND_MAX)), 0.5 *(1 + (double(rand()) / RAND_MAX)))/* ,0.5*(double(rand())/RAND_MAX)*/));
				}
				else if (choose_mat < 0.95)
				{
					list[i++] = new sphere(center, 0.2, new dielectric(1.5));
				}
				else
				{
					list[i++] = new sphere(center, 0.2, new lambertian(glm::vec3((double(rand()) / RAND_MAX)*(double(rand()) / RAND_MAX), (double(rand()) / RAND_MAX)*(double(rand()) / RAND_MAX), (double(rand()) / RAND_MAX)*(double(rand()) / RAND_MAX))));
				}
			}
		}
	}
	list[i++] = new sphere(glm::vec3(0, 1, 0), 1.0, new dielectric(1.5));
	list[i++] = new sphere(glm::vec3(-4, 1, 0), 1.0, new lambertian(glm::vec3(0.4, 0.2, 0.1)));
	list[i++] = new sphere(glm::vec3(4, 1, 0), 1.0, new metal(glm::vec3(0.7, 0.6, 0.5)));//

	return new hitable_list(list, i);
}

struct JobItems
{
	//boundary
	int widthLow;
	int widthHigh;
	int heightLow;
	int heightHigh;

	camera* pCam;
	hitable* pWorld;
	uint8_t* pOutput;
};

void g_jobRayTrace(void* p)
{
	FiberArgs* param = (FiberArgs*)p;
	JobItems* userdata = (JobItems*)param->pUserdata;
	//JobSystem2* scheduler = userdata->scheduler;


	int widthFrom = userdata->widthLow;
	int widthTo = userdata->widthHigh;
	int heightFrom = userdata->heightLow;
	int heightTo = userdata->heightHigh;
	int nS = 10;
	int nChannel = 3;
	uint8_t* outputBuffer = userdata->pOutput;
	camera* cam = userdata->pCam;
	hitable* world = userdata->pWorld;

	for (int h = heightFrom; h<heightTo; h++)
	{
		for (int w = widthFrom; w<widthTo; w++)
		{
			glm::vec3 col(0, 0, 0);
			for (int s = 0; s<nS; s++)
			{
				float rand_u = static_cast<double>(rand()) / RAND_MAX;
				float rand_v = static_cast<double>(rand()) / RAND_MAX;
				float u = float(w + rand_u) / float(WIDTH);
				float v = 1 - (float(h + rand_v) / float(HEIGHT));
				ray r = cam->get_ray(u, v);

				//fire
				col += color(r, world, 0);
			}

			col /= float(nS); //average
			col = glm::vec3(sqrt(col[0]), sqrt(col[1]), sqrt(col[2])); //gamma corrected

			int ir = int(255.99*col[0]);//256
			int ig = int(255.99*col[1]);
			int ib = int(255.99*col[2]);

			outputBuffer[(h * widthTo * nChannel) + (w * nChannel + 0)] = ir;
			outputBuffer[(h * widthTo * nChannel) + (w * nChannel + 1)] = ig;
			outputBuffer[(h * widthTo * nChannel) + (w * nChannel + 2)] = ib;
		}
	}

}

struct TaskParam
{
	int width;
	int height;
	hitable* world;
	camera* cam;
	uint8_t* outputBuffer;

	JobSystem2* scheduler;
};

void taskJob(void* args)
{
	printf("\n=============task job start============\n");
	//여러 시스템에 따라 다른 태스크함수를 사용할것임
	//태스크는 카운터를 기다리는 특별한 job이라고 생각하면됨
	//runJobs()/waitforcounter()

	FiberArgs* param = (FiberArgs*)args;
	TaskParam* task = (TaskParam*)param->pUserdata;

	//jobify해야하는데
	JobItems items[2];
	items[0].widthLow = 0;
	items[0].widthHigh = task->width;
	items[0].heightLow = 0;
	items[0].heightHigh = task->height / 2;
	items[0].pCam = task->cam;
	items[0].pWorld = task->world;
	items[0].pOutput = task->outputBuffer;

	items[1].widthLow = 0;
	items[1].widthHigh = task->width;
	items[1].heightLow = task->height / 2;
	items[1].heightHigh = task->height;
	items[1].pCam = task->cam;
	items[1].pWorld = task->world;
	items[1].pOutput = task->outputBuffer;

	JobDeclaration2 tileDecl[2];
	tileDecl[0].callback = g_jobRayTrace;
	tileDecl[0].pUserdata = &items[0];
	tileDecl[1].callback = g_jobRayTrace;
	tileDecl[1].pUserdata = &items[1];


	atomic_uint* counters = new atomic_uint();

	//스캐터
	task->scheduler->RunJobs(tileDecl, 2, &counters);

	task->scheduler->WaitForCounter(&counters, 0, param->self);

	//delete things?
	delete counters;

	printf("\n=============task job end============\n");
}

int main()
{
	int width = WIDTH;
	int height = HEIGHT;
	int nS = 10;//num of sampling; 100

	uint8_t* outputBuffer = new uint8_t[width * height * NUM_CHANNEL]; 
	memset(outputBuffer, 0, width * height * NUM_CHANNEL);

	SYSTEM_INFO SI;
	GetSystemInfo(&SI);
	printf("Hardware info:\n");
	printf("  Number of processors: %d\n", SI.dwNumberOfProcessors);
	printf("  Page size in Bytes: %d\n", SI.dwPageSize);
	//printf("Processor type: " << SI.dwProcessorType << endl;
	printf("  Minimum application address: %lx\n", SI.lpMinimumApplicationAddress);
	printf("  Maximum application address: %lx\n", SI.lpMaximumApplicationAddress);
	printf("  Active processor mask: %u\n", SI.dwActiveProcessorMask);
	printf("Scene info:\n");
	printf("  너비:%d, 높이:%d, samples per pixel:%d \n", width, height, nS);
	printf("...\n");

	Timer timer;
	timer.Start();

	hitable* world = nullptr;
	world = random_scene();

	glm::vec3 lookfrom(13, 2, 3);
	glm::vec3 lookat(0, 0, 0);
	float dist_to_focus = 10;
	float aperture = 0.1;
	camera cam(lookfrom, lookat, glm::vec3(0, 1, 0), 20, float(width) / float(height), aperture, dist_to_focus);

	double elapsedTime = 0;
	timer.Restart();

#ifdef SINGLE_THREAD
	for (int h = 0; h<height; h++)
	{
		for (int w = 0; w<width; w++)
		{
			glm::vec3 col(0, 0, 0);
			for (int s = 0; s<nS; s++)
			{
				float rand_u = static_cast<double>(rand()) / RAND_MAX;
				float rand_v = static_cast<double>(rand()) / RAND_MAX;
				float u = float(w + rand_u) / float(width);
				float v = 1 - (float(h + rand_v) / float(height));
				ray r = cam.get_ray(u, v);

				//fire
				col += color(r, world, 0);
			}

			col /= float(nS); //average
			col = glm::vec3(sqrt(col[0]), sqrt(col[1]), sqrt(col[2])); //gamma corrected

			int ir = int(255.99*col[0]);//256
			int ig = int(255.99*col[1]);
			int ib = int(255.99*col[2]);

			outputBuffer[(h * width * NUM_CHANNEL) + (w * NUM_CHANNEL + 0)] = ir;
			outputBuffer[(h * width * NUM_CHANNEL) + (w * NUM_CHANNEL + 1)] = ig;
			outputBuffer[(h * width * NUM_CHANNEL) + (w * NUM_CHANNEL + 2)] = ib;
		}
	}
#endif

#ifdef PARALLEL
	int numWorkerThread = 3;
	int numFiberMag = 32;
	int numJobQ = 32;
	JobSystem2 jobSystem;
	jobSystem.Init(numWorkerThread, numFiberMag, numJobQ);

	TaskParam taskData = {};
	taskData.scheduler = &jobSystem;
	taskData.world = world;
	taskData.cam = &cam;
	taskData.width = width;
	taskData.height = height;
	taskData.outputBuffer = outputBuffer;

	atomic_uint* taskCounter = new atomic_uint();
	atomic_fetch_add(taskCounter, 1);
	jobSystem.RunTask(taskJob, &taskData, taskCounter);

	jobSystem.WaitForTask(taskCounter, 0);

	uint32_t res = 0;
	res = atomic_load(taskCounter);
	assert(res == 0);
	delete taskCounter;
	jobSystem.Endofstory();
#endif

	elapsedTime = timer.GetElapsedTime();

	char* filename = "ch12_where_next_metal_parallel.png";
	stbi_write_png(filename, width, height, NUM_CHANNEL, outputBuffer, width * NUM_CHANNEL);

	//20220811 
	//메모리 leak 없도록 수정
	//원저자가 빼먹은 소멸자를 이용했음
	delete world;
	delete[] outputBuffer;

	char header[50];
	sprintf(header, "%s_summary.txt", filename);
	ofstream summary(header);
	summary << __DATE__ << ' ' << __TIME__ << ' '<< endl;
	summary << filename << endl;
	summary << "너비: " << width << '\n' << "높이: " << height << '\n' << "num샘플링: " << nS << endl;
	summary << "걸린시간(sec): " << elapsedTime << endl;
	summary.flush();

	printf("걸린시간(sec): %f \n", elapsedTime);
	getchar();

	return 0;
}
