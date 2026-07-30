#pragma once

#include "StringUtils.h"

struct ProgramArgs
{
	explicit ProgramArgs(int argc, char** argv);

	void ScanSTDIN();
	
	bool justexport;
	StringView inputFile;
	char stringBuffer[256];
};

