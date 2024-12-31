#include "TextManager.h"
std::vector<std::pair<uint64_t, uint32_t>> TextManager::offsetsAndCount;

VkBuffer TextManager::textBuffer;
VkDeviceMemory TextManager::textBufferMemory;
uint64_t TextManager::bufferOffset;
Font* TextManager::fonts;
uint32_t TextManager::count;
VKPipelineObject* TextManager::obj;