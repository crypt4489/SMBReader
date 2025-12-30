#pragma once

#include "LTM.h"
#include <cmath>
struct Camera
{


	void CamLookAt(const Vector3f& pos, const Vector3f& target, const Vector3f& up)
	{
		Vector3f camLook =  pos - target;

		camLook = Normalize(camLook);

		Vector3f camRight = Cross(up, camLook);

		camRight = Normalize(camRight);

		Vector3f camUp = Cross(camLook, camRight);

		LTM.SetPos(Vector4f(pos.x, pos.y, pos.z, 1.0f));
		LTM.SetForward(camLook);
		LTM.SetRight(camRight);
		LTM.SetUp(camUp);
	}

	void UpdateCamera()
	{
		float x, y, z;

		Vector3f lPos = LTM.GetPos(), lRight = LTM.GetRight(), lUp = LTM.GetUp(), lForward = LTM.GetForward();

		x = -Dot(lRight, lPos);
		y = -Dot(lUp, lPos);
		z = -Dot(lForward, lPos);


		View[0][0] = lRight.x;
		View[1][0] = lRight.y;
		View[2][0] = lRight.z;
		
		View[0][1] = lUp.x;
		View[1][1] = lUp.y;
		View[2][1] = lUp.z;
		
		View[0][2] = lForward.x;
		View[1][2] = lForward.y;
		View[2][2] = lForward.z;


		View[3][0] = x;
		View[3][1] = y;
		View[3][2] = z;
		View[3][3] = 1.0f;

	}

	void CreateProjectionMatrix(float aspect, float n, float f, float angle)
	{
		float FoyYDiv2 = angle * 0.5f;
		float cotFov = 1.0f / (tanf(FoyYDiv2));

		float w = cotFov / aspect;
		float h = cotFov;

		Projection = Identity4f();

		Projection[0][0] = w;
		Projection[1][1] = -h;
		Projection[2][2] = -(f + n) / (f - n);
		Projection[2][3] = -1.0f;
		Projection[3][2] = -(2 * f * n) / (f - n);
		Projection[3][3] = 0.0f;
	}

	LTM LTM;
	Matrix4f View;
	Matrix4f Projection;
	
};

