#pragma once

#include "LTM.h"
#include <cmath>
struct Camera
{
	void CamLookAt(const Vector3f& pos, const Vector3f& target, const Vector3f& up);

	void UpdateCamera();

	void CreateProjectionMatrix(float aspect, float n, float f, float angle);

	void CreateCameraFrustrum(float _angle, float aspect, float far);

	LTM LTM{};
	Matrix4f View{};
	Matrix4f Projection{};
	Frustrum camFrustrum{};
};

