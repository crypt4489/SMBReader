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
	static std::filesystem::path SetupDirectory(StringView* nameView);

	static void SetFileCurrentDirectory(StringView* nameView);

	static void SetFileCurrentDirectory(std::filesystem::path& path);

	static int ExtractFileNameFromPath(StringView* nameView, StringView* outView, char* inputBuffer);

	static std::filesystem::path GetCurrentDirectoryFM();

	static bool FileExists(StringView* nameView);

	static std::filesystem::path currDir;
	static std::regex filenamePattern;
};

