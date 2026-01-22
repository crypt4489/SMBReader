#include "SMBFile.h"

#include "VertexTypes.h"


void SMBGeoChunk::Create(int _numRenderables, int _numMaterials)
{
	memset(this, 0, sizeof(SMBGeoChunk));
	numRenderables = _numRenderables;
	int chunkSize = _numRenderables * 9 * sizeof(int);
	chunkSize += (sizeof(int) * _numMaterials);
	chunkSize += (sizeof(Vector4f) * +numRenderables);

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
		spheres = (Sphere*)chunkPtr;

		memset(&axialBox, 0, sizeof(AxisBox));
		memset(indexOffsetInArchive, 0xFF, sizeof(int) * _numRenderables);
		memset(indicesCount, 0xFF, sizeof(int) * _numRenderables);
	}


}

SMBGeoChunk::~SMBGeoChunk()
{
	free(renderablesTypes);
}

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
	OSFileHandle* handle;

	auto ret = FileManager::OpenFile(path, READ);

	if (!ret)
	{
		throw std::runtime_error("SMB file is unable to be opened");
	}

	OSFileHandle* fh = FileManager::GetFile(ret.value());

	ProcessFile(fh);

	return std::move(ret.value());
}

FileID SMBFile::LoadFile(const std::string& name)
{
	OSFileHandle* handle;

	auto ret = FileManager::OpenFile(name, READ);

	if (!ret)
	{
		throw std::runtime_error("SMB file is unable to be opened");
	}

	OSFileHandle* fh = FileManager::GetFile(ret.value());

	ProcessFile(fh);

	return std::move(ret.value());
}

void SMBFile::ReadHeader(OSFileHandle* fh)
{
	OSReadFile(fh, 8, reinterpret_cast<char*>(&magic));

	int stringsize = 0;
	OSReadFile(fh, 4, reinterpret_cast<char*>(&stringsize));

	name.resize(stringsize);
	OSReadFile(fh, stringsize, name.data());

	OSReadFile(fh, 36, reinterpret_cast<char*>(&fileOffset));

	chunks.resize(numResources);
}

void SMBFile::ReadChunk(OSFileHandle* fh, SMBChunk& chunk)
{
	OSReadFile(fh, 44, reinterpret_cast<char*>(&chunk.magic));

	chunk.fileName.resize(chunk.stringsize);
	OSReadFile(fh, chunk.stringsize, chunk.fileName.data());

	chunk.offsetInHeader = fh->filePointer;
	int offset = (chunk.numOfBytesAfterTag - (chunk.stringsize + 16));
	chunk.headerSize = offset;
	int next = chunk.offsetInHeader + offset;

	OSSeekFile(fh, next, BEGIN);

	if (chunk.chunkId != chunk.chunkIdCopy)
	{
		throw std::runtime_error("chunk did not equal");
	}
}

void SMBFile::ProcessFile(OSFileHandle* fh)
{
	ReadHeader(fh);

	for (auto& i : chunks)
	{
		ReadChunk(fh, i);
	
	}
}

void ProcessGeometryClass(char* data, int numMaterials, SMBGeoChunk* chunk, int contiguousOffset, int systemOffset)
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

	int numRenderables = 0;

	memcpy(&numRenderables, iter + numRenderablesOffset, 4);

	if (numRenderables <= 0 || numRenderables > 15)
	{
		return;
	}

	chunk->Create(numRenderables, 15);

	char* axialBox = iter + axialBoxOffset;
	memcpy(&chunk->axialBox.min, axialBox, sizeof(float) * 3);
	memcpy(&chunk->axialBox.max, axialBox + sizeof(float) * 3, sizeof(float) * 3);
	chunk->axialBox.min.w = 1.0f;
	chunk->axialBox.max.w = 1.0f;

	char* material = iter + geometryTypeDefSize;

	int renderableIndex = 0;

	int* lMaterialCount = chunk->materialsCount;

	int* lMaterialId = chunk->materialsId;


	while (true)
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

			int type = *((int*)(material + 4));

			if (type == -1373022986)
			{
				*lMaterialCount = 2;
			}
			else {
				*lMaterialCount = 1;
			}




			material += MaterialDefSize;

			int copy = *((int*)material);


			while (copy != 0)
			{
				material += (copy + 4);
				copy = *((int*)material);
			}



			int iter = *lMaterialCount;

			while (iter--)
			{
				if (copy == 737893) break;

				*lMaterialId = *((int*)(material + 4));

				lMaterialId++;

				material += 8;

				copy = *((int*)material);

				material += (copy + 4);

			}

			lMaterialCount++;

			renderableIndex++;

			if (renderableIndex == numRenderables)
			{
				renderableIndex = 0;
				int numberMinMaxes = *((int*)material);

				material += ((198 * numberMinMaxes) + 8);

			}

			copy = *((int*)material);

			if (-1866346045 == type)
			{
				char* start = material;
				while (*material != 0x65 && (material != start + 20)) material++;
			}

		}
	}
}


static Vector2f converttexcoords16(int16_t* huh)
{
	float x = huh[0] * dx * 16.0f;
	float y = huh[1] * dx * 16.0f;


	return Vector2f(x, y);
}

static Vector3f pack6decomp(int16_t* hello, AxisBox& box)
{
	float x = ((hello[0] * dx) + 1.0f) * 0.5f;
	float y = (((hello[1] * dx)) + 1.0f) * 0.5f;
	float z = (((hello[2] * dx)) + 1.0f) * 0.5f;

	x = ((box.max.x - box.min.x) * x) + box.min.x;
	y = ((box.max.y - box.min.y) * y) + box.min.y;
	z = ((box.max.z - box.min.z) * z) + box.min.z;

	return Vector3f(x, y, z);
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


void SMBCopyVertexData(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile& file, void* vertexDataOut, int decompressed)
{
	OSFileHandle* handle = FileManager::GetFile(file.id);

	OSSeekFile(handle, geoDefinition->vertexOffsetInArchive[renderableIndex], BEGIN);

	SMBVertexTypes type = geoDefinition->vertexTypes[renderableIndex];

	int vertexSize = geoDefinition->verticesCount[renderableIndex];


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

	std::vector<char> data(vertexSize);

	OSReadFile(handle, vertexSize, data.data());

	
	int vSize = geoDefinition->verticesCount[renderableIndex];

	unsigned char* g = (unsigned char*)data.data();

	if (decompressed)
	{

		switch (type) {
		case PosPack6_CNorm_C16Tex1_Bone2:
		{
			Vertex_PosPack6_CNorm_C16Tex1_Bone2* vertices = (Vertex_PosPack6_CNorm_C16Tex1_Bone2*)vertexDataOut;
			for (int i = 0; i < vSize; i++)
			{

				Vertex_PosPack6_CNorm_C16Tex1_Bone2* vertex = &vertices[i];
				uint32_t l = g[2], h = g[3];
				vertex->BONES.x = g[0];
				vertex->BONES.y = g[1];
				vertex->WEIGHTS.x = ((float)l) * 0.00392156f;
				vertex->WEIGHTS.y = ((float)h) * 0.00392156f;

				int32_t norms = *(int32_t*)&g[8];

				//norms = bswap32_u(norms);

				vertex->NORMAL.x = ((float)(norms << 21) * (1.0/(INT_MAX-1)));
				vertex->NORMAL.y = ((float)((norms & 0xfffff800) << 10) * (1.0/(INT_MAX-1)));
				vertex->NORMAL.z = ((float)(norms & 0xffc00000) * (1.0/(INT_MAX)));

				int16_t t[2];
				t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
				t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];

				vertex->TEXTURE = converttexcoords16(t);


				int16_t s[3];

				s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
				s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
				s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

				Vector3f pos = pack6decomp(s, geoDefinition->axialBox);

				vertex->POSITION = { pos.x, pos.y, pos.z, 1.0f };

				g += sizeof(CVertex_PosPack6_C16Tex2_Bone2);

			}
			break;
		}
		case PosPack6_C16Tex2_Bone2:
		{
			Vertex_PosPack6_C16Tex2_Bone2* vertices = (Vertex_PosPack6_C16Tex2_Bone2*)vertexDataOut;
			for (int i = 0; i < vSize; i++)
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

				vertex->TEXTURE = converttexcoords16(t);

				t[0] = (((int16_t)g[9] & 0xff) << 8) | g[8];
				t[1] = (((int16_t)g[11] & 0xff) << 8) | g[10];

				vertex->TEXTURE2 = converttexcoords16(t);

				int16_t s[3];

				s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
				s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
				s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

				Vector3f pos = pack6decomp(s, geoDefinition->axialBox);

				vertex->POSITION = { pos.x, pos.y, pos.z, 1.0f };

				g += sizeof(CVertex_PosPack6_CNorm_C16Tex1_Bone2);
			}
			break;
		}
		case PosPack6_C16Tex1_Bone2:
		{
			Vertex_PosPack6_C16Tex1_Bone2* vertices = (Vertex_PosPack6_C16Tex1_Bone2*)vertexDataOut;
			for (int i = 0; i < vSize; i++)
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

				vertex->TEXTURE = converttexcoords16(t);

				int16_t s[3];

				s[0] = (((int16_t)g[9] & 0xff) << 8) | g[8];
				s[1] = (((int16_t)g[11] & 0xff) << 8) | g[10];
				s[2] = (((int16_t)g[13] & 0xff) << 8) | g[12];

				Vector3f pos = pack6decomp(s, geoDefinition->axialBox);

				vertex->POSITION = { pos.x, pos.y, pos.z, 1.0f };

				g += sizeof(CVertex_PosPack6_C16Tex1_Bone2);
			}

			break;
		}
		}
	} else {

		memcpy(vertexDataOut, g, vertexSize);
	}
}

void SMBCopyIndices(SMBGeoChunk* geoDefinition, int renderableIndex, SMBFile& file, void* indexDataOut)
{
	int renderableType = geoDefinition->renderablesTypes[renderableIndex];
	
	int iCount = geoDefinition->indicesCount[renderableIndex];

	OSFileHandle* handle = FileManager::GetFile(file.id);

	

	uint16_t* indices = (uint16_t*)indexDataOut;

	OSSeekFile(handle, geoDefinition->indexOffsetInArchive[renderableIndex], BEGIN);

	if (renderableType == PBIVRENDERABLE)
	{

		int iter = 0;

		while (iCount > 0) {


			uint32_t god;

			OSReadFile(handle, 4, (char*)&god);

			uint32_t stride = (god >> 0x12) & 0x7ff;
			uint32_t indexType = (god & 0x3ffff);

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