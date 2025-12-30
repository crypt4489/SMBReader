#pragma once



#include <iostream>
#include "SMBFile.h"
#include "VertexTypes.h"

namespace ExportHelper
{
	inline std::ostream& operator<<(std::ostream& os, const Vector4f& vec)
	{
		os << std::vformat("{:6f} {:6f} {:6f} {:6f}\n", std::make_format_args(vec.x, vec.y, vec.z, vec.w));
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const Vector3f& vec)
	{
		os << std::vformat("{:6f} {:6f} {:6f}\n", std::make_format_args(vec.x, vec.y, vec.z));
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const Vector2f& vec)
	{
		os << std::vformat("{:6f} {:6f}\n", std::make_format_args(vec.x, vec.y));
		return os;
	}
}

struct Exporter
{
public:
	
	static void ExportChunksFromFile(SMBFile& smb);

	static void ExportTextureFromFile(const SMBFile& smb, const SMBChunk& chunk);

	static void ExportToOBJFormat(std::vector<Vertex>& vertices, std::string& outputFile);
};

