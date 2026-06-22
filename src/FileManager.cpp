#include "FileManager.h"
#include <string.h>
// Initialize static members
std::filesystem::path FileManager::currDir = std::filesystem::current_path();

std::regex FileManager::filenamePattern{"([0-9A-Za-z_]+)\\."};

std::filesystem::path FileManager::SetupDirectory(StringView* nameView)
{
	std::filesystem::path pathToDir = currDir / std::string(nameView->stringData, nameView->charCount);
	if (!std::filesystem::exists(pathToDir))
	{
		std::filesystem::create_directory(pathToDir);
	}
	return pathToDir;
}

void FileManager::SetFileCurrentDirectory(StringView* nameView)
{
	currDir = SetupDirectory(nameView);
}

void FileManager::SetFileCurrentDirectory(std::filesystem::path& path)
{
	currDir = path;
}

int FileManager::ExtractFileNameFromPath(StringView* nameView, StringView* outView, char* inputBuffer)
{
	std::smatch match;

	std::string returnName;

	std::string tempStr(nameView->stringData, nameView->charCount);

	if (std::regex_search(tempStr, match, filenamePattern))
	{
		returnName = match[1];
	}
	else
	{
		return -1;
	}

	outView->charCount = returnName.size();

	memcpy(inputBuffer, returnName.c_str(), outView->charCount);

	outView->stringData = inputBuffer;

	return 0;
}

std::filesystem::path FileManager::GetCurrentDirectoryFM()
{
	return currDir;
}

bool FileManager::FileExists(StringView* nameView)
{
	return std::filesystem::exists(std::string(nameView->stringData, nameView->charCount));
}
