

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <regex>


#include "s3tc.h"
#include "SMB.h"
#include "FileManager.h"
#include "Exporter.h"


int main(int argc, char **argv)
{

	std::cout << sizeof(SMBFile) << std::endl;
	std::cout << sizeof(SMBChunk) << std::endl;
	std::string MainSMB = "strangernew.smb";
	
	FileManager::SetCurrentDirectory(FileManager::ExtractFileNameFromPath(MainSMB));
	
	SMBFile smbfile{};
	smbfile.LoadFile(MainSMB);
	std::cout << smbfile.numContiguousBytes << std::endl;
	std::cout << smbfile.numSystemBytes << std::endl;
	std::cout << smbfile.numSystemBytes + smbfile.fileOffset << std::endl;

	int j = 0;
	
	for (auto& i : smbfile.chunks)
	{
		std::cout << (j++) << "\n" << i << "\n";
	}
	
	Exporter::ExportChunksFromFile(smbfile);
	
	return 0;
}

