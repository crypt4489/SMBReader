#include "TLSFAllocator.h"

#include <string.h>
#include <cassert>
#include <stdio.h>
#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define ALIGNMENT_BLOCK_IDENTIFIER (uintptr_t)0xBADDBEFF

#define SET_BLOCK_HEADER_FREE_FLAG(x) (x) |= 1
#define CLEAR_BLOCK_HEADER_FREE_FLAG(x) (x) &= 0xFFFFFFFE
#define SET_BLOCK_HEADER_LAST_BLOCK_FLAG(x) ((x) |= 2)
#define CLEAR_BLOCK_HEADER_LAST_BLOCK_FLAG(x) ((x) &= 0xFFFFFFFD)
#define SET_BLOCK_HEADER_SIZE_ENCODING(bitfield, size) ((bitfield) |= (((size) & 0x3FFFFFFF) << 2))
#define CLEAR_BLOCK_HEADER_SIZE_ENCODING(bitfield) ((bitfield) &= 3)

#define GET_BLOCK_HEADER_FREE_FLAG(x) (x & 1)
#define GET_BLOCK_HEADER_LAST_BLOCK_FLAG(x) ((x >> 1) & 1)
#define GET_BLOCK_HEADER_SIZE_ENCODING(x) ((x & 0xFFFFFFFC) >> 2)

#define MIN_TLSF_BLOCK_SIZE_IN_BYTES 32
#define MIN_TLSF_BLOCK_SIZE_MSB 5
#define MIN_TLSF_ALLOCATION_SIZE (MIN_TLSF_BLOCK_SIZE_IN_BYTES << 1)

#define MIN_TLSF_SUPPORT_DATA_SIZE 1024

static_assert(sizeof(BlockHeader) == MIN_TLSF_BLOCK_SIZE_IN_BYTES);

static void SegregateList(TLSFMain* tlsf_struct, unsigned int size, unsigned int* fli, unsigned int* sli);
static void Insert(TLSFMain* tlsf_struct, BlockHeader* block, unsigned int fli, unsigned int sli);
static int findLSB(unsigned int input);
static int findMSB(unsigned int input);
static BlockHeader* FindSuitableBlock(TLSFMain* tlsf_struct, unsigned int fli, unsigned int sli);
static void RemoveNodeFromFreeList(TLSFMain* tlsf_struct, BlockHeader* node);
static BlockHeader* Split(BlockHeader* initial, unsigned int oldSize, unsigned int requestedSize);
static void ComputeBlockChecksum(BlockHeader* block);
static int ValidateCheckSum(BlockHeader* block);

int TLSFInitialize(TLSFMain* tlsf_struct, void* mem, unsigned int memSize, int secondLevelIndex)
{
	uintptr_t memStart;
	uintptr_t memHead = (uintptr_t)mem;
	memStart = memHead;
	tlsf_struct->memPool = mem;

	int fli = findMSB(memSize);

	if (fli < 0)
		return -1;

	fli = MIN(fli, 30);

	unsigned int minimumBlockSize = MIN_TLSF_BLOCK_SIZE_IN_BYTES;
	unsigned int minimumBlockBits = findMSB(minimumBlockSize);
	tlsf_struct->numberofFLILevelBitmaps = (fli - minimumBlockBits) + 1;
	tlsf_struct->numberofBlockPerFLILevel = (1 << secondLevelIndex);
	tlsf_struct->sliBits = (secondLevelIndex);
	tlsf_struct->fliBitmap = 0;
	tlsf_struct->totalMemPoolSize = memSize;

	unsigned int sliBitmapsAlignmentMakeup = (unsigned int)(((memHead + (alignof(unsigned int) - 1)) & ~(alignof(unsigned int) - 1)) - memHead);

	unsigned int totalSLIBitMapSize = sizeof(unsigned int) * tlsf_struct->numberofFLILevelBitmaps;

	unsigned int freeListsAlignmentMakeup = (unsigned int)((((memHead + totalSLIBitMapSize + sliBitmapsAlignmentMakeup) + (alignof(BlockHeader*) - 1)) & ~(alignof(BlockHeader*) - 1)) - ((memHead + totalSLIBitMapSize + sliBitmapsAlignmentMakeup)));

	unsigned int totalSizeOfFreeLists = sizeof(BlockHeader*) * tlsf_struct->numberofBlockPerFLILevel * tlsf_struct->numberofFLILevelBitmaps;

	if (memSize - (totalSizeOfFreeLists + freeListsAlignmentMakeup + totalSLIBitMapSize + sliBitmapsAlignmentMakeup) <= MIN_TLSF_SUPPORT_DATA_SIZE)
		return -1;

	tlsf_struct->sliBitMaps = (unsigned int*)(memHead + sliBitmapsAlignmentMakeup);

	memset(tlsf_struct->sliBitMaps, 0, totalSLIBitMapSize);

	memHead += (totalSLIBitMapSize + sliBitmapsAlignmentMakeup);

	tlsf_struct->freeListPerBucket = (BlockHeader**)(memHead + freeListsAlignmentMakeup);

	memset(tlsf_struct->freeListPerBucket, 0, totalSizeOfFreeLists);

	memHead += (totalSizeOfFreeLists + freeListsAlignmentMakeup);

	memHead = (memHead + MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1) & ~(MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1);

	unsigned int dataUsedSize = (unsigned int)(memHead - memStart);

	unsigned int insertSize = memSize - dataUsedSize;

	BlockHeader* initial = (BlockHeader*)memHead;

	tlsf_struct->firstBlock = initial;

	initial->size = 0;
	initial->prevPhysBlock = nullptr;

	unsigned int ifli = 0, isli = 0;

	SET_BLOCK_HEADER_SIZE_ENCODING(initial->size, insertSize);
	SET_BLOCK_HEADER_FREE_FLAG(initial->size);
	SET_BLOCK_HEADER_LAST_BLOCK_FLAG(initial->size);

	ComputeBlockChecksum(initial);

	SegregateList(tlsf_struct, insertSize, &ifli, &isli);

	Insert(tlsf_struct, initial, ifli, isli);

	return insertSize;
}

void* TLSFAllocate(TLSFMain* tlsf_struct, unsigned int size)
{
	unsigned int fli = 0, sli = 0, fli2 = 0, sli2 = 0;
	BlockHeader* found, * remaining;

	size = (((size + sizeof(BlockHeader)) + (MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1)) & ~(MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1));

	SegregateList(tlsf_struct, size, &fli, &sli);

	found = FindSuitableBlock(tlsf_struct, fli, sli);

	if (!found) return found;

	RemoveNodeFromFreeList(tlsf_struct, found);

	unsigned int originalSize = found->size;

	if (GET_BLOCK_HEADER_SIZE_ENCODING(originalSize) > (size + MIN_TLSF_ALLOCATION_SIZE))
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
			ComputeBlockChecksum(nextPhysBlock);
		}

		ComputeBlockChecksum(remaining);
	}
	else 
	{
		size = GET_BLOCK_HEADER_SIZE_ENCODING(originalSize);
	}

	CLEAR_BLOCK_HEADER_SIZE_ENCODING(found->size);

	SET_BLOCK_HEADER_SIZE_ENCODING(found->size, size);

	CLEAR_BLOCK_HEADER_FREE_FLAG(found->size);

	ComputeBlockChecksum(found);

	return found + 1;
}

void* TLSFRealloc(TLSFMain* tlsf_struct, void* memaddress, unsigned int requestedSize)
{
	unsigned int fli = 0, sli = 0, fli2 = 0, sli2 = 0;

	unsigned int possibleAlignment = 0, strideSize = 0, originalSizeRequest = requestedSize;

	BlockHeader* header = ((BlockHeader*)memaddress) - 1;

	if (header->prevPhysBlock == (BlockHeader*)ALIGNMENT_BLOCK_IDENTIFIER)
	{
		strideSize = GET_BLOCK_HEADER_SIZE_ENCODING(header->size);
		header = header - strideSize;
		possibleAlignment = sizeof(BlockHeader) * (1 + strideSize);
	}

	requestedSize = (((requestedSize + sizeof(BlockHeader)) + (MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1)) & ~(MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1));

	requestedSize += (sizeof(BlockHeader) * strideSize);

	if (GET_BLOCK_HEADER_FREE_FLAG(header->size))
		return nullptr;

	if (!ValidateCheckSum(header))
		return nullptr;

	if (GET_BLOCK_HEADER_SIZE_ENCODING(header->size) == requestedSize)
		return memaddress;

	uintptr_t addrPtrT = (uintptr_t)header;

	unsigned int currentBlockSize = GET_BLOCK_HEADER_SIZE_ENCODING(header->size);

	unsigned int isCurrentBlockLastBlock = GET_BLOCK_HEADER_LAST_BLOCK_FLAG(header->size);

	BlockHeader* nextBlock = nullptr;

	BlockHeader* blockAfterNext = nullptr;

	unsigned int nextIsLastSetBlock = 0;

	if (!isCurrentBlockLastBlock)
	{
		nextBlock = (BlockHeader*)(addrPtrT + currentBlockSize);

		nextIsLastSetBlock = GET_BLOCK_HEADER_LAST_BLOCK_FLAG(nextBlock->size);

		if (!nextIsLastSetBlock)
			blockAfterNext = (BlockHeader*)((uintptr_t)nextBlock + GET_BLOCK_HEADER_SIZE_ENCODING(nextBlock->size));
	}

	if (GET_BLOCK_HEADER_SIZE_ENCODING(header->size) > (requestedSize + MIN_TLSF_ALLOCATION_SIZE))
	{
		int smallBlockSize = GET_BLOCK_HEADER_SIZE_ENCODING(header->size);

		BlockHeader* remaining = nullptr;

		if (nextBlock)
		{
			if (GET_BLOCK_HEADER_FREE_FLAG(nextBlock->size))
			{
				smallBlockSize += GET_BLOCK_HEADER_SIZE_ENCODING(nextBlock->size);

				RemoveNodeFromFreeList(tlsf_struct, nextBlock);

				remaining = Split(header, smallBlockSize, requestedSize);

				if (blockAfterNext)
				{
					blockAfterNext->prevPhysBlock = remaining;
					ComputeBlockChecksum(blockAfterNext);
				}

				if (nextIsLastSetBlock)
					SET_BLOCK_HEADER_LAST_BLOCK_FLAG(remaining->size);
			}
			else
			{
				remaining = Split(header, smallBlockSize, requestedSize);

				nextBlock->prevPhysBlock = remaining;

				ComputeBlockChecksum(nextBlock);
			}
		}
		else
		{
			remaining = Split(header, smallBlockSize, requestedSize);

			SET_BLOCK_HEADER_LAST_BLOCK_FLAG(remaining->size);
		}

		SET_BLOCK_HEADER_FREE_FLAG(remaining->size);

		remaining->prevPhysBlock = header;
		SegregateList(tlsf_struct, GET_BLOCK_HEADER_SIZE_ENCODING(remaining->size), &fli2, &sli2);
		Insert(tlsf_struct, remaining, fli2, sli2);

		ComputeBlockChecksum(remaining);

		CLEAR_BLOCK_HEADER_SIZE_ENCODING(header->size);
		CLEAR_BLOCK_HEADER_LAST_BLOCK_FLAG(header->size);

		SET_BLOCK_HEADER_SIZE_ENCODING(header->size, requestedSize);

		ComputeBlockChecksum(header);

		return memaddress;
	}

	if (nextBlock)
	{
		if (GET_BLOCK_HEADER_FREE_FLAG(nextBlock->size))
		{
			int nextBlockOriginalEncoding = nextBlock->size;

			int nextBlockSize = GET_BLOCK_HEADER_SIZE_ENCODING(nextBlockOriginalEncoding);

			unsigned int potentialNextCurrBlockSize = (nextBlockSize + currentBlockSize);

			if (potentialNextCurrBlockSize >= (requestedSize + MIN_TLSF_ALLOCATION_SIZE))
			{
				BlockHeader* forBlockAfterNext = header;

				RemoveNodeFromFreeList(tlsf_struct, nextBlock);

				if (potentialNextCurrBlockSize > (requestedSize + MIN_TLSF_ALLOCATION_SIZE))
				{
					BlockHeader* remaining = Split(header, potentialNextCurrBlockSize, requestedSize);

					forBlockAfterNext = remaining;

					SET_BLOCK_HEADER_FREE_FLAG(remaining->size);
					remaining->prevPhysBlock = header;
					SegregateList(tlsf_struct, GET_BLOCK_HEADER_SIZE_ENCODING(remaining->size), &fli2, &sli2);
					Insert(tlsf_struct, remaining, fli2, sli2);

					if (nextIsLastSetBlock)
						SET_BLOCK_HEADER_LAST_BLOCK_FLAG(remaining->size);

					ComputeBlockChecksum(remaining);
				}
				else
				{
					if (nextIsLastSetBlock)
						SET_BLOCK_HEADER_LAST_BLOCK_FLAG(header->size);
				}

				if (blockAfterNext)
				{
					blockAfterNext->prevPhysBlock = forBlockAfterNext;
					ComputeBlockChecksum(blockAfterNext);
				}

				CLEAR_BLOCK_HEADER_SIZE_ENCODING(header->size);

				SET_BLOCK_HEADER_SIZE_ENCODING(header->size, requestedSize);

				ComputeBlockChecksum(header);

				return memaddress;
			}
		}
	}

	TLSFFree(tlsf_struct, memaddress);

	void* retAddr = nullptr;

	if (possibleAlignment)
		retAddr = TLSFAllocate(tlsf_struct, originalSizeRequest, possibleAlignment);
	else
		retAddr = TLSFAllocate(tlsf_struct, originalSizeRequest);

	memcpy(retAddr, memaddress, currentBlockSize);

	return retAddr;
}

void* TLSFAllocate(TLSFMain* tlsf_struct, unsigned int size, unsigned int alignment)
{
	unsigned int fli = 0, sli = 0, fli2 = 0, sli2 = 0;
	BlockHeader* found, * remaining, *nextPhysBlock;

	unsigned int alignmentBlockStride = (MAX(alignment, sizeof(BlockHeader)) - sizeof(BlockHeader)) >> MIN_TLSF_BLOCK_SIZE_MSB;

	size = (((size + sizeof(BlockHeader)) + (MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1)) & ~(MIN_TLSF_BLOCK_SIZE_IN_BYTES - 1));

	size += (sizeof(BlockHeader) * alignmentBlockStride);

	SegregateList(tlsf_struct, size, &fli, &sli);

	found = FindSuitableBlock(tlsf_struct, fli, sli);

	if (!found) return found;

	unsigned int originalSize = found->size;

	RemoveNodeFromFreeList(tlsf_struct, found);

	if (GET_BLOCK_HEADER_SIZE_ENCODING(originalSize) > (size + MIN_TLSF_ALLOCATION_SIZE))
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
			nextPhysBlock = (BlockHeader*)((uintptr_t)found + GET_BLOCK_HEADER_SIZE_ENCODING(originalSize));
			nextPhysBlock->prevPhysBlock = remaining;
			ComputeBlockChecksum(nextPhysBlock);
		}

		ComputeBlockChecksum(remaining);
	}
	else if (GET_BLOCK_HEADER_SIZE_ENCODING(originalSize) <= (size + MIN_TLSF_ALLOCATION_SIZE))
	{
		size = GET_BLOCK_HEADER_SIZE_ENCODING(originalSize);
	}

	CLEAR_BLOCK_HEADER_SIZE_ENCODING(found->size);

	SET_BLOCK_HEADER_SIZE_ENCODING(found->size, size);

	CLEAR_BLOCK_HEADER_FREE_FLAG(found->size);

	ComputeBlockChecksum(found);

	if (alignmentBlockStride)
	{
		BlockHeader* alignmentBlock = (found + 1 + alignmentBlockStride - 1);
		alignmentBlock->prevPhysBlock = (BlockHeader*)ALIGNMENT_BLOCK_IDENTIFIER;
		alignmentBlock->size = 0;
		SET_BLOCK_HEADER_SIZE_ENCODING(alignmentBlock->size, alignmentBlockStride);
	}

	return found + (1 + alignmentBlockStride);
}

void TLSFFree(TLSFMain* tlsf_struct, void* address)
{
	if (!address)
		return;

	BlockHeader* header = ((BlockHeader*)address) - 1;

	if (header->prevPhysBlock == (BlockHeader*)ALIGNMENT_BLOCK_IDENTIFIER)
	{
		unsigned int strideSize = GET_BLOCK_HEADER_SIZE_ENCODING(header->size);
		header = header - strideSize;
	}

	if (GET_BLOCK_HEADER_FREE_FLAG(header->size))
		return;

	if (!ValidateCheckSum(header))
		return;

	uintptr_t addrPtrT = (uintptr_t)header;

	unsigned int freeBlockSize = GET_BLOCK_HEADER_SIZE_ENCODING(header->size);

	unsigned int isCurrentBlockLastBlock = GET_BLOCK_HEADER_LAST_BLOCK_FLAG(header->size);

	BlockHeader* prevBlock = header->prevPhysBlock;

	BlockHeader* nextBlock = nullptr;

	BlockHeader* blockAfterNext = nullptr;

	unsigned int nextIsLastSetBlock = 0;

	SET_BLOCK_HEADER_FREE_FLAG(header->size);

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
			freeBlockSize += GET_BLOCK_HEADER_SIZE_ENCODING(prevBlock->size);
			header = prevBlock;
		}
	}

	if (nextBlock)
	{
		if (GET_BLOCK_HEADER_FREE_FLAG(nextBlock->size))
		{
			RemoveNodeFromFreeList(tlsf_struct, nextBlock);

			freeBlockSize += GET_BLOCK_HEADER_SIZE_ENCODING(nextBlock->size);

			if (nextIsLastSetBlock)
				SET_BLOCK_HEADER_LAST_BLOCK_FLAG(header->size);

			if (blockAfterNext)
			{
				blockAfterNext->prevPhysBlock = header;
				ComputeBlockChecksum(blockAfterNext);
			}
		}
		else
		{
			nextBlock->prevPhysBlock = header;
			ComputeBlockChecksum(nextBlock);
		}
	}

	CLEAR_BLOCK_HEADER_SIZE_ENCODING(header->size);

	SET_BLOCK_HEADER_SIZE_ENCODING(header->size, freeBlockSize);

	ComputeBlockChecksum(header);

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
		header->prevFree = block;

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

	unsigned int mask = (~0u << (sli));
	unsigned int sl_map = tlsf_struct->sliBitMaps[fli_normalized] & (mask);

	int next_fli = fli_normalized;
	int next_sli = findLSB(sl_map);

	if (next_sli < 0) {
		unsigned int fl_map = (tlsf_struct->fliBitmap & (~0u << (fli_normalized + 1)));
		next_fli = findLSB(fl_map);
		if (next_fli >= 0)
			next_sli = findLSB(tlsf_struct->sliBitMaps[next_fli]);
	}

	if (next_fli >= 0 && next_sli >= 0)
	{
		BlockHeader* returnHeader = tlsf_struct->freeListPerBucket[next_fli * tlsf_struct->numberofBlockPerFLILevel + next_sli];
	
		return returnHeader;
	}

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

static int monotonicCounter = 0;
void ComputeBlockChecksum(BlockHeader* block) {
#ifdef _DEBUG
	unsigned int cs = block->size;
	cs ^= (unsigned int)(uintptr_t)block->prevPhysBlock;
	cs ^= cs >> 16;
	cs *= 0x45d9f3b;
	cs ^= cs >> 16;
	block->checksum = cs;
#endif
}

static int ValidateCheckSum(BlockHeader* block)
{
#if _DEBUG
	BlockHeader temp{};
	temp.prevPhysBlock = block->prevPhysBlock;
	temp.size = block->size;
	ComputeBlockChecksum(&temp);
	return temp.checksum == block->checksum;
#else
	return 1;
#endif
}

void ValidatePhysicalChain(TLSFMain* tlsf)
{
	BlockHeader* current = tlsf->firstBlock;// first block header
	BlockHeader* prev = nullptr;
	unsigned int totalSize = 0;
	int blockCount = 0;

	while (current)
	{
		int size = GET_BLOCK_HEADER_SIZE_ENCODING(current->size);
		int isFree = GET_BLOCK_HEADER_FREE_FLAG(current->size);
		int isLast = GET_BLOCK_HEADER_LAST_BLOCK_FLAG(current->size);

		assert(current->prevPhysBlock == prev);

		assert(ValidateCheckSum(current));

		if (prev && GET_BLOCK_HEADER_FREE_FLAG(prev->size) && isFree)
		{
			printf("COALESCE FAILURE at block %d\n", blockCount);
		}

		totalSize += size;
		blockCount++;
		prev = current;

		if (isLast) break;

		current = (BlockHeader*)((uintptr_t)current + size);
	}

	printf("Total blocks: %d, Total size: %u\n", blockCount, totalSize);
}