#pragma once

#include <cstdint>

enum RenderingBackend
{
	VULKAN = 1,
	DXD12 = 2,
};

enum DepthTest
{
	ALLPASS = 0,
	DEPTHLESS = 1,
};

enum ImageFormat : uint32_t
{
	X8L8U8V8 = 0,
	DXT1 = 1,
	DXT3 = 2,
	R8G8B8A8 = 3,
	B8G8R8A8 = 4,
	D24UNORMS8STENCIL = 5,
	D32FLOATS8STENCIL = 6,
	D32FLOAT = 7,
	R8G8B8A8_UNORM = 8,
	R8G8B8 = 9,
	IMAGE_UNKNOWN = 0x7fffffff
};

enum TextureIOType
{
	BMP = 0,
};

enum PrimitiveType
{
	TRIANGLES = 0,
	TRISTRIPS = 6,
};