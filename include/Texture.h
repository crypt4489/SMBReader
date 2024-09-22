#pragma once
#include <cstdint>
#include <fstream>
class Texture
{
public:
	uint32_t type;
	uint32_t width;
	uint32_t height;
	uint32_t miplevels;
	friend std::ostream& operator<<(std::ostream& os, const Texture& tex)
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

		inline void WriteOutBMPHeaders(std::shared_ptr<std::fstream> stream, uint32_t width, uint32_t height)
		{
			BitmapFileHeader fileheader{ 0x4d42, 14 + 40 + (width * height * 4), 0, 0, 0x36 };
			BitmapInfoHeader infoheader{ 40, width, height, 1, 32, 0, 0, 0, 0, 0, 0 };

			stream->write(reinterpret_cast<char*>(&fileheader.bfType), sizeof(BitmapFileHeader));
			stream->write(reinterpret_cast<char*>(&infoheader.biSize), sizeof(BitmapInfoHeader));
		}
	}
}



