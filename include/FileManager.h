#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <utility>


struct FileID
{

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

struct FileManager
{
public:

	static std::optional<FileID> OpenFile(const std::string& name, std::ios::openmode flags, FileHandle*& outHandle);

	static std::optional<FileID> OpenFile(const std::filesystem::path name, std::ios::openmode flags, FileHandle*& outHandle);

	static int ReadFileInFull(const std::string& name, std::vector<char>& buffer, std::ios::openmode flags);

	static FileHandle* GetFile(const FileID& id);

	static void RemoveOpenFile(const FileID& id);

	static std::filesystem::path SetupDirectory(std::string& nameOfDir);

	static void SetFileCurrentDirectory(std::string name);

	static void SetFileCurrentDirectory(std::string& name);

	static void SetFileCurrentDirectory(std::filesystem::path& path);

	static std::string ExtractFileNameFromPath(const std::string& path);

	static std::string ExtractFileNameFromPath(std::filesystem::path& path);

	static std::filesystem::path GetCurrentDirectoryFM();

	static bool FileExists(const std::string& name);

	static constexpr uint32_t NOHANDLE = 0x7FFFFFFF;
	static constexpr uint32_t MAXFILES = 10;

	static uint32_t FindAvailableHandle();

	static std::optional<FileID> HandleOpening(const std::string& name, std::ios::openmode flags, FileHandle*& outHandle);

	static std::array<FileHandle*, MAXFILES> filesopen;
	static std::filesystem::path currDir;
	static std::regex filenamePattern;
};

