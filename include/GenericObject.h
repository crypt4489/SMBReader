#pragma once

#include <vector>
#include <functional>

#include "AppTexture.h"
#include "Mesh.h"
#include "SMBFile.h"

struct GenericObject
{
public:
	GenericObject(RenderingBackend be, size_t _oi);

	~GenericObject();

	void CallUpdate();


	EntryHandle pipelineIndex;
	size_t objectIndex;
	glm::mat4 mat;
	std::function<void(GenericObject*)> updateObject;
	std::function<void(void*, size_t, size_t)> memoryCallback;
	int objVertexMemoryIndex, objIndexMemoryIndex;
	float interpolate = 0.0f;
	std::array<int, 2> objSpecificMemIndex;
	int computeHandle;
	Mesh* m;
};

