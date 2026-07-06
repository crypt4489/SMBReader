#pragma once

#include "imageutils/DXTCompression.h"
#include "SMBFile.h"

struct SMBTexture
{
public:
	size_t fileOffset;
	StringView name;
	char* data;
	SMBImageFormat type;
	uint32_t width;
	uint32_t height;
	uint32_t miplevels;
	uint32_t cumulativeSize;
	uint32_t id;
	
	SMBTexture() = default;

	SMBTexture(SMBImageFormat _type, uint32_t _width, uint32_t _height, uint32_t _mips);
	~SMBTexture()
	{
	
	}

	int CreateTextureDetails(SMBFile* smb, const SMBChunk& chunk);

	int ReadTextureData(SMBFile* smb);
};





