#include "pch.h"

#include "VKTexture.h"
#include <numeric>

VKTexture::VKTexture(EntryHandle piIndex, EntryHandle* pvIndex, int numViews, EntryHandle* psIndex, int numSamplers)
	:
	imageIndex(piIndex)
{
	for (int i = 0; i < numViews && i < MAX_VIEWS_PER_TEXTURE; i++)
	{
		viewIndex[i] = pvIndex[i];
	}

	for (int i = numViews; i < MAX_VIEWS_PER_TEXTURE; i++)
	{
		viewIndex[i] = EntryHandle();
	}

	for (int i = 0; i < numSamplers && i < MAX_SAMPLERS_PER_TEXTURE; i++)
	{
		samplerIndex[i] = psIndex[i];
	}

	for (int i = numSamplers; i < MAX_SAMPLERS_PER_TEXTURE; i++)
	{
		samplerIndex[i] = EntryHandle();
	}
}
