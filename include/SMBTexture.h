#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

#include "DXTCompression.h"
#include "SMBFile.h"


//either DX8 or DX9 format, mix of both in the SMB archives
enum class SMBImageFormat : uint32_t
{
	X8L8U8V8 = 7,
	DXT1 = 12,
	DXT3 = 14,
	R8G8B8A8 = 18,
	IMAGE_UNKNOWN = 0x7fffffff
};

inline std::ostream& operator<<(std::ostream& os, const SMBImageFormat format)
{
	switch (format)
	{
	case SMBImageFormat::X8L8U8V8:
		os << "X8L8U8V8 format";
		break;
	case SMBImageFormat::DXT1:
		os << "DXT1";
		break;
	case SMBImageFormat::DXT3:
		os << "DXT3";
		break;
	case SMBImageFormat::R8G8B8A8:
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
	SMBImageFormat type;
	uint32_t width;
	uint32_t height;
	uint32_t miplevels;
	std::vector<uint32_t> imageSizes;
	std::vector<std::vector<char>> data;


	SMBTexture(SMBImageFormat _type, uint32_t _width, uint32_t _height, uint32_t _mips) 
		: type(_type), width(_width), height(_height), miplevels(_mips)
	{
		data.resize(_mips);
		imageSizes.resize(_mips);
	}

	void MipLevelTextureData(uint32_t miplevel, std::vector<char>& _data)
	{
		auto ref = data.at(miplevel);
		std::copy(_data.begin(), _data.end(), ref.begin());
	}

	SMBTexture(const SMBFile& smb, const SMBChunk& chunk) : type(SMBImageFormat::IMAGE_UNKNOWN), height(0), width(0), miplevels(0)
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
			case SMBImageFormat::DXT1:
				size = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
				image.resize(size);
				file.read(image.data(), size);
				//DXTCompression::BlockDecompressImageDXT1(writeWidth, writeHeight, reinterpret_cast<unsigned char*>(input.data()), reinterpret_cast<unsigned long*>(image.data()));
				break;
			case SMBImageFormat::DXT3:
				size = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
				image.resize(size);
				file.read(image.data(), size);
				//DXTCompression::BlockDecompressImageDXT3(writeWidth, writeHeight, reinterpret_cast<unsigned char*>(input.data()), reinterpret_cast<unsigned char*>(image.data()));
				break;
			case SMBImageFormat::R8G8B8A8:
				size = writeWidth * writeHeight * 4;
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

namespace TexUtils
{
	inline void BGRATexture(char* image, int height, int width)
	{
		for (int i = 0; i < width * height * 4; i += 4)
		{
			std::swap(image[i], image[i + 2]);
		}
	}

	namespace BMP
	{
#pragma pack(push, 2)
		typedef struct BitmapFileHeader
		{
			uint16_t  bfType;
			uint32_t  bfSize;
			uint16_t  bfReserved1;
			uint16_t  bfReserved2;
			uint32_t  bfOffBits;
		} BitmapFileHeader;
#pragma pack(pop)
		typedef struct BitmapInfoHeader
		{
			uint32_t biSize;
			uint32_t biWidth;
			uint32_t biHeight;
			uint16_t biPlanes;
			uint16_t biBitCount;
			uint32_t biCompression;
			uint32_t biSizeImage;
			uint32_t biXPelsPerMeter;
			uint32_t biYPelsPerMeter;
			uint32_t biClrUsed;
			uint32_t biClrImportant;
		} BitmapInfoHeader;


		static_assert(sizeof(BitmapFileHeader) == 14);
		static_assert(sizeof(BitmapInfoHeader) == 40);

		inline void WriteOutBMPHeaders(std::fstream &stream, uint32_t width, uint32_t height)
		{
			BitmapFileHeader fileheader{ 0x4d42, sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + (width * height * 4), 0, 0, 0x36 };
			BitmapInfoHeader infoheader{ sizeof(BitmapInfoHeader), width, height, 1, 32, 0, 0, 0, 0, 0, 0 };

			stream.write(reinterpret_cast<char*>(&fileheader.bfType), sizeof(BitmapFileHeader));
			stream.write(reinterpret_cast<char*>(&infoheader.biSize), sizeof(BitmapInfoHeader));
		}
	}
}



