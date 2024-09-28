#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <unordered_map>
#include <utility>
#include <cstdint>
class FileManager
{
public:

	typedef std::pair<uint32_t, std::shared_ptr<std::fstream>> FileHandle;

	static std::optional<FileHandle> OpenFile(const std::string name, std::ios::openmode flags)
	{
		auto handle = std::make_shared<std::fstream>();

		handle->open(name, flags);

		if (!handle->is_open())
		{
			return std::nullopt;
		}

		filesopen[globalId] = handle;

		std::pair res = std::make_pair(globalId++, handle);

		return res;
	}

	static std::optional<FileHandle> OpenFile(const std::filesystem::path name, std::ios::openmode flags)
	{
		auto handle = std::make_shared<std::fstream>();

		handle->open(name, flags);

		if (!handle->is_open())
		{
			return std::nullopt;
		}

		filesopen[globalId] = handle;

		std::pair res = std::make_pair(globalId++, handle);

		return res;
	}

	static std::optional<std::shared_ptr<std::fstream>> GetFile(uint32_t id)
	{
		auto mapObject = filesopen.find(id);
		if (mapObject == filesopen.end())
		{
			return std::nullopt;
		}
		return mapObject->second;
	}

	static void CloseFile(uint32_t id)
	{
		auto ret = GetFile(id);
		if (!ret)
		{
			return;
		}
		std::shared_ptr<std::fstream> file = ret.value();
		file->close();
		filesopen.erase(id);
	}

	static std::filesystem::path SetupDirectory(std::string nameOfDir)
	{
		std::filesystem::path pathToDir = currDir / nameOfDir;
		if (!std::filesystem::exists(pathToDir))
		{
			std::filesystem::create_directory(pathToDir);
		}
		return pathToDir;
	}

	static void SetCurrentDirectory(std::string name)
	{
		currDir = SetupDirectory(name);
	}

	static void SetCurrentDirectory(std::filesystem::path path)
	{
		currDir = path;
	}

	static std::string ExtractFileNameFromPath(const std::string path)
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

	static std::string ExtractFileNameFromPath(std::filesystem::path path)
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


private:
	static uint32_t globalId;
	static std::unordered_map<uint32_t, std::shared_ptr<std::fstream>> filesopen;
	static std::filesystem::path currDir;
	static std::regex filenamePattern;
};

