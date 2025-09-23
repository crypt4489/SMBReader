#pragma once

#include <vector>
#include <functional>

#include "AppTexture.h"
#include "Mesh.h"
#include "SMBFile.h"

class GenericObject
{
public:
	GenericObject(const SMBFile& file, RenderingBackend be, size_t _oi);

	~GenericObject();

	void SetMatrix(glm::mat4& f);

	void SetPerObjectCallback(std::function<void(GenericObject*)> f);

	void SetPerObjectMemoryCallback(std::function<void(void*, size_t, size_t)> ptr);

	void CallUpdate();


	EntryHandle pipelineIndex;
	std::vector<AppTexture> textures;
	size_t objectIndex;
	glm::mat4 mat;
	std::function<void(GenericObject*)> updateObject;
	std::function<void(void*, size_t, size_t)> memoryCallback;
	OffsetIndex memoryOffset;
	OffsetIndex vertexBufferMemory, indexBufferMemory;
	Mesh* m;
};

