#pragma once
#include "ProgramArgs.h"

class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args);
	~ApplicationLoop();

	void Execute();

	void InitializeRuntime();

	void CleanupRuntime();
	
	ProgramArgs& args;
	
	bool running, cleaned;
};
