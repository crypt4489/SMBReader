#include "SMBTexture.h"
SMBTexture::SMBTexture(SMBImageFormat _type, uint32_t _width, uint32_t _height, uint32_t _mips)
	: type(_type), width(_width), height(_height), miplevels(_mips), cumulativeSize(0), imageSizes(nullptr), data(nullptr)
{
	
}

void SMBTexture::MipLevelTextureData(uint32_t miplevel, std::vector<char>& _data)
{
	int offset = 0;
	for (uint32_t i = 0; i < miplevel; i++)
	{
		offset += imageSizes[i];
	}
	auto ref = data + offset;
	memcpy(ref, _data.data(), _data.size());
}

SMBTexture::SMBTexture(const SMBFile& smb, const SMBChunk& chunk) 
	: type(SMBImageFormat::SMB_IMAGEUNKNOWN), height(0), width(0), miplevels(0), cumulativeSize(0), imageSizes(nullptr), data(nullptr)
{
	name = chunk.fileName.c_str();
	id = chunk.chunkId;
	auto fileHandle = FileManager::GetFile(smb.id);

	int offset = chunk.offsetInHeader + 21;

	OSSeekFile(fileHandle, offset, BEGIN);

	OSReadFile(fileHandle, 4 * 4, reinterpret_cast<char*>(this));

	imageSizes = new uint32_t[miplevels];

	OSSeekFile(fileHandle, chunk.contigOffset + smb.fileOffset, BEGIN);

	int writeWidth = width;
	int	writeHeight = height;

	int size = 0;

	int totalBlobSize = 0;

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
		case SMBImageFormat::SMB_R8G8B8A8_UNORM:
			size = writeWidth * writeHeight * 4;
			break;
		default:
			std::cerr << type << "\n";
			delete[] imageSizes;
			return;
		}

		imageSizes[i] = static_cast<uint32_t>(size);

		totalBlobSize += size;

		writeHeight >>= 1;
		writeWidth >>= 1;
	}

	cumulativeSize = totalBlobSize;

	data = new std::byte[totalBlobSize];

	std::byte* readHead = data;

	for (uint32_t i = 0; i < miplevels; i++)
	{

		OSReadFile(fileHandle, imageSizes[i], (char*)readHead);

		readHead += imageSizes[i];
	}

}
