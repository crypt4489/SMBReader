#include "OSFile.h"
#include "OSThread.h"
#include "OSWindow.h"
#include "StringUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile bool done = false;

void ScanSTDIN(void* argument)
{
    uint64_t readSize;

    char inputBuffer[512];

    OSFileHandle stdInHandle;

    OSGetSTDInput(&stdInHandle);

    int pollCommand = OS_FILE_POLL_TIMEOUT;

    while(!done)
    {
        pollCommand = OSPollFile(&stdInHandle, 500);

        if (pollCommand > 0)
        {
            OSReadFile(&stdInHandle, sizeof(inputBuffer), inputBuffer, &readSize);

            inputBuffer[readSize] = '\0';

            if (strcmp(inputBuffer, "end"))
            {
                break;
            }
            else
            {
                printf("%s\n", inputBuffer);
            }
        }
    }

    done = true;
}

int main(int argc, const char** argv)
{
    StringView mainInputFile = STRING_VIEW_FROM_LITERAL("test.txt");

    StringView mainOutputFile = STRING_VIEW_FROM_LITERAL("test3.txt");

    OSFileHandle inputFileHandle, outputFileHandle, stdOutHandle, stdInHandle;
/*
    int retCode = OSOpenFile(mainInputFile.stringData, mainInputFile.charCount, READ, &inputFileHandle);

    if (retCode < 0)
    {
        StringView errorText = STRING_VIEW_FROM_LITERAL("Could not open input file");
        OSWriteFile(&stdOutHandle, errorText.charCount, errorText.stringData);
        return -1;
    }

    char* inputBuffer = (char*)malloc(inputFileHandle.fileLength);

    int readCount = OSReadFile(&inputFileHandle, inputFileHandle.fileLength, inputBuffer);

    */
/*
    int retCode = OSCreateFile(mainOutputFile.stringData, mainOutputFile.charCount, WRITE|CREATE_IF_NOT_EXIST, &outputFileHandle);

    uint64_t writeOutSize = 0;

    if (retCode < 0)
    {
        StringView errorText = STRING_VIEW_FROM_LITERAL("Could not open output file\n");
        OSWriteFile(&stdOutHandle, errorText.charCount, errorText.stringData, &writeOutSize);
        return -1;
    }

    char inputBuffer[1024];

    int pollCommand = OS_FILE_POLL_TIMEOUT;

    while(pollCommand == OS_FILE_POLL_TIMEOUT)
    {
        pollCommand = OSPollFile(&stdInHandle, 500);
    }

    retCode = 0;

    if (pollCommand > 0)
    {
        uint64_t readCount = 0;

        OSReadFile(&stdInHandle, 1024, inputBuffer, &readCount);

        OSWriteFile(&outputFileHandle, readCount, inputBuffer, &writeOutSize);
    }
    else 
    {
        StringView errorText = STRING_VIEW_FROM_LITERAL("Could not poll std in file\n");
        
        OSWriteFile(&stdOutHandle, errorText.charCount, errorText.stringData, &writeOutSize);

        retCode = -1;
    }

    OSCloseFile(&outputFileHandle);
    */
    int retCode = 0;

    OSThreadMemoryRequirements threadMemRequirements = OSGetThreadMemoryRequirements(5);

    void* threadData = aligned_alloc(threadMemRequirements.alignment, threadMemRequirements.dataSize);

    OSSeedThreadMemory(threadData, threadMemRequirements.dataSize, 5);

    OSThreadHandle handle{};

    OSCreateThread(&handle, nullptr, ScanSTDIN, OS_THREAD_NONE);

    OSSeedWindowMemory(nullptr, 0, 0);

    CreateOSWindow(nullptr, 320, 240, nullptr);

    while(!done)
    {
        int ret = PollOSWindowEvents(nullptr);

        if (ret)
        {
            done = true;
        }
    }

    OSJoinThread(&handle);

    OSCloseThread(&handle);

    free(threadData);

    CloseAllWindows();

    return retCode;
}