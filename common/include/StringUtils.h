#pragma once

struct StringView
{
	const char* stringData;
	int charCount;
};

#define STRING_VIEW_FROM_LITERAL_ARR(str) \
	 (const char*)(str), (int)(sizeof(str) - 1)

#define STRING_VIEW_FROM_LITERAL(str) \
	{ (const char*)(str), (int)(sizeof(str) - 1) }

