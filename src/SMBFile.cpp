#include "SMBFile.h"

SMBFile::SMBFile(const std::filesystem::path& path) : id(LoadFile(path))
{
}

SMBFile::SMBFile(const std::string& file) : id(LoadFile(file))
{
}

SMBFile::~SMBFile()
{
	FileManager::RemoveOpenFile(id);
}

FileID SMBFile::LoadFile(const std::filesystem::path& path)
{
	FileHandle* handle;

	auto ret = FileManager::OpenFile(path, std::ios::binary | std::ios::in, handle);

	if (!ret)
	{
		throw std::runtime_error("SMB file is unable to be opened");
	}

	ProcessFile(handle->streamHandle);

	return std::move(ret.value());
}

FileID SMBFile::LoadFile(const std::string& name)
{
	FileHandle* handle;

	auto ret = FileManager::OpenFile(name, std::ios::binary | std::ios::in, handle);

	if (!ret)
	{
		throw std::runtime_error("SMB file is unable to be opened");
	}

	ProcessFile(handle->streamHandle);

	return std::move(ret.value());
}

void SMBFile::ReadHeader(std::fstream& fh)
{
	fh.read(reinterpret_cast<char*>(&magic), 8);
	int stringsize = 0;
	fh.read(reinterpret_cast<char*>(&stringsize), 4);
	name.resize(stringsize);
	fh.read(name.data(), stringsize);
	fh.read(reinterpret_cast<char*>(&fileOffset), 36);
	chunks.resize(numResources);
}

void SMBFile::ReadChunk(std::fstream& fh, SMBChunk& chunk)
{

	fh.read(reinterpret_cast<char*>(&chunk.magic), 44);
	chunk.fileName.resize(chunk.stringsize);
	fh.read(chunk.fileName.data(), chunk.stringsize);
	chunk.offsetInHeader = fh.tellg();
	std::streamoff offset = (chunk.numOfBytesAfterTag - (chunk.stringsize + 16LL));
	std::streamoff next = chunk.offsetInHeader + offset;
	fh.seekg(next, std::ios_base::beg);
}

void SMBFile::ProcessFile(std::fstream& fh)
{
	ReadHeader(fh);

	for (auto& i : chunks)
	{
		ReadChunk(fh, i);
	
	}
}