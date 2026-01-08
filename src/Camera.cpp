#include "Camera.h"
void Camera::CamLookAt(const Vector3f& pos, const Vector3f& target, const Vector3f& up)
{
	Vector3f camLook = pos - target;

	camLook = Normalize(camLook);

	Vector3f camRight = Cross(up, camLook);

	camRight = Normalize(camRight);

	Vector3f camUp = Cross(camLook, camRight);

	LTM.SetPos(Vector4f(pos.x, pos.y, pos.z, 1.0f));
	LTM.SetForward(camLook);
	LTM.SetRight(camRight);
	LTM.SetUp(camUp);
}

void Camera::UpdateCamera()
{
	float x, y, z;

	View = Identity4f();

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

	World = LTM.GetWorldMatrix();

}

void Camera::CreateProjectionMatrix(float aspect, float n, float f, float angle)
{
	Projection = Identity4f();

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

	CreateCameraFrustrum(angle, aspect, f);
}

void Camera::CreateCameraFrustrum(float _angle, float aspect, float far)
{
	float angle = _angle * .5f;
	float tanAngle = tanf(angle);

	float nh;

	nh = 1.0f * tanAngle;

	float nw;

	nw = (aspect * nh);

	camFrustrum.CreateFrustrumPlanes(Vector4f(0.0, 0.0, 1.0, 0.0), Vector4f(0.0, 1.0, 0.0, 0.0), Vector4f(1.0, 0.0, 0.0, 0.0), nw, nh, far);
}


void Plane::ComputePlane(const Vector4f& point, const Vector4f& normal)
{
	float d = -Dot(point, normal);
	planeEquation = Vector4f(normal.x, normal.y, normal.z, d);
	pointInPlane = point;
}

void Frustrum::CreateFrustrumPlanes(const Vector4f& forward, const Vector4f&up, const Vector4f& right, float _nearwidth, float _nearheight, float _far)
{
	nearwidth = _nearwidth;
	nearheight = _nearheight;
	farDistance = _far;


	Vector4f farCenter = forward * _far;
	Vector4f nearCenter = forward;

	

	planes[0].ComputePlane(nearCenter, forward * -1.0);
	planes[1].ComputePlane(farCenter, forward);

	

	Vector4f upTop = (up * _nearheight) + nearCenter;

	Vector3f xTop(upTop.x, upTop.y, upTop.z);

	xTop = Normalize(xTop);

	upTop.x = xTop.x;
	upTop.y = xTop.y;
	upTop.z = xTop.z;

	planes[2].ComputePlane(upTop, Cross(upTop, right));
	
	Vector4f upBottom = nearCenter - (up * _nearheight);

	Vector3f xBottom(upBottom.x, upBottom.y, upBottom.z);

	xBottom = Normalize(xBottom);

	upBottom.x = xBottom.x;
	upBottom.y = xBottom.y;
	upBottom.z = xBottom.z;

	planes[3].ComputePlane(upBottom, Cross(right, upBottom));


	Vector4f rightSide =  nearCenter - (right * _nearwidth);

	Vector3f xRight(rightSide.x, rightSide.y, rightSide.z);

	xRight = Normalize(xRight);

	rightSide.x = xRight.x;
	rightSide.y = xRight.y;
	rightSide.z = xRight.z;

	planes[4].ComputePlane(rightSide, Cross(rightSide, up));

	Vector4f leftSide = (right * _nearwidth) + nearCenter;

	Vector3f xleft(leftSide.x, leftSide.y, leftSide.z);

	xleft = Normalize(xleft);

	leftSide.x = xleft.x;
	leftSide.y = xleft.y;
	leftSide.z = xleft.z;

	planes[5].ComputePlane(leftSide, Cross(up, leftSide));

	planes[0].pointInPlane.w = 1.0f;
	planes[1].pointInPlane.w = 1.0f;
	planes[2].pointInPlane.w = 1.0f;
	planes[3].pointInPlane.w = 1.0f;
	planes[4].pointInPlane.w = 1.0f;
	planes[5].pointInPlane.w = 1.0f;

}