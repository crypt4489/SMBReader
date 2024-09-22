#include "ProgramArgs.h"
int main(int argc, char **argv)
{
	try 
	{
		ProgramArgs args(argc, argv);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what();
		return -1;
	}
	return 0;
}

