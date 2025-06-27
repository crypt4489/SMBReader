#include "AppTexture.h"
AppTexture::AppTexture(const SMBFile& smb, const SMBChunk& chunk) :
	smbTex(new SMBTexture(smb, chunk)), vkImpl(new VKTexture(*smbTex)), texImpl(nullptr)
{

}

AppTexture::AppTexture(std::vector<char>& data, TextureIOType type) : smbTex(nullptr), texImpl(new AppTextureImpl(data, type)), vkImpl(new VKTexture(*texImpl))
{
}

AppTexture::~AppTexture()
{
	if (smbTex) delete smbTex;
	if (vkImpl) delete vkImpl;
	if (texImpl) delete texImpl;
}

AppTexture::AppTexture(AppTexture&& other) noexcept {
	this->smbTex = other.smbTex;
	this->vkImpl = other.vkImpl;
	this->texImpl = other.texImpl;
	other.smbTex = nullptr;
	other.vkImpl = nullptr;
	other.texImpl = nullptr;
}

AppTexture::AppTexture(AppTexture& other) noexcept {
	this->smbTex = other.smbTex;
	this->vkImpl = other.vkImpl;
	this->texImpl = other.texImpl;
	other.smbTex = nullptr;
	other.vkImpl = nullptr;
	other.texImpl = nullptr;
}