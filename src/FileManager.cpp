#include "FileManager.h"



// Initialize static members
std::array<OSFileHandle, FileManager::MAXFILES> FileManager::filesopen{};
std::filesystem::path FileManager::currDir = std::filesystem::current_path();

std::regex FileManager::filenamePattern{"([0-9A-Za-z_]+)\\."};



FileID FileManager::OpenFile(StringView* nameView, OSFileFlags flags)
{
	uint32_t index = FindAvailableHandle();

	if (index == NOHANDLE)
	{
		return FileID(NOHANDLE);
	}

	OSFileHandle* outHandle = &filesopen[index];

	int nRet = HandleOpening(nameView, flags, outHandle);

	if (nRet)
	{
		return FileID(NOHANDLE);
	}

	return FileID(index);
}

FileID FileManager::OpenFile(const std::filesystem::path name, OSFileFlags flags)
{

	uint32_t index = FindAvailableHandle();

	if (index == NOHANDLE)
	{
		return FileID(NOHANDLE);
	}

	OSFileHandle* outHandle = &filesopen[index];

	StringView nameView = { (char*)name.string().c_str(), (int)name.string().size() };

	int nRet = HandleOpening(&nameView, flags, outHandle);

	if (nRet)
	{
		return FileID(NOHANDLE);
	}

	return FileID(index);
}

int FileManager::ReadFileInFull(StringView* nameView, SlabAllocator* allocator, void** dataOut)
{
	OSFileFlags openingFlags = READ;

	OSFileHandle outHandle{};

	int nRet = OSOpenFile(nameView->stringData, nameView->charCount, openingFlags, &outHandle);

	if (nRet)
	{
		return 1;
	}

	int size = outHandle.fileLength;

	void* dataReadIn = allocator->Allocate(size);

	OSReadFile(&outHandle, size, (char*)dataReadIn);

	OSCloseFile(&outHandle);

	*dataOut = dataReadIn;

	return 0;
}

int FileManager::ReadFileInFull(StringView* nameView, RingAllocator* allocator, void** dataOut)
{
	OSFileFlags openingFlags = READ;

	OSFileHandle outHandle{};

	int nRet = OSOpenFile(nameView->stringData, nameView->charCount, openingFlags, &outHandle);

	if (nRet)
	{
		return 1;
	}

	int size = outHandle.fileLength;

	void* dataReadIn = allocator->Allocate(size);

	OSReadFile(&outHandle, size, (char*)dataReadIn);

	OSCloseFile(&outHandle);

	*dataOut = dataReadIn;

	return 0;
}

OSFileHandle* FileManager::GetFile(const FileID& id)
{
	if (id() >= MAXFILES) 
		throw std::invalid_argument("You did what?");

	OSFileHandle* handle = &filesopen[id()];

	if (handle->osDataHandle == -1)
	{
		return nullptr;
	}

	return handle;
}

void FileManager::RemoveOpenFile(const FileID& id)
{
	OSFileHandle* ret = GetFile(id);

	if (!ret) return;

	if (OSCloseFile(ret) != OS_SUCCESS)
	{
		throw std::runtime_error("Closing unopened file");
	}

	memset(ret, -1, sizeof(OSFileHandle));
		
	return;
}

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

void FileManager::ExtractFileNameFromPath(StringView* nameView, StringView* outView)
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
		std::cerr << "Couldn't find the filename in " << nameView->stringData << "\n";
	}

	outView->charCount = returnName.size();

	memcpy(outView->stringData, returnName.c_str(), outView->charCount);
}

std::filesystem::path FileManager::GetCurrentDirectoryFM()
{
	return currDir;
}

bool FileManager::FileExists(StringView* nameView)
{
	return std::filesystem::exists(std::string(nameView->stringData, nameView->charCount));
}

uint32_t FileManager::FindAvailableHandle()
{
	uint32_t i = 0;
	for (; i < MAXFILES; i++) {
		if (filesopen[i].osDataHandle == -1)
			break;
	}
	return i;
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

int FileManager::ReadBytes(const FileID& id, int numBytes, char* buffer)
{
	OSFileHandle* handle = &filesopen[id()];

	if (handle->osDataHandle == -1)
	{
		return -1;
	}

	int ret = OSReadFile(handle, numBytes, buffer);

	return ret;
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