#include "SMBTexture.h"
SMBTexture::SMBTexture(SMBImageFormat _type, uint32_t _width, uint32_t _height, uint32_t _mips)
	: type(_type), width(_width), height(_height), miplevels(_mips), cumulativeSize(0),  data(nullptr)
{
	
}

int SMBTexture::CreateTextureDetails(SMBFile& smb, const SMBChunk& chunk)
{
	name = chunk.fileName;
	id = chunk.chunkId;
	OSFileHandle* fileHandle = &smb.fileHandle;

	int offset = chunk.offsetInHeader + 21;

	OSSeekFile(fileHandle, offset, BEGIN);

	OSReadFile(fileHandle, 4 * 4, reinterpret_cast<char*>(&this->type));

	fileOffset = chunk.contigOffset + smb.fileOffset;

	int totalBlobSize = 0;

	size_t size = 0;

	int writeWidth = width;
	int	writeHeight = height;

	for (uint32_t i = 0; i < miplevels; i++)
	{

		switch (type)
		{
		case SMBImageFormat::SMB_DXT1:
			size = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
			break;
		case SMBImageFormat::SMB_DXT3:
			size = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
			break;
		case SMBImageFormat::SMB_B8G8R8A8_UNORM:
		case SMBImageFormat::SMB_R8G8B8A8_UNORM:
			size = writeWidth * writeHeight * 4;
			break;
		default:
			//std::cerr << type << "\n";
			return -1;
		}

		totalBlobSize += size;

		writeWidth >>= 1;
		writeHeight >>= 1;
	}

	cumulativeSize = totalBlobSize;

	return 0;
}

int SMBTexture::ReadTextureData(SMBFile& smb)
{
	char* readHead = data;

	OSFileHandle* fileHandle = &smb.fileHandle;

	OSSeekFile(fileHandle, fileOffset, BEGIN);

	int writeWidth = width;
	int	writeHeight = height;

	for (uint32_t i = 0; i < miplevels; i++)
	{
		size_t size = 0;
		switch (type)
		{
		case SMBImageFormat::SMB_DXT1:
			size = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
			break;
		case SMBImageFormat::SMB_DXT3:
			size = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
			break;
		case SMBImageFormat::SMB_B8G8R8A8_UNORM:
		case SMBImageFormat::SMB_R8G8B8A8_UNORM:
			size = writeWidth * writeHeight * 4;
			break;
		default:
			//std::cerr << type << "\n";
			return -1;
		}

		writeWidth >>= 1;
		writeHeight >>= 1;


		OSReadFile(fileHandle, size, (char*)readHead);

		readHead += size;
	}

	return 0;
}