#include "AppTextureImpl.h"
AppTextureImpl::AppTextureImpl(std::vector<char>& _fileData, TextureIOType type) : width(0), height(0), miplevels(1)
{
	if (type == TextureIOType::BMP)
	{
		ParseBMP(_fileData);
	}
}

void AppTextureImpl::ParseBMP(std::vector<char>& _fileData)
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

	width = ih.biWidth;
	height = ih.biHeight;

	uint32_t bitcount = ih.biBitCount;
	uint32_t bytesPerRow = 0;

	switch (bitcount)
	{
	case 32:
		type = ImageFormat::R8G8B8A8;
		bytesPerRow = 4 * width;
		break;
	default:
		throw std::runtime_error("Unsupported BMP texture type");
	}

	if (ih.biSizeImage)
	{
		dataSize = ih.biSizeImage;
	}
	else
	{
		dataSize = fh.bfSize - fh.bfOffBits;
	}

	unsigned int uLine;
	unsigned int bottomLine = height - 1;
	uint32_t i = 0;

	int offset = 0;

	uLine = bottomLine;


	iter = _fileData.begin() + (fh.bfOffBits + (uLine * bytesPerRow));

	data.resize(dataSize);

	auto copy = data.begin();

	for (; i < bottomLine; i++)
	{
		std::copy(iter, iter + bytesPerRow, copy);
		copy += bytesPerRow;
		iter -= bytesPerRow;
	}

	std::copy(iter, iter + bytesPerRow, copy);

	TexUtils::BGRATexture(data.data(), height, width, (bitcount == 24 ? 3 : 4));

}