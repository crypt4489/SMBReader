#include "TextManager.h"


VkBuffer TextManager::textBuffer;
VkDeviceMemory TextManager::textBufferMemory;
VkBuffer TextManager::indirectDrawBuffer;
VkDeviceMemory TextManager::indirectDrawBufferMemory;
VkDeviceSize TextManager::bufferOffset = 0;
Font* TextManager::fonts;
uint32_t TextManager::vertexCount = 0;
uint32_t TextManager::commandCount = 0;
VKPipelineObject* TextManager::obj;

std::vector<VkDrawIndirectCommand> TextManager::commands;