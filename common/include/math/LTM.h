#pragma once

#include "MathTypes.h"

struct LTM
{
	Vector3f GetPos() const;

	Vector3f GetForward() const;

	Vector3f GetUp() const;

	Vector3f GetRight() const;

	Vector3f GetScale() const;

	void SetPos(const Vector4f& _pos);

	void SetPos(const Vector3f& _pos);

	void SetForward(const Vector3f& _forward);

	void SetUp(const Vector3f& _up);

	void SetRight(const Vector3f& _right);

	void SetScale(const Vector3f& _scale);

	void MoveForward(const Vector3f& _delta);

	void MoveRight(const Vector3f& _delta);

	void MoveUp(const Vector3f& _delta);

	void MoveForward(const float _delta);

	void MoveRight(const float _delta);

	void MoveUp(const float _delta);

	void RotateAroundUp(double angle);

	void PitchLTM(double angle);

	Matrix4f GetWorldMatrix();

	Matrix4f LTM;
};

