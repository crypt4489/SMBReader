#include "SMBTexture.h"
SMBTexture::SMBTexture(SMBImageFormat _type, uint32_t _width, uint32_t _height, uint32_t _mips)
	: type(_type), width(_width), height(_height), miplevels(_mips), cumulativeSize(0),  data(nullptr)
{
	
}

SMBTexture::SMBTexture(const SMBFile& smb, const SMBChunk& chunk) 
	: type(SMBImageFormat::SMB_IMAGEUNKNOWN), height(0), width(0), miplevels(0), cumulativeSize(0), data(nullptr)
{
	name = chunk.fileName;
	id = chunk.chunkId;
	auto fileHandle = FileManager::GetFile(smb.id);

	int offset = chunk.offsetInHeader + 21;

	OSSeekFile(fileHandle, offset, BEGIN);

	OSReadFile(fileHandle, 4 * 4, reinterpret_cast<char*>(this));

	fileOffset = chunk.contigOffset + smb.fileOffset;

	int totalBlobSize = 0;

	size_t size = 0;

	int writeWidth = width;
	int	writeHeight = height;

	for (uint32_t i = 0; i < miplevels; i++)
	{

		switch (type)
		{
			//	std::cerr << "X8L8U8V8 format is not exportable\n";
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
			std::cerr << type << "\n";

			return;
		}

		imageSizes[i] = static_cast<uint32_t>(size);

		totalBlobSize += size;

		writeWidth >>= 1;
		writeHeight >>= 1;
	}

	cumulativeSize = totalBlobSize;
}

void SMBTexture::ReadTextureData(const SMBFile& smb)
{
	char* readHead = data;

	auto fileHandle = FileManager::GetFile(smb.id);

	OSSeekFile(fileHandle, fileOffset, BEGIN);

	for (uint32_t i = 0; i < miplevels; i++)
	{
		OSReadFile(fileHandle, imageSizes[i], (char*)readHead);

		readHead += imageSizes[i];
	}
}