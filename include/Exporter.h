#pragma once

#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <regex>

#include "AppTextureImpl.h"
#include "FileManager.h"
#include "DXTCompression.h"
#include "SMBFile.h"
#include "SMBTexture.h"
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
	
	static void ExportChunksFromFile(SMBFile &smb)
	{
		
		for (const auto& chunk : smb.chunks)
		{
			switch (chunk.chunkType)
			{
			case GEO:
				break;
			case TEXTURE:
				ExportTextureFromFile(smb, chunk);
				break;
			case GR2:
				break;
			case Joints:
				break;
			default:
				std::cerr << "Unprocessed chunkType\n";
				break;
			}
		}
	}

	
	static void ExportTextureFromFile(const SMBFile &smb, const SMBChunk &chunk)
	{
		std::string name = FileManager::ExtractFileNameFromPath(chunk.fileName);
		
		auto pathToTextures = FileManager::SetupDirectory(name);

		SMBTexture tex(smb, chunk);

		for (uint32_t i = 0; i < tex.miplevels; i++)
		{
			std::string writeFileName = name + std::to_string(i + 1) + ".bmp";

			auto writePath = pathToTextures / writeFileName;

			uint32_t writeWidth = tex.width >> i;
			uint32_t writeHeight = tex.height >> i;

			FileHandle* handle;

			auto outputfilehandle = FileManager::OpenFile(writePath, std::ios::binary | std::ios::out, handle);
			
			if (!outputfilehandle)
			{
				std::cerr << "Something bad happened\n";
				return;
			}

			auto &outputFile = handle->streamHandle;

			TexUtils::BMP::WriteOutBMPHeaders(outputFile, writeWidth, writeHeight);

			std::vector<char> bgra = tex.data[i];

			TexUtils::BGRATexture(bgra.data(), tex.width >> i, tex.width >> i, 4);

			uint32_t bpr = writeWidth * 4;

			uint32_t offset = (writeHeight - 1) * bpr;

			for (uint32_t i = 0; i < writeHeight; i++)
			{
				outputFile.write(bgra.data() + offset, bpr);
				offset -= bpr;
			}

			

			FileManager::RemoveOpenFile(outputfilehandle.value());
		}
	}

	static void ExportToOBJFormat(std::vector<Vertex> &vertices, std::string &outputFile)
	{
		using ExportHelper::operator<<;

		FileHandle* handle;
		
		auto fileRet = FileManager::OpenFile(outputFile, std::ios_base::out, handle);

		if (!fileRet)
		{
			throw std::runtime_error("Cannot open file for OBJ export " + outputFile);
		}


		auto &fileStream = handle->streamHandle;

		for (const auto& vert : vertices) {
			fileStream << "v " << vert.POSITION;
		}

		for (const auto& vert : vertices) {
			fileStream << "vt " << vert.TEXTURE;
		}

		for (const auto& vert : vertices) {
			fileStream << "vn " << vert.NORMAL;
		}
 
		FileManager::RemoveOpenFile(fileRet.value());
	}
};

