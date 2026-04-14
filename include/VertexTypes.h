#pragma once
#include "AppTypes.h"
#include <vector>
#include "AppAllocator.h"
#include "MathTypes.h"
#include "VKUtilities.h"



typedef struct text_vertex_t
{
	text_vertex_t() = default;
	text_vertex_t(Vector2f _p, Vector2f _t, Vector4f _c);

	Vector2f POSITION;
	Vector2f TEXTURE;
	Vector4f COLOR;

	static std::array<VertexInputDescription, 3> getAttributeDescriptions();

	bool operator==(const text_vertex_t& v);

} TextVertex;

typedef struct vertex_type_h
{
	
	Vector2f TEXTURE;
	Vector3f NORMAL;
	Vector4f POSITION;

	vertex_type_h() = default;
	vertex_type_h(Vector4f _p, Vector2f _t, Vector3f _n);

	static std::array<VertexInputDescription, 3> getAttributeDescriptions();

	bool operator==(const vertex_type_h& v);

} Vertex;

typedef struct basic_vertex_type_h
{
	Vector4f POSITION;
	Vector4f TEXTURE;

	basic_vertex_type_h() = default;
	basic_vertex_type_h(Vector4f _p, Vector4f _t);

	static std::array<VertexInputDescription, 2> getAttributeDescriptions();

	bool operator==(const basic_vertex_type_h& v);

} BasicVertex;



Vector3f DecompressPosition(Vector3s vector, AxisBox& box);

Vector3s CompressPosition(Vector4f vector, AxisBox& box);
Vector3s CompressPosition(Vector3f vector, AxisBox& box);

int32_t CompressNormal (Vector3f normal);

Vector2s CompressTexCoords(Vector2f in);
Vector2f DecompressTexCoords(Vector2s in);

int32_t CompressTangent(Vector4f tangent);
Vector4f DecompressTangent(int32_t ctangent);

int32_t CompressColor(Vector4f color);

void CreateBitTangentFromNormal(Vector4f* pos, Vector2f* uvs, uint16_t* indices, int totalIndexCount, int totalVertCount, Vector4f* tangents, Vector3f* outNormals, RingAllocator* tempAllocator);

int CompressMeshFromVertexStream(VertexInputDescription* inputDesc, int descCount, int vertexStride, int vertexCount,
	AxisBox& box, void* vertexStream, void* memoryOut, int* compressedSize, int* vertexFlags);

int GetCompressedSize(VertexInputDescription* inputDesc, int descCount);