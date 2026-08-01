
#pragma once
#include "OS.h"

struct OSFileHandle
{
	uint64_t fileLength;
	uint64_t filePointer;
	int osDataHandle;

	OSFileHandle()
	{
		fileLength = 0;
		filePointer = 0;
		osDataHandle = -1;
	}
};

enum OSFileFlagsTypes
{
	READ = 1,
	WRITE = 2,
	CREATE = 4,
	CREATE_IF_NOT_EXIST = 8
};

typedef int OSFileFlags;

enum OSRelativeFlags
{
	BEGIN = 0,
	CURRENT = 1,
	END = 2,
};

enum OSDirectoryFlags
{
	PRIVATE_DIR = 1,
	PUBLIC_DIR = 2,
};

typedef int OSDirectoryFlag;

enum OSFileErrorFlags
{
	OS_FILE_SUCCESS = 0,
	OS_FILE_FAILED_CREATE = -1,
	OS_FILE_FAILED_SIZE = -2,
	OS_FILE_INVALID_ARGUMENT = -3,
	OS_FILE_FAILED_SEEK = -4,
	OS_FILE_FAILED_READ = -5,
	OS_FILE_FAILED_WRITE = -6,
	OS_FILE_FAILED_SEARCH_ITER = -7,
	OS_FILE_REACH_ITER_END = -8,
	OS_FILE_STD_HANDLE_INVALID = -9,
	OS_FILE_POLL_TIMEOUT = -10, 
	OS_FILE_CLOSED_FAILED = -11,
	OS_FILE_FUNCTION_NOT_IMPLEMENTED = -12,
	OS_FILE_FAILED_POLL = -13,
	OS_FILE_FAILED_CREATE_DIRECTORY = -14,
	OS_FILE_FAILED_GET_CURRENT_DIRECTORY = -15,
	OS_FILE_FAILED_SET_CURRENT_DIRECTORY = -16,
	OS_FILE_FAILED_EXISTING = -17,
	OS_FILE_HANDLE_EXHASUTED = -18,
	OS_FILE_HANDLE_OUT_OF_BOUNDS = -19,
	OS_FILE_FAILED_CLOSE = -20
};

struct OSFileIterator
{
	char currentFileName[250];
	int osDataHandle;

	OSFileIterator()
	{
		osDataHandle = -1;
	}
};

struct OSFileMemoryRequirements
{
	int dataSize;
	int alignment;
};

OSFileMemoryRequirements OSGetFileMemoryRequirements(int maxNumberOfOpenFiles);

int OSSeedFileMemory(void* dataSource, int dataSize, int numberOfOpenFiles);

int OSCreateFile(const char* filename, int nameLength,  OSFileFlags flags, OSFileHandle* fileHandle);

int OSOpenFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle);

int OSCloseFile(OSFileHandle* fileHandle);

int64_t OSReadFile(OSFileHandle* fileHandle, int size, char* buffer);

int OSSeekFile(OSFileHandle* fileHandle, size_t pointer, OSRelativeFlags flags);

int64_t OSWriteFile(OSFileHandle* fileHandle, int size, const char* buffer);

int OSCreateFileIterator(const char* searchString, int nameLength, OSFileIterator* iterator);

int OSNextFile(OSFileIterator* iterator);

void OSGetSTDInput(OSFileHandle* fileHandle);
void OSGetSTDOutput(OSFileHandle* fileHandle);
void OSGetSTDError(OSFileHandle* fileHandle);

void CloseAllFiles();

int OSPollFile(OSFileHandle* fileHandle, int millisecondTimeOut);

int OSCreateDirectory(const char* directoryPath, int charCount, OSDirectoryFlag directoryFlag);

int OSGetCurrentDirectorySize();
int OSGetCurrentDirectory(int bufferSize, char* outputBuffer);
int OSSetCurrentDirectory(const char* inputPath, int charCount);

int OSExtractFileName(const char* inputFilePath, int inputFilePathCount, char* outputBuffer);

int OSGetSystemFileTerminator();

int OSFileExist(const char* inputFile, int charCount, OSFileFlags flags);