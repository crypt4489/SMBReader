#include "TextManager.h"


VkBuffer TextManager::textBuffer;
VkBuffer TextManager::indirectDrawBuffer;
VkDeviceSize TextManager::bufferOffset = 0;
Font* TextManager::fonts;
VkDeviceSize TextManager::vertexCount = 0;
VkDeviceSize TextManager::commandCount = 0;
VkDeviceSize TextManager::textVertexGlobalOffset = 0U, TextManager::indirectGlobalOffset = 0U;
void* TextManager::textGlobalMemory, *TextManager::indirectGlobalMemory;
VKPipelineObject* TextManager::obj;
std::vector<std::tuple<Text *, VkDeviceSize, VkDeviceSize>> TextManager::textsCommand;