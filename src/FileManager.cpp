#include "FileManager.h"

#include <string.h>

#include <string>


// Initialize static members
std::filesystem::path FileManager::currDir = std::filesystem::current_path();

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
	std::string tempStr(nameView->stringData, nameView->charCount);

#if defined (WIN32)
	char filePathTerminator = '\\';
#endif

	size_t beginPos = std::string::npos, endPos = std::string::npos;

	beginPos = tempStr.find_last_of(filePathTerminator);

	endPos = tempStr.find_last_of('.');

	if (std::string::npos == beginPos || std::string::npos == endPos)
		return -1;
		
	tempStr = tempStr.substr(beginPos + 1, (endPos - (beginPos+1)));

	outView->charCount = tempStr.size();

	memcpy(inputBuffer, tempStr.c_str(), outView->charCount);

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
