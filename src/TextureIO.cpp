#include "TextureIO.h"
void ParseBMP(std::vector<char>& _fileData, TextureDetails* details)
{
	auto iter = _fileData.begin();
	TexUtils::BMP::BitmapFileHeader fh;
	std::copy(iter, iter + sizeof(TexUtils::BMP::BitmapFileHeader), reinterpret_cast<char*>(&fh));
	if (fh.bfType != 0x4D42)
	{
		throw std::runtime_error("not a valid bitmap file");
	}

	iter += sizeof(TexUtils::BMP::BitmapFileHeader);

	TexUtils::BMP::BitmapInfoHeader ih;

	std::copy(iter, iter + sizeof(TexUtils::BMP::BitmapInfoHeader), reinterpret_cast<char*>(&ih));

	uint32_t width = details->width = ih.biWidth;
	uint32_t height = details->height = ih.biHeight;

	uint32_t bitcount = ih.biBitCount;
	uint32_t bytesPerRow = 0;

	switch (bitcount)
	{
	case 32:
		details->type = ImageFormat::B8G8R8A8;
		bytesPerRow = 4 * width;
		break;
	default:
		throw std::runtime_error("Unsupported BMP texture type");
	}

	if (ih.biSizeImage)
	{
		details->dataSize = ih.biSizeImage;
	}
	else
	{
		details->dataSize = fh.bfSize - fh.bfOffBits;
	}

	unsigned int uLine;
	unsigned int bottomLine = height - 1;
	uint32_t i = 0;

	int offset = 0;

	uLine = bottomLine;


	auto iter2 = _fileData.data() + (fh.bfOffBits + (uLine * bytesPerRow));

	details->data = new char[details->dataSize];

	auto copy = details->data;

	for (; i < bottomLine; i++)
	{
		memcpy(copy, iter2, bytesPerRow);
		copy += bytesPerRow;
		iter2 -= bytesPerRow;
	}

	memcpy(copy, iter2, bytesPerRow);

	//TexUtils::BGRATexture((char*)details->data, height, width, (bitcount == 24 ? 3 : 4));
	details->miplevels = 1;
}