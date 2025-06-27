#pragma once
#include <cstdint>
#include <vector>

#include "AppTypes.h"
#include "TextureData.h"


class AppTextureImpl
{
public:
	AppTextureImpl(std::vector<char>& _fileData, TextureIOType type);

	ImageFormat type;
	uint32_t dataSize;
	uint32_t width, height, miplevels;
	std::vector<char> data;
private:
	void ParseBMP(std::vector<char>& _fileData);
};

