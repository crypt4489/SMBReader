#include "TextManager.h"

VkDeviceSize TextManager::bufferOffset = 0;
Font* TextManager::fonts;
VkDeviceSize TextManager::vertexCount = 0;
VkDeviceSize TextManager::commandCount = 0;
VKPipelineObject* TextManager::obj;
std::vector<std::tuple<Text *, VkDeviceSize, VkDeviceSize>> TextManager::textsCommand;
uint32_t TextManager::vertexBufferOffset, TextManager::indirectCommandsOffset;