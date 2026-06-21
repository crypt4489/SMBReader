#pragma once

#include "allocator/AppAllocator.h"
#include "SMBFile.h"

void ExportChunksFromFile(SMBFile* smb, Allocator* inputScratchMemory);
void ExportTextureFromFile(SMBFile* smb, SMBChunk& chunk, Allocator* inputScratchMemory);


