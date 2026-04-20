#pragma once


#include <format>
#include <iostream>
#include "allocator/AppAllocator.h"
#include "SMBFile.h"
#include "math/VertexTypes.h"

namespace ExportHelper
{
	inline std::ostream& operator<<(std::ostream& os, const Vector4f& vec)
	{
		os << std::vformat("{:.6f} {:.6f} {:.6f} {:.6f}\n", std::make_format_args(vec.x, vec.y, vec.z, vec.w));
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const Vector3f& vec)
	{
		os << std::vformat("{:.6f} {:.6f} {:.6f}\n", std::make_format_args(vec.x, vec.y, vec.z));
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const Vector2f& vec)
	{
		os << std::vformat("{:.6f} {:.6f}\n", std::make_format_args(vec.x, vec.y));
		return os;
	}
}


void ExportChunksFromFile(SMBFile* smb, Allocator* inputScratchMemory);
void ExportTextureFromFile(SMBFile* smb, SMBChunk& chunk, Allocator* inputScratchMemory);
void ExportToOBJFormat(void* vertices, int vertexCount, StringView outputFile);


