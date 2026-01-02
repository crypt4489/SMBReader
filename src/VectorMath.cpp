#include "MathTypes.h"
#include <cmath>


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

float Length(Vector2f v)
{
	float mag = sqrtf(v.x * v.x + v.y * v.y);
	return mag;
}

float Length(Vector3f v)
{
	float mag = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	return mag;
}

float Length(Vector4f v)
{
	float mag = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w + v.w);
	return mag;
}

Vector2f Normalize(Vector2f a)
{
	float mag = Length(a);
	return Divide(a, mag);
}

Vector3f Normalize(Vector3f a)
{
	float mag = Length(a);
	return Divide(a, mag);
}

Vector3f Cross(Vector3f a, Vector3f b)
{
	return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

Vector4f Normalize(Vector4f a)
{
	float mag = Length(a);
	return Divide(a, mag);
}

float Dot(Vector2f a, Vector2f b)
{
	return a.x * b.x + a.y * b.y;
}

float Dot(Vector3f a, Vector3f b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Dot(Vector4f a, Vector4f b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vector2f::Vector2f(float _x, float _y)
{
	x = _x;
	y = _y;
}

Vector2f::Vector2f(const Vector2f& old)
{
	x = old.x;
	y = old.y;
}

bool Vector2f::operator==(const Vector2f& compare) const
{
	return (x == compare.x) && (y == compare.y);
}


Vector3f::Vector3f(float _x, float _y, float _z)
{
	x = _x;
	y = _y;
	z = _z;
}

Vector3f::Vector3f(const Vector3f& old)
{
	x = old.x;
	y = old.y;
	z = old.z;
}

bool Vector3f::operator==(const Vector3f& compare) const
{
	return (x == compare.x) && (y == compare.y) && (z == compare.z);
}


Vector4f::Vector4f(float _x, float _y, float _z, float _w)
{
	x = _x;
	y = _y;
	z = _z;
	w = _w;
}

Vector4f::Vector4f(const Vector4f& old)
{
	x = old.x;
	y = old.y;
	z = old.z;
	w = old.w;
}

bool Vector4f::operator==(const Vector4f& compare) const
{
	return (x == compare.x) && (y == compare.y) && (z == compare.z) && (w == compare.w);
}

Vector2c::Vector2c(char _x, char _y)
{
	x = _x;
	y = _y;
}

Vector2c::Vector2c(const Vector2c& old)
{
	x = old.x;
	y = old.y;
}

bool Vector2c::operator==(const Vector2c& compare) const
{
	return (x == compare.x) && (y == compare.y);
}


Vector2uc::Vector2uc(unsigned char _x, unsigned char _y)
{
	x = _x;
	y = _y;
}

Vector2uc::Vector2uc(const Vector2uc& old)
{
	x = old.x;
	y = old.y;
}

bool Vector2uc::operator==(const Vector2uc& compare) const
{
	return (x == compare.x) && (y == compare.y);
}


Vector2s::Vector2s(short _x, short _y)
{
	x = _x;
	y = _y;
}

Vector2s::Vector2s(const Vector2s& old)
{
	x = old.x;
	y = old.y;
}

bool Vector2s::operator==(const Vector2s& compare) const
{
	return (x == compare.x) && (y == compare.y);
}

Vector3s::Vector3s(short _x, short _y, short _z)
{
	x = _x;
	y = _y;
	z = _z;
}

Vector3s::Vector3s(const Vector3s& old)
{
	x = old.x;
	y = old.y;
	z = old.z;
}

bool Vector3s::operator==(const Vector3s& compare) const
{
	return (x == compare.x) && (y == compare.y) && (z == compare.z);
}

Vector2i::Vector2i(int _x, int _y)
{
	x = _x;
	y = _y;
}

Vector2i::Vector2i(const Vector2i& old)
{
	x = old.x;
	y = old.y;
}

bool Vector2i::operator==(const Vector2i& compare) const
{
	return (x == compare.x) && (y == compare.y);
}

