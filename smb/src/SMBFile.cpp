#include "SMBFile.h"

#include "VertexTypes.h"
int VertexCompressedSizes[3] = {
	18,
	18,
	14,
};


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
	chunk.headerSize = offset;
	std::streamoff next = chunk.offsetInHeader + offset;
	fh.seekg(next, std::ios_base::beg);

	if (chunk.chunkId != chunk.chunkIdCopy)
	{
		throw std::runtime_error("chunk did not equal");
	}
}

void SMBFile::ProcessFile(std::fstream& fh)
{
	ReadHeader(fh);

	for (auto& i : chunks)
	{
		ReadChunk(fh, i);
	
	}
}

SMBGeoChunk* ProcessGeometryClass(char* data, int numMaterials)
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
		return nullptr;
	}

	SMBGeoChunk* geoChunk = new SMBGeoChunk(numRenderables, 15);

	char* axialBox = iter + axialBoxOffset;
	memcpy(&geoChunk->axialBox.min, axialBox, sizeof(float) * 3);
	memcpy(&geoChunk->axialBox.max, axialBox + sizeof(float) * 3, sizeof(float) * 3);

	char* material = iter + geometryTypeDefSize;

	int renderableIndex = 0;

	int* lMaterialCount = geoChunk->materialsCount;

	int* lMaterialId = geoChunk->materialsId;

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
			int indexCount = *((int*)(renderable + (18 + 48)));
			int vertexCount = *((int*)(renderable + (18 + 16)));
			int vertexOffset = *((int*)(renderable + (18 + 16 + 16)));
			int indexOffset = *((int*)(renderable + (18 + 16 + 16 + 8)));
			SMBVertexTypes vertexType = *((SMBVertexTypes*)(renderable + (18 + 16 + 8)));
			geoChunk->indicesCount[renderableIndex] = indexCount;
			geoChunk->verticesCount[renderableIndex] = vertexCount;
			geoChunk->renderablesTypes[renderableIndex] = PBIVRENDERABLE;
			geoChunk->vertexTypes[renderableIndex] = vertexType;
			geoChunk->vertexOffsetInArchive[renderableIndex] = vertexOffset;
			geoChunk->indexOffsetInArchive[renderableIndex] = indexOffset;
			material += (64 + 26);
			renderableIndex++;
		}
		else if (definitionID == RenderableByIndexNonPB)
		{
			char* renderable = material + 4 + 8 + 8;
			int vertexCount = *((int*)(renderable + (18 + 16)));
			int vertexOffset = *((int*)(renderable + (18 + 16 + 16)));
			int indexCount = *((int*)(renderable + (18 + 16 + 16+4)));
			int indexOffset = *((int*)(renderable + (18 + 16 + 16+8)));
			SMBVertexTypes vertexType = *((SMBVertexTypes*)(renderable + (18 + 16 + 8)));
			
			geoChunk->verticesCount[renderableIndex] = vertexCount;
			geoChunk->renderablesTypes[renderableIndex] = IVRENDERABLE;
			geoChunk->vertexTypes[renderableIndex] = vertexType;
			geoChunk->vertexOffsetInArchive[renderableIndex] = vertexOffset;
			geoChunk->indicesCount[renderableIndex] = indexCount;
			geoChunk->indexOffsetInArchive[renderableIndex] = indexOffset;
			material += (64 + 18);
			renderableIndex++;
		}
		else {

			int type = *((int*)(material+4));

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
				
				material += ((198 * numberMinMaxes)+8);
				
			}

			copy = *((int*)material);

			if (-1866346045 == type)
			{
				char* start = material;
				while (*material != 0x65 && (material != start + 20)) material++;
			}

		}
	}
	return geoChunk;
}

static glm::vec2 converttexcoords16(int16_t* huh)
{
	float x = huh[0] * dx * 16.0f;
	float y = huh[1] * dx * 16.0f;


	return glm::vec2(x, y);
}

static glm::vec3 pack6decomp(int16_t* hello, AxisBox& box)
{
	float x = ((hello[0] * dx) + 1.0f) * 0.5f;
	float y = (((hello[1] * dx)) + 1.0f) * 0.5f;
	float z = (((hello[2] * dx)) + 1.0f) * 0.5f;

	x = ((box.max.x - box.min.x) * x) + box.min.x;
	y = ((box.max.y - box.min.y) * y) + box.min.y;
	z = ((box.max.z - box.min.z) * z) + box.min.z;

	return glm::vec3(x, y, z);
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
	FileHandle* handle = FileManager::GetFile(file.id);

	handle->streamHandle.seekg(geoDefinition->vertexAndIndicesInfo + geoDefinition->vertexOffsetInArchive[renderableIndex]);

	SMBVertexTypes type = geoDefinition->vertexTypes[renderableIndex];

	int vertexSize = geoDefinition->verticesCount[renderableIndex];


	switch (type)
	{
	case PosPack6_CNorm_C16Tex1_Bone2:
		vertexSize *= VertexCompressedSizes[PosPack6_CNorm_C16Tex1_Bone2_Size];
		break;
	case PosPack6_C16Tex2_Bone2:
		vertexSize *= VertexCompressedSizes[PosPack6_C16Tex2_Bone2_Size];
		break;
	case PosPack6_C16Tex1_Bone2:
		vertexSize *= VertexCompressedSizes[PosPack6_C16Tex1_Bone2_Size];
		break;
	}

	std::vector<char> data(vertexSize);

	handle->streamHandle.read(data.data(), vertexSize);

	
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

				vertex->NORMAL.x = (((float)(norms & 0x7ff)) * ax) - 1.0f;
				vertex->NORMAL.y = (((float)((norms & 0x3ff800) >> 11)) * ax) - 1.0f;
				vertex->NORMAL.z = (((float)((norms & 0xffc00000) >> 22)) * bx) - 1.0f;

				int16_t t[2];
				t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
				t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];

				vertex->TEXTURE = converttexcoords16(t);


				int16_t s[3];

				s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
				s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
				s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

				vertex->POSITION = glm::vec4(pack6decomp(s, geoDefinition->axialBox), 1.0f);

				g += VertexCompressedSizes[PosPack6_CNorm_C16Tex1_Bone2_Size];

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

				vertex->POSITION = glm::vec4(pack6decomp(s, geoDefinition->axialBox), 1.0f);

				g += VertexCompressedSizes[PosPack6_C16Tex2_Bone2_Size];
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

				vertex->POSITION = glm::vec4(pack6decomp(s, geoDefinition->axialBox), 1.0f);

				g += VertexCompressedSizes[PosPack6_C16Tex1_Bone2_Size];
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

	FileHandle* handle = FileManager::GetFile(file.id);

	uint16_t* indices = (uint16_t*)indexDataOut;

	auto& stream = handle->streamHandle;

	if (renderableType == PBIVRENDERABLE)
	{
		stream.seekg(geoDefinition->vertexAndIndicesInfo + geoDefinition->indexOffsetInArchive[renderableIndex]);

		int iter = 0;

		while (iCount > 0) {


			uint32_t god;

			stream.read((char*)&god, 4);

			uint32_t stride = (god >> 0x12) & 0x7ff;
			uint32_t indexType = (god & 0x3ffff);

			if (indexType == 0x1800)
			{
				stream.read(
					(char*)indices,
					stride * 4
				);

				indices += (stride * 2);

				iCount -= (stride * 2);
			}

			else if (indexType == 0x1808)
			{
				stream.read((char*)indices, 2);
				iCount--;
			}
			else if (indexType == 0x17fc) {
				
			}
		}

	} 
	else if (renderableType == IVRENDERABLE)
	{
		stream.seekg(geoDefinition->indexOffsetInArchive[renderableIndex]);

		stream.read((char*)indices, iCount * 2);
	}

	

	
}