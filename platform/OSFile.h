
#pragma once
struct OSFileHandle
{
	unsigned int fileLength;
	int filePointer;
	int osDataHandle;

	OSFileHandle()
	{
		fileLength = -1;
		filePointer = -1;
		osDataHandle = -1;
	}
};

enum OSFileFlags
{
	READ = 1,
	WRITE = 2,
	CREATE = 4,
};

enum OSRelativeFlags
{
	BEGIN = 0,
	CURRENT = 1,
	END = 2,
};

enum OSErrorFlags
{
	OS_SUCCESS = 0,
	OS_FAILED_CREATE = -1,
	OS_FAILED_SIZE = -2,
	OS_INVALID_ARGUMENT = -3,
	OS_FAILED_SEEK = -4,
	OS_FAILED_READ = -5,
	OS_FAILED_WRITE = -6
};

int OSCreateFile(const char* filename, OSFileFlags flags, OSFileHandle* fileHandle);

int OSOpenFile(const char* filename, OSFileFlags flags, OSFileHandle* fileHandle);

int OSCloseFile(OSFileHandle* fileHandle);

int OSReadFile(OSFileHandle* fileHandle, int size, char* buffer);

int OSSeekFile(OSFileHandle* fileHandle, int pointer, OSRelativeFlags flags);

int OSWriteFile(OSFileHandle* fileHandle, int size, char* buffer);