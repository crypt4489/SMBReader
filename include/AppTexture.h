#pragma once

#include "AppTextureImpl.h"
#include "SMBFile.h"
#include "SMBTexture.h"
#include "VKTexture.h"


class AppTexture
{
public:

	AppTexture(const SMBFile& smb, const SMBChunk& chunk) : 
		smbTex(new SMBTexture(smb, chunk)), vkImpl(new VKTexture(*smbTex)), texImpl(nullptr)
	{
		
	}

	AppTexture(std::vector<char>&data, TextureIOType type) : smbTex(nullptr), texImpl(new AppTextureImpl(data, type)), vkImpl(new VKTexture(*texImpl))
	{}

	~AppTexture()
	{
		if (smbTex) delete smbTex;
		if (vkImpl) delete vkImpl;
		if (texImpl) delete texImpl;
	}

	AppTexture(AppTexture&& other) noexcept {
		this->smbTex = other.smbTex;
		this->vkImpl = other.vkImpl;
		this->texImpl = other.texImpl;
		other.smbTex = nullptr;
		other.vkImpl = nullptr;
		other.texImpl = nullptr;
	}

	AppTexture(AppTexture& other) noexcept {
		this->smbTex = other.smbTex;
		this->vkImpl = other.vkImpl;
		this->texImpl = other.texImpl;
		other.smbTex = nullptr;
		other.vkImpl = nullptr;
		other.texImpl = nullptr;
	}

	AppTextureImpl* texImpl;
	SMBTexture* smbTex;
	VKTexture* vkImpl;
};

