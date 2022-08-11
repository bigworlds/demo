#pragma once

#include "glm/glm.hpp"

class ray
{
public:
	ray(){}
	ray(const glm::vec3& a, const glm::vec3& b)
	{ 
		A = a; 
		v = b;
	}

	glm::vec3 origin() const {return A;}
	glm::vec3 direction() const {return v;}
	glm::vec3 point_at_parameter(float t) const {return A + t*v;}

	glm::vec3 A;
	glm::vec3 v;
};
