#pragma once
enum class RenderingBackend
{
	VULKAN = 1,
	DXD12 = 2,
};

enum class DepthTest
{
	ALLPASS = 0,
	DEPTHLESS = 1,
};