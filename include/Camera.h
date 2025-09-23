#pragma once

#include "LTM.h"
struct Camera
{


	void CamLookAt(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up)
	{
		glm::vec3 camLook =  pos - target;

		camLook = glm::normalize(camLook);

		glm::vec3 camRight = glm::cross(up, camLook);

		camRight = glm::normalize(camRight);

		glm::vec3 camUp = cross(camLook, camRight);

		LTM.SetPos(glm::vec4(pos, 1.0f));
		LTM.SetForward(camLook);
		LTM.SetRight(camRight);
		LTM.SetUp(camUp);
	}

	void UpdateCamera()
	{
		float x, y, z;

		glm::vec3 lPos = LTM.GetPos(), lRight = LTM.GetRight(), lUp = LTM.GetUp(), lForward = LTM.GetForward();

		x = -glm::dot(lRight, lPos);
		y = -glm::dot(lUp, lPos);
		z = -glm::dot(lForward, lPos);

		View[0] = glm::vec4(lRight, 0.0f);
		View[1] = glm::vec4(lUp, 0.0f);
		View[2] = glm::vec4(lForward, 0.0f);


		View[3][0] = x;
		View[3][1] = y;
		View[3][2] = z;
		View[3][3] = 1.0f;

	}

	void CreateProjectionMatrix(float aspect, float n, float f, float angle)
	{
		float FoyYDiv2 = angle * 0.5f;
		float cotFov = 1.0f / (glm::tan(FoyYDiv2));

		float w = cotFov / aspect;
		float h = cotFov;

		Projection = glm::identity<glm::mat4>();

		Projection[0][0] = w;
		Projection[1][1] = -h;
		Projection[2][2] = -(f + n) / (f - n);
		Projection[2][3] = -1.0f;
		Projection[3][2] = -(2 * f * n) / (f - n);
		Projection[3][3] = 0.0f;
	}

	LTM LTM;
	glm::mat4 View;
	glm::mat4 Projection;
	
};

