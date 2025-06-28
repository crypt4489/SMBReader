#include "FileManager.h"


// Initialize static members
std::array<FileHandle*, FileManager::MAXFILES> FileManager::filesopen;
std::filesystem::path FileManager::currDir = std::filesystem::current_path();

std::regex FileManager::filenamePattern{"([A-Za-z_]+)\\."};



std::optional<FileID> FileManager::OpenFile(const std::string& name, std::ios::openmode flags, FileHandle*& outHandle)
{
	return HandleOpening(name, flags, outHandle);
}

std::optional<FileID> FileManager::OpenFile(const std::filesystem::path name, std::ios::openmode flags, FileHandle*& outHandle)
{
	return HandleOpening(name.string(), flags, outHandle);
}

int FileManager::ReadFileInFull(const std::string& name, std::vector<char>& buffer)
{
	std::fstream stream;

	stream.open(name, std::ios::binary | std::ios::in | std::ios::ate);

	if (!stream.is_open())
	{
		return 1;
	}

	std::streampos fileEnd = stream.tellg();

	buffer.resize(fileEnd);

	stream.seekg(0);

	stream.read(buffer.data(), fileEnd);

	stream.close();

	return 0;
}

FileHandle* FileManager::GetFile(const FileID& id)
{
	if (id() >= MAXFILES) throw std::invalid_argument("You did what?");
	return filesopen[id()];
}

void FileManager::RemoveOpenFile(const FileID& id)
{
	FileHandle* ret = GetFile(id);

	if (!ret)
		return;

	if (ret->streamHandle.is_open())
	{
		ret->streamHandle.close();
	}
	delete ret;
	filesopen[id()] = nullptr;
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

std::string FileManager::ExtractFileNameFromPath(const std::string& path)
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
		if (filesopen[i] == nullptr)
			break;
	}
	return i;
}

std::optional<FileID> FileManager::HandleOpening(const std::string& name, std::ios::openmode flags, FileHandle*& outHandle)
{
	uint32_t index = FindAvailableHandle();

	if (index == NOHANDLE)
	{
		return std::nullopt;
	}

	std::fstream stream;

	std::ios::openmode openingFlags = flags;

	if (flags & std::ios::in)
	{
		openingFlags |= std::ios::ate;
	}

	stream.open(name, openingFlags);

	if (!stream.is_open())
	{
		return std::nullopt;
	}

	FileHandle* handle = new FileHandle(stream, flags);

	filesopen[index] = outHandle = handle;

	return FileID(index);
}