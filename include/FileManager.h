#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <utility>

#include "OSFile.h"

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


struct FileManager
{
public:

	static FileID OpenFile(const std::string& name, OSFileFlags flags);

	static FileID OpenFile(const std::filesystem::path name, OSFileFlags flags);

	static int ReadFileInFull(const std::string& name, std::vector<char>& buffer);

	static OSFileHandle* GetFile(const FileID& id);

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

	static int HandleOpening(const std::string& name, OSFileFlags flags, OSFileHandle* outHandle);

	static int ReadBytes(const FileID& id, int numBytes, char* buffer);

	static int CreateFileIterator(std::string searchstring, OSFileIterator* file);

	static int NextFileIterator(OSFileIterator* file);

	static std::array<OSFileHandle, MAXFILES> filesopen;
	static std::filesystem::path currDir;
	static std::regex filenamePattern;
};

