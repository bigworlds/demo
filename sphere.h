#pragma once

#include "hitable.h"

class sphere : public hitable
{
public:
	glm::vec3 center;
	float radius;
	material* mat_ptr;

	sphere(){}
	sphere(glm::vec3 cen, float r, material* m)
	{
		center = cen;
		radius = r;
		mat_ptr = m;
	}

	bool hit(const ray& r, float t_min, float t_max, hit_record& rec) const; //override
};

bool::sphere::hit(const ray& r, float t_min, float t_max, hit_record& rec) const
{
	glm::vec3 oc = r.origin() - center;
	float a = glm::dot(r.direction(), r.direction());
	float b = glm::dot(oc, r.direction());
	float c = glm::dot(oc, oc) - radius*radius;
	float discriminant = b*b - a*c;
	if(discriminant > 0)
	{
		float temp = (-b - sqrt(b*b-a*c))/a;
		if( temp<t_max && temp>t_min)
		{
			rec.t = temp;
			rec.p = r.point_at_parameter(rec.t);
			rec.normal = (rec.p - center)/radius;
			rec.pMat = mat_ptr;
			return true;
		}
		temp = (-b +sqrt(b*b - a*c))/a;
		if( temp < t_max && temp > t_min)
		{
			rec.t = temp;
			rec.p = r.point_at_parameter(rec.t);
			rec.normal = (rec.p - center)/radius;
			rec.pMat = mat_ptr;
			return true;
		}
	}
	return false;
}
