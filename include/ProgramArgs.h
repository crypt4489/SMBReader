#pragma once
#include <filesystem>
#include <iostream> 
#include <cctype>
#include <locale>


class ProgramArgs
{
public:
	explicit ProgramArgs(int argc, char** argv) : justexport(false)
	{
		if (argc <= 1)
		{
			ScanSTDIN();
		}
		else 
		{
			if (strcmp(argv[1], "-d") == 0)
			{
				std::string in = std::string(argv[2]);
				size_t off = 0;
				size_t size = in.size();
				if (in[0] == '\"') off++;
				if (in[size - 1] == '\"') size--;
				inputFile = in.substr(off, size - off);
				return;
			}
		}
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
		if (in[size-1] == '\"') size--;		
		inputFile = in.substr(off, size - off);
	}
	
	bool justexport;
	std::filesystem::path inputFile;
};

