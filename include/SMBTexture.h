#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

#include "DXTCompression.h"
#include "SMB.h"

class SMBTexture
{
public:
	uint32_t type;
	uint32_t width;
	uint32_t height;
	uint32_t miplevels;
	std::vector<uint32_t> imageSizes;
	std::vector<std::vector<char>> data;


	SMBTexture(uint32_t _type, uint32_t _width, uint32_t _height, uint32_t _mips) 
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

	SMBTexture(const SMBFile& smb, const SMBChunk& chunk)
	{
		auto& file = *smb.fileHandle.second;
		std::streamoff offset = chunk.offsetInHeader + 21LL;
		file.seekg(offset, std::ios_base::beg);

		file.read(reinterpret_cast<char*>(this), 4 * 4);

		data.resize(miplevels);
		imageSizes.resize(miplevels);

		file.seekg(chunk.contigOffset + smb.fileOffset, std::ios_base::beg);

		int writeWidth = width;
		int	writeHeight = height;

		size_t compressedsize = 0;
		
		std::vector<char> input;

		for (uint32_t i = 0; i < miplevels; i++)
		{
			std::vector<char> &image = data[i];
			image.resize(writeWidth * writeHeight * 4);
			imageSizes[i] = writeWidth * writeHeight * 4;
			switch (type)
			{
			case 7:
				std::cerr << "X8L8U8V8 format is not exportable\n";
				return;
			case 12:
				compressedsize = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
				input.resize(compressedsize);
				file.read(input.data(), compressedsize);
				DXTCompression::BlockDecompressImageDXT1(writeWidth, writeHeight, reinterpret_cast<unsigned char*>(input.data()), reinterpret_cast<unsigned long*>(image.data()));
				break;
			case 14:
				compressedsize = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
				input.resize(compressedsize);
				file.read(input.data(), compressedsize);
				DXTCompression::BlockDecompressImageDXT3(writeWidth, writeHeight, reinterpret_cast<unsigned char*>(input.data()), reinterpret_cast<unsigned char*>(image.data()));
				break;
			case 18:
				file.read(image.data(), writeWidth * writeHeight * 4);
				break;
			default:
				std::cerr << "Unsupported/Unknown texture type " << type << "\n";
				return;
			}

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



