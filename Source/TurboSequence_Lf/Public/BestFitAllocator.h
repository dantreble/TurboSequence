#pragma once

#include "CoreMinimal.h"

template <int32 BlockSize, int32 TotalSize>
class TBestFitAllocator
{
public:
    TBestFitAllocator()
    {
        static_assert(BlockSize > 0, "BlockSize must be greater than zero.");
        static_assert(TotalSize >= BlockSize, "TotalSize must be at least as large as BlockSize.");
    
        // Initially, the entire memory pool is free
        AddFreeBlock(0, TotalSize);
    }

    // Allocate a block of memory
    int32 Allocate(const int32 Size)
    {
        if (Size == 0 || Size > TotalSize)
        {
            //UE_LOG(LogBestFitAllocator, Error, TEXT("Invalid allocation size requested."));
            return -1;
        }

        // Align the requested size to the block size
        const int32 AlignedSize = FMath::DivideAndRoundUp(Size, BlockSize) * BlockSize;

        // Find the best-fit free block
        int32 BestFitSize = INT32_MAX;
        int32 BestFitIndex = -1;

        for (const auto& Pair : FreeBlocks)
        {
            const int32 FreeSize = Pair.Key;

            if (FreeSize >= AlignedSize && FreeSize < BestFitSize)
            {
                BestFitSize = FreeSize;
                BestFitIndex = Pair.Value[0]; // Take the first available block
                if (BestFitSize == AlignedSize)
                {
                    break; // Exact fit found
                }
            }
        }

        if (BestFitIndex == -1)
        {
            //UE_LOG(LogBestFitAllocator, Error, TEXT("No suitable free block found for size %d."), Size);
            return -1;
        }

        // Allocate the block
        RemoveFreeBlock(BestFitIndex, BestFitSize);

        // If there's leftover space, add it back as a free block
        if (BestFitSize > AlignedSize)
        {
            const int32 NewFreeIndex = BestFitIndex + AlignedSize;
            const int32 NewFreeSize = BestFitSize - AlignedSize;
            AddFreeBlock(NewFreeIndex, NewFreeSize);
        }

        //UE_LOG(LogBestFitAllocator, Log, TEXT("Allocated %d bytes at index %d."), AlignedSize, BestFitIndex);
        return BestFitIndex;
    };

    // Free a block of memory
    void Free(const int32 Index, const int32 Size)
    {
        if (Index < 0 || Index >= TotalSize)
        {
            //UE_LOG(LogBestFitAllocator, Error, TEXT("Invalid index provided for free operation."));
            return;
        }

        const int32 AlignedSize = FMath::DivideAndRoundUp(Size, BlockSize) * BlockSize;

        // Add the block back to the free list
        AddFreeBlock(Index, AlignedSize);

        // Optionally, you can implement coalescing of adjacent free blocks here

        //UE_LOG(LogBestFitAllocator, Log, TEXT("Freed %d bytes at index %d."), AlignedSize, Index);
    };

private:
    // Free block list: maps block sizes to lists of free block start indices
    TMap<int32, TArray<int32>> FreeBlocks;

    // Helper functions
    void AddFreeBlock(const int32 Index, const int32 Size)
    {
        // Add the index to the list of free blocks of this size
        FreeBlocks.FindOrAdd(Size).Add(Index);
    }

    void RemoveFreeBlock(const int32 Index, const int32 Size)
    {
        if (TArray<int32>* FreeList = FreeBlocks.Find(Size))
        {
            FreeList->Remove(Index);
            if (FreeList->Num() == 0)
            {
                FreeBlocks.Remove(Size);
            }
        };
    }
};
