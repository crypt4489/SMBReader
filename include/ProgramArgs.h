#pragma once
#include <filesystem>
class ProgramArgs
{
public:
	explicit ProgramArgs(int argc, char** argv);

	void ScanSTDIN();
	
	bool justexport;
	std::filesystem::path inputFile;
};

