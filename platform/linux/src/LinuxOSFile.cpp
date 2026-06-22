#include "OSFile.h"

OSFileMemoryRequirements OSGetFileMemoryRequirements(int maxNumberOfOpenFiles)
{
    OSFileMemoryRequirements memReqs{ 0, 1 };

    return memReqs;
}

void CloseAllFiles()
{
  
}

int OSSeedFileMemory(void* dataSource, int dataSize, int numberOfOpenFiles)
{
    return 0;
}

int OSCreateFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    return OS_SUCCESS;
}

int OSOpenFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    return OS_SUCCESS;
}

int OSCloseFile(OSFileHandle* fileHandle)
{
    return OS_SUCCESS;
}

int OSReadFile(OSFileHandle* fileHandle, int size, char* buffer)
{
    return 0;
}

int OSWriteFile(OSFileHandle* fileHandle, int size, char* buffer)
{
    return 0;
}

int OSSeekFile(OSFileHandle* fileHandle, int pointer, OSRelativeFlags flags)
{
    return OS_SUCCESS;
}

int OSCreateFileIterator(const char* searchString, int nameLength, OSFileIterator* iterator)
{
   // char pathscratch[MAX_PATH];

    return 0;
}

int OSNextFile(OSFileIterator* iterator)
{
    return 0;
}

void OSGetSTDInput(OSFileHandle* fileHandle)
{
    fileHandle->osDataHandle = 0;
    fileHandle->fileLength = -1;
    fileHandle->filePointer = -1;
}

void OSGetSTDOutput(OSFileHandle* fileHandle)
{
    fileHandle->osDataHandle = 0;
    fileHandle->fileLength = -1;
    fileHandle->filePointer = -1;

}

void OSGetSTDError(OSFileHandle* fileHandle)
{
    fileHandle->osDataHandle = 0;
    fileHandle->fileLength = -1;
    fileHandle->filePointer = -1;
}

int OSPollFile(OSFileHandle* fileHandle, int millisecondTimeOut)
{
    return 0;
}