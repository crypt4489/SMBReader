#include "FileManager.h"



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

void FileManager::ExtractFileNameFromPath(StringView* nameView, StringView* outView, char* inputBuffer)
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
		//std::cerr << "Couldn't find the filename in " << nameView->stringData << "\n";
	}

	outView->charCount = returnName.size();

	memcpy(inputBuffer, returnName.c_str(), outView->charCount);

	outView->stringData = inputBuffer;
}

std::filesystem::path FileManager::GetCurrentDirectoryFM()
{
	return currDir;
}

bool FileManager::FileExists(StringView* nameView)
{
	return std::filesystem::exists(std::string(nameView->stringData, nameView->charCount));
}


int FileManager::HandleOpening(StringView* nameView, OSFileFlags flags, OSFileHandle* outHandle)
{
	

	OSFileFlags openingFlags = flags;

	int nRet = OSOpenFile(nameView->stringData, nameView->charCount, openingFlags, outHandle);

	/*if (nRet != OS_SUCCESS)
	{
		return std::nullopt;
	} */

	return nRet;
}

int FileManager::CreateFileIterator(StringView* nameView, OSFileIterator* file)
{
	int ret = OSCreateFileIterator(nameView->stringData, nameView->charCount, file);
	return ret;
}

int FileManager::NextFileIterator(OSFileIterator* file)
{
	int ret = OSNextFile(file);
	return ret;
}