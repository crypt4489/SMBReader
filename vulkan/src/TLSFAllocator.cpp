#include "TLSFAllocator.h"

#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#define MIN(a, b) (a) > (b) ? (b) : (a)
#define MAX(a, b) (a) < (b) ? (b) : (a)

#define SET_BLOCK_HEADER_FREE_FLAG(x) x |= 1
#define CLEAR_BLOCK_HEADER_FREE_FLAG(x) x &= 0xFFFFFFFE
#define SET_BLOCK_HEADER_LAST_BLOCK_FLAG(x) (x |= 2)
#define CLEAR_BLOCK_HEADER_LAST_BLOCK_FLAG(x) (x &= 0xFFFFFFFD)
#define SET_BLOCK_HEADER_SIZE_ENCODING(bitfield, size) (bitfield |= ((size & 0x3FFFFFFF) << 2))
#define CLEAR_BLOCK_HEADER_SIZE_ENCODING(bitfield) (bitfield &= 3)

#define GET_BLOCK_HEADER_FREE_FLAG(x) (x & 1)
#define GET_BLOCK_HEADER_LAST_BLOCK_FLAG(x) ((x >> 1) & 1)
#define GET_BLOCK_HEADER_SIZE_ENCODING(x) ((x & 0xFFFFFFFC) >> 2)

#define MIN_TSLF_BLOCK_SIZE_IN_BYTES 32

static void SegregateList(TLSFMain* tlsf_struct, unsigned int size, unsigned int* fli, unsigned int* sli);
static void Insert(TLSFMain* tlsf_struct, BlockHeader* block, unsigned int fli, unsigned int sli);
static int findLSB(unsigned int input);
static int findMSB(unsigned int input);
static BlockHeader* FindSuitableBlock(TLSFMain* tlsf_struct, unsigned int fli, unsigned int sli);
static void RemoveNodeFromFreeList(TLSFMain* tlsf_struct, BlockHeader* node);
static BlockHeader* Split(BlockHeader* initial, unsigned int oldSize, unsigned int requestedSize);

int Initialize(TLSFMain* tlsf_struct, const void* mem, unsigned int memSize, int secondLevelIndex)
{
	uintptr_t memStart;
	uintptr_t memHead = (uintptr_t)mem;
	
	memStart = memHead;
	
	int fli = findMSB(memSize);

	if (fli < 0)
		return -1;

	fli = MIN(fli, 30);

	tlsf_struct->memPool = mem;
	tlsf_struct->minimumBlockSize = MIN_TSLF_BLOCK_SIZE_IN_BYTES;
	tlsf_struct->minimumBlockBits = findMSB(tlsf_struct->minimumBlockSize);
	tlsf_struct->numberSLIBitmaps = fli - tlsf_struct->minimumBlockBits;
	tlsf_struct->numberofBlockPerFLILevel = (1 << secondLevelIndex);
	tlsf_struct->sliBits = (secondLevelIndex);
	tlsf_struct->fliBitmap = 0;
	tlsf_struct->totalMemPoolSize = memSize;

	tlsf_struct->sliBitMaps = (unsigned int*)memHead;

	unsigned int totalSLIBitMapSize = sizeof(unsigned int) * tlsf_struct->numberSLIBitmaps;

	memset(tlsf_struct->sliBitMaps, 0, totalSLIBitMapSize);

	memHead += totalSLIBitMapSize;

	tlsf_struct->freeListPerBucket = (BlockHeader**)memHead;

	unsigned int totalSizeOfFreeLists = sizeof(BlockHeader*) * tlsf_struct->numberofBlockPerFLILevel * tlsf_struct->numberSLIBitmaps;

	memset(tlsf_struct->freeListPerBucket, 0, totalSizeOfFreeLists);

	memHead += totalSizeOfFreeLists;

	unsigned int dataUsedSize = (unsigned int)(memHead - memStart);

	unsigned int insertSize = memSize - dataUsedSize;

	BlockHeader* initial = (BlockHeader*)memHead;

	initial->size = 0;
	initial->prevPhysBlock = nullptr;

	unsigned int ifli = 0, isli = 0;

	SET_BLOCK_HEADER_SIZE_ENCODING(initial->size, insertSize);
	SET_BLOCK_HEADER_FREE_FLAG(initial->size);
	SET_BLOCK_HEADER_LAST_BLOCK_FLAG(initial->size);

	SegregateList(tlsf_struct, insertSize, &ifli, &isli);

	Insert(tlsf_struct, initial, ifli, isli);

	return insertSize;
}

void* Allocate(TLSFMain* tlsf_struct, unsigned int size)
{
	unsigned int fli = 0, sli = 0, fli2 = 0, sli2 = 0;
	BlockHeader* found, * remaining;

	size = (((size + sizeof(BlockHeader)) + (MIN_TSLF_BLOCK_SIZE_IN_BYTES - 1)) & ~(MIN_TSLF_BLOCK_SIZE_IN_BYTES - 1));

	SegregateList(tlsf_struct, size, &fli, &sli);

	found = FindSuitableBlock(tlsf_struct, fli, sli);

	if (!found) return found;

	RemoveNodeFromFreeList(tlsf_struct, found);

	unsigned int originalSize = found->size;

	if (GET_BLOCK_HEADER_SIZE_ENCODING(originalSize) > (size + MIN_TSLF_BLOCK_SIZE_IN_BYTES))
	{
		remaining = Split(found, GET_BLOCK_HEADER_SIZE_ENCODING(originalSize), size);
		SET_BLOCK_HEADER_FREE_FLAG(remaining->size);
		remaining->prevPhysBlock = found;
		SegregateList(tlsf_struct, GET_BLOCK_HEADER_SIZE_ENCODING(remaining->size), &fli2, &sli2);
		Insert(tlsf_struct, remaining, fli2, sli2);

		if (GET_BLOCK_HEADER_LAST_BLOCK_FLAG(originalSize))
		{
			SET_BLOCK_HEADER_LAST_BLOCK_FLAG(remaining->size);
			CLEAR_BLOCK_HEADER_LAST_BLOCK_FLAG(found->size);
		}
		else
		{
			BlockHeader* nextPhysBlock = (BlockHeader*)((uintptr_t)found + GET_BLOCK_HEADER_SIZE_ENCODING(originalSize));
			nextPhysBlock->prevPhysBlock = remaining;
		}

	}

	CLEAR_BLOCK_HEADER_SIZE_ENCODING(found->size);

	SET_BLOCK_HEADER_SIZE_ENCODING(found->size, size);

	CLEAR_BLOCK_HEADER_FREE_FLAG(found->size);

	return found + 1;
}

void Free(TLSFMain* tlsf_struct, void* address)
{
	BlockHeader* header = ((BlockHeader*)address) - 1;
	uintptr_t addrPtrT = (uintptr_t)header;

	unsigned int freeBlockSize = GET_BLOCK_HEADER_SIZE_ENCODING(header->size);

	unsigned int isCurrentBlockLastBlock = GET_BLOCK_HEADER_LAST_BLOCK_FLAG(header->size);

	BlockHeader* prevBlock = header->prevPhysBlock;

	BlockHeader* nextBlock = nullptr;

	BlockHeader* blockAfterNext = nullptr;

	unsigned int nextIsLastSetBlock = 0;

	if (!isCurrentBlockLastBlock)
	{
		nextBlock = (BlockHeader*)(addrPtrT + freeBlockSize);

		nextIsLastSetBlock = GET_BLOCK_HEADER_LAST_BLOCK_FLAG(nextBlock->size);

		if (!nextIsLastSetBlock)
			blockAfterNext = (BlockHeader*)((uintptr_t)nextBlock + GET_BLOCK_HEADER_SIZE_ENCODING(nextBlock->size));
	}

	if (prevBlock)
	{
		if (GET_BLOCK_HEADER_FREE_FLAG(prevBlock->size))
		{
			RemoveNodeFromFreeList(tlsf_struct, prevBlock);
			CLEAR_BLOCK_HEADER_FREE_FLAG(prevBlock->size);
			freeBlockSize += GET_BLOCK_HEADER_SIZE_ENCODING(prevBlock->size);
			header = prevBlock;
		}
	}

	if (nextBlock)
	{
		if (GET_BLOCK_HEADER_FREE_FLAG(nextBlock->size))
		{
			RemoveNodeFromFreeList(tlsf_struct, nextBlock);
			CLEAR_BLOCK_HEADER_FREE_FLAG(nextBlock->size);
			freeBlockSize += GET_BLOCK_HEADER_SIZE_ENCODING(nextBlock->size);
			if (nextIsLastSetBlock)
			{
				SET_BLOCK_HEADER_LAST_BLOCK_FLAG(header->size);
			}

			if (blockAfterNext)
				blockAfterNext->prevPhysBlock = header;

		}
	}

	CLEAR_BLOCK_HEADER_SIZE_ENCODING(header->size);

	SET_BLOCK_HEADER_SIZE_ENCODING(header->size, freeBlockSize);

	SET_BLOCK_HEADER_FREE_FLAG(header->size);

	unsigned int fli = 0, sli = 0;

	SegregateList(tlsf_struct, freeBlockSize, &fli, &sli);

	Insert(tlsf_struct, header, fli, sli);
}

void SegregateList(TLSFMain* tlsf_struct, unsigned int size, unsigned int* fli, unsigned int* sli)
{
	int input = findMSB(size);

	if (input < 0) return;

	*fli = (unsigned int)input;

	unsigned int intermediary = (size - (1ull << (unsigned int)*fli));

	*sli = intermediary >> (*fli - tlsf_struct->sliBits);
}

void Insert(TLSFMain* tlsf_struct, BlockHeader* block, unsigned int fli, unsigned int sli)
{
	int fli_normalized = fli - tlsf_struct->sliBits;

	BlockHeader* header = tlsf_struct->freeListPerBucket[(fli_normalized * tlsf_struct->numberofBlockPerFLILevel) + sli];

	block->prevFree = nullptr;
	block->nextFree = header;

	if (header)
	{
		header->prevFree = block;
	}

	tlsf_struct->freeListPerBucket[fli_normalized * tlsf_struct->numberofBlockPerFLILevel + sli] = block;

	tlsf_struct->fliBitmap |= (1 << (fli_normalized));
	tlsf_struct->sliBitMaps[fli_normalized] |= (1 << (sli));
}

int findLSB(unsigned int input)
{
	if (!input) return -1;
#ifdef _MSC_VER
	unsigned long index;
	_BitScanForward(&index, input);
	return index;
#else
	return __builtin_ctz(input);
#endif
}

int findMSB(unsigned int input)
{
	if (!input) return -1;

#ifdef _MSC_VER
	unsigned long index;
	_BitScanReverse(&index, input);
	return index;
#else
	return 31 - __builtin_clz(input);
#endif
}

BlockHeader* FindSuitableBlock(TLSFMain* tlsf_struct, unsigned int fli, unsigned int sli)
{
	unsigned int fli_normalized = fli - tlsf_struct->sliBits;

	if (tlsf_struct->freeListPerBucket[fli_normalized * tlsf_struct->numberofBlockPerFLILevel + sli])
		return tlsf_struct->freeListPerBucket[fli_normalized * tlsf_struct->numberofBlockPerFLILevel + sli];

	unsigned int sl_map = tlsf_struct->sliBitMaps[fli_normalized] & (~0 << (sli + 1));

	int next_fli = fli_normalized;
	int next_sli = findLSB(sl_map);

	if (next_sli < 0) {
		unsigned int fl_map = (tlsf_struct->fliBitmap & (~0 << (fli_normalized + 1)));
		next_fli = findLSB(fl_map);
		if (next_fli >= 0)
			next_sli = findLSB(tlsf_struct->sliBitMaps[next_fli]);
	}

	if (next_fli >= 0 && next_sli >= 0)
		return tlsf_struct->freeListPerBucket[next_fli * tlsf_struct->numberofBlockPerFLILevel + next_sli];

	return nullptr;

}

void RemoveNodeFromFreeList(TLSFMain* tlsf_struct, BlockHeader* node)
{
	if (node->nextFree)
	{
		node->nextFree->prevFree = node->prevFree;
	}

	if (node->prevFree)
	{
		node->prevFree->nextFree = node->nextFree;
	}
	else
	{
		unsigned int size = GET_BLOCK_HEADER_SIZE_ENCODING(node->size);

		unsigned int fli = 0, sli = 0;

		SegregateList(tlsf_struct, size, &fli, &sli);

		unsigned int fli_normalized = fli - tlsf_struct->sliBits;

		tlsf_struct->freeListPerBucket[fli_normalized * tlsf_struct->numberofBlockPerFLILevel + sli] = node->nextFree;

		if (!node->nextFree)
		{
			tlsf_struct->sliBitMaps[fli_normalized] &= ~(1 << sli);

			unsigned int sliMap = tlsf_struct->sliBitMaps[fli_normalized];

			if (!sliMap)
			{
				tlsf_struct->fliBitmap &= ~(1 << fli_normalized);
			}
		}
	}

	node->nextFree = nullptr;
	node->prevFree = nullptr;
}

BlockHeader* Split(BlockHeader* initial, unsigned int oldSize, unsigned int requestedSize)
{
	uintptr_t initialPtrT = (uintptr_t)initial;

	BlockHeader* ret = (BlockHeader*)(initialPtrT + requestedSize);

	ret->size = 0;

	SET_BLOCK_HEADER_SIZE_ENCODING(ret->size, oldSize - requestedSize);
	ret->nextFree = nullptr;
	ret->prevFree = nullptr;

	return ret;
}