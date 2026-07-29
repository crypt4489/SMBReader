#include "SMBExporter.h"

#include "imageutils/DXTCompression.h"
#include "imageutils/TextureIO.h"

#include "SMBTexture.h"

#include <string.h>
#include <stdio.h>

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

	char* strBuf = (char*)inputScratchMemory->Allocate(MAX_SMB_ARCHIVE_OBJECT_NAME);

	int fileNameLength = OSExtractFileName(chunk.fileName.stringData, chunk.fileName.charCount, strBuf);

	strBuf[fileNameLength] = '\0';

	int currentDirectorySize = OSGetCurrentDirectorySize() + 2;

	char* rootDirectory = (char*)inputScratchMemory->Allocate(currentDirectorySize);

	OSGetCurrentDirectory(currentDirectorySize, rootDirectory);

	int filePathScratchSize = currentDirectorySize + (fileNameLength*2) + 10;

	char* filePathScratch = (char*)inputScratchMemory->Allocate(filePathScratchSize);

	int filePathSlash = OSGetSystemFileTerminator();

	int filePathTotalSize = snprintf(filePathScratch, filePathScratchSize, "%s%c%s%c", rootDirectory, filePathSlash, strBuf, filePathSlash);

	OSCreateDirectory(filePathScratch, filePathTotalSize, PUBLIC_DIR);

	SMBTexture tex;
	
	tex.CreateTextureDetails(smb, chunk);

	tex.data = (char*)inputScratchMemory->Allocate(tex.cumulativeSize);

	tex.ReadTextureData(smb);

	auto ptr = tex.data;

	size_t individualSize = 0;

	for (uint32_t i = 0; i < tex.miplevels; i++)
	{
		filePathTotalSize = snprintf(filePathScratch, filePathScratchSize, "%s%c%s%c%s%d%s", rootDirectory, filePathSlash, strBuf, filePathSlash, strBuf, i + 1, ".bmp");

		uint32_t writeWidth = tex.width >> i;
		uint32_t writeHeight = tex.height >> i;

		OSFileHandle handle;

		OSOpenFile(filePathScratch, filePathTotalSize, CREATE_IF_NOT_EXIST | WRITE, &handle);

		TexUtils::BMP::BitmapFileHeader fileheader{};
		TexUtils::BMP::BitmapInfoHeader infoheader{};

		TexUtils::BMP::WriteOutBMPHeaders(&fileheader, &infoheader, writeWidth, writeHeight);

		OSWriteFile(&handle, sizeof(TexUtils::BMP::BitmapFileHeader), reinterpret_cast<char*>(&fileheader.bfType));

		OSWriteFile(&handle, sizeof(TexUtils::BMP::BitmapInfoHeader), reinterpret_cast<char*>(&infoheader.biSize));

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
			OSWriteFile(&handle, bpr, input + offset);
			offset -= bpr;
		}

		ptr += individualSize;

		OSCloseFile(&handle);
	}
}
