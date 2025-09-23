#pragma once


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct LTM
{
	glm::vec3 GetPos() const {
		return glm::vec3(LTM[3]);
	}

	glm::vec3 GetForward() const {
		return glm::vec3(LTM[2]);
	}

	glm::vec3 GetUp() const {
		return glm::vec3(LTM[1]);
	}

	glm::vec3 GetRight() const {
		return glm::vec3(LTM[0]);
	}

	glm::vec3 GetScale() const {
		return glm::vec3(LTM[0][3], LTM[1][3], LTM[2][3]);
	}

	void SetPos(const glm::vec4& _pos)
	{
		LTM[3] = _pos;
	}

	void SetPos(const glm::vec3& _pos)
	{
		LTM[3][0] = _pos[0];
		LTM[3][1] = _pos[1];
		LTM[3][2] = _pos[2];
	}

	void SetForward(const glm::vec3& _forward)
	{
		LTM[2][0] = _forward[0];
		LTM[2][1] = _forward[1];
		LTM[2][2] = _forward[2];
	}

	void SetUp(const glm::vec3& _up)
	{
		LTM[1][0] = _up[0];
		LTM[1][1] = _up[1];
		LTM[1][2] = _up[2];
	}

	void SetRight(const glm::vec3& _right)
	{
		LTM[0][0] = _right[0];
		LTM[0][1] = _right[1];
		LTM[0][2] = _right[2];
	}

	void SetScale(const glm::vec3& _scale)
	{
		LTM[0][3] = _scale[0];
		LTM[1][3] = _scale[1];
		LTM[2][3] = _scale[2];
	}

	void MoveForward(const glm::vec3& _delta)
	{
		SetPos(GetPos() + (GetForward() * _delta));
	}

	void MoveRight(const glm::vec3& _delta)
	{
		SetPos(GetPos() + (GetRight() * _delta));
	}

	void MoveUp(const glm::vec3& _delta)
	{
		SetPos(GetPos() + (GetUp() * _delta));
	}


	glm::mat4 LTM;
};

