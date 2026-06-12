#include "ApplicationLoop.h"
#include "ProgramArgs.h"
int main(int argc, char **argv)
{
	ProgramArgs args(argc, argv);
	ApplicationLoop app(args);
	return 0;
}

