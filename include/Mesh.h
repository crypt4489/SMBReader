#pragma once
#include <cstdint>
#include "AppTypes.h"
#if 0
struct Mesh
{
	uint32_t vertexCount;
	size_t vertexPositon;
	size_t vertexSize;
	size_t vertexStride;
	uint32_t indexCount;
	size_t indexSize;
	size_t indexPosition;
	size_t indexStride;
	void* data;
	PrimitiveType type;

	Mesh(uint32_t count);

	Mesh();

	~Mesh();

	void* GetVertexData() const;

	void* GetIndexData() const;

	static void GenerateCube(Mesh* m);
};
#endif
