#pragma once
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

enum ShaderType
{
	PIX_SHADER = 0,
	VERT_SHADER = 1,
};

enum ImageFormat : uint32_t
{
	X8L8U8V8 = 7,
	DXT1 = 12,
	DXT3 = 14,
	R8G8B8A8 = 18,
	R8G8B8 = 0x7ffffffe,
	IMAGE_UNKNOWN = 0x7fffffff
};

enum TextureIOType
{
	BMP = 0,
};

enum PrimitiveType
{
	TRIANGLES = 0,
	TRISTRIPS = 1,
	WHAT = 2
};