#include "LTM.h"
Vector3f LTM::GetPos() const {
	return Vector3f(LTM.translate.x, LTM.translate.y, LTM.translate.z);
}

Vector3f LTM::GetForward() const {
	return Vector3f(LTM.forward.x, LTM.forward.y, LTM.forward.z);
}

Vector3f LTM::GetUp() const {
	return Vector3f(LTM.up.x, LTM.up.y, LTM.up.z);
}

Vector3f LTM::GetRight() const {
	return Vector3f(LTM.right.x, LTM.right.y, LTM.right.z);
}

Vector3f LTM::GetScale() const {
	return Vector3f(LTM.right.w, LTM.up.w, LTM.forward.w);
}

void LTM::SetPos(const Vector4f& _pos)
{
	LTM.translate = _pos;
}

void LTM::SetPos(const Vector3f& _pos)
{
	LTM[3][0] = _pos.x;
	LTM[3][1] = _pos.y;
	LTM[3][2] = _pos.z;
}

void LTM::SetForward(const Vector3f& _forward)
{
	LTM[2][0] = _forward.x;
	LTM[2][1] = _forward.y;
	LTM[2][2] = _forward.z;
}

void LTM::SetUp(const Vector3f& _up)
{
	LTM[1][0] = _up.x;
	LTM[1][1] = _up.y;
	LTM[1][2] = _up.z;
}

void LTM::SetRight(const Vector3f& _right)
{
	LTM[0][0] = _right.x;
	LTM[0][1] = _right.y;
	LTM[0][2] = _right.z;
}

void LTM::SetScale(const Vector3f& _scale)
{
	LTM[0][3] = _scale.x;
	LTM[1][3] = _scale.y;
	LTM[2][3] = _scale.z;
}

void LTM::MoveForward(const Vector3f& _delta)
{
	SetPos(GetPos() + (GetForward() * _delta));
}

void LTM::MoveRight(const Vector3f& _delta)
{
	SetPos(GetPos() + (GetRight() * _delta));
}

void LTM::MoveUp(const Vector3f& _delta)
{
	SetPos(GetPos() + (GetUp() * _delta));
}

void LTM::MoveForward(const float _delta)
{
	SetPos(GetPos() + (GetForward() * _delta));
}

void LTM::MoveRight(const float _delta)
{
	SetPos(GetPos() + (GetRight() * _delta));
}

void LTM::MoveUp(const float _delta)
{
	SetPos(GetPos() + (GetUp() * _delta));
}

void LTM::RotateAroundUp(double angle)
{
	Matrix3f rot = CreateRotationMatrix(Vector3f(0.0, 1.0f, 0.0f), DegToRad(static_cast<float>(angle)));


	Vector3f right = Vector3f(LTM.right.x, LTM.right.y, LTM.right.z);
	Vector3f up = Vector3f(LTM.up.x, LTM.up.y, LTM.up.z);
	Vector3f forward = Vector3f(LTM.forward.x, LTM.forward.y, LTM.forward.z);

	right = right * rot;
	up = up * rot;


	forward = forward * rot;

	forward = Normalize(forward);

	LTM.right = Vector4f(right.x, right.y, right.z, LTM[0][3]);
	LTM.up = Vector4f(up.x, up.y, up.z, LTM[1][3]);
	LTM.forward = Vector4f(forward.x, forward.y, forward.z, LTM[2][3]);

}

void LTM::PitchLTM(double angle)
{

	Vector3f right = { LTM.right.x, LTM.right.y, LTM.right.z };

	Vector3f up = Vector3f(LTM.up.x, LTM.up.y, LTM.up.z);
	Vector3f forward = Vector3f(LTM.forward.x, LTM.forward.y, LTM.forward.z);

	Matrix3f rot = CreateRotationMatrix(right, DegToRad(static_cast<float>(angle)));

	up = up * rot;


	forward = forward * rot;

	forward = Normalize(forward);

	LTM.up = Vector4f(up.x, up.y, up.z, LTM[1][3]);
	LTM.forward = Vector4f(forward.x, forward.y, forward.z, LTM[2][3]);
}

Matrix4f LTM::GetWorldMatrix()
{
	
	Matrix4f ret = Identity4f();

	ret[0][0] = LTM[0][0];
	ret[0][1] = LTM[0][1];
	ret[0][2] = LTM[0][2];
	ret[0][3] = 0.0;
	ret[1][0] = LTM[1][0];
	ret[1][1] = LTM[1][1];
	ret[1][2] = LTM[1][2];
	ret[1][3] = 0.0;
	ret[2][0] = LTM[2][0];
	ret[2][1] = LTM[2][1];
	ret[2][2] = LTM[2][2];
	ret[2][3] = 0.0;

	ret[3][0] = LTM[3][0];
	ret[3][1] = LTM[3][1];
	ret[3][2] = LTM[3][2];
	ret[3][3] = 1.0;

	return ret;
}