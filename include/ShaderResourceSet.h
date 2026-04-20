#pragma once

#include <array>
#include <atomic>

#include "AppAllocator.h"
#include "AppTypes.h"
#include "Logger.h"
#include "FileManager.h"
#include "ResourceDependencies.h"


BarrierStage ConvertShaderStageToBarrierStage(ShaderStageType type);


struct ShaderResourceSetContext
{
	Logger* contextLogger;
	bool contextFailed;
};



template <int N>
struct ShaderResourceManager
{
	SlabAllocator shaderResourceInstAllocator;
	std::array<uintptr_t, N> descriptorSets{};
	std::array<EntryHandle, N> vkDescriptorSets{};
	std::atomic<int> descriptorSetIndex{};

	EntryHandle deviceResourceHeap;

	ShaderResourceManager() = default;

	ShaderResourceManager(void* srSlabHead, int srSlabSize) :
		shaderResourceInstAllocator{srSlabHead, srSlabSize},
		deviceResourceHeap(EntryHandle())
	{

	}

	int AddShaderToSets(uintptr_t location)
	{
		int indexRet = descriptorSetIndex.fetch_add(1);

		descriptorSets[indexRet] = location;

		return indexRet;
	}

	void SetVariableArrayCount(ShaderResourceSetContext* context, int descriptorSet, int bindingIndex, int varArrayCount)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		
		uintptr_t* setOffsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceHeader* header = (ShaderResourceHeader*)setOffsets[bindingIndex];

		if (!(header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY))
		{
			//do something
		}

		header->arrayCount = (header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY) | (varArrayCount & DESCRIPTOR_COUNT_MASK);

	}

	void BindBufferToShaderResource(ShaderResourceSetContext* context, int descriptorSet, int* allocationIndex, int* offsets,  int firstBuffer, int bufferCount, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* setOffsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceBuffer* header = (ShaderResourceBuffer*)setOffsets[bindingIndex];

		if (header->type != ShaderResourceType::UNIFORM_BUFFER && header->type != ShaderResourceType::STORAGE_BUFFER)
			return;

		header->allocationIndex = allocationIndex;
		header->offsets = offsets;
		header->firstBuffer = firstBuffer;
		header->bufferCount = bufferCount;
	}

	void BindImageResourceToShaderResource(ShaderResourceSetContext* context, int descriptorSet, EntryHandle* index, int textureCount, int firstTexture, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::IMAGESTORE2D && header->type != ShaderResourceType::IMAGE2D)
			return;

		header->textureHandles = index;
		header->textureCount = textureCount;
		header->firstTexture = firstTexture;
	}

	void BindSamplerResourceToShaderResource(int descriptorSet, EntryHandle* indices, int samplerCount, int firstSampler, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceSampler* header = (ShaderResourceSampler*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLERSTATE)
			return;

		header->samplerHandles = indices;
		header->samplerCount = samplerCount;
		header->firstSampler = firstSampler;
	}

	void BindSampledImageToShaderResource(ShaderResourceSetContext* context, int descriptorSet, EntryHandle* index, int textureCount, int firstTexture, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLER2D && header->type != ShaderResourceType::SAMPLERCUBE && header->type != ShaderResourceType::SAMPLER3D)
			return;

		header->textureHandles = index;
		header->textureCount = textureCount;
		header->firstTexture = firstTexture;
	}

	void BindSampledImageArrayToShaderResource(ShaderResourceSetContext* context, int descriptorSet, EntryHandle* indices, int texCount, int firstTexture, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLER3D && header->type != ShaderResourceType::SAMPLER2D && header->type != ShaderResourceType::SAMPLERCUBE)
			return;

		header->textureHandles = indices;
		header->textureCount = texCount;
		header->firstTexture = firstTexture;
	}

	void BindBufferView(ShaderResourceSetContext* context, int descriptorSet, int* allocationIndex, int firstView, int viewCount, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceBufferView* header = (ShaderResourceBufferView*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::BUFFER_VIEW)
			return;


		header->viewCount = viewCount;
		header->firstView = firstView;
		header->allocationIndex = allocationIndex;
	}

	void BindBarrier(ShaderResourceSetContext* context, int descriptorSet, int binding, BarrierStage stage, BarrierAction action)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));


		head = offsets[binding];
		ShaderResourceHeader* desc = (ShaderResourceHeader*)offsets[binding];



		switch (desc->type)
		{
		case ShaderResourceType::IMAGE2D:
		case ShaderResourceType::IMAGESTORE2D:
		case ShaderResourceType::SAMPLER2D:
		{
			head += sizeof(ShaderResourceImage);
			ShaderResourceBarrier* barrier = (ShaderResourceBarrier*)head;

			barrier->dstAction = action;
			barrier->dstStage = stage;
			break;
		}
		case ShaderResourceType::BUFFER_VIEW:
		{
			head += sizeof(ShaderResourceBufferView);
			ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)head;
			barrier->dstStage = stage;
			barrier->dstAction = action;
			break;
		}
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:
		{

			head += sizeof(ShaderResourceBuffer);
			ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)head;
			barrier->dstStage = stage;
			barrier->dstAction = action;
			break;
		}
		}


	}

	void BindImageBarrier(ShaderResourceSetContext* context, int descriptorSet, int binding, int barrierIndex, BarrierStage stage, BarrierAction action, ImageLayout oldLayout, ImageLayout dstLayout, bool location)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));


		head = offsets[binding];
		ShaderResourceHeader* desc = (ShaderResourceHeader*)offsets[binding];

		if (desc->type != ShaderResourceType::SAMPLER2D && desc->type != ShaderResourceType::SAMPLERCUBE && desc->type != ShaderResourceType::SAMPLER3D && desc->type != ShaderResourceType::IMAGESTORE2D && desc->type != ShaderResourceType::IMAGE2D)
			return;

		switch (desc->type)
		{
		case ShaderResourceType::IMAGE2D:
		case ShaderResourceType::IMAGESTORE2D:
		case ShaderResourceType::SAMPLER2D:
			head += sizeof(ShaderResourceImage);

			break;
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:

			head += sizeof(ShaderResourceBuffer);
			break;
		}


		ImageShaderResourceBarrier* imageBarrier = (ImageShaderResourceBarrier*)head;


		imageBarrier[barrierIndex].imageType = ImageUsage::COLOR;
		imageBarrier[barrierIndex].dstResourceLayout = dstLayout;
		imageBarrier[barrierIndex].srcResourceLayout = oldLayout;

		if (location)
		{
			imageBarrier[barrierIndex].dstAction = action;
			imageBarrier[barrierIndex].dstStage = stage;
		}
		else {
			imageBarrier[barrierIndex].srcAction = action;
			imageBarrier[barrierIndex].srcStage = stage;
		}
	}

	ShaderResourceHeader* GetConstantBuffer(int descriptorSet, int constantBuffer)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceHeader* ret = (ShaderResourceHeader*)(offsets[set->bindingCount - (constantBuffer + 1)]);

		if (ret->type != ShaderResourceType::CONSTANT_BUFFER) return nullptr;

		return ret;
	}

	int GetConstantBufferCount(int descriptorSet)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		int iter = set->bindingCount - 1;

		int count = 0;

		while (iter >= 0)
		{
			ShaderResourceHeader* ret = (ShaderResourceHeader*)(offsets[iter--]);
			if (ret->type == ShaderResourceType::CONSTANT_BUFFER) count++;
			else break;
		}

		return count;
	}

	int GetBarrierCount(int descriptorSet)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		return set->barrierCount;
	}

	void UploadConstant(ShaderResourceSetContext* context, int descriptorset, void* data, int bufferLocation)
	{
		ShaderResourceConstantBuffer* header = (ShaderResourceConstantBuffer*)GetConstantBuffer(descriptorset, bufferLocation);
		if (!header) return;
		header->data = data;
	}
};






struct ShaderXMLTag
{
	unsigned long hashCode;
};

struct ShaderGLSLShaderXMLTag : ShaderXMLTag //followed by shaderNameLen Bytes
{
	ShaderStageType type;
};

struct ShaderComputeLayoutXMLTag : ShaderXMLTag
{
	ShaderComputeLayout comps;
};

struct ShaderResourceItemXMLTag : ShaderXMLTag
{
	ShaderStageType shaderstage;
	ShaderResourceType resourceType;
	ShaderResourceAction resourceAction;
	int arrayCount;
	int size;
	int offset;
	int pushRangeStage;
};

struct ShaderResourceSetXMLTag : ShaderXMLTag
{
	int resourceCount;
};

static constexpr unsigned long
hash(const char* str);

ShaderGraph* CreateShaderGraph(StringView filename, RingAllocator* readerMemory, Allocator* graphAllocator, Allocator* shaderAllocator, int* shaderDetailCount);

static int ProcessTag(char* fileData, int size, int currentLocation, unsigned long* hash, bool* opening);

static int SkipLine(char* fileData, int size, int currentLocation);
static int ReadValue(char* fileData, int size, int currentLocation, char* str, int* len);

static int ReadAttributeName(char* fileData, int size, int currentLocation, unsigned long* hash);

static int ReadAttributeValueHash(char* fileData, int size, int currentLocation, unsigned long* hash);

static int ReadAttributeValueVal(char* fileData, int size, int currentLocation, unsigned long* val);

static int ReadAttributes(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize);

static int HandleGLSLShader(char* fileData, int size, int currentLocation, uintptr_t* offset, Allocator* shaderAllocator);

static int HandleShaderResourceItem(char* fileData, int size, int currentLocation, uintptr_t* offset);

static constexpr int ASCIIToInt(char* str);

static int HandleComputeLayout(char* fileData, int size, int currentLocation, uintptr_t* offset);




struct PipelineXMLTag
{
	unsigned long hashCode;
};

struct PipelineDescriptionXMLTag : PipelineXMLTag //followed by shaderNameLen Bytes
{
	int sampLo;
	int sampHi;
};

struct PrimitiveXMLTag : PipelineXMLTag
{
	PrimitiveType primType;
};

struct DepthXMLTag : PipelineXMLTag
{
	bool enabled;
	RasterizerTest depthOp;
};

struct CullModeXMLTag : PipelineXMLTag
{
	CullMode mode;
};


void CreatePipelineDescription(StringView filename, GenericPipelineStateInfo* stateInfo, RingAllocator* tempAllocator);



static int HandlePipelineDescription(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo);
static int HandleCullMode(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo);
static int HandleDepthTest(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo);
static int HandleStencilTest(char* fileData, int size, int currentLocation, FaceStencilData* face);
static int HandlePrimitiveType(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo);

static int HandleVertexComponentInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation, int perVertexSlotLocation);

static int HandleVertexInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation);


void CreateAttachmentGraphFromFile(StringView filename, AttachmentGraph* graph, RingAllocator* inputScratchAllocator);
static int ReadAttributesAttachments(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize);
static int HandleAttachment(char* fileData, int size, int currentLocation, AttachmentDescriptionType descType, AttachmentDescription* description);
static int HandleAttachmentDesc(char* fileData, int size, int currentLocation, AttachmentRenderPass* holder);
static int HandleAttachmentResource(char* fileData, int size, int currentLocation, AttachmentResource* resource);