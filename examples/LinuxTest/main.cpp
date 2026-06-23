#include "OSFile.h"
#include "StringUtils.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char** argv)
{
    StringView mainInputFile = STRING_VIEW_FROM_LITERAL("test.txt");

    StringView mainOutputFile = STRING_VIEW_FROM_LITERAL("test3.txt");

    OSFileHandle inputFileHandle, outputFileHandle, stdOutHandle, stdInHandle;

    OSGetSTDOutput(&stdOutHandle);

    OSGetSTDInput(&stdInHandle);
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

    int retCode = OSCreateFile(mainOutputFile.stringData, mainOutputFile.charCount, WRITE|CREATE_IF_NOT_EXIST, &outputFileHandle);

    if (retCode < 0)
    {
        StringView errorText = STRING_VIEW_FROM_LITERAL("Could not open output file\n");
        OSWriteFile(&stdOutHandle, errorText.charCount, errorText.stringData);
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
        int readCount = OSReadFile(&stdInHandle, 1024, inputBuffer);

        OSWriteFile(&outputFileHandle, readCount, inputBuffer);
    }
    else 
    {
        StringView errorText = STRING_VIEW_FROM_LITERAL("Could not poll std in file\n");
        
        OSWriteFile(&stdOutHandle, errorText.charCount, errorText.stringData);

        retCode = -1;
    }

    OSCloseFile(&outputFileHandle);

    return retCode;
}