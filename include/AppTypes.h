#pragma once

#include "IndexTypes.h"
#include <cstdint>


/* common types */

#define KiB 1024
#define MiB 1024 * KiB
#define GiB 1024 * MiB

/* Rendering State Types */

enum RenderingBackend
{
	VULKAN = 1,
	DXD12 = 2,
};

enum RasterizerTest
{
	NEVER = 0,
	LESS = 1,
	EQUAL = 2,
	LESSEQUAL = 3,
	GREATER = 4,
	NOTEQUAL = 5,
	GREATEREQUAL = 6,
	ALLPASS = 7
};

enum ImageFormat
{
	X8L8U8V8 = 0,
	DXT1 = 1,
	DXT3 = 2,
	R8G8B8A8 = 3,
	B8G8R8A8 = 4,
	D24UNORMS8STENCIL = 5,
	D32FLOATS8STENCIL = 6,
	D32FLOAT = 7,
	R8G8B8A8_UNORM = 8,
	R8G8B8 = 9,
	B8G8R8A8_UNORM = 10,
	IMAGE_UNKNOWN = 0x7fffffff
};

enum TextureIOType
{
	BMP = 0,
};

enum PrimitiveType
{
	TRIANGLES = 0,
	TRISTRIPS = 6,
	TRIFAN = 7,
	POINTSLIST = 8,
	LINELIST = 9,
	LINESTRIPS = 10
};

enum class ImageLayout
{
	UNDEFINED = 0,
	WRITEABLE = 1,
	SHADERREADABLE = 2,
	COLORATTACHMENT = 3,
	DEPTHSTENCILATTACHMENT = 4
};

enum class ImageUsage
{
	DEPTHSTENCIL = 0,
	COLOR = 1
};


/* Render Management Types */

enum class AllocationType
{
	STATIC = 0,
	PERFRAME = 1,
	PERDRAW = 2
};

enum class ComponentFormatType
{
	NO_BUFFER_FORMAT = 0,
	RAW_8BIT_BUFFER = 1,
	R32_UINT = 2,
	R32_SINT = 3,
	R32G32B32A32_SFLOAT = 4,
	R32G32B32_SFLOAT = 5,
	R32G32_SFLOAT = 6,
	R32_SFLOAT = 7,
	R32G32_SINT = 8,
	R8G8_UINT = 9,
	R16G16_SINT = 10,
	R16G16B16_SINT = 11

};


enum class TransferType
{
	CACHED = 0,
	MEMORY = 1,
};


/* Shader Resource Definitions */

enum BarrierActionBits
{
	WRITE_SHADER_RESOURCE = 1,
	READ_SHADER_RESOURCE = 2,
	READ_UNIFORM_BUFFER = 4,
	READ_VERTEX_INPUT = 8,
	READ_INDIRECT_COMMAND = 16
};

enum BarrierStageBits
{
	VERTEX_SHADER_BARRIER = 1,
	VERTEX_INPUT_BARRIER = 2,
	COMPUTE_BARRIER = 4,
	FRAGMENT_BARRIER = 8,
	BEGINNING_OF_PIPE = 16,
	INDIRECT_DRAW_BARRIER = 32,
};

typedef int BarrierAction;

typedef int BarrierStage;

enum class MemoryBarrierType
{
	MEMORY_BARRIER = 0,
	IMAGE_BARRIER = 1,
	BUFFER_BARRIER = 2,
	BARRIER_MAX_ENUM
};

struct ShaderResourceBarrier
{
	MemoryBarrierType type;
	BarrierStage srcStage;
	BarrierStage dstStage;
	BarrierAction srcAction;
	BarrierAction dstAction;
};

struct ImageShaderResourceBarrier : public ShaderResourceBarrier
{
	ImageLayout srcResourceLayout;
	ImageLayout dstResourceLayout;
	ImageUsage imageType;
};

struct ShaderResourceBufferBarrier : public ShaderResourceBarrier
{
};

enum class ShaderResourceType
{
	SAMPLER2D = 1,
	STORAGE_BUFFER = 2,
	UNIFORM_BUFFER = 4,
	CONSTANT_BUFFER = 8,
	IMAGESTORE2D = 16,
	IMAGESTORE3D = 32,
	IMAGE2D = 33,
	IMAGE3D = 34,
	BUFFER_VIEW = 128,
	SAMPLER3D = 129,
	SAMPLERCUBE = 130,
	SAMPLERSTATE = 131,
	INVALID_SHADER_RESOURCE = 0x7FFFFFFF
};

enum class ShaderResourceAction
{
	SHADERREAD = 1,
	SHADERWRITE = 2,
	SHADERREADWRITE = 3
};

enum ShaderStageTypeBits
{
	VERTEXSHADERSTAGE = 1,
	FRAGMENTSHADERSTAGE = 2,
	COMPUTESHADERSTAGE = 4,
};

typedef int ShaderStageType;


struct ShaderSetLayout
{
	int vulkanDescLayout;
	int bindingCount;
	int resourceStart;
};

struct ShaderResource
{
	ShaderStageType stages;
	ShaderResourceAction action;
	ShaderResourceType type;
	int set;
	int binding;
	int arrayCount;
	int size;
	int offset;
};

struct ShaderResourceSet
{
	int bindingCount;
	int layoutHandle;
	int setCount;
	int barrierCount;
};

struct ShaderResourceHeader
{
	ShaderResourceType type;
	ShaderResourceAction action;
	int binding;
	int arrayCount;
};

struct ShaderResourceSampler : public ShaderResourceHeader
{
	EntryHandle* samplerHandles;
	int samplerCount;
	int firstSampler;
};

struct ShaderResourceImage : public ShaderResourceHeader
{
	EntryHandle* textureHandles;
	int textureCount;
	int firstTexture;
};

struct ShaderResourceBuffer : public ShaderResourceHeader
{
	int allocation;
	int offset;
};

struct ShaderResourceBufferView : public ShaderResourceHeader
{
	int subAllocations;
	int allocationIndex;
};


struct ShaderResourceConstantBuffer : public ShaderResourceHeader
{
	ShaderStageType stage;
	int size;
	int offset;
	void* data;
};

struct ShaderComputeLayout
{
	unsigned long x;
	unsigned long y;
	unsigned long z;
};

/* Shader Resource Update */

struct ShaderResourceUpdate
{
	ShaderResourceType type;
	int descriptorSet;
	int bindingIndex;
	int copyCount;
	void* data;
	int dataSize;
};

struct ResourceArrayUpdate
{
	int dstBegin;
	int count;
	EntryHandle* handles;
};



/* Intermediary Pipeline Object */

struct GraphicsIntermediaryPipelineInfo
{
	uint32_t drawType;
	int vertexBufferHandle;
	uint32_t vertexCount;
	uint32_t pipelinename;
	uint32_t descCount;
	int* descriptorsetid;
	int indexBufferHandle;
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t indexSize;
	uint32_t indexOffset;
	uint32_t vertexOffset;
	int indirectAllocation;
	int indirectDrawCount;
	int indirectCountAllocation;
};

struct ComputeIntermediaryPipelineInfo
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t pipelinename;
	uint32_t descCount;
	int* descriptorsetid;
};



/* Host memory update */

struct HostTransferRegion
{
	TransferType type;
	int size;
	int copyCount;
	int allocationIndex;
	int allocoffset;
	void* data;
};

struct DeviceTransferRegion
{
	TransferType transferType;
	int size;
	int copyCount;
	int allocationIndex;
	int allocoffset;
	void* data;
};

struct TextureMemoryRegion
{
	void* data;
	uint32_t* imageSizes;
	size_t totalSize;
	EntryHandle textureIndex;
	int width;
	int height;
	int mipLevels;
	int layers;
	ImageFormat format;
};

struct TransferRegionLink
{
	int region;
	int next;
};

struct TransferCommand
{
	int fillVal;
	int size;
	int offset;
	int allocationIndex;
	int copycount;
	BarrierStage dstStage;
	BarrierAction dstAction;
};

 /* Allocation management */
struct RenderAllocation
{
	EntryHandle memIndex;
	size_t offset;
	size_t deviceAllocSize;
	size_t requestedSize;
	size_t alignment;
	AllocationType allocType;
	ComponentFormatType formatType;
	EntryHandle viewIndex;
};


/* */

enum class VertexUsage
{
	POSITION = 0,
	TEX0 = 1,
	TEX1 = 2,
	TEX2 = 3,
	TEX3 = 4,
	NORMAL = 5,
	BONES = 6,
	WEIGHTS = 7,
	COLOR0 = 8
};

enum class VertexBufferRate
{
	PERVERTEX = 0,
	PERINSTANCE = 1,
};

struct VertexInputDescription
{
	ComponentFormatType format;
	int byteoffset;
	VertexUsage vertexusage;
};

struct VertexBufferDescription
{
	VertexBufferRate rate;
	int descCount;
	int perInputSize;
	VertexInputDescription descriptions[10];
};

enum TriangleWinding
{
	CW = 0,
	CCW = 1
};

enum class CullMode
{
	CULL_NONE = 0,
	CULL_BACK = 1,
	CULL_FRONT = 2,
};

enum class BlendOp
{
	LOGIC_COPY = 1
};

enum class StencilOp
{
	REPLACE = 0,
	KEEP = 1,
	ZERO = 2,
};

struct FaceStencilData
{
	StencilOp failOp;
	StencilOp passOp;
	StencilOp depthFailOp;
	RasterizerTest stencilCompare;
	int writeMask;
	int compareMask;
	int reference;
};

struct GenericPipelineStateInfo
{
	PrimitiveType primType;
	float lineWidth;
	TriangleWinding windingOrder;
	bool depthEnable;
	bool depthWrite;
	RasterizerTest depthTest;
	bool StencilEnable;
	FaceStencilData frontFace;
	FaceStencilData backFace;
	int sampleCountLow;
	int sampleCountHigh;
	ImageFormat colorFormat;
	ImageFormat depthFormat;
	BlendOp blendOp;
	CullMode cullMode;
	int vertexBufferDescCount;
	VertexBufferDescription vertexBufferDesc[4];
};

enum AppPipelineHandleType
{
	COMPUTESO,
	GRAPHICSO,
	INDIRECTSO,
};

struct PipelineHandle
{
	int group;
	int indexForHandles;
	int numHandles;
	int graphIndex;
	int graphCount;
};