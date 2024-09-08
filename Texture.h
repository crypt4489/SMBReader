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
};

namespace TexUtils
{
	inline void BGRATexture(char* image, int height, int width)
	{
		for (int i = 0; i < width * height * 4; i += 4)
		{
			int temp3 = image[i + 2];
			image[i + 2] = image[i];
			image[i] = temp3;
		}
	}

	namespace BMP
	{
#pragma pack(2)
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
			int	biWidth;
			int	biHeight;
			uint16_t biPlanes;
			uint16_t biBitCount;
			uint32_t biCompression;
			uint32_t biSizeImage;
			int	biXPelsPerMeter;
			int	biYPelsPerMeter;
			uint32_t biClrUsed;
			uint32_t biClrImportant;
		} BitmapInfoHeader;
#pragma pack(pop)

		inline void WriteOutBMPHeaders(std::shared_ptr<std::fstream> stream, int width, int height)
		{
			BitmapFileHeader fileheader{ 0x4d42, 14 + 40 + (width * height * 4), 0, 0, 0x36 };
			BitmapInfoHeader infoheader{ 40, width, height, 1, 32, 0, 0, 0, 0, 0, 0 };

			stream->write(reinterpret_cast<char*>(&fileheader.bfType), 14);
			stream->write(reinterpret_cast<char*>(&infoheader.biSize), 40);
		}
	}
}



