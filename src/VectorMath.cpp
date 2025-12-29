#include "MathTypes.h"

Vector2f Add(Vector2f a, Vector2f b)
{
	return { a.x + b.x, a.y + b.y };
}

Vector2f Sub(Vector2f a, Vector2f b)
{
	return { a.x - b.x, a.y - b.y };
}

Vector2f Multiply(Vector2f a, Vector2f b)
{
	return { a.x * b.x, a.y * b.y };
}

Vector2f Scale(Vector2f v, float s)
{
	return { v.x * s, v.y * s };
}

Vector2f Divide(Vector2f v, float s)
{
	return { v.x / s, v.y / s };
}

Vector2f Splat2f(float s)
{
	return { s, s };
}

Vector3f Add(Vector3f a, Vector3f b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3f Sub(Vector3f a, Vector3f b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vector3f Multiply(Vector3f a, Vector3f b)
{
	return { a.x * b.x, a.y * b.y, a.z * b.z };
}

Vector3f Scale(Vector3f v, float s)
{
	return { v.x * s, v.y * s, v.z * s };
}

Vector3f Divide(Vector3f v, float s)
{
	return { v.x / s, v.y / s, v.z / s };
}

Vector3f Splat3f(float s)
{
	return { s, s, s };
}

Vector4f Add(Vector4f a, Vector4f b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z , a.w + b.w };
}

Vector4f Sub(Vector4f a, Vector4f b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w };
}

Vector4f Multiply(Vector4f a, Vector4f b)
{
	return { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w };
}

Vector4f Scale(Vector4f v, float s)
{
	return { v.x * s, v.y * s, v.z * s, v.w * s };
}

Vector4f Divide(Vector4f v, float s)
{
	return { v.x / s, v.y / s, v.z / s, v.w / s };
}

Vector4f Splat4f(float s)
{
	return { s, s, s, s };
}

Vector2f operator+(Vector2f a, Vector2f b)
{
	return Add(a, b);
}

Vector2f operator-(Vector2f a, Vector2f b)
{
	return Sub(a, b);
}

Vector2f operator*(Vector2f v, float s)
{
	return Scale(v, s);
}

Vector2f operator*(Vector2f v, Vector2f v1)
{
	return Multiply(v, v1);
}

Vector2f operator/(Vector2f v, float s) 
{
	return Divide(v, s);
}

Vector3f operator+(Vector3f a, Vector3f b)
{
	return Add(a, b);
}

Vector3f operator-(Vector3f a, Vector3f b)
{
	return Sub(a, b);
}

Vector3f operator*(Vector3f a, Vector3f b)
{
	return Multiply(a, b);
}

Vector3f operator*(Vector3f v, float s)
{
	return Scale(v, s);
}

Vector3f operator/(Vector3f v, float s)
{
	return Divide(v, s);
}

Vector4f operator+(Vector4f a, Vector4f b)
{
	return Add(a, b);
}

Vector4f operator-(Vector4f a, Vector4f b)
{
	return Sub(a, b);
}

Vector4f operator*(Vector4f a, Vector4f b)
{
	return Multiply(a, b);
}

Vector4f operator*(Vector4f v, float s)
{
	return Scale(v, s);
}

Vector4f operator/(Vector4f v, float s)
{
	return Divide(v, s);
}

Vector2f Normalize(Vector2f a)
{
	float mag = a.x + a.y;
	return Divide(a, mag);
}

Vector3f Normalize(Vector3f a)
{
	float mag = a.x + a.y + a.z;
	return Divide(a, mag);
}

Vector3f Cross(Vector3f a, Vector3f b)
{
	return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

Vector4f Normalize(Vector4f a)
{
	float mag = a.x + a.y + a.z + a.w;
	return Divide(a, mag);
}
