#pragma once
#include <filesystem>
#include <iostream> 
#include <cctype>
#include <locale>


class ProgramArgs
{
public:
	explicit ProgramArgs(int argc, char** argv);

	void ScanSTDIN();
	
	bool justexport;
	std::filesystem::path inputFile;
};

