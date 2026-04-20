#pragma once

#include "OSFile.h"
#include "StringUtils.h"
#include <cstdint>

enum LogMessageType
{
	LOGERROR = 1,
	LOGWARNING = 2,
	LOGINFO = 3,
};

struct LogMessage
{
	int charCount;
	LogMessageType logType;
	int allocSize;

	char* GetString()
	{
		return (char*)(this + 1);
	};
};

struct Logger
{
	uint64_t size;
	uint64_t head, tail;
	char* structuredLogBuffer;
	OSFileHandle fileHandle;

	Logger(OSFileHandle _handle, char* _buffer, size_t _bufferSize);
	
	Logger(char* _buffer, size_t _bufferSize);

	void AddLogMessage(LogMessageType type, const char* format, int charCount);

	void AddLogMessage(LogMessageType type, const StringView& stringView);

	int ProcessMessage();

	void* Allocate(size_t allocSize, uint64_t allocAlignment);

};

