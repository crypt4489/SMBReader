#include "OSFile.h"
#include "Windows.h"

static HANDLE* intFileHandles;
static int* handleTypes;
static MPMCQueueData* freeList;
static int maxFreeListEntry = 0;

static HANDLE stdInputHandle = INVALID_HANDLE_VALUE;
static HANDLE stdOutputHandle = INVALID_HANDLE_VALUE;
static HANDLE stdErrorHandle = INVALID_HANDLE_VALUE;

ALIGNAS(64) static std::atomic<int> boundedLinearAllocator{ 0 };
ALIGNAS(64) static std::atomic<size_t> enqueuePos{ 0 };
ALIGNAS(64) static std::atomic<size_t> dequeuePos{ 0 };



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

static int PopFromFreeList()
{
    MPMCQueueData* cell;
    
    size_t pos = dequeuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
        if (diff == 0)
        {
            if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return -1;
        else
            pos = dequeuePos.load(std::memory_order_relaxed);
    }

    cell->currentSequence.store(pos + maxFreeListEntry, std::memory_order_release);

    int freeListIndex = cell->freeIndex;

    cell->freeIndex = -1;

    return freeListIndex;
}

static void ReturnIndex(int index) 
{
    MPMCQueueData* cell;
    
    size_t pos = enqueuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0)
        {
            if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return;
        else
            pos = enqueuePos.load(std::memory_order_relaxed);
    }

    intFileHandles[index] = INVALID_HANDLE_VALUE;
    cell->freeIndex = index;
    cell->currentSequence.store(pos + 1, std::memory_order_release);
}

static int FindFreeIndex()
{
    int ret = PopFromFreeList();
    
    if (ret < 0)
    {
        int linearTop = boundedLinearAllocator.load(std::memory_order_acquire);

        while (linearTop < maxFreeListEntry)
        {
            if (boundedLinearAllocator.compare_exchange_weak(linearTop, linearTop + 1, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                ret = linearTop;
                break;
            }
        }
    }

    return ret;
}

OSFileMemoryRequirements OSGetFileMemoryRequirements(int maxNumberOfOpenFiles)
{
    int handlesSize = (maxNumberOfOpenFiles) * sizeof(HANDLE);
    int freeListSize = (maxNumberOfOpenFiles) * sizeof(MPMCQueueData);

    OSFileMemoryRequirements memReqs{ handlesSize + freeListSize, alignof(HANDLE) };

    return memReqs;
}

void CloseAllFiles()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
        int idx = i;

        if (intFileHandles[idx] != INVALID_HANDLE_VALUE)
        {
            CloseHandle(intFileHandles[idx]);
            intFileHandles[idx] = INVALID_HANDLE_VALUE;  
        }

        freeList[i].currentSequence.store(i, std::memory_order_relaxed);
    }

    enqueuePos.store(0, std::memory_order_relaxed);
    dequeuePos.store(0, std::memory_order_relaxed);
    boundedLinearAllocator.store(0, std::memory_order_relaxed);
}

int OSSeedFileMemory(void* dataSource, int dataSize, int numberOfOpenFiles)
{
    uintptr_t dataHead = (uintptr_t)dataSource;
    uintptr_t dataStart = dataHead;

    intFileHandles = (HANDLE*)dataHead;

    int handleSize = numberOfOpenFiles;

    dataHead += handleSize * sizeof(HANDLE);

    freeList = (MPMCQueueData*)dataHead;

    for (int i = 0; i < handleSize; i++)
    {
        intFileHandles[i] = INVALID_HANDLE_VALUE;
        freeList[i].currentSequence.store(i, std::memory_order_relaxed);
    }

    maxFreeListEntry = handleSize;

    return OS_FILE_SUCCESS;
}

int OSCreateFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    char pathscratch[MAX_PATH];

    if (nameLength <= 0 || nameLength >= MAX_PATH)
    {
        return OS_FILE_INVALID_ARGUMENT;
    }

    int internalHandlePtr = FindFreeIndex();

    if (internalHandlePtr < 0)
    {
        return OS_FILE_HANDLE_EXHASUTED;
    }

    HANDLE hFile;
    DWORD fileShare = 0, creationFlags = 0;
    DWORD hAccess = ConvertOSFlags(flags, &fileShare, &creationFlags);

    memcpy(pathscratch, filename, nameLength);
    pathscratch[nameLength] = '\0';

    hFile = CreateFileA(pathscratch, hAccess, fileShare, NULL, creationFlags, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ReturnIndex(internalHandlePtr);
        return OS_FILE_FAILED_CREATE;
    }

    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;

    intFileHandles[internalHandlePtr] = hFile;

    fileHandle->osDataHandle = internalHandlePtr;

    return OS_FILE_SUCCESS;
}

int OSOpenFile(const char* filename, int nameLength, OSFileFlags flags, OSFileHandle* fileHandle)
{
    char pathscratch[MAX_PATH];

    if (nameLength <= 0 || nameLength >= MAX_PATH)
    {
        return OS_FILE_INVALID_ARGUMENT;
    }

    int internalHandlePtr = FindFreeIndex();

    if (internalHandlePtr < 0)
    {
        return OS_FILE_HANDLE_EXHASUTED;
    }

    HANDLE hFile;
    DWORD fileShare = 0, creationFlags = 0;
    DWORD hAccess = ConvertOSFlags(flags, &fileShare, &creationFlags);

    memcpy(pathscratch, filename, nameLength);
    pathscratch[nameLength] = '\0';

    hFile = CreateFileA(pathscratch, hAccess, fileShare, NULL, creationFlags, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        ReturnIndex(internalHandlePtr);
        return OS_FILE_FAILED_CREATE;
    }

    LARGE_INTEGER fileSize;

    BOOL retVal = GetFileSizeEx(hFile, &fileSize);

    if (!retVal)
    {
        CloseHandle(hFile);
        ReturnIndex(internalHandlePtr);
        return OS_FILE_FAILED_SIZE;
    }

    fileHandle->fileLength = (uint64_t)fileSize.QuadPart;
    fileHandle->filePointer = 0;

    
    intFileHandles[internalHandlePtr] = hFile;

    fileHandle->osDataHandle = internalHandlePtr;

    return OS_FILE_SUCCESS;
}

int OSCloseFile(OSFileHandle* fileHandle)
{
    int fileIndex = fileHandle->osDataHandle;

    if (fileIndex < 0 || fileIndex >= maxFreeListEntry)
    {
        return OS_FILE_HANDLE_OUT_OF_BOUNDS;
    }
        
    HANDLE hFile = intFileHandles[fileIndex];

    int retCode = 0;

    if (!CloseHandle(hFile))
    {
        retCode = OS_FILE_FAILED_CLOSE;
    }

    ReturnIndex(fileHandle->osDataHandle);

    fileHandle->osDataHandle = -1;
    fileHandle->fileLength = 0;
    fileHandle->filePointer = 0;

    return retCode;
}

int64_t OSReadFile(OSFileHandle* fileHandle, int size, char* buffer)
{
    int fileIndex = fileHandle->osDataHandle;

    if (fileIndex < 0 || fileIndex >= maxFreeListEntry+1)
    {
        return OS_FILE_HANDLE_OUT_OF_BOUNDS;
    }

    HANDLE hFile = INVALID_HANDLE_VALUE;

    if (fileHandle->osDataHandle == maxFreeListEntry)
    {
        hFile = stdInputHandle;
    }
    else if (fileHandle->osDataHandle < maxFreeListEntry)
    {
        hFile = intFileHandles[fileIndex];
    }

    DWORD hBytesRead = 0;

    if (ReadFile(hFile, buffer, size, &hBytesRead, NULL) == FALSE)
    {
        return OS_FILE_FAILED_READ;
    }

    fileHandle->filePointer += hBytesRead;

    return hBytesRead;
}

int64_t OSWriteFile(OSFileHandle* fileHandle, int size, const char* buffer)
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
        return OS_FILE_FAILED_WRITE;
    }

    DWORD hBytesWrite = 0;

    if (WriteFile(hFile, buffer, size, &hBytesWrite, NULL) == FALSE)
    {
        return OS_FILE_FAILED_WRITE;
    }

    fileHandle->filePointer += hBytesWrite;

    return hBytesWrite;
}

int OSSeekFile(OSFileHandle* fileHandle, size_t pointer, OSRelativeFlags flags)
{
    int fileIndex = fileHandle->osDataHandle;

    HANDLE hFile;

    if (fileHandle->osDataHandle == maxFreeListEntry + 1)
    {
        hFile = stdErrorHandle;
    }
    else if (fileHandle->osDataHandle < maxFreeListEntry && fileHandle->osDataHandle >= 0)
    {
        hFile = intFileHandles[fileHandle->osDataHandle];
    }
    else
    {
        return OS_FILE_FAILED_SEEK;
    }

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
        return OS_FILE_INVALID_ARGUMENT;
    }

    LARGE_INTEGER winSeekPointer, setSeekPointer;

    winSeekPointer.QuadPart = pointer;

    BOOL wRet = SetFilePointerEx(hFile, winSeekPointer, &setSeekPointer, moveMethod);

    if (!wRet)
    {
        return OS_FILE_FAILED_SEEK;
    }

    fileHandle->filePointer = setSeekPointer.QuadPart;

    return OS_FILE_SUCCESS;
}

int OSCreateFileIterator(const char* searchString, int nameLength, OSFileIterator* iterator)
{
    char pathscratch[MAX_PATH];

    if (!searchString || !iterator || nameLength <= 0 || nameLength >= MAX_PATH)
    {
        return OS_FILE_INVALID_ARGUMENT;
    }

    int index = FindFreeIndex();

    if (index < 0)
    {
        return OS_FILE_HANDLE_EXHASUTED;
    }

    WIN32_FIND_DATAA data;

    memcpy(pathscratch, searchString, nameLength);
    pathscratch[nameLength] = '\0';

    HANDLE searchIdx = FindFirstFileA(pathscratch, &data);

    if (searchIdx == INVALID_HANDLE_VALUE)
    {
        ReturnIndex(index);
        return OS_FILE_FAILED_SEARCH_ITER;
    }

    intFileHandles[index] = searchIdx;

    strncpy(iterator->currentFileName, data.cFileName, 250);
    iterator->osDataHandle = index;

    return OS_FILE_SUCCESS;
}

int OSNextFile(OSFileIterator* iterator)
{
    if (!iterator) return OS_FILE_INVALID_ARGUMENT;

    int fileIndex = iterator->osDataHandle;

    if (fileIndex < 0 || fileIndex >= maxFreeListEntry + 1)
    {
        return OS_FILE_HANDLE_OUT_OF_BOUNDS;
    }

    WIN32_FIND_DATAA data;

    BOOL ret = FindNextFileA(intFileHandles[fileIndex], &data);

    if (!ret)
    {
        CloseHandle(intFileHandles[fileIndex]);
        ReturnIndex(fileIndex);
        return OS_FILE_REACH_ITER_END;
    }

    strncpy(iterator->currentFileName, data.cFileName, MAX_PATH);

    return OS_FILE_SUCCESS;
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
        return OS_FILE_HANDLE_OUT_OF_BOUNDS;
    }

    DWORD ret = WaitForSingleObject(hFile, millisecondTimeOut);

    if (ret == WAIT_TIMEOUT)
    {
        return OS_FILE_POLL_TIMEOUT;
    }

    return OS_FILE_SUCCESS;
}

int OSCreateDirectory(const char* directoryPath, int charCount, OSDirectoryFlag directoryFlag)
{
    if (charCount <= 0 || charCount >= MAX_PATH)
    {
        return OS_FILE_INVALID_ARGUMENT;
    }

    char pathscratch[MAX_PATH];

    memcpy(pathscratch, directoryPath, charCount);

    pathscratch[charCount] = '\0';

    SECURITY_DESCRIPTOR securityDescriptor;

    SECURITY_ATTRIBUTES attributes;

    InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION);

    BOOL daclRet = FALSE;

    if (directoryFlag == OSDirectoryFlags::PUBLIC_DIR)
    {
        daclRet = SetSecurityDescriptorDacl(&securityDescriptor, TRUE, NULL, FALSE);
    }
    else if (directoryFlag == OSDirectoryFlags::PRIVATE_DIR)
    {

    }

    if (daclRet == FALSE)
    {
        return OS_FILE_FAILED_CREATE_DIRECTORY;
    }

    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.lpSecurityDescriptor = &securityDescriptor;
    attributes.bInheritHandle = FALSE;

    BOOL dirRet = CreateDirectory(pathscratch, &attributes);

    if (dirRet == FALSE)
    {
        return OS_FILE_FAILED_CREATE_DIRECTORY;
    }

    return OS_FILE_SUCCESS;
}

int OSGetCurrentDirectorySize()
{
   return GetCurrentDirectory(0, NULL);
}

int OSGetCurrentDirectory(int bufferSize, char* outputBuffer)
{
    return GetCurrentDirectory(bufferSize, outputBuffer);
}

int OSSetCurrentDirectory(const char* inputPath, int charCount)
{
    if (charCount <= 0 || charCount >= MAX_PATH)
    {
        return OS_FILE_INVALID_ARGUMENT;
    }

    char pathscratch[MAX_PATH];

    memcpy(pathscratch, inputPath, charCount);

    pathscratch[charCount] = '\0';

    BOOL setCurrentDirectoryRet = SetCurrentDirectory(pathscratch);

    if (setCurrentDirectoryRet == FALSE)
    {
        return OS_FILE_FAILED_SET_CURRENT_DIRECTORY;
    }

    return OS_FILE_SUCCESS;
}

int OSExtractFileName(const char* inputFilePath, int inputFilePathCount, char* outputBuffer)
{
    if (inputFilePathCount <= 0 || inputFilePathCount >= MAX_PATH)
    {
        return OS_FILE_INVALID_ARGUMENT;
    }

    char filePathTerminator1 = '\\';
    char filePathTerminator2 = '/';

    int searchForFileTerminatorIter = inputFilePathCount-1;

    while (searchForFileTerminatorIter >= 0)
    {
        if (inputFilePath[searchForFileTerminatorIter] == filePathTerminator2 || inputFilePath[searchForFileTerminatorIter] == filePathTerminator1)
            break;
     
        searchForFileTerminatorIter--;
    }

    int searchForFileExtensionBeginIter = inputFilePathCount;

    while (searchForFileExtensionBeginIter >= 0)
    {
        if (inputFilePath[searchForFileExtensionBeginIter] == '.')
            break;

        searchForFileExtensionBeginIter--;
    }

    if (searchForFileExtensionBeginIter < 0)
        return -1;

    int fileNameSize = (searchForFileExtensionBeginIter - searchForFileTerminatorIter) - 1;

    memcpy(outputBuffer, inputFilePath + searchForFileTerminatorIter + 1, fileNameSize);
    
    return fileNameSize;
}

int OSGetSystemFileTerminator()
{
    return '\\';
}

int OSFileExist(const char* inputFile, int charCount, OSFileFlags flags)
{
    char pathscratch[MAX_PATH];

    if (charCount <= 0 || charCount >= MAX_PATH)
    {
        return OS_FILE_INVALID_ARGUMENT;
    }

    DWORD fileShare = 0, creationFlags = 0;
    DWORD hAccess = ConvertOSFlags(flags, &fileShare, &creationFlags);

    memcpy(pathscratch, inputFile, charCount);
    pathscratch[charCount] = '\0';

    HANDLE hFile = CreateFileA(pathscratch, hAccess, fileShare, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return OS_FILE_FAILED_EXISTING;
    }

    CloseHandle(hFile);

    return OS_FILE_SUCCESS;
}