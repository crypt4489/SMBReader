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
	FileID id;

	SMBFile() = delete;

	SMBFile(const std::filesystem::path& path);

	SMBFile(const std::string& file);

	~SMBFile();

	FileID LoadFile(const std::filesystem::path& path);

	FileID LoadFile(const std::string& name);
private:
	void ReadHeader(std::fstream& fh);

	void ReadChunk(std::fstream& fh, SMBChunk& chunk);

	void ProcessFile(std::fstream& fh);
};

