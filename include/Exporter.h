#pragma once
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <regex>
#include "FileManager.h"
#include "DXTCompression.h"
#include "SMB.h"
#include "VertexTypes.h"
#include "Texture.h"

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
		auto file = smb.fileHandle.second;
		auto pathToTextures = FileManager::SetupDirectory(name);
		std::streamoff offset = chunk.offsetInHeader + 21LL;
		file->seekg(offset, std::ios_base::beg);
		Texture tex {};
		file->read(reinterpret_cast<char*>(&tex), sizeof(Texture));
		file->seekg(chunk.contigOffset + smb.fileOffset, std::ios_base::beg);


		int writeWidth = tex.width;
		int	writeHeight = tex.height;


		size_t compressedsize = 0;
		std::vector<char> image(writeWidth * writeHeight * 4);
		std::vector<char> input;

		for (uint32_t i = 0; i < tex.miplevels; i++)
		{
			std::string writeFileName = name + std::to_string(i + 1) + ".bmp";

			auto writePath = pathToTextures / writeFileName;

			switch (tex.type)
			{
			case 7:
				std::cerr << "X8L8U8V8 format is not exportable\n";
				return;
			case 12:
				compressedsize = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
				input.resize(compressedsize);
				file->read(input.data(), compressedsize);
				DXTCompression::BlockDecompressImageDXT1(writeWidth, writeHeight, (unsigned char*)input.data(), (unsigned long*)image.data());
				break;
			case 14:
				compressedsize = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
				input.resize(compressedsize);
				file->read(input.data(), compressedsize);
				DXTCompression::BlockDecompressImageDXT3(writeWidth, writeHeight, (unsigned char*)input.data(), (unsigned char*)image.data());
				break;
			case 18:
				file->read(image.data(), writeWidth * writeHeight * 4);
				break;
			default:
				std::cerr << "Unsupported/Unknown texture type " << tex.type << "\n";
				return;
			}

			auto outputfilehandle = FileManager::OpenFile(writePath, std::ios::binary | std::ios::out);
			
			if (!outputfilehandle)
			{
				std::cerr << "Something bad happened\n";
				return;
			}

			auto outputFile = outputfilehandle->second;

			TexUtils::BMP::WriteOutBMPHeaders(outputFile, writeWidth, writeHeight);

			TexUtils::BGRATexture(image.data(), writeHeight, writeWidth);

			outputFile->write(image.data(), writeWidth * writeHeight * 4);

			FileManager::CloseFile(outputfilehandle->first);

			image.clear();
			writeHeight >>= 1;
			writeWidth >>= 1;
			image.resize(writeWidth * writeHeight * 4);
		}
	}

	static void ExportToOBJFormat(std::vector<Vertex> &vertices, std::string &outputFile)
	{
		using ExportHelper::operator<<;
		
		auto fileRet = FileManager::OpenFile(outputFile, std::ios_base::out);

		if (!fileRet)
		{
			throw std::runtime_error("Cannot open file for OBJ export " + outputFile);
		}

		FileManager::FileHandle fileHandle = fileRet.value();

		auto fileStream = fileHandle.second;

		for (const auto& vert : vertices) {
			*fileStream << "v " << vert.POSITION;
		}

		for (const auto& vert : vertices) {
			*fileStream << "vt " << vert.TEXTURE;
		}

		for (const auto& vert : vertices) {
			*fileStream << "vn " << vert.NORMAL;
		}
 
		FileManager::CloseFile(fileHandle.first);
	}
};

