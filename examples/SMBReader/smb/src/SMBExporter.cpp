#include "SMBExporter.h"

#include "imageutils/TextureIO.h"
#include "FileManager.h"
#include "imageutils/DXTCompression.h"
#include "SMBTexture.h"

#include <string.h>

void ExportChunksFromFile(SMBFile* smb, Allocator* inputScratchMemory)
{
	auto& chunk = smb->chunks;
	for (uint32_t j = 0; j<smb->numResources; j++)
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
		uint64_t writeOutSize;

		std::string writeFileName = std::string(imageName.stringData, imageName.charCount) + std::to_string(i + 1) + ".bmp";

		auto writePath = pathToTextures / writeFileName;

		uint32_t writeWidth = tex.width >> i;
		uint32_t writeHeight = tex.height >> i;

		OSFileHandle handle;

		OSOpenFile(writePath.string().c_str(), writePath.string().size(), CREATE_IF_NOT_EXIST | WRITE, &handle);

		TexUtils::BMP::BitmapFileHeader fileheader{};
		TexUtils::BMP::BitmapInfoHeader infoheader{};

		TexUtils::BMP::WriteOutBMPHeaders(&fileheader, &infoheader, writeWidth, writeHeight);

		OSWriteFile(&handle, sizeof(TexUtils::BMP::BitmapFileHeader), reinterpret_cast<char*>(&fileheader.bfType), &writeOutSize);

		OSWriteFile(&handle, sizeof(TexUtils::BMP::BitmapInfoHeader), reinterpret_cast<char*>(&infoheader.biSize), &writeOutSize);

		char* bgra = ptr;

		char* input = (char*)inputScratchMemory->Allocate(writeWidth * writeHeight * 4);

		int compressedSize = 0;
	
		switch (tex.type)
		{
		case SMBImageFormat::SMB_X8L8U8V8:
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
			memcpy(input, ptr, individualSize);
			break;
		default:
			OSCloseFile(&handle);
			return;
		}

		uint32_t bpr = writeWidth * 4;

		uint32_t offset = (writeHeight - 1) * bpr;

		for (uint32_t i = 0; i < writeHeight; i++)
		{
			OSWriteFile(&handle, bpr, input + offset, &writeOutSize);
			offset -= bpr;
		}

		ptr += individualSize;

		OSCloseFile(&handle);
	}
}
