#include "imageutils/TextureIO.h"
#include <string.h>
void ReadBMPData(char* _fileData, int dataPointer, TextureDetails* details)
{
	int uLine;
	int bottomLine = details->height - 1;
	int bytesPerRow = (details->bitcount>>3) * details->width;
	int i = 0;

	uLine = bottomLine;

	auto fileDataIter = _fileData + (dataPointer + (uLine * bytesPerRow));

	auto textureMemoryCache = details->currPointer;

	for (; i < bottomLine; i++)
	{
		memcpy(textureMemoryCache, fileDataIter, bytesPerRow);
		textureMemoryCache += bytesPerRow;
		fileDataIter -= bytesPerRow;
	}

	memcpy(textureMemoryCache, fileDataIter, bytesPerRow);
}

int ReadBMPDetails(char* _fileData, TextureDetails* details)
{
	char* iter = _fileData;

	TexUtils::BMP::BitmapFileHeader fh;

	memcpy(reinterpret_cast<char*>(&fh), iter, sizeof(TexUtils::BMP::BitmapFileHeader));

	if (fh.bfType != 0x4D42)
	{
		return -1;
	}

	iter += sizeof(TexUtils::BMP::BitmapFileHeader);

	TexUtils::BMP::BitmapInfoHeader ih;

	memcpy(reinterpret_cast<char*>(&ih), iter, sizeof(TexUtils::BMP::BitmapInfoHeader));

	uint32_t width = details->width = ih.biWidth;
	uint32_t height = details->height = ih.biHeight;
	uint32_t bitcount = details->bitcount = ih.biBitCount;

	switch (bitcount)
	{
	case 32:
		details->type = ImageFormat::B8G8R8A8;
		break;
	default:
		return -1;
	}

	if (ih.biSizeImage)
	{
		details->dataSize = ih.biSizeImage;
	}
	else
	{
		details->dataSize = fh.bfSize - fh.bfOffBits;
	}

	details->miplevels = 1;

	return fh.bfOffBits;
}