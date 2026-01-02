#include "MathTypes.h"
#include <cmath>

Matrix2f Add(Matrix2f a, Matrix2f b)
{
	return { a.x + b.x, a.y + b.y };
}

Matrix2f Sub(Matrix2f a, Matrix2f b)
{
	return { a.x - b.x, a.y - b.y };
}

Matrix2f Scale(Matrix2f m, float s)
{
	return { m.x * s, m.y * s };
}


Matrix2f Multiply(Matrix2f a, Matrix2f b)
{
	Matrix2f res;
	res.x = Multiply(a, b.x);
	res.y = Multiply(a, b.y);
	return res;
}

Vector2f Multiply(Matrix2f m, Vector2f v)
{
	return { (m.x.x * v.x + m.y.x * v.y), (m.x.y * v.x + m.y.y * v.y) };
}

Vector2f Multiply(Vector2f v, Matrix2f m)
{
	m = Transpose(m);
	return { (m.x.x * v.x + m.y.x * v.y), (m.x.y * v.x + m.y.y * v.y) };
}

Matrix2f Transpose(Matrix2f m)
{
	Matrix2f res;
	res.x = Vector2f(m.x.x, m.y.x);
	res.y = Vector2f(m.x.y, m.y.y);
	return res;
}

float Determinant(Matrix2f m)
{
	return 0.0;
}

Matrix2f Inverse(Matrix2f m)
{
	return {};
}

Matrix2f Identity2f()
{
	return { {1.0,  0.0}, { 0.0, 1.0} };
}



Matrix2f operator+(Matrix2f a, Matrix2f b) 
{
	return Add(a, b);
}

Matrix2f operator-(Matrix2f a, Matrix2f b)
{
	return Sub(a, b);
}

Matrix2f operator*(Matrix2f a, Matrix2f b) 
{
	return Multiply(a, b);
}

Vector2f operator*(Matrix2f m, Vector2f v) 
{
	return Multiply(m, v);
}

Vector2f operator*(Vector2f v, Matrix2f m)
{
	return Multiply(v, m);
}

Matrix2f operator*(Matrix2f m, float s) 
{
	return Scale(m, s);
}


Matrix3f Add(Matrix3f a, Matrix3f b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Matrix3f Sub(Matrix3f a, Matrix3f b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Matrix3f Scale(Matrix3f m, float s)
{
	return { m.x * s, m.y * s, m.z * s };
}

Matrix3f Multiply(Matrix3f a, Matrix3f b)
{
	Matrix3f res;
	res.x = Multiply(a, b.x);
	res.y = Multiply(a, b.y);
	res.z = Multiply(a, b.z);
	return res;
}

Vector3f Multiply(Matrix3f m, Vector3f v)
{
	return { (m.x.x * v.x + m.y.x * v.y + m.z.x * v.z ), (m.x.y * v.x + m.y.y * v.y + m.z.y * v.z),  (m.x.z * v.x + m.y.z * v.y + m.z.z * v.z) };
}

Vector3f Multiply(Vector3f v, Matrix3f m)
{
	m = Transpose(m);
	return { (m.x.x * v.x + m.y.x * v.y + m.z.x * v.z), (m.x.y * v.x + m.y.y * v.y + m.z.y * v.z),  (m.x.z * v.x + m.y.z * v.y + m.z.z * v.z) };
}

Matrix3f Transpose(Matrix3f m)
{
	Matrix3f res;
	res.x = Vector3f(m.x.x, m.y.x, m.z.x);
	res.y = Vector3f(m.x.y, m.y.y, m.z.y);
	res.z = Vector3f(m.x.z, m.y.z, m.z.z);
	return res;
}

float Determinant(Matrix3f m)
{
	return 0.0;
}

Matrix3f Inverse(Matrix3f m)
{
	return {};
}

Matrix3f Identity3f()
{
	return { {1.0,  0.0,  0.0}, { 0.0, 1.0, 0.0}, {0.0, 0.0, 1.0 } };
}

Matrix3f operator+(Matrix3f a, Matrix3f b)
{
	return Add(a, b);
}

Matrix3f operator-(Matrix3f a, Matrix3f b)
{
	return Sub(a, b);
}

Matrix3f operator*(Matrix3f a, Matrix3f b)
{
	return Multiply(a, b);
}

Vector3f operator*(Matrix3f m, Vector3f v)
{
	return Multiply(m , v);
}

Vector3f operator*(Vector3f v, Matrix3f m)
{
	return Multiply(v, m);
}

Matrix3f operator*(Matrix3f m, float s)
{
	return Scale(m, s);
}

Matrix4f Add(Matrix4f a, Matrix4f b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

Matrix4f Sub(Matrix4f a, Matrix4f b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w };
}

Matrix4f Scale(Matrix4f m, float s)
{
	return { m.x * s, m.y * s, m.z * s, m.w };
}

Matrix4f Multiply(Matrix4f a, Matrix4f b)
{
	Matrix4f res;
	res.x = Multiply(a, b.x);
	res.y = Multiply(a, b.y);
	res.z = Multiply(a, b.z);
	res.w = Multiply(a, b.w);
	return res;
}

Vector4f Multiply(Matrix4f m, Vector4f v)
{
	return { 
		(m.x.x * v.x + m.y.x * v.y + m.z.x * v.z + m.w.x * v.w), 
		(m.x.y * v.x + m.y.y * v.y + m.z.y * v.z + m.w.y * v.w),  
		(m.x.z * v.x + m.y.z * v.y + m.z.z * v.z + m.w.z * v.w),  
		(m.x.w * v.x + m.y.w * v.y + m.z.w * v.w + m.w.w * v.w) 
	};
}


Vector4f Multiply(Vector4f v, Matrix4f m)
{
	m = Transpose(m);
	return {
		(m.x.x * v.x + m.y.x * v.y + m.z.x * v.z + m.w.x * v.w),
		(m.x.y * v.x + m.y.y * v.y + m.z.y * v.z + m.w.y * v.w),
		(m.x.z * v.x + m.y.z * v.y + m.z.z * v.z + m.w.z * v.w),
		(m.x.w * v.x + m.y.w * v.y + m.z.w * v.w + m.w.w * v.w)
	};
}

Matrix4f Transpose(Matrix4f m)
{
	Matrix4f res;
	res.x = Vector4f(m.x.x, m.y.x, m.z.x, m.w.x);
	res.y = Vector4f(m.x.y, m.y.y, m.z.y, m.w.y);
	res.z = Vector4f(m.x.z, m.y.z, m.z.z, m.w.z);
	res.w = Vector4f(m.x.w, m.y.w, m.z.w, m.w.w);
	return res;
}

Matrix4f Inverse(Matrix4f m)
{
	return {};
}

Matrix4f Identity4f()
{
	return { {1.0,  0.0,  0.0, 0.0}, { 0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0 }, {0.0, 0.0, 0.0, 1.0 } };
}

Matrix4f operator+(Matrix4f a, Matrix4f b)
{
	return Add(a, b);
}

Matrix4f operator-(Matrix4f a, Matrix4f b)
{
	return Sub(a, b);
}

Matrix4f operator*(Matrix4f a, Matrix4f b)
{
	return Multiply(a, b);
}

Vector4f operator*(Matrix4f m, Vector4f v)
{
	return Multiply(m, v);
}

Vector4f operator*(Vector4f v, Matrix4f m)
{
	return Multiply(v, m);
}

Matrix4f operator*(Matrix4f m, float s)
{
	return Scale(m, s);
}

Matrix2f::Matrix2f(Vector2f _x, Vector2f _y)
{
	x = _x;
	y = _y;
}

Matrix2f::Matrix2f(const Matrix2f& old) {
	x = old.x;
	y = old.y;
};

float* Matrix2f::operator[](int x)
{
	return &comp[x * 2];
}

Matrix3f::Matrix3f(Vector3f _x, Vector3f _y, Vector3f _z)
{
	x = _x;
	y = _y;
	z = _z;
}

Matrix3f::Matrix3f(const Matrix3f& old) {
	x = old.x;
	y = old.y;
	z = old.z;
};

float* Matrix3f::operator[](int x)
{
	return &comp[x * 3];
}


Matrix4f::Matrix4f(Vector4f _x, Vector4f _y, Vector4f _z, Vector4f _w)
{
	x = _x;
	y = _y;
	z = _z;
	w = _w;
}

Matrix4f::Matrix4f(const Matrix4f& old) {
	x = old.x;
	y = old.y;
	z = old.z;
	w = old.w;
};

float* Matrix4f::operator[](int x)
{
	return &comp[x * 4];
}


Matrix3f CreateRotationMatrix(const Vector3f& up, float angle)
{

	Matrix3f ret = Identity3f();

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

Matrix4f CreateRotationMatrixMat4(const Vector3f& up, float angle)
{
	Matrix4f ret = Identity4f();

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