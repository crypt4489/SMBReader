#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

#include "AppTextureImpl.h"
#include "DXTCompression.h"
#include "SMBFile.h"




inline std::ostream& operator<<(std::ostream& os, const ImageFormat format)
{
	switch (format)
	{
	case ImageFormat::X8L8U8V8:
		os << "X8L8U8V8 format";
		break;
	case ImageFormat::DXT1:
		os << "DXT1";
		break;
	case ImageFormat::DXT3:
		os << "DXT3";
		break;
	case ImageFormat::R8G8B8A8:
		os << "R8G8B8A8";
		break;
	default:
		std::cerr << "Unsupported/Unknown texture type";
		break;
	}

	return os;
}

class SMBTexture
{
public:
	ImageFormat type;
	uint32_t width;
	uint32_t height;
	uint32_t miplevels;
	std::vector<uint32_t> imageSizes;
	std::vector<std::vector<char>> data;


	SMBTexture(ImageFormat _type, uint32_t _width, uint32_t _height, uint32_t _mips) 
		: type(_type), width(_width), height(_height), miplevels(_mips)
	{
		data.resize(_mips);
		imageSizes.resize(_mips);
	}

	SMBTexture(SMBTexture&& other) = default;
	SMBTexture(SMBTexture& other) = default;

	void MipLevelTextureData(uint32_t miplevel, std::vector<char>& _data)
	{
		auto ref = data.at(miplevel);
		std::copy(_data.begin(), _data.end(), ref.begin());
	}

	SMBTexture(const SMBFile& smb, const SMBChunk& chunk) : type(ImageFormat::IMAGE_UNKNOWN), height(0), width(0), miplevels(0)
	{
		auto fileHandle = FileManager::GetFile(smb.id);

		auto &file = fileHandle->streamHandle;
		std::streamoff offset = chunk.offsetInHeader + 21LL;
		file.seekg(offset, std::ios_base::beg);

		file.read(reinterpret_cast<char*>(this), 4 * 4);

		data.resize(miplevels);
		imageSizes.resize(miplevels);

		file.seekg(chunk.contigOffset + smb.fileOffset, std::ios_base::beg);

		int writeWidth = width;
		int	writeHeight = height;

		size_t size = 0;

		for (uint32_t i = 0; i < miplevels; i++)
		{
			std::vector<char> &image = data[i];
			
			switch (type)
			{
			//case SMBImageFormat::X8L8U8V8:
			//	std::cerr << "X8L8U8V8 format is not exportable\n";
			//	return;
			case ImageFormat::DXT1:
				size = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
				image.resize(size);
				file.read(image.data(), size);
				//DXTCompression::BlockDecompressImageDXT1(writeWidth, writeHeight, reinterpret_cast<unsigned char*>(input.data()), reinterpret_cast<unsigned long*>(image.data()));
				break;
			case ImageFormat::DXT3:
				size = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
				image.resize(size);
				file.read(image.data(), size);
				//DXTCompression::BlockDecompressImageDXT3(writeWidth, writeHeight, reinterpret_cast<unsigned char*>(input.data()), reinterpret_cast<unsigned char*>(image.data()));
				break;
			case ImageFormat::R8G8B8A8:
				size = writeWidth * writeHeight * 4;
				image.resize(size);
				file.read(image.data(), size);
				break;
			default:
				std::cerr << type << "\n";
				data.clear();
				imageSizes.clear();
				return;
			}

			imageSizes[i] = static_cast<uint32_t>(size);

			writeHeight >>= 1;
			writeWidth >>= 1;			
		}
	}


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





