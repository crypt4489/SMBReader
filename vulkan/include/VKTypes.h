#pragma once

#define MINOR_CODE_PACK(x) ((int)x << 6)
#define GET_MINOR_CODE(x) ((int)((x) &= 0x0000FFC0) >> 6)
#define GET_MAJOR_CODE(x) ((int)((x) &= 0x0000003F))

struct VKInstance;
struct VKDevice;
struct VKSwapChain;
struct VKPipelineCache;
struct VKTexture;
struct VKPipelineObject;
struct VKDescriptorLayoutCache;
struct VKDescriptorSetCache;
struct DescriptorSetLayoutBuilder;
struct VKRenderPassBuilder;
struct RecordingBufferObject;
struct VKGraphicsPipelineObjectCreateInfo;
struct VKComputePipelineObjectCreateInfo;
struct DescriptorSetLayoutBuilder;
struct DescriptorSetBuilder;
struct PipelineCacheObject;
struct VKGraphicsPipelineBuilder;
struct VKComputePipelineBuilder;
struct VKAllocationCB;
struct DeviceOwnedAllocator;
struct tlsf_main_t;


