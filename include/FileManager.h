#pragma once
#include <cstdint>
#include <filesystem>
#include <regex>
#include <utility>


#include "StringUtils.h"
#include "OSFile.h"

#include "allocator/AppAllocator.h"

struct FileManager
{
public:

	static std::filesystem::path SetupDirectory(StringView* nameView);

	static void SetFileCurrentDirectory(StringView* nameView);

	static void SetFileCurrentDirectory(std::filesystem::path& path);

	static void ExtractFileNameFromPath(StringView* nameView, StringView* outView);

	static std::filesystem::path GetCurrentDirectoryFM();

	static bool FileExists(StringView* nameView);

	static constexpr uint32_t NOHANDLE = 0x7FFFFFFF;
	static constexpr uint32_t MAXFILES = 10;

	static int HandleOpening(StringView* nameView, OSFileFlags flags, OSFileHandle* outHandle);

	static int CreateFileIterator(StringView* nameView, OSFileIterator* file);

	static int NextFileIterator(OSFileIterator* file);

	static std::filesystem::path currDir;
	static std::regex filenamePattern;
};

