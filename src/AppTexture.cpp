#include "AppTexture.h"
#include "RenderInstance.h"
AppTexture::AppTexture(const SMBFile& smb, const SMBChunk& chunk) :
	smbTex(new SMBTexture(smb, chunk)), vkImpl(), texImpl(nullptr)
{
	vkImpl = VKRenderer::gRenderInstance->CreateVulkanImage((char*)smbTex->data, smbTex->imageSizes, smbTex->width, smbTex->height, smbTex->miplevels, smbTex->type);
}

AppTexture::AppTexture(std::vector<char>& data, TextureIOType type) : smbTex(nullptr), texImpl(new AppTextureImpl(data, type))
{
	vkImpl = VKRenderer::gRenderInstance->CreateVulkanImage((char*)texImpl->data, &texImpl->dataSize, texImpl->width, texImpl->height, texImpl->miplevels, texImpl->type);
}

AppTexture::~AppTexture()
{
	if (smbTex) delete smbTex;
	if (texImpl) delete texImpl;
	if (vkImpl != EntryHandle()) VKRenderer::gRenderInstance->DestoryTexture(vkImpl);
	//vkImpl = EntryHandle();
}

AppTexture::AppTexture(AppTexture&& other) noexcept {
	this->smbTex = other.smbTex;
	this->vkImpl = other.vkImpl;
	this->texImpl = other.texImpl;
	other.smbTex = nullptr;
	other.texImpl = nullptr;
	other.vkImpl = EntryHandle();
}

AppTexture::AppTexture(AppTexture& other) noexcept {
	this->smbTex = other.smbTex;
	this->vkImpl = other.vkImpl;
	this->texImpl = other.texImpl;
	other.smbTex = nullptr;
	other.texImpl = nullptr;
	other.vkImpl = EntryHandle();
}