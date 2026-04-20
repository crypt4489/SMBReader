#include "logger/Logger.h"
#include <bit>
Logger::Logger(OSFileHandle _handle, char* _buffer, size_t _bufferSize) :
	structuredLogBuffer(_buffer), size(_bufferSize), head(0), tail(0), fileHandle(_handle)
{
	if (_bufferSize & (_bufferSize - 1))
	{
		size = ((size_t)1 << std::bit_width(_bufferSize) - 1);
	}
}

Logger::Logger( char* _buffer, size_t _bufferSize) :
	structuredLogBuffer(_buffer), size(_bufferSize), head(0), tail(0), fileHandle({})
{
	if (_bufferSize & (_bufferSize - 1))
	{
		size = ((size_t)1 << std::bit_width(_bufferSize) - 1);
	}
}


void Logger::AddLogMessage(LogMessageType type, const char* format, int charCount)
{
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
}

void Logger::AddLogMessage(LogMessageType type, const StringView& stringView)
{
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
}

int Logger::ProcessMessage()
{
	uint64_t currentHead = head;

	uint64_t currentTail = tail;

	int messageCount = 0;

	while (currentTail > currentHead)
	{
		LogMessage* currentMessage = (LogMessage*)(structuredLogBuffer + (currentHead & (size - 1)));

		char* string = currentMessage->GetString();

		const char* prefixFormat = "[INFO]:";

		switch (currentMessage->logType)
		{
		case LOGERROR:
			prefixFormat = "[ERROR]:";
			break;
		case LOGWARNING:
			prefixFormat = "[WARNING]:";
			break;
		}

		OSWriteFile(&fileHandle, strlen(prefixFormat), (char*)prefixFormat);

		OSWriteFile(&fileHandle, currentMessage->charCount - 1, string);

		currentHead += currentMessage->allocSize;

		messageCount++;
	}

	head = currentHead;

	return messageCount;
}

void* Logger::Allocate(size_t allocSize, uint64_t allocAlignment)
{
	uint64_t out = tail;

	out = (out + allocAlignment - 1) & ~(allocAlignment - 1);

	tail += (out - tail) + allocSize;

	return (void*)(structuredLogBuffer + (out & (size - 1)));
}