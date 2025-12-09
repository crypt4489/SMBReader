#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

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
	case SMBImageFormat::SMB_R8G8B8A8:
		os << "R8G8B8A8";
		break;
	default:
		std::cerr << "Unsupported/Unknown texture type";
		break;
	}

	return os;
}

struct SMBTexture
{
public:
	SMBImageFormat type;
	uint32_t width;
	uint32_t height;
	uint32_t miplevels;
	uint32_t cumulativeSize;
	uint32_t* imageSizes;
	std::byte* data;


	SMBTexture(SMBImageFormat _type, uint32_t _width, uint32_t _height, uint32_t _mips);
	~SMBTexture()
	{
		if (data)
		{
			delete[] data;
		}

		if (imageSizes)
		{
			delete[] imageSizes;
		}
	}

	SMBTexture(SMBTexture&& other) {
		this->data = other.data;
		this->imageSizes = other.imageSizes;
		this->height = other.height;
		this->width = other.width;
		this->miplevels = other.miplevels;
		this->type = other.type;
		other.data = nullptr;
		other.imageSizes = nullptr;
	};

	SMBTexture(SMBTexture& other) {
		this->data = other.data;
		this->imageSizes = other.imageSizes;
		this->height = other.height;
		this->width = other.width;
		this->miplevels = other.miplevels;
		this->type = other.type;
		other.data = nullptr;
		other.imageSizes = nullptr;
	};

	void MipLevelTextureData(uint32_t miplevel, std::vector<char>& _data);

	SMBTexture(const SMBFile& smb, const SMBChunk& chunk);


	friend std::ostream& operator<<(std::ostream& os, const SMBTexture& tex)
	{
		os  << tex.type << "\n"
			<< tex.width << "\n"
			<< tex.height << "\n"
			<< tex.miplevels << "\n"
			<< "------------------" << "\n";
		return os;
	}
};





