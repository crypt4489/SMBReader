#include "Exporter.h"

#include <cstdint>
#include <format>
#include <fstream>

#include "AppTextureImpl.h"
#include "FileManager.h"
#include "DXTCompression.h"
#include "SMBTexture.h"


//IndexedRenderable or Renderable, not sure

//0x24 is primitive type (internal)

//0x28 is indicies count



//VPTR TABLE
//3-4 4) 3 float 5) packed
//5-8 5-6) 3float 7-8) packed Setter Getter for Position -pospack6 6 bytes, pospack4 4 bytes, regular position is 4 bytes per componenet, 3 in total
//9-10-11 Normals - 4 bytes compressed-same format as position for compressed 4
// uncompressed normals are 4 bytes per component(3, 12 in total)
//12-13-14 Diffuse -- diffuse is 4 bytes, 3 bytes for RGB only
//15-17 Textures - Tex1 uncompressed 4 bytes each coord, 2 bytes compressed (8 total, 4 total)
// Tex2 8 uncompressed 4 bytes compressed (16 total, 8 total)
// Tex3 16 uncompressed 8 bytes compressed per cord (32 total, 16 total)
// Compressed textures are stored in 4:12

//18-19 Bone and weight indicator
//20-24 Bone and Weights -Bone2 is 4 bytes
//25-27 Binormals
//28-32 sprites

std::vector<std::vector<glm::vec3>> positions;
std::vector<std::vector<glm::vec3>> normals;
std::vector<std::vector<glm::vec2>> texCoords;
std::vector<std::vector<glm::vec2>> texCoordsP2;
std::vector<std::vector<glm::ivec2>> bonesID;
std::vector<std::vector<glm::vec2>> weights;

std::vector<std::vector<uint16_t>> indices;;





glm::vec2 converttexcoords16(int16_t* huh)
{
	float x = huh[0] * dx * 16.0f;
	float y = huh[1] * dx * 16.0f;


	return glm::vec2(x, y);
}


float top2[6];



glm::vec3 pack6decomp(int16_t* hello, float *top)
{
	float x = ((hello[0] * dx) + 1.0) * 0.5;
	float y = (((hello[1] * dx)) + 1.0) * 0.5;
	float z = (((hello[2] * dx)) + 1.0) * 0.5;

	x = ((top[3] - top[0]) * x) + top[0];
	y = ((top[4] - top[1]) * y) + top[1];
	z = ((top[5] - top[2]) * z) + top[2];

	return glm::vec3(x, y, z);
}


void Exporter::ExportChunksFromFile(SMBFile& smb)
{

	auto& chunk = smb.chunks;
	for (size_t j = 0; j<smb.chunks.size(); j++)
	{
		
			
			
		switch (smb.chunks[j].chunkType)
		{
		case GEO:
		{

			

			FileHandle* handle = FileManager::GetFile(smb.id);

			auto& geoChunk = handle->streamHandle;

			size_t seekpos = chunk[j].offsetInHeader;

			geoChunk.seekg(seekpos);

			std::vector<char> data2(chunk[j].headerSize);

			geoChunk.read(data2.data(), chunk[j].headerSize);

			SMBGeoChunk* geoDef = ProcessGeometryClass(data2.data());

			geoDef->vertexAndIndicesInfo = chunk[j].contigOffset + smb.fileOffset;

			seekpos = chunk[j + 1].contigOffset + smb.fileOffset;

			geoDef->verticesandIndexCompressedSize = seekpos - geoDef->vertexAndIndicesInfo;


			break;
		}
		
		case TEXTURE:
			ExportTextureFromFile(smb, chunk[j]);
			break;
		case GR2:
			break;
		case Joints:
			break;
		default:
			std::cerr << "Unprocessed chunkType\n";
			break;
		}
	}
}


void Exporter::ExportTextureFromFile(const SMBFile& smb, const SMBChunk& chunk)
{
	std::string name = FileManager::ExtractFileNameFromPath(chunk.fileName);

	auto pathToTextures = FileManager::SetupDirectory(name);

	SMBTexture tex(smb, chunk);

	auto ptr = tex.data;

	for (uint32_t i = 0; i < tex.miplevels; i++)
	{
		std::string writeFileName = name + std::to_string(i + 1) + ".bmp";

		auto writePath = pathToTextures / writeFileName;

		uint32_t writeWidth = tex.width >> i;
		uint32_t writeHeight = tex.height >> i;

		FileHandle* handle;

		auto outputfilehandle = FileManager::OpenFile(writePath, std::ios::binary | std::ios::out, handle);

		if (!outputfilehandle)
		{
			std::cerr << "Something bad happened\n";
			return;
		}

		auto& outputFile = handle->streamHandle;

		TexUtils::BMP::WriteOutBMPHeaders(outputFile, writeWidth, writeHeight);

		std::byte* bgra = ptr;

		std::vector<char> input;

		int compressedSize = 0;
		input.resize(writeWidth * writeHeight * 4);;
		switch (tex.type)
		{
		case X8L8U8V8:
			std::cerr << "X8L8U8V8 format is not exportable\n";
			return;
		case DXT1:
			//compressedSize = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
			
			DXTCompression::BlockDecompressImageDXT1(writeWidth, writeHeight, (unsigned char*)ptr, (unsigned long*)input.data());
			break;
		case DXT3:
			//compressedSize = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
			DXTCompression::BlockDecompressImageDXT3(writeWidth, writeHeight, (unsigned char*)ptr, (unsigned char*)input.data());
			break;
		case R8G8B8A8:
			std::memcpy(input.data(), ptr, writeWidth * writeHeight * 4);
			break;
		default:
			std::cerr << "Unsupported/Unknown texture type " << tex.type << "\n";
			return;
		}

		TexUtils::BGRATexture(input.data(), tex.height >> i, tex.width >> i, 4);


		uint32_t bpr = writeWidth * 4;

		uint32_t offset = (writeHeight - 1) * bpr;

		for (uint32_t i = 0; i < writeHeight; i++)
		{
			outputFile.write(input.data() + offset, bpr);
			offset -= bpr;
		}

		ptr += tex.imageSizes[i];

		FileManager::RemoveOpenFile(outputfilehandle.value());
	}
}

void Exporter::ExportToOBJFormat(std::vector<Vertex>& vertices, std::string& outputFile)
{
	using ExportHelper::operator<<;

	FileHandle* handle;

	auto fileRet = FileManager::OpenFile(outputFile, std::ios_base::out, handle);

	if (!fileRet)
	{
		throw std::runtime_error("Cannot open file for OBJ export " + outputFile);
	}


	auto& fileStream = handle->streamHandle;

	for (const auto& vert : vertices) {
		fileStream << "v " << vert.POSITION;
	}

	for (const auto& vert : vertices) {
		fileStream << "vt " << vert.TEXTURE;
	}

	for (const auto& vert : vertices) {
		fileStream << "vn " << vert.NORMAL;
	}

	FileManager::RemoveOpenFile(fileRet.value());
}