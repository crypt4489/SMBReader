#pragma once
#include <cstdint>
#include <fstream>
#include <iostream>
#include <regex>
#include "FileManager.h"
#include "s3tc.h"
#include "SMB.h"

#include "Texture.h"
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

	
	static void ExportTextureFromFile(SMBFile &smb, const SMBChunk &chunk)
	{
		std::string name = FileManager::ExtractFileNameFromPath(chunk.fileName);
		auto file = smb.fileHandle.second;
		auto pathToTextures = FileManager::SetupDirectory(name);
		std::streampos offset = chunk.offsetInHeader + 21LL;
		file->seekg(offset, std::ios_base::beg);
		Texture tex{};
		file->read(reinterpret_cast<char*>(&tex), sizeof(Texture));
		std::cout << name << "\n" << tex;
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
				compressedsize = DXT1CompressedSize(writeWidth, writeHeight);
				input.resize(compressedsize);
				file->read(input.data(), compressedsize);
				BlockDecompressImageDXT1(writeWidth, writeHeight, (unsigned char*)input.data(), (unsigned long*)image.data());
				TexUtils::BGRATexture(image.data(), writeHeight, writeWidth);		
				break;
			case 14:
				compressedsize = DXT3CompressedSize(writeWidth, writeHeight);
				input.resize(compressedsize);
				file->read(input.data(), compressedsize);
				BlockDecompressImageDXT3(writeWidth, writeHeight, (unsigned char*)input.data(), (unsigned char*)image.data());
				TexUtils::BGRATexture(image.data(), writeHeight, writeWidth);
				break;
			case 18:
				file->read(image.data(), writeWidth * writeHeight * 4);
				TexUtils::BGRATexture(image.data(), writeHeight, writeWidth);
				break;
			default:
				std::cerr << "Unsupported/Unknown texture type " << tex.type << "\n";
				return;
			}

			auto outputfilehandle = FileManager::OpenFile(writePath.string(), std::ios::binary | std::ios::out);
			
			if (!outputfilehandle)
			{
				std::cerr << "Something bad happened\n";
				return;
			}

			auto outputFile = outputfilehandle->second;

			TexUtils::BMP::WriteOutBMPHeaders(outputFile, writeWidth, writeHeight);

			outputFile->write(image.data(), writeWidth * writeHeight * 4);

			FileManager::CloseFile(outputfilehandle->first);

			image.clear();
			writeHeight >>= 1;
			writeWidth >>= 1;
			image.resize(writeWidth * writeHeight * 4);

		}
		
	} 
};

