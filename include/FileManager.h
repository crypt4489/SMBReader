#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <utility>


class FileID
{
public:
	FileID() = delete;
	explicit FileID(uint32_t _id) : ID(_id) {};
	//delete copies / integer assignment
	constexpr FileID& operator=(uint32_t) = delete;
	constexpr FileID& operator=(FileID&) = delete;
	FileID(FileID&) = delete;

	//keep moves
	FileID(FileID&&) = default;
	constexpr FileID& operator=(FileID&&) = default;

	constexpr uint32_t operator()() const {
		return ID;
	}
private:
	uint32_t ID;
};

struct FileHandle
{
	FileHandle(std::fstream& stream, std::ios::openmode _flags) : streamHandle(std::move(stream)), flags(_flags) 
	{
		if (flags & std::ios::in)
		{
			size = static_cast<uint64_t>(streamHandle.tellg());
			streamHandle.seekg(0);
		}
		else 
		{
			size = 0;
		}
	};
	std::fstream streamHandle;
	std::ios::openmode flags;
	uint64_t size;
};

class FileManager
{
public:

	static std::optional<FileID> OpenFile(const std::string& name, std::ios::openmode flags, FileHandle* &outHandle)
	{
		return HandleOpening(name, flags, outHandle);
	}

	static std::optional<FileID> OpenFile(const std::filesystem::path name, std::ios::openmode flags, FileHandle* &outHandle)
	{
		return HandleOpening(name.string(), flags, outHandle);
	}

	static int ReadFileInFull(const std::string& name, std::vector<char> &buffer)
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

	static FileHandle* GetFile(const FileID &id)
	{
		if (id() >= MAXFILES) throw std::invalid_argument("You did what?");
		return filesopen[id()];
	}

	static void RemoveOpenFile(const FileID &id)
	{
		FileHandle *ret = GetFile(id);

		if (!ret)
			return;

		if (ret->streamHandle.is_open())
		{
			ret->streamHandle.close();
		}
		delete ret;
		filesopen[id()] = nullptr;
	}

	static std::filesystem::path SetupDirectory(std::string &nameOfDir)
	{
		std::filesystem::path pathToDir = currDir / nameOfDir;
		if (!std::filesystem::exists(pathToDir))
		{
			std::filesystem::create_directory(pathToDir);
		}
		return pathToDir;
	}

	static void SetFileCurrentDirectory(std::string name)
	{
		currDir = SetupDirectory(name);
	}

	static void SetFileCurrentDirectory(std::string &name)
	{
		currDir = SetupDirectory(name);
	}

	static void SetFileCurrentDirectory(std::filesystem::path &path)
	{
		currDir = path;
	}

	static std::string ExtractFileNameFromPath(const std::string &path)
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

	static std::string ExtractFileNameFromPath(std::filesystem::path &path)
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

	static std::filesystem::path GetCurrentDirectoryFM()
	{
		return currDir;
	}

	static constexpr uint32_t NOHANDLE = 0x7FFFFFFF;
	static constexpr uint32_t MAXFILES = 10;
private:

	static uint32_t FindAvailableHandle()
	{
		uint32_t i = 0;
		for (; i < MAXFILES; i++) {
			if (filesopen[i] == nullptr)
				break;
		}
		return i;
	}

	static std::optional<FileID> HandleOpening(const std::string& name, std::ios::openmode flags, FileHandle* &outHandle)
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

	static std::array<FileHandle*, MAXFILES> filesopen;
	static std::filesystem::path currDir;
	static std::regex filenamePattern;
};

