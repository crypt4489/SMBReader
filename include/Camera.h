#pragma once

#include "LTM.h"
struct Camera
{


	void CamLookAt(const Vector3f& pos, const Vector3f& target, const Vector3f& up)
	{
		Vector3f camLook =  pos - target;

		camLook = Normalize(camLook);

		Vector3f camRight = Cross(up, camLook);

		camRight = Normalize(camRight);

		Vector3f camUp = Cross(camLook, camRight);

		LTM.SetPos(glm::vec4(pos.x, pos.y, pos.z, 1.0f));
		LTM.SetForward(glm::vec3(camLook.x, camLook.y, camLook.z));
		LTM.SetRight(glm::vec3(camRight.x, camRight.y, camRight.z));
		LTM.SetUp(glm::vec3(camUp.x, camUp.y, camUp.z));
	}

	void UpdateCamera()
	{
		float x, y, z;

		glm::vec3 lPos = LTM.GetPos(), lRight = LTM.GetRight(), lUp = LTM.GetUp(), lForward = LTM.GetForward();

		x = -glm::dot(lRight, lPos);
		y = -glm::dot(lUp, lPos);
		z = -glm::dot(lForward, lPos);


		View[0][0] = lRight[0];
		View[1][0] = lRight[1];
		View[2][0] = lRight[2];
		
		View[0][1] = lUp[0];
		View[1][1] = lUp[1];
		View[2][1] = lUp[2];
		
		View[0][2] = lForward[0];
		View[1][2] = lForward[1];
		View[2][2] = lForward[2];


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

