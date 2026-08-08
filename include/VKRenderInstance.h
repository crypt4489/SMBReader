#pragma once
#include "RenderInstanceManagement.h"
#include "VKInstance.h"
#include "VKDevice.h"


struct RHIDevice
{
	RenderLogicalDeviceContainer container;
	VKDevice* device;
};