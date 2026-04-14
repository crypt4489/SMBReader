#include "ApplicationLoop.h"
#include "ProgramArgs.h"
#include <iostream>
int main(int argc, char **argv)
{
	try 
	{
		ProgramArgs args(argc, argv);
		ApplicationLoop app(args);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what();
		return -1;
	}
	return 0;
}

