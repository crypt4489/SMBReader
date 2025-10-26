#include "pch.h"

#include "VKTexture.h"
#include <numeric>

VKTexture::VKTexture(EntryHandle piIndex, EntryHandle* pvIndex, int numViews, EntryHandle* psIndex, int numSamplers)
	:
	imageIndex(piIndex)
{
	for (int i = 0; i < numViews; i++)
	{
		viewIndex[i] = pvIndex[i];
	}

	for (int i = numViews; i < 4; i++)
	{
		viewIndex[i] = EntryHandle();
	}

	for (int i = 0; i < numSamplers; i++)
	{
		samplerIndex[i] = psIndex[i];
	}

	for (int i = numSamplers; i < 4; i++)
	{
		samplerIndex[i] = EntryHandle();
	}
}

VKTexture::VKTexture(VKTexture&& other) noexcept
{
	this->imageIndex = std::move(other.imageIndex);
	//this->viewIndex = std::move(other.viewIndex);
	//this->samplerIndex = std::move(other.samplerIndex);
}


VKTexture::~VKTexture()
{

}
