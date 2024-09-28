#include "FileManager.h"
// Initialize static members
uint32_t FileManager::globalId = 0;
std::unordered_map<uint32_t, std::shared_ptr<std::fstream>> FileManager::filesopen;
std::filesystem::path FileManager::currDir = std::filesystem::current_path();

std::regex FileManager::filenamePattern{ "\([A-Za-z_]+)\\." };