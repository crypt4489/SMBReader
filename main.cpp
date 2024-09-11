#include "ProgramArgs.h"
int main(int argc, char **argv)
{
	try 
	{
		ProgramArgs args(argc, argv);
		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << e.what();
		return -1;
	}
	
}

