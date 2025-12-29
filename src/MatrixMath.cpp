#include "MathTypes.h"

glm::mat3 CreateRotationMatrix(const Vector3f& up, float angle)
{

	glm::mat3 ret = glm::identity<glm::mat3>();

	float s = sinf(angle);
	float c = cosf(angle);

	float x = up.x;
	float y = up.y;
	float z = up.z;

	float invC = 1 - c;

	float xy = x * y;
	float xz = x * z;
	float xs = x * s;
	float ys = y * s;
	float zs = z * s;

	ret[0][0] = c + (x * x) * invC;
	ret[0][1] = xy * invC - zs;
	ret[0][2] = xz * invC + ys;
	ret[1][0] = xy * invC + zs;
	ret[1][1] = c + (y * y) * invC;
	ret[1][2] = xy * invC - xs;
	ret[2][0] = xz * invC - ys;
	ret[2][1] = xy * invC + xs;
	ret[2][2] = c + (z * z) * invC;

	return ret;

}

glm::mat4 CreateRotationMatrixMat4(const Vector3f& up, float angle)
{
	glm::mat4 ret = glm::identity<glm::mat4>();

	float s = sinf(angle);
	float c = cosf(angle);

	float x = up.x;
	float y = up.y;
	float z = up.z;

	float invC = 1 - c;

	float xy = x * y;
	float xz = x * z;
	float xs = x * s;
	float ys = y * s;
	float zs = z * s;

	ret[0][0] = c + (x * x) * invC;
	ret[0][1] = xy * invC - zs;
	ret[0][2] = xz * invC + ys;
	ret[1][0] = xy * invC + zs;
	ret[1][1] = c + (y * y) * invC;
	ret[1][2] = xy * invC - xs;
	ret[2][0] = xz * invC - ys;
	ret[2][1] = xy * invC + xs;
	ret[2][2] = c + (z * z) * invC;

	return ret;

}