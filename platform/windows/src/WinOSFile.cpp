#include "OSFile.h"
#include "Windows.h"
#include <atomic>

static HANDLE* intFileHandles;
static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;

static HANDLE stdInputHandle = INVALID_HANDLE_VALUE;
static HANDLE stdOutputHandle = INVALID_HANDLE_VALUE;
static HANDLE stdErrorHandle = INVALID_HANDLE_VALUE;

static DWORD ConvertOSFlags(OSFileFlags flags, DWORD* shareMode, DWORD* creationFlags)
{
    DWORD outflags = 0;
    if (flags & READ) {
        outflags |= GENERIC_READ;
        *shareMode |= FILE_SHARE_READ;
    }

    if (flags & WRITE) {
        outflags |= GENERIC_WRITE;
        *shareMode |= FILE_SHARE_WRITE;
    }

    if (flags & CREATE_IF_NOT_EXIST)
    {
        *creationFlags = OPEN_ALWAYS;
    }
    else if (flags & CREATE)
    {
        *creationFlags = CREATE_NEW;
    }
    else
    {
        *creationFlags = OPEN_EXISTING;
    }

    return outflags;
}

static int FindFreeIndex()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
        int idx = i;

        int8_t expected = 1; 
        if (freeList[idx].compare_exchange_strong(
            expected,
            -1,
            std::memory_order_acquire,
            std::memory_order_relaxed))
        {
            return idx;
        }
    }
    return -1;
}

OSFileMemoryRequirements OSGetFileMemoryRequirements(int maxNumberOfOpenFiles)
{
    int handlesSize = (maxNumberOfOpenFiles) * sizeof(HANDLE);
    int freeListSize = (maxNumberOfOpenFiles) * sizeof(std::atomic<int8_t>);

    OSFileMemoryRequirements memReqs{ handlesSize + freeListSize, alignof(HANDLE) };

    return memReqs;
}


void CloseAllFiles()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
        int idx = i;

        int8_t expected = 1;
        if (freeList[idx].load(
            std::memory_order_acquire) == -1)
        {
            CloseHandle(intFileHandles[idx]);
            freeList[idx].store(1);
        }
    }
}

int OSSeedFileMemory(void* dataSource, int dataSize, int numberOfOpenFiles)
{
    uintptr_t dataHead = (uintptr_t)dataSource;
    uintptr_t dataStart = dataHead;

    intFileHandles = (HANDLE*)dataHead;

    int handleSize = numberOfOpenFiles;

    dataHead += handleSize * sizeof(HANDLE);

    freeList = (std::atomic<int8_t>*)dataHead;

    for (int i = 0; i < handleSize; i++)
    {
        freeList[i] = 1;
    }

    maxFreeListEntry = handleSize;

    return OS_SUCCESS;
}

int OSCreateFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    char pathscratch[MAX_PATH];

     if (nameLength <= 0 || nameLength >= MAX_PATH)
        return OS_FAILED_CREATE;

    HANDLE hFile;
    DWORD fileShare = 0, creationFlags = 0;
    DWORD hAccess = ConvertOSFlags(flags, &fileShare, &creationFlags);

    memcpy(pathscratch, filename, nameLength);
    pathscratch[nameLength] = '\0';

    hFile = CreateFileA(pathscratch, hAccess, fileShare, NULL, creationFlags, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return OS_FAILED_CREATE;
    }

    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;

    int internalHandlePtr = FindFreeIndex();
    intFileHandles[internalHandlePtr] = hFile;

    fileHandle->osDataHandle = internalHandlePtr;

    return OS_SUCCESS;
}

int OSOpenFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    char pathscratch[MAX_PATH];

    if (nameLength <= 0 || nameLength >= MAX_PATH)
        return OS_FAILED_CREATE;

    HANDLE hFile;
    DWORD fileShare = 0, creationFlags = 0;
    DWORD hAccess = ConvertOSFlags(flags, &fileShare, &creationFlags);

    memcpy(pathscratch, filename, nameLength);
    pathscratch[nameLength] = '\0';

    hFile = CreateFileA(pathscratch, hAccess, fileShare, NULL, creationFlags, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return OS_FAILED_CREATE;
    }

    LARGE_INTEGER fileSize;

    BOOL retVal = GetFileSizeEx(hFile, &fileSize);

    if (!retVal)
    {
        CloseHandle(hFile);
        return OS_FAILED_SIZE;
    }

    fileHandle->fileLength = (uint64_t)fileSize.QuadPart;
    fileHandle->filePointer = 0;

    int internalHandlePtr = FindFreeIndex();
    intFileHandles[internalHandlePtr] = hFile;

    fileHandle->osDataHandle = internalHandlePtr;

    return OS_SUCCESS;
}

int OSCloseFile(OSFileHandle* fileHandle)
{
    if (fileHandle->osDataHandle < 0 || fileHandle->osDataHandle >= maxFreeListEntry)
        return OS_FILE_CLOSED_FAILED;

    HANDLE hFile = intFileHandles[fileHandle->osDataHandle];

    CloseHandle(hFile);

    intFileHandles[fileHandle->osDataHandle] = INVALID_HANDLE_VALUE;
    freeList[fileHandle->osDataHandle].store(1, std::memory_order_release);

    fileHandle->osDataHandle = -1;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;

    return OS_SUCCESS;
}

int OSReadFile(OSFileHandle* fileHandle, int size, char* buffer, uint64_t* dataReadSize)
{
    if (fileHandle->osDataHandle >= maxFreeListEntry+1)
    {
        return OS_STD_HANDLE_INVALID;
    }

    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (fileHandle->osDataHandle == maxFreeListEntry)
    {
        hFile = stdInputHandle;
    }
    else if (fileHandle->osDataHandle < maxFreeListEntry && fileHandle->osDataHandle >= 0)
    {
        hFile = intFileHandles[fileHandle->osDataHandle];
    }
    else 
    {
        return OS_FAILED_READ;
    }

    DWORD hBytesRead = 0;

    if (ReadFile(hFile, buffer, size, &hBytesRead, NULL) == FALSE)
    {
        return OS_FAILED_READ;
    }

    fileHandle->filePointer += hBytesRead;
    
    *dataReadSize = hBytesRead;

    return OS_SUCCESS;
}

int OSWriteFile(OSFileHandle* fileHandle, int size, const char* buffer, uint64_t* dataWriteSize)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (fileHandle->osDataHandle == maxFreeListEntry + 1)
    {
        hFile = stdErrorHandle;
    }
    else if (fileHandle->osDataHandle == maxFreeListEntry + 2)
    {
        hFile = stdOutputHandle;
    }
    else if (fileHandle->osDataHandle < maxFreeListEntry && fileHandle->osDataHandle >= 0)
    {
        hFile = intFileHandles[fileHandle->osDataHandle];
    }
    else 
    {
        return OS_FAILED_WRITE;
    }

    DWORD hBytesWrite = 0;

    if (WriteFile(hFile, buffer, size, &hBytesWrite, NULL) == FALSE)
    {
        return OS_FAILED_WRITE;
    }

    fileHandle->filePointer += hBytesWrite;

    *dataWriteSize = hBytesWrite;

    return OS_SUCCESS;
}

int OSSeekFile(OSFileHandle* fileHandle, size_t pointer, OSRelativeFlags flags)
{
    if (fileHandle->osDataHandle >= maxFreeListEntry)
    {
        return OS_STD_HANDLE_INVALID;
    }
    else if (fileHandle->osDataHandle < 0)
    {
        return OS_FAILED_SEEK;
    }

    HANDLE hFile = intFileHandles[fileHandle->osDataHandle];

    DWORD moveMethod = FILE_BEGIN;

    switch (flags)
    {
    case BEGIN:
        break;
    case CURRENT:
        moveMethod = FILE_CURRENT;
        break;
    case END:
        moveMethod = FILE_END;
        break;
    default:
        return OS_INVALID_ARGUMENT;
    }

    LARGE_INTEGER winSeekPointer, setSeekPointer;

    winSeekPointer.QuadPart = pointer;

    BOOL wRet = SetFilePointerEx(hFile, winSeekPointer, &setSeekPointer, moveMethod);

    if (!wRet)
    {
        return OS_FAILED_SEEK;
    }

    fileHandle->filePointer = setSeekPointer.QuadPart;

    return OS_SUCCESS;
}

int OSCreateFileIterator(const char* searchString, int nameLength, OSFileIterator* iterator)
{
    char pathscratch[MAX_PATH];

    if (!searchString || !iterator || nameLength <= 0) 
        return OS_INVALID_ARGUMENT;

    int index = FindFreeIndex();

    WIN32_FIND_DATAA data;

    memcpy(pathscratch, searchString, nameLength);
    pathscratch[nameLength] = '\0';

    HANDLE searchIdx = FindFirstFileA(pathscratch, &data);

    if (searchIdx == INVALID_HANDLE_VALUE)
    {
        return OS_FAILED_SEARCH_ITER;
    }

    intFileHandles[index] = searchIdx;

    strncpy(iterator->currentFileName, data.cFileName, 250);
    iterator->osDataHandle = index;

    return OS_SUCCESS;
}

int OSNextFile(OSFileIterator* iterator)
{
    if (!iterator) return OS_INVALID_ARGUMENT;

    int index = iterator->osDataHandle;

    WIN32_FIND_DATAA data;

    BOOL ret = FindNextFileA(intFileHandles[index], &data);

    if (!ret)
    {
        CloseHandle(intFileHandles[index]);
        return OS_REACH_ITER_END;
    }

    strncpy(iterator->currentFileName, data.cFileName, 250);

    return OS_SUCCESS;
}

void OSGetSTDInput(OSFileHandle* fileHandle)
{
    if (stdInputHandle == INVALID_HANDLE_VALUE)
    {
        stdInputHandle = GetStdHandle(STD_INPUT_HANDLE);
    }

    fileHandle->osDataHandle = maxFreeListEntry;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;
}

void OSGetSTDOutput(OSFileHandle* fileHandle)
{
    if (stdOutputHandle == INVALID_HANDLE_VALUE)
    {
        stdOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    fileHandle->osDataHandle = maxFreeListEntry + 2;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;

}

void OSGetSTDError(OSFileHandle* fileHandle)
{
    if (stdErrorHandle == INVALID_HANDLE_VALUE)
    {
        stdErrorHandle = GetStdHandle(STD_ERROR_HANDLE);
    }

    fileHandle->osDataHandle = maxFreeListEntry + 1;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;
}

int OSPollFile(OSFileHandle* fileHandle, int millisecondTimeOut)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (fileHandle->osDataHandle == maxFreeListEntry)
    {
        hFile = stdInputHandle;
    }
    else if (fileHandle->osDataHandle == maxFreeListEntry + 1)
    {
        hFile = stdErrorHandle;
    }
    else if (fileHandle->osDataHandle == maxFreeListEntry + 2)
    {
        hFile = stdOutputHandle;
    }
    else if (fileHandle->osDataHandle < maxFreeListEntry && fileHandle->osDataHandle >= 0)
    {
        hFile = intFileHandles[fileHandle->osDataHandle];
    }
    else 
    {
        return OS_FAILED_POLL;
    }

    DWORD ret = WaitForSingleObject(hFile, millisecondTimeOut);

    if (ret == WAIT_TIMEOUT) 
        return OS_FILE_POLL_TIMEOUT;

    return OS_SUCCESS;
}