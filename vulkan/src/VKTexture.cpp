#include "pch.h"

#include "VKTexture.h"
#include <numeric>

VKTexture::VKTexture(EntryHandle piIndex, EntryHandle pvIndex, EntryHandle psIndex)
	:
	imageIndex(piIndex),
	viewIndex(pvIndex),
	samplerIndex(psIndex)
{

}

VKTexture::VKTexture(VKTexture&& other) noexcept
{
	this->imageIndex = std::move(other.imageIndex);
	this->viewIndex = std::move(other.viewIndex);
	this->samplerIndex = std::move(other.samplerIndex);
}


VKTexture::~VKTexture()
{

}
