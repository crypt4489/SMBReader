#include "SMBFile.h"
#include "math/VertexTypes.h"

//constexpr float dx = 3.051851e-05f;
//constexpr float ax = 0.0009770395f;
//constexpr float bx = 0.0019550342f;
#define MAX_STRING_SIZE 250

SMBFile::SMBFile(StringView file, Allocator* inputDataAllocator, Logger* scratch) 
{
	LoadFile(file, inputDataAllocator, scratch);
}

SMBFile::~SMBFile()
{
	OSCloseFile(&fileHandle);
}

int SMBFile::LoadFile(StringView name, Allocator* inputDataAllocator, Logger* scratch)
{
	int osFlag = OSOpenFile(name.stringData, name.charCount, READ, &fileHandle);

	if (osFlag)
	{
		return -1;
	}

	int processReturn = ProcessFile(inputDataAllocator, scratch);

	return processReturn;
}

int SMBFile::ReadHeader(Allocator* inputDataAllocator, Logger* scratch)
{
	OSReadFile(&fileHandle, 8, reinterpret_cast<char*>(&magic));

	int stringsize = 0;
	OSReadFile(&fileHandle, 4, reinterpret_cast<char*>(&stringsize));

	if (stringsize < 0 || stringsize >= MAX_STRING_SIZE)
	{
		scratch->AddLogMessage(LOGERROR, inputDataAllocator->AllocateFromNullStringCopy("SMB Broken String Size Count"));
		return -1;
	}

	name.stringData = (char*)inputDataAllocator->Allocate(stringsize);
	name.charCount = stringsize;

	OSReadFile(&fileHandle, stringsize, name.stringData);

	OSReadFile(&fileHandle, 36, reinterpret_cast<char*>(&fileOffset));

	return 0;
}

int SMBFile::ReadChunk(SMBChunk& chunk, Allocator* inputDataAllocator, Logger* scratch)
{
	OSReadFile(&fileHandle, 44, reinterpret_cast<char*>(&chunk.magic));

	if (chunk.chunkId != chunk.chunkIdCopy)
	{
		scratch->AddLogMessage(LOGERROR, inputDataAllocator->AllocateFromNullStringCopy("Chunk ID does not match on both for chunk header"));
		return -1;
	}

	if ((chunk.stringsize <= 0 || chunk.stringsize >= MAX_STRING_SIZE) && chunk.chunkType != Joints)
	{
		scratch->AddLogMessage(LOGERROR, inputDataAllocator->AllocateFromNullStringCopy("SMB Broken String Size Count"));
		return -1;
	}

	chunk.fileName.charCount = chunk.stringsize;
	chunk.fileName.stringData = (char*)inputDataAllocator->Allocate(chunk.stringsize);
	OSReadFile(&fileHandle, chunk.stringsize, chunk.fileName.stringData);

	chunk.offsetInHeader = fileHandle.filePointer;
	int offset = (chunk.numOfBytesAfterTag - (chunk.stringsize + 16));
	chunk.headerSize = offset;
	int next = chunk.offsetInHeader + offset;

	OSSeekFile(&fileHandle, next, BEGIN);

	return 0;
}

int SMBFile::ProcessFile(Allocator* inputDataAllocator, Logger* scratch)
{
	if (ReadHeader(inputDataAllocator, scratch))
	{
		return -1;
	}

	chunks = (SMBChunk*)inputDataAllocator->Allocate(sizeof(SMBChunk) * numResources);

	for (uint32_t i = 0; i<numResources; i++)
	{
		if (ReadChunk(chunks[i], inputDataAllocator, scratch))
		{
			return -1;
		}
	}

	return 0;
}

void SMBGeoChunk::Create(int _numRenderables, int _numMaterials, Allocator* inputDataAllocator)
{
	memset(this, 0, sizeof(SMBGeoChunk));

	numRenderables = _numRenderables;

	int chunkSize = _numRenderables * 9 * sizeof(int);

	chunkSize += (sizeof(int) * _numMaterials);
	chunkSize += (sizeof(Vector4f) * _numRenderables);

	int* chunkPtr = (int*)inputDataAllocator->Allocate(chunkSize);

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
	spheres = (Sphere*)chunkPtr;

	memset(&axialBox, 0, sizeof(AxisBox));
	memset(indexOffsetInArchive, 0xFF, sizeof(int) * _numRenderables);
	memset(indicesCount, 0xFF, sizeof(int) * _numRenderables);
}

SMBGeoChunk::~SMBGeoChunk()
{
}

#define MAX_POSSIBLE_RENDERABLES 100
#define MAX_MATERIALS 15

int ProcessGeometryClass(char* data, int numMaterials, SMBGeoChunk* chunk, int contiguousOffset, int systemOffset, Logger* outputLogger, Allocator* inputAllocator)
{
	char* iter = data;

	int geometryType = *((int*)(iter + 8));

	int numRenderablesOffset = 0;
	int axialBoxOffset = 0;
	int geometryTypeDefSize = 0;

	if (geometryType == 1)
	{
		numRenderablesOffset = 68;
		axialBoxOffset = 36;
		geometryTypeDefSize = GeometryBaseDefSize;
	}
	else if (geometryType == 2)
	{
		numRenderablesOffset = 76;
		axialBoxOffset = 44;
		geometryTypeDefSize = 80;
	}
	else
	{
		outputLogger->AddLogMessage(LOGERROR, inputAllocator->AllocateFromNullStringCopy("Unhandled geometry type"));
		return -1;
	}

	int numRenderables = 0;

	memcpy(&numRenderables, iter + numRenderablesOffset, 4);

	if (numRenderables <= 0 || numRenderables > MAX_POSSIBLE_RENDERABLES)
	{
		outputLogger->AddLogMessage(LOGERROR, inputAllocator->AllocateFromNullStringCopy("Bad number of renderables read from file"));
		return -1;
	}

	chunk->Create(numRenderables, MAX_MATERIALS, inputAllocator);

	char* axialBox = iter + axialBoxOffset;
	
	memcpy(&chunk->axialBox.min, axialBox, sizeof(float) * 3);
	memcpy(&chunk->axialBox.max, axialBox + sizeof(float) * 3, sizeof(float) * 3);

	chunk->axialBox.min.w = 1.0f;
	chunk->axialBox.max.w = 1.0f;

	char* material = iter + geometryTypeDefSize;

	int renderableIndex = 0;

	int* lMaterialCount = chunk->materialsCount;

	int* lMaterialId = chunk->materialsId;

	int materialCount = 0;

	int maxGeometryDepth = 100;

	while (maxGeometryDepth--)
	{
		int header = *((int*)material);

		if (header != 737893)
		{
			break;
		}

		uint64_t definitionID = *((uint64_t*)(material + 4));

		if (definitionID == RenderableByIndex)
		{
			char* renderable = material + 4 + 8 + 8;
			memcpy(&chunk->spheres[renderableIndex], renderable + 8, sizeof(Vector4f));
			int indexCount = *((int*)(renderable + (18 + 48)));
			int vertexCount = *((int*)(renderable + (18 + 16)));
			int vertexOffset = *((int*)(renderable + (18 + 16 + 16)));
			int indexOffset = *((int*)(renderable + (18 + 16 + 16 + 8)));
			SMBVertexTypes vertexType = *((SMBVertexTypes*)(renderable + (18 + 16 + 8)));
			chunk->indicesCount[renderableIndex] = indexCount;
			chunk->verticesCount[renderableIndex] = vertexCount;
			chunk->renderablesTypes[renderableIndex] = PBIVRENDERABLE;
			chunk->vertexTypes[renderableIndex] = vertexType;
			chunk->vertexOffsetInArchive[renderableIndex] = vertexOffset+ contiguousOffset;
			chunk->indexOffsetInArchive[renderableIndex] = indexOffset + contiguousOffset;
			material += (64 + 26);
			renderableIndex++;
		}
		else if (definitionID == RenderableByIndexNonPB)
		{
			char* renderable = material + 4 + 8 + 8;
			memcpy(&chunk->spheres[renderableIndex], renderable + 8, sizeof(Vector4f));
			int vertexCount = *((int*)(renderable + (18 + 16)));
			int vertexOffset = *((int*)(renderable + (18 + 16 + 16)));
			int indexCount = *((int*)(renderable + (18 + 16 + 16 + 4)));
			int indexOffset = *((int*)(renderable + (18 + 16 + 16 + 8)));
			SMBVertexTypes vertexType = *((SMBVertexTypes*)(renderable + (18 + 16 + 8)));

			chunk->verticesCount[renderableIndex] = vertexCount;
			chunk->renderablesTypes[renderableIndex] = IVRENDERABLE;
			chunk->vertexTypes[renderableIndex] = vertexType;
			chunk->vertexOffsetInArchive[renderableIndex] = vertexOffset + contiguousOffset;
			chunk->indicesCount[renderableIndex] = indexCount ;
			chunk->indexOffsetInArchive[renderableIndex] = indexOffset+ systemOffset;
			material += (64 + 18);
			renderableIndex++;
		}
		else {
			int materialType = *((int*)(material + 4));

			if (materialType == -1373022986)
			{
				*lMaterialCount = 2;
			}
			else {
				*lMaterialCount = 1;
			}

			materialCount += *lMaterialCount;

			material += MaterialDefSize;

			int skipCounter = *((int*)material);

			int skipMax = 100;

			while (skipCounter != 0 && skipMax > 0)
			{
				material += (skipCounter + 4);
				skipCounter = *((int*)material);
			}

			int currMaterialCount = *lMaterialCount;

			while (currMaterialCount--)
			{
				if (skipCounter == 737893) break;

				*lMaterialId = *((int*)(material + 4));

				lMaterialId++;

				material += 8;

				skipCounter = *((int*)material);

				material += (skipCounter + 4);
			}

			lMaterialCount++;

			renderableIndex++;

			if (renderableIndex == numRenderables)
			{
				renderableIndex = 0;
				int numberMinMaxes = *((int*)material);

				material += ((198 * numberMinMaxes) + 8);
			}

			skipCounter = *((int*)material);

			if (-1866346045 == materialType)
			{
				char* start = material;
				while (*material != 0x65 && (material != start + 20)) material++;
			}
		}
	}

	chunk->numMaterials = materialCount;

	return 0;
}

int GetSMBVertexSize(SMBGeoChunk* geoDef, int renderableIndex)
{
	SMBVertexTypes type = geoDef->vertexTypes[renderableIndex];
	int size = 0;

	switch (type)
	{
	case PosPack6_CNorm_C16Tex1_Bone2:
		size = sizeof(Vertex_PosPack6_CNorm_C16Tex1_Bone2);
		break;
	case PosPack6_C16Tex2_Bone2:
		size = sizeof(Vertex_PosPack6_C16Tex2_Bone2);
		break;
	case PosPack6_C16Tex1_Bone2:
		size = sizeof(Vertex_PosPack6_C16Tex1_Bone2);
		break;
	}

	return size * geoDef->verticesCount[renderableIndex];
}

int GetSMBIndexSize(SMBGeoChunk* geoDef, int renderableIndex)
{
	int type = geoDef->renderablesTypes[renderableIndex];
	return sizeof(uint16_t) * geoDef->indicesCount[renderableIndex];
}

void SMBCopyVertexData(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile* file, void* vertexDataOut, int decompressed, Allocator* tempMemoryPool)
{
	OSFileHandle* handle = &file->fileHandle;

	OSSeekFile(handle, geoDefinition->vertexOffsetInArchive[renderableIndex], BEGIN);

	SMBVertexTypes type = geoDefinition->vertexTypes[renderableIndex];

	int vCount = geoDefinition->verticesCount[renderableIndex];

	int vertexSize = vCount;

	switch (type)
	{
	case PosPack6_CNorm_C16Tex1_Bone2:
		vertexSize *= sizeof(CVertex_PosPack6_CNorm_C16Tex1_Bone2);
		break;
	case PosPack6_C16Tex2_Bone2:
		vertexSize *= sizeof(CVertex_PosPack6_C16Tex2_Bone2);
		break;
	case PosPack6_C16Tex1_Bone2:
		vertexSize *= sizeof(CVertex_PosPack6_C16Tex1_Bone2);
		break;
	}

	if (decompressed)
	{
		void* data = tempMemoryPool->Allocate(vertexSize);

		OSReadFile(handle, vertexSize, (char*)data);

		unsigned char* g = (unsigned char*)data;

		switch (type) {
		case PosPack6_CNorm_C16Tex1_Bone2:
		{
			Vertex_PosPack6_CNorm_C16Tex1_Bone2* vertices = (Vertex_PosPack6_CNorm_C16Tex1_Bone2*)vertexDataOut;
			for (int i = 0; i < vCount; i++)
			{

				Vertex_PosPack6_CNorm_C16Tex1_Bone2* vertex = &vertices[i];
				uint32_t l = g[2], h = g[3];
				vertex->BONES.x = g[0];
				vertex->BONES.y = g[1];
				vertex->WEIGHTS.x = ((float)l) * 0.00392156f;
				vertex->WEIGHTS.y = ((float)h) * 0.00392156f;

				int32_t norms = *(int32_t*)&g[8];

				

				vertex->NORMAL.x = ((float)(norms << 21) * (1.0 / (INT_MAX - 1)));
				vertex->NORMAL.y = ((float)((norms & 0xfffff800) << 10) * (1.0 / (INT_MAX - 1)));
				vertex->NORMAL.z = ((float)(norms & 0xffc00000) * (1.0 / (INT_MAX)));


				int16_t t[2];
				t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
				t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];

				vertex->TEXTURE = DecompressTexCoords(Vector2s(t[0], t[1]));


				int16_t s[3];

				s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
				s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
				s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

				Vector3f pos = DecompressPosition(Vector3s(s[0], s[1], s[2]), geoDefinition->axialBox);

				vertex->POSITION = { pos.x, pos.y, pos.z, 1.0f };

				g += sizeof(CVertex_PosPack6_C16Tex2_Bone2);

			}
			break;
		}
		case PosPack6_C16Tex2_Bone2:
		{
			Vertex_PosPack6_C16Tex2_Bone2* vertices = (Vertex_PosPack6_C16Tex2_Bone2*)vertexDataOut;
			for (int i = 0; i < vCount; i++)
			{
				Vertex_PosPack6_C16Tex2_Bone2* vertex = &vertices[i];
				uint32_t l = g[2], h = g[3];
				vertex->BONES.x = g[0];
				vertex->BONES.y = g[1];
				vertex->WEIGHTS.x = ((float)l) * 0.00392156f;
				vertex->WEIGHTS.y = ((float)h) * 0.00392156f;



				int16_t t[2];
				t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
				t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];

				vertex->TEXTURE = DecompressTexCoords(Vector2s(t[0], t[1]));

				t[0] = (((int16_t)g[9] & 0xff) << 8) | g[8];
				t[1] = (((int16_t)g[11] & 0xff) << 8) | g[10];

				vertex->TEXTURE2 = DecompressTexCoords(Vector2s(t[0], t[1]));

				int16_t s[3];

				s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
				s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
				s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

				Vector3f pos = DecompressPosition(Vector3s(s[0], s[1], s[2]), geoDefinition->axialBox);

				vertex->POSITION = { pos.x, pos.y, pos.z, 1.0f };

				g += sizeof(CVertex_PosPack6_CNorm_C16Tex1_Bone2);
			}
			break;
		}
		case PosPack6_C16Tex1_Bone2:
		{
			Vertex_PosPack6_C16Tex1_Bone2* vertices = (Vertex_PosPack6_C16Tex1_Bone2*)vertexDataOut;
			for (int i = 0; i < vCount; i++)
			{
				Vertex_PosPack6_C16Tex1_Bone2* vertex = &vertices[i];
				uint32_t l = g[2], h = g[3];
				vertex->BONES.x = g[0];
				vertex->BONES.y = g[1];
				vertex->WEIGHTS.x = ((float)l) * 0.00392156f;
				vertex->WEIGHTS.y = ((float)h) * 0.00392156f;

				int16_t t[2];
				t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
				t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];

				vertex->TEXTURE = DecompressTexCoords(Vector2s(t[0], t[1]));

				int16_t s[3];

				s[0] = (((int16_t)g[9] & 0xff) << 8) | g[8];
				s[1] = (((int16_t)g[11] & 0xff) << 8) | g[10];
				s[2] = (((int16_t)g[13] & 0xff) << 8) | g[12];

				Vector3f pos = DecompressPosition(Vector3s(s[0], s[1], s[2]), geoDefinition->axialBox);

				vertex->POSITION = { pos.x, pos.y, pos.z, 1.0f };

				g += sizeof(CVertex_PosPack6_C16Tex1_Bone2);
			}

			break;
		}
		}
	} else {
		OSReadFile(handle, vertexSize, (char*)vertexDataOut);
	}
}

void SMBCopyIndices(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile* file, void* indexDataOut)
{
	int renderableType = geoDefinition->renderablesTypes[renderableIndex];
	
	int iCount = std::min(geoDefinition->indicesCount[renderableIndex], 65535);

	OSFileHandle* handle = &file->fileHandle;

	uint16_t* indices = (uint16_t*)indexDataOut;

	OSSeekFile(handle, geoDefinition->indexOffsetInArchive[renderableIndex], BEGIN);

	if (renderableType == PBIVRENDERABLE)
	{
		int iter = 0;

		while (iCount > 0) {

			uint32_t byteInput;

			OSReadFile(handle, 4, (char*)&byteInput);

			uint32_t stride = (byteInput >> 0x12) & 0x7ff;
			uint32_t indexType = (byteInput & 0x3ffff);

			if (indexType == 0x1800)
			{
				OSReadFile(handle, stride * 4, (char*)indices);

				indices += (stride * 2);

				iCount -= (stride * 2);
			}
			else if (indexType == 0x1808)
			{
				OSReadFile(handle, 2, (char*)indices);
				iCount--;
			}
			else if (indexType == 0x17fc) {
				
			}
		}
	} 
	else if (renderableType == IVRENDERABLE)
	{
		OSReadFile(handle, iCount * 4, (char*)indices);
	}
}

pospack6_cnorm_c16tex1_bone2_type_h::pospack6_cnorm_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector3f& _n, const Vector2i& _b, const Vector2f& _w) :
	POSITION(_p), TEXTURE(_t), NORMAL(_n), BONES(_b), WEIGHTS(_w)
{
}


bool pospack6_cnorm_c16tex1_bone2_type_h::operator==(const pospack6_cnorm_c16tex1_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.NORMAL == this->NORMAL) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

pospack6_c16tex1_bone2_type_h::pospack6_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector2i& _b, const Vector2f& _w) :
	POSITION(_p), TEXTURE(_t), BONES(_b), WEIGHTS(_w)
{
}


bool pospack6_c16tex1_bone2_type_h::operator==(const pospack6_c16tex1_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

pospack6_c16tex2_bone2_type_h::pospack6_c16tex2_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector2f& _t2, const Vector2i& _b, const Vector2f& _w) :
	POSITION(_p), TEXTURE(_t), TEXTURE2(_t2), BONES(_b), WEIGHTS(_w)
{
}


bool pospack6_c16tex2_bone2_type_h::operator==(const pospack6_c16tex2_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.TEXTURE2 == this->TEXTURE2) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

cpospack6_cnorm_c16tex1_bone2_type_h::cpospack6_cnorm_c16tex1_bone2_type_h(
	const Vector3s& _p,
	const Vector2s& _t,
	const int _n,
	const Vector2uc& _b,
	const Vector2uc& _w)
	: POSITION(_p), TEXTURE(_t), NORMAL(_n), BONES(_b), WEIGHTS(_w)
{
}

bool cpospack6_cnorm_c16tex1_bone2_type_h::operator==(const cpospack6_cnorm_c16tex1_bone2_type_h& v) const
{
	return POSITION == v.POSITION &&
		TEXTURE == v.TEXTURE &&
		NORMAL == v.NORMAL &&
		WEIGHTS == v.WEIGHTS &&
		BONES == v.BONES;
}


cpospack6_c16tex1_bone2_type_h::cpospack6_c16tex1_bone2_type_h(
	const Vector3s& _p,
	const Vector2s& _t,
	const Vector2uc& _b,
	const Vector2uc& _w)
	: POSITION(_p), TEXTURE(_t), BONES(_b), WEIGHTS(_w)
{
}

bool cpospack6_c16tex1_bone2_type_h::operator==(const cpospack6_c16tex1_bone2_type_h& v) const
{
	return POSITION == v.POSITION &&
		TEXTURE == v.TEXTURE &&
		WEIGHTS == v.WEIGHTS &&
		BONES == v.BONES;
}

cpospack6_c16tex2_bone2_type_h::cpospack6_c16tex2_bone2_type_h(const Vector3s& _p, const Vector2s& _t, const Vector2s& _t2, const Vector2uc& _b, const Vector2uc& _w) :
	POSITION(_p), TEXTURE(_t), TEXTURE2(_t2), BONES(_b), WEIGHTS(_w)
{
}


bool cpospack6_c16tex2_bone2_type_h::operator==(const cpospack6_c16tex2_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.TEXTURE2 == this->TEXTURE2) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

char* GetJointNames(Allocator* inputAllocator, SMBFile* file, SMBChunk* chunk)
{
	OSSeekFile(&file->fileHandle, chunk->offsetInHeader, BEGIN);

	int jointNameSize = 0;

	OSReadFile(&file->fileHandle, 4, (char*)&jointNameSize);

	if (jointNameSize != chunk->stringsize)
		return nullptr;

	char* stringData = (char*)inputAllocator->Allocate(jointNameSize);

	OSReadFile(&file->fileHandle, jointNameSize, stringData);

	return stringData;
}

int GetBoneData(Allocator* inputAllocator, SMBSkeleton* skel, SMBFile* file)
{
	OSReadFile(&file->fileHandle, 4, (char*)&skel->jointCount);

	skel->joints = (SMBJoint*)inputAllocator->Allocate(sizeof(SMBJoint) * skel->jointCount);

	for (uint32_t i = 0; i < skel->jointCount; i++)
	{
		SMBJoint* joint = &skel->joints[i];

		OSReadFile(&file->fileHandle, sizeof(Matrix4f), (char*)&joint->granny_inverseBindPose);

		OSReadFile(&file->fileHandle, 4, (char*)&joint->granny_flags);

		OSReadFile(&file->fileHandle, sizeof(Vector3f), (char*)&joint->granny_position);

		OSReadFile(&file->fileHandle, sizeof(Vector4f), (char*)&joint->granny_orientation);

		OSReadFile(&file->fileHandle, sizeof(float), (char*)&joint->granny_scale);

		OSSeekFile(&file->fileHandle, 4, CURRENT); //random ptr

		OSReadFile(&file->fileHandle, sizeof(uint32_t), (char*)&joint->granny_parentIndex);

		OSSeekFile(&file->fileHandle, 4, CURRENT); // struct padding
	}

	return 0;
}

int GetStringOffset(SMBFile* file, SMBSkeleton* skel)
{
	int index = 0;

	int numJoints = skel->jointCount;

	while (numJoints--)
	{
		uint32_t input = 0;

		OSReadFile(&file->fileHandle, sizeof(int), (char*)&input);

		skel->joints[index].nameOffsetInSMB = input;

		index++;
	}

	return 1;
}