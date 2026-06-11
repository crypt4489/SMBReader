#pragma once

typedef struct block_header_t
{
	block_header_t* prevPhysBlock;
	block_header_t* nextFree;
	block_header_t* prevFree;
	unsigned int size; // bits 0 = Free Block Indicator, bits 1 = whether the block is the last one of the pool or no, bits 2-31 = Actual Size of Block
	int checksum;
#if !defined(_WIN64)
	int pad32[3];
#endif
} BlockHeader;

typedef struct tlsf_main_t
{
	const void* memPool;
	unsigned int* sliBitMaps; // ptr to which blocks in block list are free
	BlockHeader** freeListPerBucket;// ptr to free blocks row = fli, col = sli 
	BlockHeader* firstBlock;
	unsigned int fliBitmap;
	unsigned int numberofFLILevelBitmaps; // FLI, min(log2(memPool), 31) - log2(MIN_TLSF_BLOCK_SIZE_IN_BYTES)
	unsigned int numberofBlockPerFLILevel; // 2^SECOND LEVEL INDEX SIZE
	unsigned int sliBits;
	unsigned int totalMemPoolSize;
} TLSFMain;


int TLSFInitialize(TLSFMain* tlsf_struct, void* mem, unsigned int memSize, int secondLevelIndex);
void TLSFDestroy(TLSFMain* tlsf_struct);
void* TLSFAllocate(TLSFMain* tlsf_struct, unsigned int size);
void* TLSFAllocate(TLSFMain* tlsf_struct, unsigned int size, unsigned int alignment);
void* TLSFRealloc(TLSFMain* tlsf_struct, void* memaddress, unsigned int requestedSize);
void TLSFFree(TLSFMain* tlsf_struct, void* address);
void ValidatePhysicalChain(TLSFMain* tlsf);