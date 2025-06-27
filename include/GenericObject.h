#pragma once

#include <vector>
#include <functional>

#include "AppTexture.h"
#include "AppTypes.h"
#include "VKDescriptorSetCache.h"
#include "GenericObject.h"
#include "RenderInstance.h"
#include "SMBFile.h"
#include "VertexTypes.h"
#include "VKPipelineObject.h"


class GenericObject
{
public:
	GenericObject(const SMBFile& file, RenderingBackend be, size_t _oi);

	~GenericObject();

	VKPipelineObject* GetPipelineObject() const;

	void SetMatrix(glm::mat4& f);

	void SetFunctionPointer(std::function<void(GenericObject*)> f);

	void CallUpdate();

protected:
	VKPipelineObject* vkPipelineObject;
	size_t vertexCount;
	std::vector<AppTexture> textures;
	size_t objectIndex;
	glm::mat4 mat;
	std::function<void()> updateObject;
};

