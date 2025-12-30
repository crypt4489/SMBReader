#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "FileManager.h"
#include "MathTypes.h"

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

enum SMBImageFormat : uint32_t
{
	SMB_X8L8U8V8 = 7,
	SMB_DXT1 = 12,
	SMB_DXT3 = 14,
	SMB_R8G8B8A8 = 18,
	SMB_IMAGEUNKNOWN = 0xffffffff
};


//First Part of SMB GEo Chunk is the BaseGeometry Definition BaseGeometryDef

//Second part is I think the GeometryDef

#define MaterialDefSize 136 //bytes
#define GeometryBaseDefSize 72 //bytes

#define RenderableByIndexNonPB 5292387491162064043
#define RenderableByIndex 5792287050554945273


constexpr float dx = 3.051851e-05f;
constexpr float ax = 0.0009770395f;
constexpr float bx = 0.0019550342f;




enum RenderableFlags
{
	IVRENDERABLE = 0,
	PBIVRENDERABLE = 1
};

extern int VertexCompressedSizes[3];

enum SMBVertexSize
{
	PosPack6_CNorm_C16Tex1_Bone2_Size = 0,
	PosPack6_C16Tex2_Bone2_Size = 1,
	PosPack6_C16Tex1_Bone2_Size = 2,
};

enum SMBVertexTypes
{
	PosPack6_CNorm_C16Tex1_Bone2 = 122,
	PosPack6_C16Tex2_Bone2 = 119,
	PosPack6_C16Tex1_Bone2 = 114
};

struct AxisBox
{
	Vector4f min;
	Vector4f max;
};

struct SMBGeoChunk
{
	int vertexAndIndicesInfo;
	int numRenderables;
	int numMaterials;
	int* renderablesTypes;
	SMBVertexTypes* vertexTypes;
	int* indicesCount;
	int* indexOffsetInArchive;
	int* verticesCount;
	int* vertexOffsetInArchive;
	int* primitiveTypes;
	int* materialsCount;
	int* materialStart;
	int* materialsId;
	AxisBox axialBox;

	SMBGeoChunk() = delete;
	SMBGeoChunk(int _numRenderables, int _numMaterials)
	{
		memset(this, 0, sizeof(SMBGeoChunk));
		numRenderables = _numRenderables;
		int chunkSize = _numRenderables * 9 * sizeof(int);
		chunkSize += (sizeof(int) * _numMaterials);

		int* chunkPtr = (int*)malloc(chunkSize);
		int* cP = chunkPtr;

		if (chunkPtr)
		{

			renderablesTypes = chunkPtr;
			chunkPtr += (_numRenderables);
			vertexTypes = (SMBVertexTypes*)chunkPtr;
			chunkPtr += (_numRenderables);
			indicesCount = chunkPtr;
			chunkPtr += (_numRenderables);
			indexOffsetInArchive = chunkPtr;
			chunkPtr += (_numRenderables);
			verticesCount = chunkPtr;
			chunkPtr += (_numRenderables);
			vertexOffsetInArchive = chunkPtr;
			chunkPtr += (_numRenderables);
			primitiveTypes = chunkPtr;
			chunkPtr += (_numRenderables);
			materialsCount = chunkPtr;
			chunkPtr += (_numRenderables);
			materialsId = chunkPtr;
			chunkPtr += (_numMaterials);
			materialStart = chunkPtr;
			chunkPtr += (_numRenderables);

			memset(&axialBox, 0, sizeof(AxisBox));
			memset(indexOffsetInArchive, 0xFF, sizeof(int) * _numRenderables);
			memset(indicesCount, 0xFF, sizeof(int) * _numRenderables);
		}

	
	}

	~SMBGeoChunk()
	{
		free(renderablesTypes);
	}
	
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
	uint32_t chunkIdCopy;
	uint32_t stringsize;
	std::string fileName;
	std::size_t offsetInHeader;
	std::size_t headerSize;

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


SMBGeoChunk* ProcessGeometryClass(char* data, int numMaterials);

int GetSMBVertexSize(SMBGeoChunk* geoDef, int renderableIndex);

int GetSMBIndexSize(SMBGeoChunk* geoDef, int renderableIndex);

void SMBCopyVertexData(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile& file, void* vertexDataOut, int decompressed);

void SMBCopyIndices(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile& file, void* indexDataOut);