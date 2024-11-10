#pragma once
#include <filesystem>
#include <functional>
#include <iostream>
#include <algorithm> 
#include <cctype>
#include <locale>
#include "Exporter.h"
#include "RenderInstance.h"
#include "SMB.h"
#include "VertexTypes.h"
class ProgramArgs
{
public:
	explicit ProgramArgs(int argc, char** argv) : justexport(false), mainSMB(new SMBFile())
	{
		if (argc <= 1)
		{
			//ScanSTDIN();
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
		if (in[off] == '\"') off++;
		if (in[size] == '\"') size--;
		in = in.substr(off, size-off);			
		inputFile = in;
	}


	

private:
	void Process()
	{
		//FileManager::SetCurrentDirectory(FileManager::ExtractFileNameFromPath(inputFile.string()));
		//mainSMB->LoadFile(inputFile);
		//if (justexport) Exporter::ExportChunksFromFile(*mainSMB);

		::VK::Renderer::gRenderInstance = new RenderInstance();
		::VK::Renderer::gRenderInstance->CreateVulkanRenderer();
		::VK::Renderer::gRenderInstance->RenderLoop();
		::VK::Renderer::gRenderInstance->DestroyVulkanRenderer();
		delete ::VK::Renderer::gRenderInstance;
		delete mainSMB;
		
	}
	SMBFile *mainSMB;
	bool justexport;
	std::filesystem::path inputFile;
};

