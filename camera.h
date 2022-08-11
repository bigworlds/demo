#pragma once

#include "ray.h"

glm::vec3 random_in_unit_disk()
{
	glm::vec3 p;
	do{
		p = glm::vec3(2.0*double(rand())/RAND_MAX, 2.0*double(rand())/RAND_MAX, 0) - glm::vec3(1, 1, 0);
	}while(dot(p,p) >= 1.0);
	return p;
}

class camera
{
public:
	glm::vec3 origin;
	glm::vec3 lower_left_corner;
	glm::vec3 horizontal;
	glm::vec3 vertical;
	glm::vec3 u, v, w;
	float lens_radius;

	camera(glm::vec3 lookfrom, glm::vec3 lookat, glm::vec3 vup, float vfov, float aspect, float aperture, float focus_dist)
	{
		lens_radius = aperture / 2;
		float theta = vfov*3.14/180.0;
		float half_height = tan(theta/2);
		float half_width = aspect * half_height;
		origin = lookfrom;
		w = normalize(lookfrom - lookat);
		u = glm::normalize(cross(vup, w));
		v = cross(w, u);
		lower_left_corner = origin - half_width*focus_dist*u - half_height*focus_dist*v - focus_dist*w;
		horizontal = 2*half_width*focus_dist*u;
		vertical = 2*half_height*focus_dist*v;
	}

	ray get_ray(float s, float t)
	{
		glm::vec3 rd = lens_radius*random_in_unit_disk();
		glm::vec3 offset = u*rd.x + v*rd.y;
		return ray(origin+offset, (lower_left_corner + s*horizontal + t*vertical) - origin - offset);
	}
};
