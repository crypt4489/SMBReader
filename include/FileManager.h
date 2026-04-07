#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <utility>


#include "AppTypes.h"
#include "OSFile.h"

#include "AppAllocator.h"

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

	static FileID OpenFile(StringView* nameView, OSFileFlags flags);

	static FileID OpenFile(const std::filesystem::path name, OSFileFlags flags);

	static OSFileHandle* GetFile(const FileID& id);

	static void RemoveOpenFile(const FileID& id);

	static std::filesystem::path SetupDirectory(StringView* nameView);

	static void SetFileCurrentDirectory(StringView* nameView);

	static void SetFileCurrentDirectory(std::filesystem::path& path);

	static void ExtractFileNameFromPath(StringView* nameView, StringView* outView);

	static std::filesystem::path GetCurrentDirectoryFM();

	static bool FileExists(StringView* nameView);

	static constexpr uint32_t NOHANDLE = 0x7FFFFFFF;
	static constexpr uint32_t MAXFILES = 10;

	static uint32_t FindAvailableHandle();

	static int HandleOpening(StringView* nameView, OSFileFlags flags, OSFileHandle* outHandle);

	static int ReadBytes(const FileID& id, int numBytes, char* buffer);

	static int CreateFileIterator(StringView* nameView, OSFileIterator* file);

	static int NextFileIterator(OSFileIterator* file);

	static int ReadFileInFull(StringView* nameView, SlabAllocator* allocator, void** dataOut);

	static int ReadFileInFull(StringView* nameView, RingAllocator* allocator, void** dataOut);

	static std::array<OSFileHandle, MAXFILES> filesopen;
	static std::filesystem::path currDir;
	static std::regex filenamePattern;
};

