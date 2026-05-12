#pragma once
#include <cstdint>
#include <fstream>
#include "allocator/AppAllocator.h"
#include "logger/Logger.h"
#include "math/MathTypes.h"

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
	SMB_B8G8R8A8_UNORM = 18,
	SMB_R8G8B8A8_UNORM = 0x6fff,
	
	SMB_IMAGEUNKNOWN = 0xffffffff
};

typedef struct pospack6_cnorm_c16tex1_bone2_type_h
{
	Vector4f POSITION;
	Vector2f TEXTURE;
	Vector3f NORMAL;
	Vector2i BONES;
	Vector2f WEIGHTS;

	pospack6_cnorm_c16tex1_bone2_type_h() = default;
	pospack6_cnorm_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector3f& _n, const Vector2i& _b, const Vector2f& _w);

	bool operator==(const pospack6_cnorm_c16tex1_bone2_type_h& v);

} Vertex_PosPack6_CNorm_C16Tex1_Bone2;

typedef struct pospack6_c16tex1_bone2_type_h
{
	Vector4f POSITION;
	Vector2f TEXTURE;
	Vector2i BONES;
	Vector2f WEIGHTS;

	pospack6_c16tex1_bone2_type_h() = default;
	pospack6_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector2i& _b, const Vector2f& _w);

	bool operator==(const pospack6_c16tex1_bone2_type_h& v);

} Vertex_PosPack6_C16Tex1_Bone2;

typedef struct pospack6_c16tex2_bone2_type_h
{
	Vector4f POSITION;
	Vector2f TEXTURE;
	Vector2f TEXTURE2;
	Vector2i BONES;
	Vector2f WEIGHTS;

	pospack6_c16tex2_bone2_type_h() = default;
	pospack6_c16tex2_bone2_type_h(
		const Vector4f& _p,
		const Vector2f& _t,
		const Vector2f& _t2,
		const Vector2i& _b,
		const Vector2f& _w);

	bool operator==(const pospack6_c16tex2_bone2_type_h& v);

} Vertex_PosPack6_C16Tex2_Bone2;

#pragma pack(push, 1)
typedef struct cpospack6_cnorm_c16tex1_bone2_type_h
{
	Vector2uc BONES;
	Vector2uc WEIGHTS;
	Vector2s TEXTURE;
	int NORMAL;
	Vector3s POSITION;

	cpospack6_cnorm_c16tex1_bone2_type_h() = default;
	cpospack6_cnorm_c16tex1_bone2_type_h(
		const Vector3s& _p,
		const Vector2s& _t,
		const int _n,
		const Vector2uc& _b,
		const Vector2uc& _w);

	bool operator==(const cpospack6_cnorm_c16tex1_bone2_type_h& v) const;

} CVertex_PosPack6_CNorm_C16Tex1_Bone2;
#pragma pack(pop)

typedef struct cpospack6_c16tex1_bone2_type_h
{
	Vector2uc  BONES;
	Vector2uc  WEIGHTS;
	Vector2s TEXTURE;
	Vector3s POSITION;

	cpospack6_c16tex1_bone2_type_h() = default;
	cpospack6_c16tex1_bone2_type_h(
		const Vector3s& _p,
		const Vector2s& _t,
		const Vector2uc& _b,
		const Vector2uc& _w);

	bool operator==(const cpospack6_c16tex1_bone2_type_h& v) const;

} CVertex_PosPack6_C16Tex1_Bone2;

typedef struct cpospack6_c16tex2_bone2_type_h
{
	Vector2uc BONES;
	Vector2uc WEIGHTS;
	Vector2s TEXTURE;
	Vector2s TEXTURE2;
	Vector3s POSITION;


	cpospack6_c16tex2_bone2_type_h() = default;
	cpospack6_c16tex2_bone2_type_h(
		const Vector3s& _p,
		const Vector2s& _t,
		const Vector2s& _t2,
		const Vector2uc& _b,
		const Vector2uc& _w
	);

	bool operator==(const cpospack6_c16tex2_bone2_type_h& v);

} CVertex_PosPack6_C16Tex2_Bone2;

//First Part of SMB GEo Chunk is the BaseGeometry Definition BaseGeometryDef

//Second part is I think the GeometryDef

#define MaterialDefSize 136 //bytes
#define GeometryBaseDefSize 72 //bytes

#define RenderableByIndexNonPB 5292387491162064043
#define RenderableByIndex 5792287050554945273
#define MAX_JOINT_NAME 48


struct SMBJoint
{
	Matrix4f granny_inverseBindPose;
	uint32_t granny_flags;
	Vector3f granny_position;
	Vector4f granny_orientation;
	float granny_scale;
	uint32_t granny_parentIndex;
	uint32_t nameOffsetInSMB;
	char name[MAX_JOINT_NAME];
};

struct SMBSkeleton
{
	uint32_t jointCount;
	SMBJoint* joints;
};




enum RenderableFlags
{
	IVRENDERABLE = 0,
	PBIVRENDERABLE = 1
};

enum SMBVertexTypes
{
	PosPack6_CNorm_C16Tex1_Bone2 = 122,
	PosPack6_C16Tex2_Bone2 = 119,
	PosPack6_C16Tex1_Bone2 = 114
};

struct SMBGeoChunk
{
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
	Sphere* spheres;

	SMBGeoChunk() = default;
	void Create(int _numRenderables, int _numMaterials, Allocator* inputDataAllocator);
	~SMBGeoChunk();
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
	StringView fileName;
	std::size_t offsetInHeader;
	std::size_t headerSize;

	friend std::ostream& operator<<(std::ostream& os, const SMBChunk& chunk)
	{
		os << "File Reference Name ";
		os.write(chunk.fileName.stringData, chunk.fileName.charCount) << std::hex << "\n"
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
	StringView name;
	SMBChunk* chunks;
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
	OSFileHandle fileHandle;

	SMBFile() = default;

	SMBFile(StringView file, Allocator* inputDataAllocator, Logger* scratch);

	~SMBFile();

	int LoadFile(StringView name, Allocator* inputDataAllocator, Logger* scratch);

	friend std::ostream& operator<<(std::ostream& os, const SMBFile& file)
	{
		os << std::hex << "File Offset (where real data begins) " << file.fileOffset << "\n"
			<< "File Header Ends " << file.headerStreamEnd << "\n"
			<< "Something " << file.numContiguousBytes << "\n"
			<< "Number of Resources " << file.numResources << "\n"
			<< "------------------" << "\n";
		return os;
	}

	int ReadHeader(Allocator* inputDataAllocator, Logger* scratch);

	int ReadChunk(SMBChunk& chunk, Allocator* inputDataAllocator, Logger* scratch);

	int ProcessFile(Allocator* inputDataAllocator, Logger* scratch);
};


int ProcessGeometryClass(char* data, int numMaterials, SMBGeoChunk* chunk, int contiguousOffset, int systemOffset, Logger* outputLogger, Allocator* inputAllocator);

int GetSMBVertexSize(SMBGeoChunk* geoDef, int renderableIndex);

int GetSMBIndexSize(SMBGeoChunk* geoDef, int renderableIndex);

void SMBCopyVertexData(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile* file, void* vertexDataOut, int decompressed, Allocator* tempMemoryPool);

void SMBCopyIndices(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile* file, void* indexDataOut);

char* GetJointNames(Allocator* inputAllocator, SMBFile* file, SMBChunk* chunk);

int GetBoneData(Allocator* inputAllocator, SMBSkeleton* skel, SMBFile* file);

int GetStringOffset(SMBFile* file, SMBSkeleton* skel);
