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
		delete smbTex;
		if (vkImpl) delete vkImpl;
	}

	SMBTexture* smbTex;
	VKTexture* vkImpl;
};

