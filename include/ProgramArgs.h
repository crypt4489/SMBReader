#pragma once
#include <filesystem>
#include <iostream>
#include <algorithm> 
#include <cctype>
#include <locale>
#include "Exporter.h"
#include "SMB.h"
class ProgramArgs
{
public:
	explicit ProgramArgs(int argc, char** argv) : justexport(false), mainSMB(std::make_unique<SMBFile>())
	{
		if (argc <= 1)
		{
			ScanSTDIN();
		}
		else 
		{

		}

		Process();
	}


	void ScanSTDIN()
	{
		std::string in;
		std::cout << "Enter in a SMB File : ";
		std::getline(std::cin, in);
		std::cout << "\nDo you want to export (1) or view (0) SMB file? : ";
		std::cin >> justexport;
		std::cout << "\n";	
		size_t size = in.length(), off = 0;
		if (in[0] == '\"') off++;
		if (in[size] == '\"') size--;
		in = in.substr(off, size-off);			
		inputFile = in;
	}

	

private:
	void Process()
	{
		FileManager::SetCurrentDirectory(FileManager::ExtractFileNameFromPath(inputFile.string()));
		mainSMB->LoadFile(inputFile);
		if (justexport) Exporter::ExportChunksFromFile(*mainSMB);
	}
	std::unique_ptr<SMBFile> mainSMB;
	bool justexport;
	std::filesystem::path inputFile;
};

