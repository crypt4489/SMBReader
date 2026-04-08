#include "OSFile.h"
#include "Windows.h"
#include <atomic>

static HANDLE* intFileHandles;
static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;


static DWORD ConvertOSFlags(OSFileFlags flags, DWORD* shareMode)
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

    return 0;
}

int OSCreateFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    char pathscratch[MAX_PATH];
    HANDLE hFile;
    DWORD fileShare = 0;
    DWORD hAccess = ConvertOSFlags(flags, &fileShare);

    memcpy(pathscratch, filename, nameLength);
    pathscratch[nameLength] = '\0';

    hFile = CreateFileA(pathscratch, hAccess, fileShare, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

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

    HANDLE hFile;
    DWORD fileShare = 0;
    DWORD hAccess = ConvertOSFlags(flags, &fileShare);

    memcpy(pathscratch, filename, nameLength);
    pathscratch[nameLength] = '\0';

    hFile = CreateFileA(pathscratch, hAccess, fileShare, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return OS_FAILED_CREATE;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);

    if (fileSize == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);
        return OS_FAILED_SIZE;
    }

    fileHandle->fileLength = fileSize;
    fileHandle->filePointer = 0;

    int internalHandlePtr = FindFreeIndex();
    intFileHandles[internalHandlePtr] = hFile;

    fileHandle->osDataHandle = internalHandlePtr;

    return OS_SUCCESS;
}

int OSCloseFile(OSFileHandle* fileHandle)
{
    HANDLE hFile = intFileHandles[fileHandle->osDataHandle];
    CloseHandle(hFile);
    intFileHandles[fileHandle->osDataHandle] = INVALID_HANDLE_VALUE;
    freeList[fileHandle->osDataHandle].store(1, std::memory_order_release);
    memset(fileHandle, -1, sizeof(OSFileHandle));
    return OS_SUCCESS;
}

int OSReadFile(OSFileHandle* fileHandle, int size, char* buffer)
{
    HANDLE hFile = intFileHandles[fileHandle->osDataHandle];

    DWORD hBytesRead = 0;

    if (ReadFile(hFile, buffer, size, &hBytesRead, NULL) == FALSE)
    {
        return OS_FAILED_READ;
    }

    fileHandle->filePointer += hBytesRead;

    return hBytesRead;
}

int OSWriteFile(OSFileHandle* fileHandle, int size, char* buffer)
{
    HANDLE hFile = intFileHandles[fileHandle->osDataHandle];

    DWORD hBytesWrite = 0;

    if (WriteFile(hFile, buffer, size, &hBytesWrite, NULL) == FALSE)
    {
        return OS_FAILED_WRITE;
    }

    fileHandle->filePointer += hBytesWrite;

    return hBytesWrite;
}

int OSSeekFile(OSFileHandle* fileHandle, int pointer, OSRelativeFlags flags)
{

    HANDLE hFile = intFileHandles[fileHandle->osDataHandle];

    int currentPointer = fileHandle->filePointer;

    DWORD moveMethod = FILE_BEGIN;

    switch (flags)
    {
    case BEGIN:
        currentPointer = pointer;
        break;
    case CURRENT:
        moveMethod = FILE_CURRENT;
        currentPointer += pointer;
        break;
    case END:
        moveMethod = FILE_END;
        currentPointer = fileHandle->fileLength;
        break;
    default:
        return OS_INVALID_ARGUMENT;
    }

    DWORD nRet = SetFilePointer(hFile, pointer, NULL, moveMethod);

    if (nRet == INVALID_SET_FILE_POINTER)
    {
        return OS_FAILED_SEEK;
    }

    fileHandle->filePointer = currentPointer;

    return OS_SUCCESS;
}

int OSCreateFileIterator(const char* searchString, int nameLength, OSFileIterator* iterator)
{
    char pathscratch[MAX_PATH];

    if (!searchString || !iterator) return OS_INVALID_ARGUMENT;

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

    return 0;
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

    return 0;
}