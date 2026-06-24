#include "OSFile.h"

#include <string.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 4096

#define FILE_DESCRIPTOR_STD_IN 0
#define FILE_DESCRIPTOR_STD_OUT 1
#define FILE_DESCRIPTOR_STD_ERR 2

static int ConvertOSFileFlags(OSFileFlags flags)
{
    int osFlags = 0;

    if ((flags & READ) && (flags & WRITE))
    {   
        osFlags |= O_RDWR;
    } 
    else
    {
        if (flags & READ)
            osFlags |= O_RDONLY;
        if (flags & WRITE)
            osFlags |= O_WRONLY;
    }
    
    if (flags & CREATE)
    {
        osFlags |= (O_CREAT | O_TRUNC);
    }
    else if (flags & CREATE_IF_NOT_EXIST)
    {
        osFlags |= O_CREAT;
    }

    return osFlags;
}

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
    char namescratch[MAX_PATH];

    if (nameLength <= 0 || nameLength >= MAX_PATH)
        return OS_INVALID_ARGUMENT;

    memcpy(namescratch, filename, nameLength);

    namescratch[nameLength] = '\0';

    int fcntlFlags = ConvertOSFileFlags(flags);

    int fd = open(namescratch, fcntlFlags, S_IRUSR | S_IWUSR | S_IXUSR /*| S_IRGRP | S_IWGRP | S_IXGRP*/);

    if (fd < 0)
    {
        return OS_FAILED_CREATE;
    }

    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;
    fileHandle->osDataHandle = fd;

    return OS_SUCCESS;
}

int OSOpenFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    char namescratch[MAX_PATH];

    if (nameLength <= 0 || nameLength >= MAX_PATH)
        return OS_INVALID_ARGUMENT;

    memcpy(namescratch, filename, nameLength);

    namescratch[nameLength] = '\0';

    int fcntlFlags = ConvertOSFileFlags(flags);

    int fd = open(namescratch, fcntlFlags);

    if (fd < 0)
        return OS_FAILED_CREATE;

    struct stat statBuffer{};

    int fstatRet = fstat(fd, &statBuffer);

    if (fstatRet < 0)
    {
        close(fd);
        return OS_FAILED_SIZE;
    }

    fileHandle->fileLength = statBuffer.st_size;
    fileHandle->filePointer = 0;
    fileHandle->osDataHandle = fd;

    return OS_SUCCESS;
}

int OSCloseFile(OSFileHandle* fileHandle)
{
    if (fileHandle->osDataHandle <= 2)
        return OS_INVALID_ARGUMENT;

    int retCode = close(fileHandle->osDataHandle);

    if (retCode < 0)
        return OS_FILE_CLOSED_FAILED;

    fileHandle->osDataHandle = -1;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;

    return OS_SUCCESS;
}

int OSReadFile(OSFileHandle* fileHandle, int size, char* buffer, uint64_t* dataReadSize)
{
    if (fileHandle->osDataHandle < 0)
        return OS_FAILED_READ;

    ssize_t readCount = read(fileHandle->osDataHandle, buffer, size);

    if (readCount < 0)
        return OS_FAILED_READ;

    fileHandle->filePointer += readCount;

    *dataReadSize = readCount;

    return OS_SUCCESS;
}

int OSWriteFile(OSFileHandle* fileHandle, int size, const char* buffer, uint64_t* dataWriteSize)
{
    if (fileHandle->osDataHandle < 0)
        return OS_FAILED_WRITE;

    ssize_t writeCount = write(fileHandle->osDataHandle, buffer, size);

    if (writeCount < 0)
        return OS_FAILED_WRITE;

    fileHandle->filePointer += writeCount;

    *dataWriteSize = writeCount;

    return OS_SUCCESS;
}

int OSSeekFile(OSFileHandle* fileHandle, size_t pointer, OSRelativeFlags flags)
{
    if (fileHandle->osDataHandle < 0)
        return OS_FAILED_SEEK;

    int whence = SEEK_SET;

    switch (flags)
    {
    case BEGIN:
        break;
    case CURRENT:
        whence = SEEK_CUR;
        break;
    case END:
        whence = SEEK_END;
        break;
    default:
        return OS_INVALID_ARGUMENT;
    }

    off64_t newOffset = lseek64(fileHandle->osDataHandle, pointer, whence);

    if (newOffset < 0)
        return OS_FAILED_SEEK;

    fileHandle->filePointer = newOffset;

    return OS_SUCCESS;
}

int OSCreateFileIterator(const char* searchString, int nameLength, OSFileIterator* iterator)
{
    return OS_FILE_FUNCTION_NOT_IMPLEMENTED;
}

int OSNextFile(OSFileIterator* iterator)
{
    return OS_FILE_FUNCTION_NOT_IMPLEMENTED;
}

void OSGetSTDInput(OSFileHandle* fileHandle)
{
    fileHandle->osDataHandle = FILE_DESCRIPTOR_STD_IN;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;
}

void OSGetSTDOutput(OSFileHandle* fileHandle)
{
    fileHandle->osDataHandle = FILE_DESCRIPTOR_STD_OUT;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;
}

void OSGetSTDError(OSFileHandle* fileHandle)
{
    fileHandle->osDataHandle = FILE_DESCRIPTOR_STD_ERR;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;
}

int OSPollFile(OSFileHandle* fileHandle, int millisecondTimeOut)
{
    if (fileHandle->osDataHandle < 0)
        return OS_FAILED_POLL;

    struct pollfd fds{};

    fds.fd = fileHandle->osDataHandle;
    fds.events = POLLIN;
    fds.revents = 0;

    int pollRet = poll(&fds, 1, millisecondTimeOut);

    if (pollRet < 0)
        return OS_FAILED_POLL;

    if (!pollRet)
        return OS_FILE_POLL_TIMEOUT;

    return pollRet;
}