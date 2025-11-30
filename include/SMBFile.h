#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "FileManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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


//First Part of SMB GEo Chunk is the BaseGeometry Definition BaseGeometryDef

//Second part is I think the GeometryDef

struct GEOBaseChunk
{
	uint32_t numLods;
	uint32_t geometryFlags;
	uint32_t lopSpecs[6];
	glm::vec4 sphere;
	glm::vec3 boxecorner;
};

struct GEOResourceChunk
{
	uint32_t unknown[2];
	uint32_t resourceType;
	uint32_t unknown2[2];
};

struct GEOGeometryBase
{
	uint32_t materialSize;
	uint32_t collidableSize;
	uint32_t renderablesize;
	uint32_t numberofinstances;

	uint32_t numberofskels;
	uint32_t numskinnedcollidables;
	uint32_t rigidrenderables;
	uint32_t skinnedRenderables;

	uint32_t rigidcollidables;
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
		os << std::hex << "File Reference Name " << chunk.fileName << "\n"
		<< "Offset in header " << chunk.offsetInHeader << "\n"
			<< "File SMB Offset "  << chunk.fileOffset << "\n"
		<< "File SMB Number of Bytes after tag " << chunk.numOfBytesAfterTag << "\n"
		<< "Chunk Id number " << chunk.chunkId << "\n"
		<< "Chunk Id type " << chunk.chunkType << "\n"
		<< "Chunk Continguos Offset " << chunk.contigOffset << "\n"
			<< chunk.tag << "\n"
			<< chunk.pad3 << " " << chunk.pad4 << "\n"
			<< "------------------" << "\n";

		return os;
	}
};

struct SMBFile
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

	friend std::ostream& operator<<(std::ostream& os, const SMBFile& file)
	{
		os << std::hex << "File Offset (where real data begins) " << file.fileOffset << "\n"
			<< "File Header Ends " << file.headerStreamEnd << "\n"
			<< "Something " << file.numContiguousBytes << "\n"
			<< "Number of Resources " << file.numResources << "\n"
			<< "Number of Chunks " << file.chunks.size() << "\n"
			<< "------------------" << "\n";
		return os;
	}


private:
	void ReadHeader(std::fstream& fh);

	void ReadChunk(std::fstream& fh, SMBChunk& chunk);

	void ProcessFile(std::fstream& fh);
};

