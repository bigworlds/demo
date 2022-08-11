#pragma once

#include "ray.h"

class material;

struct hit_record
{
	float t;
	glm::vec3 p;
	glm::vec3 normal;
	material* pMat;
};

class hitable
{
public:
	virtual bool hit(const ray& r, float t_min, float t_max, hit_record& rec) const = 0;
};
