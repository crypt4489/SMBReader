#pragma once

#include "AppTextureImpl.h"
#include "IndexTypes.h"
#include "SMBFile.h"
#include "SMBTexture.h"


class AppTexture
{
public:

	AppTexture(const SMBFile& smb, const SMBChunk& chunk);

	AppTexture(std::vector<char>& data, TextureIOType type);

	~AppTexture();

	AppTexture(AppTexture&& other) noexcept;

	AppTexture(AppTexture& other) noexcept;

	//AppTextureImpl* texImpl;
	//SMBTexture* smbTex;
	//EntryHandle vkImpl;
};

