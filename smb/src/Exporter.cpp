#include "Exporter.h"

#include <iostream>
#include "imageutils/TextureIO.h"
#include "FileManager.h"
#include "imageutils/DXTCompression.h"
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




void ExportChunksFromFile(SMBFile* smb, Allocator* inputScratchMemory)
{
	auto& chunk = smb->chunks;
	for (size_t j = 0; j<smb->numResources; j++)
	{
		switch (smb->chunks[j].chunkType)
		{
		case GEO:
			break;	
		case TEXTURE:
			ExportTextureFromFile(smb, chunk[j], inputScratchMemory);
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

void ExportTextureFromFile(SMBFile* smb, SMBChunk& chunk, Allocator* inputScratchMemory)
{
	const int MAX_SMB_ARCHIVE_OBJECT_NAME = 55;

	StringView imageName{};

	char* strBuf = (char*)inputScratchMemory->Allocate(MAX_SMB_ARCHIVE_OBJECT_NAME);

	FileManager::ExtractFileNameFromPath(&chunk.fileName, &imageName, strBuf);

	auto pathToTextures = FileManager::SetupDirectory(&imageName);

	SMBTexture tex;
	
	tex.CreateTextureDetails(smb, chunk);

	tex.data = (char*)inputScratchMemory->Allocate(tex.cumulativeSize);

	tex.ReadTextureData(smb);

	auto ptr = tex.data;

	size_t individualSize = 0;

	for (uint32_t i = 0; i < tex.miplevels; i++)
	{
		std::string writeFileName = std::string(imageName.stringData, imageName.charCount) + std::to_string(i + 1) + ".bmp";

		auto writePath = pathToTextures / writeFileName;

		uint32_t writeWidth = tex.width >> i;
		uint32_t writeHeight = tex.height >> i;

		OSFileHandle handle;

		OSOpenFile(writePath.string().c_str(), writePath.string().size(), CREATE_IF_NOT_EXIST | WRITE, &handle);

		TexUtils::BMP::WriteOutBMPHeaders(&handle, writeWidth, writeHeight);

		char* bgra = ptr;

		char* input = (char*)inputScratchMemory->Allocate(writeWidth * writeHeight * 4);

		int compressedSize = 0;
	
		switch (tex.type)
		{
		case SMBImageFormat::SMB_X8L8U8V8:
			std::cerr << "X8L8U8V8 format is not exportable\n";
			return;
		case SMBImageFormat::SMB_DXT1:
			individualSize = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
			DXTCompression::BlockDecompressImageDXT1(writeWidth, writeHeight, (unsigned char*)ptr, (unsigned long*)input);
			TexUtils::BGRATexture(input, tex.height >> i, tex.width >> i, 4);
			break;
		case SMBImageFormat::SMB_DXT3:
			individualSize = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
			DXTCompression::BlockDecompressImageDXT3(writeWidth, writeHeight, (unsigned char*)ptr, (unsigned char*)input);
			TexUtils::BGRATexture(input, tex.height >> i, tex.width >> i, 4);
			break;
		case SMBImageFormat::SMB_B8G8R8A8_UNORM:
			individualSize = writeWidth * writeHeight * 4;
			std::memcpy(input, ptr, individualSize);
			break;
		default:
			std::cerr << "Unsupported/Unknown texture type " << tex.type << "\n";
			return;
		}

		uint32_t bpr = writeWidth * 4;

		uint32_t offset = (writeHeight - 1) * bpr;

		for (uint32_t i = 0; i < writeHeight; i++)
		{
			OSWriteFile(&handle, bpr, input + offset);
			offset -= bpr;
		}

		ptr += individualSize;

		OSCloseFile(&handle);
	}
}

void ExportToOBJFormat(void* vertices, int vertexCount, StringView outputFile)
{
	/*
	using ExportHelper::operator<<;

	//FileHandle* handle;

	auto fileRet = FileManager::OpenFile(outputFile, std::ios_base::out, handle);

	if (!fileRet)
	{
		throw std::runtime_error("Cannot open file for OBJ export " + outputFile);
	}

	/*
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
	*/
}