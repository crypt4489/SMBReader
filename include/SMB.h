#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "FileManager.h"


#define BEGINNINGSMBChunk 0xa77e4dfa
#define ENDTAG 0xbeef1234
#define ENDGEOTAG 0xdc7c9d74

enum chunktype
{
	GEO = 0,
	TEXTURE = 1,
	GR2 = 6,
	Joints = 20,
};

struct SMBChunk
{
	uint32_t magic;
	uint32_t chunkType;
	uint32_t chunkId;
	uint32_t contigOffset;
	uint32_t fileOffset; //monotonically increasing for each chunk
	uint32_t numOfBytesAfterTag;
	uint32_t pad3;
	uint32_t tag;
	uint32_t pad4;
	uint32_t endmag;
	uint32_t stringsize;
	std::string fileName;
	std::streampos offsetInHeader;

	friend std::ostream& operator<<(std::ostream& os, const SMBChunk& chunk)
	{
		os << chunk.fileName << "\n"
			<< chunk.fileOffset << "\n"
			<< chunk.numOfBytesAfterTag << "\n"
			<< chunk.chunkId << "\n"
			<< chunk.chunkType << "\n"
			<< chunk.contigOffset << "\n"
			<< chunk.tag << "\n"
			<< chunk.pad3 << " " << chunk.pad4 << "\n"
			<< "------------------" << "\n";
		return os;
	}
};

class SMBFile
{
public:
	uint32_t magic;
	uint32_t version;
	uint32_t fileOffset; // number of bytes from beginning of file
	uint32_t headerStreamEnd; //number of bytes from beginning of file
	uint32_t numContiguousBytes;
	uint32_t numSystemBytes;
	uint32_t contiguousSizeUnpadded;
	uint32_t systemSizeUnpadded;
	uint32_t numResources;
	uint32_t ioMode;
	uint32_t endcode;
	std::string name;
	std::vector<SMBChunk> chunks;
	FileManager::FileHandle fileHandle;

	~SMBFile()
	{
		FileManager::CloseFile(fileHandle.first);
	}

	void LoadFile(std::string name)
	{
		auto ret = FileManager::OpenFile(name, std::ios::binary | std::ios::in);

		if (!ret)
		{
			throw std::runtime_error("SMB file is unable to be opened");
		}

		fileHandle = ret.value();

		ReadHeader();

		int prev = 0;

		for (auto& i : chunks)
		{
			ReadChunk(i);	
		}
	}

	void ReadHeader()
	{
		if (!fileHandle.second) return;
		auto fh = fileHandle.second;
		fh->read(reinterpret_cast<char*>(&magic), 8);
		int stringsize = 0;
		fh->read(reinterpret_cast<char*>(&stringsize), 4);
		name.resize(stringsize);
		fh->read(name.data(), stringsize);
		fh->read(reinterpret_cast<char*>(&fileOffset), 36);
		chunks.resize(numResources);
	}

	void ReadChunk(SMBChunk &chunk)
	{
		if (!fileHandle.second) return;
		auto fh = fileHandle.second;
		fh->read(reinterpret_cast<char*>(&chunk.magic), 44);
		chunk.fileName.resize(chunk.stringsize);
		fh->read(chunk.fileName.data(), chunk.stringsize);
		chunk.offsetInHeader = fh->tellg();
		std::streamoff offset = (chunk.numOfBytesAfterTag - (chunk.stringsize + 16));
		std::streamoff next = chunk.offsetInHeader + offset;
		fh->seekg(next, std::ios_base::beg);
	}
};

