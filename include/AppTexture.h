#pragma once

#include "SMB.h"
#include "SMBTexture.h"
#include "TextureVKImpl.h"
class AppTexture
{
public:

	AppTexture(const SMBFile& smb, const SMBChunk& chunk) : smbTex(new SMBTexture(smb, chunk)), vkImpl(new TextureVKImpl(*smbTex))
	{
		
	}

	~AppTexture()
	{
		delete smbTex;
		delete vkImpl;
	}

	SMBTexture* smbTex;
	TextureVKImpl* vkImpl;
};

