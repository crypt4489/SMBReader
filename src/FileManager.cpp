#include "FileManager.h"
// Initialize static members
std::array<FileHandle*, FileManager::MAXFILES> FileManager::filesopen;
std::filesystem::path FileManager::currDir = std::filesystem::current_path();

std::regex FileManager::filenamePattern{"([A-Za-z_]+)\\."};