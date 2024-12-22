#pragma once

#include "SMBFile.h"
#include "SMBTexture.h"
#include "VKTexture.h"
class AppTexture
{
public:

	AppTexture(const SMBFile& smb, const SMBChunk& chunk) : smbTex(new SMBTexture(smb, chunk)), vkImpl(new VKTexture(*smbTex))
	{
		
	}

	~AppTexture()
	{
		if (smbTex) delete smbTex;
		if (vkImpl) delete vkImpl;
	}

	AppTexture(AppTexture&& other) noexcept {
		this->smbTex = other.smbTex;
		this->vkImpl = other.vkImpl;
		other.smbTex = nullptr;
		other.vkImpl = nullptr;
	}

	AppTexture(AppTexture& other) noexcept {
		this->smbTex = other.smbTex;
		this->vkImpl = other.vkImpl;
		other.smbTex = nullptr;
		other.vkImpl = nullptr;
	}

	SMBTexture* smbTex;
	VKTexture* vkImpl;
};

