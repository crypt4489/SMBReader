#pragma once

#ifdef _MSC_VER
#define VEC_ALIGN //__declspec(align(16))
#define PACKED_BEGIN __pragma(pack(push, 1))
#define PACKED_END __pragma(pack(pop))
#endif

#define TWOPI 6.283185307179586476925286766559f
#define PI 3.1415926535897932384626433832795f
#define PIHALF 1.5707963267948966192313216916398f
#define PIDIV4 0.78539816339744830961566084581988f


inline float DegToRad(float Deg)
{
	return (Deg / 180.0f) * PI;
}



PACKED_BEGIN
struct Vector2f
{
	union {
		struct { float x, y; };
		float comp[2];
	};

	Vector2f() = default;
	Vector2f(float _x, float _y)
	{
		x = _x;
		y = _y;
	}

	Vector2f(const Vector2f& old)
	{
		x = old.x;
		y = old.y;
	}

	bool operator==(const Vector2f& compare) const
	{
		return (x == compare.x) && (y == compare.y);
	}
};

struct Vector3f
{
	union {
		struct { float x, y, z; };
		float comp[3];
	};

	Vector3f() = default;
	Vector3f(float _x, float _y, float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}

	Vector3f(const Vector3f& old)
	{
		x = old.x;
		y = old.y;
		z = old.z;
	}

	bool operator==(const Vector3f& compare) const
	{
		return (x == compare.x) && (y == compare.y) && (z == compare.z);
	}

};

VEC_ALIGN
struct Vector4f
{
	union {
		struct { float x, y, z, w; };
		float comp[4];
	};

	Vector4f() = default;
	Vector4f(float _x, float _y, float _z, float _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	Vector4f(const Vector4f& old)
	{
		x = old.x;
		y = old.y;
		z = old.z;
		w = old.w;
	}

	bool operator==(const Vector4f& compare) const
	{
		return (x == compare.x) && (y == compare.y) && (z == compare.z) && (w == compare.w);
	}

};


struct Vector2c
{
	union {
		struct { char x, y; };
		char comp[2];
	};

	Vector2c() = default;
	Vector2c(char _x, char _y)
	{
		x = _x;
		y = _y;
	}

	Vector2c(const Vector2c& old)
	{
		x = old.x;
		y = old.y;
	}

	bool operator==(const Vector2c& compare) const
	{
		return (x == compare.x) && (y == compare.y);
	}
};


struct Vector3c
{
	union {
		struct { char x, y, z; };
		char comp[3];
	};
};


struct Vector4c
{
	union {
		struct { char x, y, z, w; };
		char comp[4];
	};



};


struct Vector2uc
{
	union {
		struct { unsigned char x, y; };
		unsigned char comp[2];
	};


	Vector2uc() = default;
	Vector2uc(unsigned char _x, unsigned char _y)
	{
		x = _x;
		y = _y;
	}

	Vector2uc(const Vector2uc& old)
	{
		x = old.x;
		y = old.y;
	}

	bool operator==(const Vector2uc& compare) const
	{
		return (x == compare.x) && (y == compare.y);
	}
};


struct Vector3uc
{
	union {
		struct { unsigned char x, y, z; };
		unsigned char comp[3];
	};
};


struct Vector4uc
{
	union {
		struct { unsigned char x, y, z, w; };
		unsigned char comp[4];
	};
};


struct Vector2s
{
	union {
		struct { short x, y; };
		short comp[2];
	};

	Vector2s() = default;
	Vector2s(short _x, short _y)
	{
		x = _x;
		y = _y;
	}

	Vector2s(const Vector2s& old)
	{
		x = old.x;
		y = old.y;
	}

	bool operator==(const Vector2s& compare) const
	{
		return (x == compare.x) && (y == compare.y);
	}

};


struct Vector3s
{
	union {
		struct { short x, y, z; };
		short comp[3];
	};

	Vector3s() = default;
	Vector3s(short _x, short _y, short _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}

	Vector3s(const Vector3s& old)
	{
		x = old.x;
		y = old.y;
		z = old.z;
	}

	bool operator==(const Vector3s& compare) const
	{
		return (x == compare.x) && (y == compare.y) && (z == compare.z);
	}
};


struct Vector4s
{
	union {
		struct { short x, y, z, w; };
		short comp[4];
	};
};


struct Vector2us
{
	union {
		struct { unsigned short x, y; };
		unsigned short comp[2];
	};
};


struct Vector3us
{
	union {
		struct { unsigned short x, y, z; };
		unsigned short comp[3];
	};
};


struct Vector4us
{
	union {
		struct { unsigned short x, y, z, w; };
		unsigned short comp[4];
	};
};




struct Vector2i
{
	union {
		struct { int x, y; };
		int comp[2];
	};

	Vector2i() = default;
	Vector2i(int _x, int _y)
	{
		x = _x;
		y = _y;
	}

	Vector2i(const Vector2i& old)
	{
		x = old.x;
		y = old.y;
	}

	bool operator==(const Vector2i& compare) const
	{
		return (x == compare.x) && (y == compare.y);
	}
};


struct Vector2ui
{
	union {
		struct { unsigned int x, y; };
		unsigned int comp[2];
	};
};



struct Vector3i
{
	union {
		struct { int x, y, z; };
		int comp[3];
	};
};


struct Vector3ui
{
	union {
		struct { unsigned int x, y, z; };
		unsigned int comp[3];
	};
};


VEC_ALIGN
struct Vector4i
{
	union {
		struct { int x, y, z, w; };
		int comp[4];
	};
};

VEC_ALIGN
struct Vector4ui
{
	union {
		struct { unsigned int x, y, z, w; };
		unsigned int comp[4];
	};
};

VEC_ALIGN
struct Vector2d
{
	union {
		struct { double x, y; };
		double comp[2];
	};
};

struct Vector3d
{
	union {
		struct { double x, y, z; };
		double comp[3];
	};
};

VEC_ALIGN
struct Vector4d
{
	union {
		struct { double x, y, z, w; };
		double comp[4];
	};
};

VEC_ALIGN
struct Vector2ll
{
	union {
		struct { long long x, y; };
		long long comp[2];
	};
};

struct Vector3ll
{
	union {
		struct { long long x, y, z; };
		long long comp[3];
	};
};

VEC_ALIGN
struct Vector4ll
{
	union {
		struct { long long x, y, z, w; };
		long long comp[4];
	};
};

VEC_ALIGN
struct Vector2ull
{
	union {
		struct { unsigned long long x, y; };
		unsigned long long comp[2];
	};
};

struct Vector3ull
{
	union {
		struct { unsigned long long x, y, z; };
		unsigned long long comp[3];
	};
};

VEC_ALIGN
struct Vector4ull
{
	union {
		struct { unsigned long long x, y, z, w; };
		unsigned long long comp[4];
	};
};

PACKED_END


static_assert(sizeof(Vector2f) == 8, "Vector2f size mismatch");
static_assert(sizeof(Vector3f) == 12, "Vector3f size mismatch");
static_assert(sizeof(Vector4f) == 16, "Vector4f size mismatch");

static_assert(sizeof(Vector2i) == 8, "Vector2i size mismatch");
static_assert(sizeof(Vector3i) == 12, "Vector3i size mismatch");
static_assert(sizeof(Vector4i) == 16, "Vector4i size mismatch");

static_assert(sizeof(Vector2ui) == 8, "Vector2u size mismatch");
static_assert(sizeof(Vector3ui) == 12, "Vector3ui size mismatch");
static_assert(sizeof(Vector4ui) == 16, "Vector4u size mismatch");

static_assert(sizeof(Vector2d) == 16, "Vector2d size mismatch");
static_assert(sizeof(Vector3d) == 24, "Vector3d size mismatch");
static_assert(sizeof(Vector4d) == 32, "Vector4d size mismatch");

static_assert(sizeof(Vector2ll) == 16, "Vector2ll size mismatch");
static_assert(sizeof(Vector3ll) == 24, "Vector3ll size mismatch");
static_assert(sizeof(Vector4ll) == 32, "Vector4ll size mismatch");

static_assert(sizeof(Vector2ull) == 16, "Vector2ull size mismatch");
static_assert(sizeof(Vector3ull) == 24, "Vector3ull size mismatch");
static_assert(sizeof(Vector4ull) == 32, "Vector4ull size mismatch");

static_assert(sizeof(Vector2c) == 2, "Vector2c size mismatch");
static_assert(sizeof(Vector3c) == 3, "Vector3c size mismatch");
static_assert(sizeof(Vector4c) == 4, "Vector4c size mismatch");

static_assert(sizeof(Vector2uc) == 2, "Vector2uc size mismatch");
static_assert(sizeof(Vector3uc) == 3, "Vector3uc size mismatch");
static_assert(sizeof(Vector4uc) == 4, "Vector4uc size mismatch");

static_assert(sizeof(Vector2s) == 4, "Vector2s size mismatch");
static_assert(sizeof(Vector3s) == 6, "Vector3s size mismatch");
static_assert(sizeof(Vector4s) == 8, "Vector4s size mismatch");

static_assert(sizeof(Vector2us) == 4, "Vector2us size mismatch");
static_assert(sizeof(Vector3us) == 6, "Vector3us size mismatch");
static_assert(sizeof(Vector4us) == 8, "Vector4us size mismatch");


Vector2f Add(Vector2f a, Vector2f b);
Vector2f Sub(Vector2f a, Vector2f b);
Vector2f Multiply(Vector2f a, Vector2f b);
Vector2f Scale(Vector2f v, float s);
Vector2f Divide(Vector2f v, float s);
Vector2f Splat2f(float s);
Vector2f Normalize(Vector2f a);
float Dot(Vector2f a, Vector2f b);
float Length(Vector2f v);

Vector3f Add(Vector3f a, Vector3f b);
Vector3f Sub(Vector3f a, Vector3f b);
Vector3f Multiply(Vector3f a, Vector3f b);
Vector3f Scale(Vector3f v, float s);
Vector3f Divide(Vector3f v, float s);
Vector3f Splat3f(float s);
Vector3f Normalize(Vector3f a);
Vector3f Cross(Vector3f a, Vector3f b);
float Dot(Vector3f a, Vector3f b);
float Length(Vector3f v);

Vector4f Add(Vector4f a, Vector4f b);
Vector4f Sub(Vector4f a, Vector4f b);
Vector4f Multiply(Vector4f a, Vector4f b);
Vector4f Scale(Vector4f v, float s);
Vector4f Divide(Vector4f v, float s);
Vector4f Splat4f(float s);
Vector4f Normalize(Vector4f a);
float Dot(Vector4f a, Vector4f b);
float Length(Vector4f v);

Vector2f operator+(Vector2f a, Vector2f b);
Vector2f operator-(Vector2f a, Vector2f b);
Vector2f operator*(Vector2f v, float s);
Vector2f operator*(Vector2f a, Vector2f b);
Vector2f operator/(Vector2f v, float s);

Vector3f operator+(Vector3f a, Vector3f b);
Vector3f operator-(Vector3f a, Vector3f b);
Vector3f operator*(Vector3f a, Vector3f b);
Vector3f operator*(Vector3f v, float s);
Vector3f operator/(Vector3f v, float s);

Vector4f operator+(Vector4f a, Vector4f b);
Vector4f operator-(Vector4f a, Vector4f b);
Vector4f operator*(Vector4f v, float s);
Vector4f operator*(Vector4f a, Vector4f b);
Vector4f operator/(Vector4f v, float s);

//  (Signed 32-bit)
Vector2i Add(Vector2i a, Vector2i b);
Vector2i Sub(Vector2i a, Vector2i b);
Vector2i Multiply(Vector2i a, Vector2i b);
Vector2i Scale(Vector2i v, int s);
Vector2i Divide(Vector2i v, int s);
Vector2i Splat2i(int s);

Vector2i operator+(Vector2i a, Vector2i b);
Vector2i operator-(Vector2i a, Vector2i b);
Vector2i operator*(Vector2i a, Vector2i b);
Vector2i operator*(Vector2i v, int s);
Vector2i operator/(Vector2i v, int s);

Vector3i Add(Vector3i a, Vector3i b);
Vector3i Sub(Vector3i a, Vector3i b);
Vector3i Multiply(Vector3i a, Vector3i b);
Vector3i Scale(Vector3i v, int s);
Vector3i Divide(Vector3i v, int s);
Vector3i Splat3i(int s);

Vector3i operator+(Vector3i a, Vector3i b);
Vector3i operator-(Vector3i a, Vector3i b);
Vector3i operator*(Vector3i a, Vector3i b);
Vector3i operator*(Vector3i v, int s);
Vector3i operator/(Vector3i v, int s);


Vector4i Add(Vector4i a, Vector4i b);
Vector4i Sub(Vector4i a, Vector4i b);
Vector4i Multiply(Vector4i a, Vector4i b);
Vector4i Scale(Vector4i v, int s);
Vector4i Divide(Vector4i v, int s);
Vector4i Splat4i(int s);

Vector4i operator+(Vector4i a, Vector4i b);
Vector4i operator-(Vector4i a, Vector4i b);
Vector4i operator*(Vector4i a, Vector4i b);
Vector4i operator*(Vector4i v, int s);
Vector4i operator/(Vector4i v, int s);


//  (Unsigned 32-bit)
Vector2ui Add(Vector2ui a, Vector2ui b);
Vector2ui Sub(Vector2ui a, Vector2ui b);
Vector2ui Multiply(Vector2ui a, Vector2ui b);
Vector2ui Scale(Vector2ui v, unsigned int s);
Vector2ui Divide(Vector2ui v, unsigned int s);
Vector2ui Splat2u(unsigned int s);

Vector2ui operator+(Vector2ui a, Vector2ui b);
Vector2ui operator-(Vector2ui a, Vector2ui b);
Vector2ui operator*(Vector2ui a, Vector2ui b);
Vector2ui operator*(Vector2ui v, unsigned int s);
Vector2ui operator/(Vector2ui v, unsigned int s);

Vector3ui Add(Vector3ui a, Vector3ui b);
Vector3ui Sub(Vector3ui a, Vector3ui b);
Vector3ui Multiply(Vector3ui a, Vector3ui b);
Vector3ui Scale(Vector3ui v, unsigned int s);
Vector3ui Divide(Vector3ui v, unsigned int s);
Vector3ui Splat3u(unsigned int s);

Vector3ui operator+(Vector3ui a, Vector3ui b);
Vector3ui operator-(Vector3ui a, Vector3ui b);
Vector3ui operator*(Vector3ui a, Vector3ui b);
Vector3ui operator*(Vector3ui v, unsigned int s);
Vector3ui operator/(Vector3ui v, unsigned int s);

Vector4ui Add(Vector4ui a, Vector4ui b);
Vector4ui Sub(Vector4ui a, Vector4ui b);
Vector4ui Multiply(Vector4ui a, Vector4ui b);
Vector4ui Scale(Vector4ui v, unsigned int s);
Vector4ui Divide(Vector4ui v, unsigned int s);
Vector4ui Splat4u(unsigned int s);

Vector4ui operator+(Vector4ui a, Vector4ui b);
Vector4ui operator-(Vector4ui a, Vector4ui b);
Vector4ui operator*(Vector4ui a, Vector4ui b);
Vector4ui operator*(Vector4ui v, unsigned int s);
Vector4ui operator/(Vector4ui v, unsigned int s);

// doubles

Vector2d Add(Vector2d a, Vector2d b);
Vector2d Sub(Vector2d a, Vector2d b);
Vector2d Multiply(Vector2d a, Vector2d b);
Vector2d Scale(Vector2d v, double s);
Vector2d Divide(Vector2d v, double s);
Vector2d Splat2d(double s);

Vector2d operator+(Vector2d a, Vector2d b);
Vector2d operator-(Vector2d a, Vector2d b);
Vector2d operator*(Vector2d a, Vector2d b);
Vector2d operator*(Vector2d v, double s);
Vector2d operator/(Vector2d v, double s);


Vector3d Add(Vector3d a, Vector3d b);
Vector3d Sub(Vector3d a, Vector3d b);
Vector3d Multiply(Vector3d a, Vector3d b);
Vector3d Scale(Vector3d v, double s);
Vector3d Divide(Vector3d v, double s);
Vector3d Splat3d(double s);

Vector3d operator+(Vector3d a, Vector3d b);
Vector3d operator-(Vector3d a, Vector3d b);
Vector3d operator*(Vector3d a, Vector3d b);
Vector3d operator*(Vector3d v, double s);
Vector3d operator/(Vector3d v, double s);

Vector4d Add(Vector4d a, Vector4d b);
Vector4d Sub(Vector4d a, Vector4d b);
Vector4d Multiply(Vector4d a, Vector4d b);
Vector4d Scale(Vector4d v, double s);
Vector4d Divide(Vector4d v, double s);
Vector4d Splat4d(double s);

Vector4d operator+(Vector4d a, Vector4d b);
Vector4d operator-(Vector4d a, Vector4d b);
Vector4d operator*(Vector4d a, Vector4d b);
Vector4d operator*(Vector4d v, double s);
Vector4d operator/(Vector4d v, double s);

//long longs

Vector2ll Add(Vector2ll a, Vector2ll b);
Vector2ll Sub(Vector2ll a, Vector2ll b);
Vector2ll Multiply(Vector2ll a, Vector2ll b);
Vector2ll Scale(Vector2ll v, long long s);
Vector2ll Divide(Vector2ll v, long long s);
Vector2ll Splat2ll(long long s);

Vector2ll operator+(Vector2ll a, Vector2ll b);
Vector2ll operator-(Vector2ll a, Vector2ll b);
Vector2ll operator*(Vector2ll a, Vector2ll b);
Vector2ll operator*(Vector2ll v, long long s);
Vector2ll operator/(Vector2ll v, long long s);

Vector3ll Add(Vector3ll a, Vector3ll b);
Vector3ll Sub(Vector3ll a, Vector3ll b);
Vector3ll Multiply(Vector3ll a, Vector3ll b);
Vector3ll Scale(Vector3ll v, long long s);
Vector3ll Divide(Vector3ll v, long long s);
Vector3ll Splat3ll(long long s);

Vector3ll operator+(Vector3ll a, Vector3ll b);
Vector3ll operator-(Vector3ll a, Vector3ll b);
Vector3ll operator*(Vector3ll a, Vector3ll b);
Vector3ll operator*(Vector3ll v, long long s);
Vector3ll operator/(Vector3ll v, long long s);

Vector4ll Add(Vector4ll a, Vector4ll b);
Vector4ll Sub(Vector4ll a, Vector4ll b);
Vector4ll Multiply(Vector4ll a, Vector4ll b);
Vector4ll Scale(Vector4ll v, long long s);
Vector4ll Divide(Vector4ll v, long long s);
Vector4ll Splat4ll(long long s);

Vector4ll operator+(Vector4ll a, Vector4ll b);
Vector4ll operator-(Vector4ll a, Vector4ll b);
Vector4ll operator*(Vector4ll a, Vector4ll b);
Vector4ll operator*(Vector4ll v, long long s);
Vector4ll operator/(Vector4ll v, long long s);


Vector2ull Add(Vector2ull a, Vector2ull b);
Vector2ull Sub(Vector2ull a, Vector2ull b);
Vector2ull Multiply(Vector2ull a, Vector2ull b);
Vector2ull Scale(Vector2ull v, unsigned long long s);
Vector2ull Divide(Vector2ull v, unsigned long long s);
Vector2ull Splat2ull(unsigned long long s);

Vector2ull operator+(Vector2ull a, Vector2ull b);
Vector2ull operator-(Vector2ull a, Vector2ull b);
Vector2ull operator*(Vector2ull a, Vector2ull b);
Vector2ull operator*(Vector2ull v, unsigned long long s);
Vector2ull operator/(Vector2ull v, unsigned long long s);


Vector3ull Add(Vector3ull a, Vector3ull b);
Vector3ull Sub(Vector3ull a, Vector3ull b);
Vector3ull Multiply(Vector3ull a, Vector3ull b);
Vector3ull Scale(Vector3ull v, unsigned long long s);
Vector3ull Divide(Vector3ull v, unsigned long long s);
Vector3ull Splat3ull(unsigned long long s);

Vector3ull operator+(Vector3ull a, Vector3ull b);
Vector3ull operator-(Vector3ull a, Vector3ull b);
Vector3ull operator*(Vector3ull a, Vector3ull b);
Vector3ull operator*(Vector3ull v, unsigned long long s);
Vector3ull operator/(Vector3ull v, unsigned long long s);


Vector4ull Add(Vector4ull a, Vector4ull b);
Vector4ull Sub(Vector4ull a, Vector4ull b);
Vector4ull Multiply(Vector4ull a, Vector4ull b);
Vector4ull Scale(Vector4ull v, unsigned long long s);
Vector4ull Divide(Vector4ull v, unsigned long long s);
Vector4ull Splat4ull(unsigned long long s);

Vector4ull operator+(Vector4ull a, Vector4ull b);
Vector4ull operator-(Vector4ull a, Vector4ull b);
Vector4ull operator*(Vector4ull a, Vector4ull b);
Vector4ull operator*(Vector4ull v, unsigned long long s);
Vector4ull operator/(Vector4ull v, unsigned long long s);

//chars

Vector2uc Add(Vector2uc a, Vector2uc b);
Vector2uc Sub(Vector2uc a, Vector2uc b);
Vector2uc Multiply(Vector2uc a, Vector2uc b);
Vector2uc Scale(Vector2uc v, unsigned char s);
Vector2uc Divide(Vector2uc v, unsigned char s);
Vector2uc Splat2uc(unsigned char s);

Vector2uc operator+(Vector2uc a, Vector2uc b);
Vector2uc operator-(Vector2uc a, Vector2uc b);
Vector2uc operator*(Vector2uc a, Vector2uc b);
Vector2uc operator*(Vector2uc v, unsigned char s);
Vector2uc operator/(Vector2uc v, unsigned char s);

Vector3uc Add(Vector3uc a, Vector3uc b);
Vector3uc Sub(Vector3uc a, Vector3uc b);
Vector3uc Multiply(Vector3uc a, Vector3uc b);
Vector3uc Scale(Vector3uc v, unsigned char s);
Vector3uc Divide(Vector3uc v, unsigned char s);
Vector3uc Splat3uc(unsigned char s);

Vector3uc operator+(Vector3uc a, Vector3uc b);
Vector3uc operator-(Vector3uc a, Vector3uc b);
Vector3uc operator*(Vector3uc a, Vector3uc b);
Vector3uc operator*(Vector3uc v, unsigned char s);
Vector3uc operator/(Vector3uc v, unsigned char s);

Vector4uc Add(Vector4uc a, Vector4uc b);
Vector4uc Sub(Vector4uc a, Vector4uc b);
Vector4uc Multiply(Vector4uc a, Vector4uc b);
Vector4uc Scale(Vector4uc v, unsigned char s);
Vector4uc Divide(Vector4uc v, unsigned char s);
Vector4uc Splat4uc(unsigned char s);

Vector4uc operator+(Vector4uc a, Vector4uc b);
Vector4uc operator-(Vector4uc a, Vector4uc b);
Vector4uc operator*(Vector4uc a, Vector4uc b);
Vector4uc operator*(Vector4uc v, unsigned char s);
Vector4uc operator/(Vector4uc v, unsigned char s);

Vector2c Add(Vector2c a, Vector2c b);
Vector2c Sub(Vector2c a, Vector2c b);
Vector2c Multiply(Vector2c a, Vector2c b);
Vector2c Scale(Vector2c v, char s);
Vector2c Divide(Vector2c v, char s);
Vector2c Splat2c(char s);

Vector2c operator+(Vector2c a, Vector2c b);
Vector2c operator-(Vector2c a, Vector2c b);
Vector2c operator*(Vector2c a, Vector2c b);
Vector2c operator*(Vector2c v, char s);
Vector2c operator/(Vector2c v, char s);

Vector3c Add(Vector3c a, Vector3c b);
Vector3c Sub(Vector3c a, Vector3c b);
Vector3c Multiply(Vector3c a, Vector3c b);
Vector3c Scale(Vector3c v, char s);
Vector3c Divide(Vector3c v, char s);
Vector3c Splat3c(char s);

Vector3c operator+(Vector3c a, Vector3c b);
Vector3c operator-(Vector3c a, Vector3c b);
Vector3c operator*(Vector3c a, Vector3c b);
Vector3c operator*(Vector3c v, char s);
Vector3c operator/(Vector3c v, char s);

Vector4c Add(Vector4c a, Vector4c b);
Vector4c Sub(Vector4c a, Vector4c b);
Vector4c Multiply(Vector4c a, Vector4c b);
Vector4c Scale(Vector4c v, char s);
Vector4c Divide(Vector4c v, char s);
Vector4c Splat4c(char s);

Vector4c operator+(Vector4c a, Vector4c b);
Vector4c operator-(Vector4c a, Vector4c b);
Vector4c operator*(Vector4c a, Vector4c b);
Vector4c operator*(Vector4c v, char s);
Vector4c operator/(Vector4c v, char s);

//shorts

Vector2s Add(Vector2s a, Vector2s b);
Vector2s Sub(Vector2s a, Vector2s b);
Vector2s Multiply(Vector2s a, Vector2s b);
Vector2s Scale(Vector2s v, short s);
Vector2s Divide(Vector2s v, short s);
Vector2s Splat2s(short s);

Vector2s operator+(Vector2s a, Vector2s b);
Vector2s operator-(Vector2s a, Vector2s b);
Vector2s operator*(Vector2s a, Vector2s b);
Vector2s operator*(Vector2s v, short s);
Vector2s operator/(Vector2s v, short s);


Vector3s Add(Vector3s a, Vector3s b);
Vector3s Sub(Vector3s a, Vector3s b);
Vector3s Multiply(Vector3s a, Vector3s b);
Vector3s Scale(Vector3s v, short s);
Vector3s Divide(Vector3s v, short s);
Vector3s Splat3s(short s);

Vector3s operator+(Vector3s a, Vector3s b);
Vector3s operator-(Vector3s a, Vector3s b);
Vector3s operator*(Vector3s a, Vector3s b);
Vector3s operator*(Vector3s v, short s);
Vector3s operator/(Vector3s v, short s);


Vector4s Add(Vector4s a, Vector4s b);
Vector4s Sub(Vector4s a, Vector4s b);
Vector4s Multiply(Vector4s a, Vector4s b);
Vector4s Scale(Vector4s v, short s);
Vector4s Divide(Vector4s v, short s);
Vector4s Splat4s(short s);

Vector4s operator+(Vector4s a, Vector4s b);
Vector4s operator-(Vector4s a, Vector4s b);
Vector4s operator*(Vector4s a, Vector4s b);
Vector4s operator*(Vector4s v, short s);
Vector4s operator/(Vector4s v, short s);


Vector2us Add(Vector2us a, Vector2us b);
Vector2us Sub(Vector2us a, Vector2us b);
Vector2us Multiply(Vector2us a, Vector2us b);
Vector2us Scale(Vector2us v, unsigned short s);
Vector2us Divide(Vector2us v, unsigned short s);
Vector2us Splat2us(unsigned short s);

Vector2us operator+(Vector2us a, Vector2us b);
Vector2us operator-(Vector2us b, Vector2us a);
Vector2us operator*(Vector2us a, Vector2us b);
Vector2us operator*(Vector2us v, unsigned short s);
Vector2us operator/(Vector2us v, unsigned short s);


Vector3us Add(Vector3us a, Vector3us b);
Vector3us Sub(Vector3us a, Vector3us b);
Vector3us Multiply(Vector3us a, Vector3us b);
Vector3us Scale(Vector3us v, unsigned short s);
Vector3us Divide(Vector3us v, unsigned short s);
Vector3us Splat3us(unsigned short s);

Vector3us operator+(Vector3us a, Vector3us b);
Vector3us operator-(Vector3us a, Vector3us b);
Vector3us operator*(Vector3us a, Vector3us b);
Vector3us operator*(Vector3us v, unsigned short s);
Vector3us operator/(Vector3us v, unsigned short s);


Vector4us Add(Vector4us a, Vector4us b);
Vector4us Sub(Vector4us a, Vector4us b);
Vector4us Multiply(Vector4us a, Vector4us b);
Vector4us Scale(Vector4us v, unsigned short s);
Vector4us Divide(Vector4us v, unsigned short s);
Vector4us Splat4us(unsigned short s);

Vector4us operator+(Vector4us a, Vector4us b);
Vector4us operator-(Vector4us a, Vector4us b);
Vector4us operator*(Vector4us a, Vector4us b);
Vector4us operator*(Vector4us v, unsigned short s);
Vector4us operator/(Vector4us v, unsigned short s);



struct Matrix2f
{
	Matrix2f() = default;
	Matrix2f(Vector2f _x, Vector2f _y)
	{
		x = _x;
		y = _y;
	}

	Matrix2f(const Matrix2f& old) {
		x = old.x;
		y = old.y;
	};

	float* operator[](int x)
	{
		return &comp[x * 2];
	}


	union {
		struct {
			Vector2f x;
			Vector2f y;
		};

		float comp[4];
	};
};


struct Matrix3f
{

	Matrix3f() = default;
	Matrix3f(Vector3f _x, Vector3f _y, Vector3f _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}

	Matrix3f(const Matrix3f& old) {
		x = old.x;
		y = old.y;
		z = old.z;
	};

	float* operator[](int x)
	{
		return &comp[x * 3];
	}

	union {
		struct {
			Vector3f right;
			Vector3f up;
			Vector3f forward;
		};

		struct {
			Vector3f x;
			Vector3f y;
			Vector3f z;
		};

		float comp[9];
	};
};

struct Matrix4f
{
	Matrix4f() = default;
	Matrix4f(Vector4f _x, Vector4f _y, Vector4f _z, Vector4f _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	Matrix4f(const Matrix4f& old) {
		x = old.x;
		y = old.y;
		z = old.z;
		w = old.w;
	};

	float* operator[](int x)
	{
		return &comp[x * 4];
	}

	union {
		struct {
			Vector4f right;
			Vector4f up;
			Vector4f forward;
			Vector4f translate;
		};

		struct {
			Vector4f x;
			Vector4f y;
			Vector4f z;
			Vector4f w;
		};

		float comp[16];
	};
};


Matrix2f Add(Matrix2f a, Matrix2f b);
Matrix2f Sub(Matrix2f a, Matrix2f b);
Matrix2f Scale(Matrix2f m, float s);


Matrix2f Multiply(Matrix2f a, Matrix2f b);    
Vector2f Multiply(Matrix2f m, Vector2f v); 
Vector2f Multiply(Vector2f v, Matrix2f m);
Matrix2f Transpose(Matrix2f m);
float    Determinant(Matrix2f m);
Matrix2f Inverse(Matrix2f m);
Matrix2f Identity2f();


Matrix2f operator+(Matrix2f a, Matrix2f b);
Matrix2f operator-(Matrix2f a, Matrix2f b);
Matrix2f operator*(Matrix2f a, Matrix2f b);
Vector2f operator*(Matrix2f m, Vector2f v);
Vector2f operator*(Vector2f v, Matrix2f m);
Matrix2f operator*(Matrix2f m, float s);



Matrix3f Add(Matrix3f a, Matrix3f b);
Matrix3f Sub(Matrix3f a, Matrix3f b);
Matrix3f Scale(Matrix3f m, float s);


Matrix3f Multiply(Matrix3f a, Matrix3f b);    
Vector3f Multiply(Matrix3f m, Vector3f v); 
Vector3f Multiply(Vector3f v, Matrix3f m);
Matrix3f Transpose(Matrix3f m);
float    Determinant(Matrix3f m);
Matrix3f Inverse(Matrix3f m);
Matrix3f Identity3f();


Matrix3f operator+(Matrix3f a, Matrix3f b);
Matrix3f operator-(Matrix3f a, Matrix3f b);
Matrix3f operator*(Matrix3f a, Matrix3f b);
Vector3f operator*(Matrix3f m, Vector3f v);
Vector3f operator*(Vector3f v, Matrix3f m);
Matrix3f operator*(Matrix3f m, float s);



Matrix4f Add(Matrix4f a, Matrix4f b);
Matrix4f Sub(Matrix4f a, Matrix4f b);
Matrix4f Scale(Matrix4f m, float s);


Matrix4f Multiply(Matrix4f a, Matrix4f b);   
Vector4f Multiply(Matrix4f m, Vector4f v); 
Vector4f Multiply(Vector4f v, Matrix4f m);
Matrix4f Transpose(Matrix4f m);
Matrix4f Inverse(Matrix4f m);                 
Matrix4f Identity4f();


Matrix4f operator+(Matrix4f a, Matrix4f b);
Matrix4f operator-(Matrix4f a, Matrix4f b);
Matrix4f operator*(Matrix4f a, Matrix4f b);
Vector4f operator*(Matrix4f m, Vector4f v);
Vector4f operator*(Vector4f v, Matrix4f m);
Matrix4f operator*(Matrix4f m, float s);


Matrix3f CreateRotationMatrix(const Vector3f& up, float angle);

Matrix4f CreateRotationMatrixMat4(const Vector3f& up, float angle);