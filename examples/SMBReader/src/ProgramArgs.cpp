#include "ProgramArgs.h"

#include <iostream>
#include <cctype>
#include <locale>
#include <string>

ProgramArgs::ProgramArgs(int argc, char** argv) : justexport(false)
{
	if (argc <= 1)
	{
		ScanSTDIN();
	}
	else
	{
		if (strcmp(argv[1], "-d") == 0)
		{
			size_t off = 0;
			size_t size = strnlen(argv[2], 256);
			if (argv[2][0] == '\"') off++;
			if (argv[2][size - 1] == '\"') size--;
			strncpy(stringBuffer, argv[2] + off, (size - off));
			inputFile.charCount = size-off;
		}
	}

	inputFile.stringData = stringBuffer;
}


void ProgramArgs::ScanSTDIN()
{
	std::string in;
	std::cout << "Enter in a SMB File : ";
	std::getline(std::cin, in);
	std::cout << "\nDo you want to export (1) or view (0) SMB file? : ";
	std::cin >> justexport;
	std::cout << "\n";
	size_t size = in.length(), off = 0;
	if (in[off] == '\"') off++;
	if (in[size - 1] == '\"') size--;
	
	strncpy(stringBuffer, in.substr(off, size - off).c_str(), 256);

	inputFile.charCount = size - off;
}