#include "FileManager.h"



// Initialize static members
std::array<OSFileHandle, FileManager::MAXFILES> FileManager::filesopen{};
std::filesystem::path FileManager::currDir = std::filesystem::current_path();

std::regex FileManager::filenamePattern{"([0-9A-Za-z_]+)\\."};



FileID FileManager::OpenFile(const std::string& name, OSFileFlags flags)
{
	uint32_t index = FindAvailableHandle();

	if (index == NOHANDLE)
	{
		return FileID(NOHANDLE);
	}

	OSFileHandle* outHandle = &filesopen[index];

	int nRet = HandleOpening(name, flags, outHandle);

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

	int nRet = HandleOpening(name.string(), flags, outHandle);

	if (nRet)
	{
		return FileID(NOHANDLE);
	}

	return FileID(index);
}

int FileManager::ReadFileInFull(const std::string& name, SlabAllocator* allocator, void** dataOut)
{
	OSFileFlags openingFlags = READ;

	OSFileHandle outHandle{};

	int nRet = OSOpenFile(name.c_str(), openingFlags, &outHandle);

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

int FileManager::ReadFileInFull(const std::string& name, RingAllocator* allocator, void** dataOut)
{
	OSFileFlags openingFlags = READ;

	OSFileHandle outHandle{};

	int nRet = OSOpenFile(name.c_str(), openingFlags, &outHandle);

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

std::filesystem::path FileManager::SetupDirectory(std::string& nameOfDir)
{
	std::filesystem::path pathToDir = currDir / nameOfDir;
	if (!std::filesystem::exists(pathToDir))
	{
		std::filesystem::create_directory(pathToDir);
	}
	return pathToDir;
}

void FileManager::SetFileCurrentDirectory(std::string name)
{
	currDir = SetupDirectory(name);
}

void FileManager::SetFileCurrentDirectory(std::string& name)
{
	currDir = SetupDirectory(name);
}

void FileManager::SetFileCurrentDirectory(std::filesystem::path& path)
{
	currDir = path;
}

std::string FileManager::ExtractFileNameFromPath(const std::string path)
{
	std::smatch match;

	std::string name;

	if (std::regex_search(path, match, filenamePattern))
	{
		name = match[1];
	}
	else
	{
		std::cerr << "Couldn't find the filename in " << path << "\n";
	}

	return name;
}

std::string FileManager::ExtractFileNameFromPath(std::filesystem::path& path)
{

	std::smatch match;

	std::string name, search = path.string();

	if (std::regex_search(search, match, filenamePattern))
	{
		name = match[1];
	}
	else
	{
		std::cerr << "Couldn't find the filename in " << path << "\n";
	}

	return name;
}

std::filesystem::path FileManager::GetCurrentDirectoryFM()
{
	return currDir;
}

bool FileManager::FileExists(const std::string& name)
{
	return std::filesystem::exists(name);
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

int FileManager::HandleOpening(const std::string& name, OSFileFlags flags, OSFileHandle* outHandle)
{
	

	OSFileFlags openingFlags = flags;

	int nRet = OSOpenFile(name.c_str(), openingFlags, outHandle);

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

int FileManager::CreateFileIterator(std::string searchstring, OSFileIterator* file)
{
	int ret = OSCreateFileIterator(searchstring.c_str(), file);
	return ret;
}

int FileManager::NextFileIterator(OSFileIterator* file)
{
	int ret = OSNextFile(file);
	return ret;
}