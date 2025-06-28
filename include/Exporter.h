#pragma once



#include <iostream>
#include "SMBFile.h"
#include "VertexTypes.h"

namespace ExportHelper
{
	inline std::ostream& operator<<(std::ostream& os, const glm::vec4& vec)
	{
		os << std::vformat("{:6f} {:6f} {:6f} {:6f}\n", std::make_format_args(vec.x, vec.y, vec.z, vec.w));
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const glm::vec3& vec)
	{
		os << std::vformat("{:6f} {:6f} {:6f}\n", std::make_format_args(vec.x, vec.y, vec.z));
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const glm::vec2& vec)
	{
		os << std::vformat("{:6f} {:6f}\n", std::make_format_args(vec.x, vec.y));
		return os;
	}
}

class Exporter
{
public:
	
	static void ExportChunksFromFile(SMBFile& smb);

	static void ExportTextureFromFile(const SMBFile& smb, const SMBChunk& chunk);

	static void ExportToOBJFormat(std::vector<Vertex>& vertices, std::string& outputFile);
};

