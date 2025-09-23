#include "Mesh.h"
#include <glm/glm.hpp>

Mesh::Mesh(uint32_t count) {

};

Mesh::Mesh() {
    GenerateCube(this);
};

Mesh::~Mesh()
{
    free(data);
}

void Mesh::GenerateCube(Mesh* m)
{
#define BOXVERTSCOUNT 24

	static glm::vec4 BoxVerts[BOXVERTSCOUNT] =
	{
        glm::vec4(10.0,  10.0,  10.0, 1.0),
        glm::vec4(10.0,  10.0, -10.0, 1.0),
        glm::vec4(10.0, -10.0,  10.0, 1.0),
        glm::vec4(10.0, -10.0, -10.0, 1.0),
        glm::vec4(-10.0,  10.0,  10.0, 1.0),
        glm::vec4(-10.0,  10.0, -10.0, 1.0),
        glm::vec4(-10.0, -10.0,  10.0, 1.0),
        glm::vec4(-10.0, -10.0, -10.0, 1.0),
        glm::vec4(-10.0,  10.0,  10.0, 1.0),
        glm::vec4(10.0,  10.0,  10.0, 1.0),
        glm::vec4(-10.0,  10.0, -10.0, 1.0),
        glm::vec4(10.0,  10.0, -10.0, 1.0),
        glm::vec4(-10.0, -10.0,  10.0, 1.0),
        glm::vec4(10.0, -10.0,  10.0, 1.0),
        glm::vec4(-10.0, -10.0, -10.0, 1.0),
        glm::vec4(10.0, -10.0, -10.0, 1.0),
        glm::vec4(-10.0,  10.0,  10.0, 1.0),
        glm::vec4(10.0,  10.0,  10.0, 1.0),
        glm::vec4(-10.0, -10.0,  10.0, 1.0),
        glm::vec4(10.0, -10.0,  10.0, 1.0),
        glm::vec4(-10.0,  10.0, -10.0, 1.0),
        glm::vec4(10.0,  10.0, -10.0, 1.0),
        glm::vec4(-10.0, -10.0, -10.0, 1.0),
        glm::vec4(10.0, -10.0, -10.0, 1.0)
	};

	static glm::vec4 BoxTex[BOXVERTSCOUNT] =
	{
        glm::vec4(0.0, 0.0, 1.0, 0.0),
        glm::vec4(1.0, 0.0, 1.0, 0.0),
        glm::vec4(0.0, 1.0, 1.0, 0.0),
        glm::vec4(1.0, 1.0, 1.0, 0.0),
        glm::vec4(0.0, 0.0, 1.0, 0.0),
        glm::vec4(1.0, 0.0, 1.0, 0.0),
        glm::vec4(0.0, 1.0, 1.0, 0.0),
        glm::vec4(1.0, 1.0, 1.0, 0.0),
        glm::vec4(0.0, 0.0, 1.0, 0.0),
        glm::vec4(1.0, 0.0, 1.0, 0.0),
        glm::vec4(0.0, 1.0, 1.0, 0.0),
        glm::vec4(1.0, 1.0, 1.0, 0.0),
        glm::vec4(0.0, 0.0, 1.0, 0.0),
        glm::vec4(1.0, 0.0, 1.0, 0.0),
        glm::vec4(0.0, 1.0, 1.0, 0.0),
        glm::vec4(1.0, 1.0, 1.0, 0.0),
        glm::vec4(0.0, 0.0, 1.0, 0.0),
        glm::vec4(1.0, 0.0, 1.0, 0.0),
        glm::vec4(0.0, 1.0, 1.0, 0.0),
        glm::vec4(1.0, 1.0, 1.0, 0.0),
        glm::vec4(0.0, 0.0, 1.0, 0.0),
        glm::vec4(1.0, 0.0, 1.0, 0.0),
        glm::vec4(0.0, 1.0, 1.0, 0.0),
        glm::vec4(1.0, 1.0, 1.0, 0.0)
	};

    static uint32_t BoxIndices[36] = {
         0,  1,  2,
         3,  2,  1,
         6,  5,  4,
         5,  6,  7,
        10,  9,  8,
         9, 10, 11,
        12, 13, 14,
        15, 14, 13,
        16, 17, 18,
        19, 18, 17,
        22, 21, 20,
        21, 22, 23
    };

    m->vertexCount = BOXVERTSCOUNT;
    m->indexCount = 36;

    m->data = _aligned_malloc(sizeof(BoxVerts) + sizeof(BoxIndices) + sizeof(BoxTex), alignof(glm::vec4));

    
    m->vertexSize = sizeof(BoxVerts) + sizeof(BoxTex);
    m->vertexStride = sizeof(glm::vec4) * 2;

    m->vertexPositon = 0;

    glm::vec4* ptr = (glm::vec4*)m->data;

    for (size_t i = 0; i < m->vertexCount; i++)
    {
        memcpy(ptr, &BoxVerts[i], sizeof(glm::vec4));
        ptr++;
        memcpy(ptr, &BoxTex[i], sizeof(glm::vec4));
        ptr++;
    }

    m->indexPosition = sizeof(BoxVerts) + sizeof(BoxTex);
    m->indexStride = sizeof(uint32_t);
    m->indexSize = sizeof(BoxIndices);

    uint32_t* intPtr = (uint32_t*)ptr;

    for (size_t i = 0; i < m->indexCount; i++)
    {
        memcpy(intPtr, &BoxIndices[i], sizeof(uint32_t));
        intPtr++;
    }
	
}

void* Mesh::GetVertexData() const {
    uintptr_t head = (uintptr_t)data;
    head += vertexPositon;
    return (void*)head;
}

void* Mesh::GetIndexData() const {
    uintptr_t head = (uintptr_t)data;
    head += indexPosition;
    return (void*)head;
}