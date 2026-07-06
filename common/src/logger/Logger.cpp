#include "logger/Logger.h"
#include <string.h>

#define SENTINEL_VALUE 0xFFFFFFFF

static size_t RoundDownPower2(size_t _inSize)
{
	size_t v = _inSize - 1;
	v |= v >> 1; v |= v >> 2; v |= v >> 4;
	v |= v >> 8; v |= v >> 16; v |= v >> 32;
	return  (v + 1) >> 1;
}



void Logger::InitLogger(char* _buffer, size_t _bufferSize) {
	structuredLogBuffer = _buffer;
	size = _bufferSize;
	head = tail = 0;
	if (_bufferSize & (_bufferSize - 1))
	{
		size = RoundDownPower2(_bufferSize);
	}
	CreateOSSharedExclusive(&writerLock);  // always init here
}

void Logger::AddLogMessage(LogMessageType type, const char* format, int charCount)
{
	ExclusiveAcquireOSSharedExclusive(&writerLock);
	
	int internalCharCount = charCount + 2;
	int align = alignof(LogMessage);
	int messageSize = sizeof(LogMessage) + ((internalCharCount + align - 1) & ~(align - 1));

	LogMessage* message = (LogMessage*)Allocate(messageSize, 1);

	message->allocSize = messageSize;
	message->charCount = internalCharCount;
	message->logType = type;

	char* outputMessage = message->GetString();

	memcpy(outputMessage, format, charCount);

	outputMessage[charCount] = '\n';
	outputMessage[charCount + 1] = '\0';

	ExclusiveReleaseOSSharedExclusive(&writerLock);
}

void Logger::AddLogMessage(LogMessageType type, const StringView& stringView)
{
	ExclusiveAcquireOSSharedExclusive(&writerLock);

	int internalCharCount = stringView.charCount + 2;
	int align = alignof(LogMessage);
	int messageSize = sizeof(LogMessage) + ((internalCharCount + align - 1) & ~(align - 1));

	LogMessage* message = (LogMessage*)Allocate(messageSize, 1);

	message->allocSize = messageSize;
	message->charCount = internalCharCount;
	message->logType = type;

	char* outputMessage = message->GetString();

	memcpy(outputMessage, stringView.stringData, stringView.charCount);

	outputMessage[stringView.charCount] = '\n';
	outputMessage[stringView.charCount + 1] = '\0';


	ExclusiveReleaseOSSharedExclusive(&writerLock);
}

int Logger::ProcessMessage()
{
	ExclusiveAcquireOSSharedExclusive(&writerLock);

	uint64_t currentHead = head;

	uint64_t currentTail = tail;

	int messageCount = 0;

	while (currentHead < currentTail)
	{
		LogMessage* currentMessage = (LogMessage*)(structuredLogBuffer + (currentHead & (size - 1)));

		if (currentMessage->charCount < 0)
		{
			currentHead = (currentHead + (size - 1)) & ~(size - 1);
			continue;
		}

		char* string = currentMessage->GetString();

		const char* prefixFormat = "[INFO]:";
		int prefixLen = 7;

		switch (currentMessage->logType)
		{
		case LOGERROR:
			prefixFormat = "[ERROR]:";
			prefixLen = 8;
			break;
		case LOGWARNING:
			prefixFormat = "[WARNING]:";
			prefixLen = 10;
			break;
		case LOGINFO:
			break;
		default:
			prefixLen = 0;
			break;
		}

		uint64_t writeOutSize;

		OSWriteFile(&fileHandle, prefixLen, (char*)prefixFormat, &writeOutSize);

		OSWriteFile(&fileHandle, currentMessage->charCount - 1, string, &writeOutSize);

		currentHead += currentMessage->allocSize;

		messageCount++;
	}

	head = currentHead;

	ExclusiveReleaseOSSharedExclusive(&writerLock);

	return messageCount;
}

void* Logger::Allocate(size_t allocSize, uint64_t allocAlignment)
{
	uint64_t out = tail;
	out = (out + allocAlignment - 1) & ~(allocAlignment - 1);

	uint64_t maskedOut = out & (size - 1);

	if ((maskedOut + allocSize) > size)
	{
		uint32_t* sentinelPtr = (uint32_t*)(structuredLogBuffer + maskedOut);

		*sentinelPtr = SENTINEL_VALUE;

		uint32_t makeUpStride = size - maskedOut;

		maskedOut = 0;
		
		out += makeUpStride;
	}

	tail = out + allocSize;

	return (void*)(structuredLogBuffer + maskedOut);
}