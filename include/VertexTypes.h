#pragma once
#include "AppTypes.h"
#include <vector>
#include "MathTypes.h"
#include "VKUtilities.h"



typedef struct text_vertex_t
{
	text_vertex_t() = default;
	text_vertex_t(Vector2f _p, Vector2f _t, Vector4f _c);

	Vector2f POSITION;
	Vector2f TEXTURE;
	Vector4f COLOR;

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const text_vertex_t& v);

} TextVertex;

typedef struct vertex_type_h
{
	
	Vector2f TEXTURE;
	Vector3f NORMAL;
	Vector4f POSITION;

	vertex_type_h() = default;
	vertex_type_h(Vector4f _p, Vector2f _t, Vector3f _n);

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const vertex_type_h& v);

} Vertex;

typedef struct basic_vertex_type_h
{
	Vector4f POSITION;
	Vector4f TEXTURE;

	basic_vertex_type_h() = default;
	basic_vertex_type_h(Vector4f _p, Vector4f _t);

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const basic_vertex_type_h& v);

} BasicVertex;



typedef struct pospack6_cnorm_c16tex1_bone2_type_h
{
	Vector4f POSITION;
	Vector2f TEXTURE;
	Vector3f NORMAL;
	Vector2i BONES;
	Vector2f WEIGHTS;

	pospack6_cnorm_c16tex1_bone2_type_h() = default;
	pospack6_cnorm_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector3f& _n, const Vector2i& _b, const Vector2f& _w);

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const pospack6_cnorm_c16tex1_bone2_type_h& v);

} Vertex_PosPack6_CNorm_C16Tex1_Bone2;



typedef struct pospack6_c16tex1_bone2_type_h
{
	Vector4f POSITION;
	Vector2f TEXTURE;
	Vector2i BONES;
	Vector2f WEIGHTS;

	pospack6_c16tex1_bone2_type_h() = default;
	pospack6_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector2i& _b, const Vector2f& _w);

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const pospack6_c16tex1_bone2_type_h& v);

} Vertex_PosPack6_C16Tex1_Bone2;


typedef struct pospack6_c16tex2_bone2_type_h
{
	Vector4f POSITION;
	Vector2f TEXTURE;
	Vector2f TEXTURE2;
	Vector2i BONES;
	Vector2f WEIGHTS;

	pospack6_c16tex2_bone2_type_h() = default;
	pospack6_c16tex2_bone2_type_h(
		const Vector4f& _p, 
		const Vector2f& _t, 
		const Vector2f& _t2, 
		const Vector2i& _b, 
		const Vector2f& _w);

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const pospack6_c16tex2_bone2_type_h& v);

} Vertex_PosPack6_C16Tex2_Bone2;

#pragma pack(push, 1)
typedef struct cpospack6_cnorm_c16tex1_bone2_type_h
{
	Vector2uc BONES;
	Vector2uc WEIGHTS;
	Vector2s TEXTURE;
	int NORMAL;
	Vector3s POSITION;

	cpospack6_cnorm_c16tex1_bone2_type_h() = default;
	cpospack6_cnorm_c16tex1_bone2_type_h(
		const Vector3s& _p,
		const Vector2s& _t,
		const int _n,
		const Vector2uc& _b,
		const Vector2uc& _w);

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const cpospack6_cnorm_c16tex1_bone2_type_h& v) const;

} CVertex_PosPack6_CNorm_C16Tex1_Bone2;
#pragma pack(pop)

// ==============================
// PosPack6 + C16Tex1 + Bone2
// ==============================

typedef struct cpospack6_c16tex1_bone2_type_h
{
	Vector2uc  BONES;
	Vector2uc  WEIGHTS;
	Vector2s TEXTURE;
	Vector3s POSITION;

	cpospack6_c16tex1_bone2_type_h() = default;
	cpospack6_c16tex1_bone2_type_h(
		const Vector3s& _p,
		const Vector2s& _t,
		const Vector2uc& _b,
		const Vector2uc& _w);


	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const cpospack6_c16tex1_bone2_type_h& v) const;

} CVertex_PosPack6_C16Tex1_Bone2;

#pragma pack(push, 1)
typedef struct cpospack6_c16tex2_bone2_type_h
{
	Vector2uc BONES;
	Vector2uc WEIGHTS;
	Vector2s TEXTURE;
	Vector2s TEXTURE2;
	Vector3s POSITION;
	

	cpospack6_c16tex2_bone2_type_h() = default;
	cpospack6_c16tex2_bone2_type_h(
		const Vector3s& _p, 
		const Vector2s& _t, 
		const Vector2s& _t2, 
		const Vector2uc& _b, 
		const Vector2uc& _w
		);

	static std::vector<VertexInputDescription> getAttributeDescriptions();

	bool operator==(const cpospack6_c16tex2_bone2_type_h& v);

} CVertex_PosPack6_C16Tex2_Bone2;
#pragma pack(pop)