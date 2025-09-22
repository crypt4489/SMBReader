#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
struct Camera
{


	void CamLookAt(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up)
	{
		glm::vec3 camLook =  pos - target;

		camLook = glm::normalize(camLook);

		glm::vec3 camRight = glm::cross(up, camLook);

		camRight = glm::normalize(camRight);

		glm::vec3 camUp = cross(camLook, camRight);

		LTM[3] = glm::vec4(pos, 1.0f);
		LTM[2] = glm::vec4(camLook, 0.0f);
		LTM[1] = glm::vec4(camUp, 0.0f);
		LTM[0] = glm::vec4(camRight, 0.0f);
	}

	void UpdateCamera()
	{
		float x, y, z;

		x = -glm::dot(LTM[0], LTM[3]);
		y = -glm::dot(LTM[1], LTM[3]);
		z = -glm::dot(LTM[2], LTM[3]);

		View[0][0] = LTM[0][0];
		View[0][1] = LTM[0][1];
		View[0][2] = LTM[0][2];
		View[0][3] = 0.0f;

		View[1][0] = LTM[1][0];
		View[1][1] = LTM[1][1];
		View[1][2] = LTM[1][2];
		View[1][3] = 0.0f;

		View[2][0] = LTM[2][0];
		View[2][1] = LTM[2][1];
		View[2][2] = LTM[2][2];
		View[2][3] = 0.0f;


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

	void MoveCamera(const glm::vec3& delta)
	{
		LTM[3][0] += delta[0];
		LTM[3][1] += delta[1];
		LTM[3][2] += delta[2];
	}

	glm::mat4 LTM;
	glm::mat4 View;
	glm::mat4 Projection;
	
};

