#pragma once
#include "CommonRenderTypes.h"

namespace TexUtils
{
	inline void BGRATexture(char* image, int height, int width, int stride)
	{
		for (int i = 0; i < width * height * stride; i += stride)
		{
			char temp = image[i];
			image[i] = image[i + 2];
			image[i+2] = temp;
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

		inline void WriteOutBMPHeaders(BitmapFileHeader *fileheader, BitmapInfoHeader *infoheader, uint32_t width, uint32_t height)
		{
			*fileheader = { 0x4d42, (40 + 14 + (width * height * 4)), 0, 0, 0x36 };
			*infoheader = { sizeof(BitmapInfoHeader), width, height, 1, 32, 0, 0, 0, 0, 0, 0 };
		}
	}
}

enum TextureIOType
{
	BMP = 0,
};

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
