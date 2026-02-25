#pragma once
#include "AppTypes.h"
#include "OSFile.h"
#include <vector>

#include <cstdint>
#include <fstream>
#include <utility>
namespace TexUtils
{
	inline void BGRATexture(char* image, int height, int width, int stride)
	{
		for (int i = 0; i < width * height * stride; i += stride)
		{
			std::swap(image[i], image[i + 2]);
		}
	}

	namespace BMP
	{
#pragma pack(push, 1)
		typedef struct BitmapFileHeader
		{
			uint16_t  bfType;
			uint32_t  bfSize;
			uint16_t  bfReserved1;
			uint16_t  bfReserved2;
			uint32_t  bfOffBits;
		} BitmapFileHeader;

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
#pragma pack(pop)

		static_assert(sizeof(BitmapFileHeader) == 14);
		static_assert(sizeof(BitmapInfoHeader) == 40);

		inline void WriteOutBMPHeaders(OSFileHandle* handle, uint32_t width, uint32_t height)
		{
			BitmapFileHeader fileheader{ 0x4d42, sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + (width * height * 4), 0, 0, 0x36 };
			BitmapInfoHeader infoheader{ sizeof(BitmapInfoHeader), width, height, 1, 32, 0, 0, 0, 0, 0, 0 };

			OSWriteFile(handle, sizeof(BitmapFileHeader), reinterpret_cast<char*>(&fileheader.bfType));
			

			OSWriteFile(handle, sizeof(BitmapInfoHeader), reinterpret_cast<char*>(&infoheader.biSize));
	
		}
	}
}

struct TextureDetails
{
	ImageFormat type;
	uint32_t dataSize;
	uint32_t width, height, miplevels;
	char* data;
	char* currPointer;
	uint32_t arrayLayers;
	uint32_t bitcount;
};


void ReadBMPData(char* _fileData, int dataPointer, TextureDetails* details);

int ReadBMPDetails(char* _fileData, TextureDetails* details);
