#pragma once

#include "DXTCompression.h"
#include "SMBFile.h"




inline std::ostream& operator<<(std::ostream& os, const SMBImageFormat format)
{
	switch (format)
	{
	case SMBImageFormat::SMB_X8L8U8V8:
		os << "X8L8U8V8 format";
		break;
	case SMBImageFormat::SMB_DXT1:
		os << "DXT1";
		break;
	case SMBImageFormat::SMB_DXT3:
		os << "DXT3";
		break;
	case SMBImageFormat::SMB_R8G8B8A8_UNORM:
		os << "R8G8B8A8UNORM";
		break;
	default:
		os << "Unsupported/Unknown texture type";
		break;
	}

	return os;
}


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

	SMBTexture(SMBTexture&& other) noexcept {
		this->data = other.data;
		this->height = other.height;
		this->width = other.width;
		this->miplevels = other.miplevels;
		this->type = other.type;
		this->id = other.id;
		this->cumulativeSize = other.cumulativeSize;
		this->fileOffset = other.fileOffset;
		this->name = other.name;
		other.data = nullptr;
	};

	SMBTexture(SMBTexture& other) {
		this->data = other.data;
		this->height = other.height;
		this->width = other.width;
		this->miplevels = other.miplevels;
		this->type = other.type;
		this->id = other.id;
		this->cumulativeSize = other.cumulativeSize;
		this->name = other.name;
		this->fileOffset = other.fileOffset;
		other.data = nullptr;
	};

	int CreateTextureDetails(SMBFile* smb, const SMBChunk& chunk);

	int ReadTextureData(SMBFile* smb);

	friend std::ostream& operator<<(std::ostream& os, SMBTexture& tex)
	{
		os  << tex.type << "\n"
			<< tex.width << "\n"
			<< tex.height << "\n"
			<< tex.miplevels << "\n"
			<< "------------------" << "\n";
		return os;
	}
};





