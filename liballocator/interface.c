#include "api.h"
#include "my_memory.h"
#include <stdio.h>

void *buddyAllocation(int size) {
    int totalSizeForBlock = size;
    if (setUpConfig.type == MALLOC_BUDDY) {
        totalSizeForBlock += setUpConfig.header_size;
    }

    if (totalSizeForBlock < setUpConfig.min_mem_chunk_size) {
        totalSizeForBlock = setUpConfig.min_mem_chunk_size;
    } else if (totalSizeForBlock > setUpConfig.memory_size) {
        return NULL;
    }

    int neededSize = requiredSizeFor(totalSizeForBlock);
    
    int sizePower = getPowerFromSize(neededSize);
    // printf("Needed Size: %d and sizePower: %d\n",neededSize, sizePower);

    // We are Searching for smallest size to fit the block.
    int smallestBucketPower = -1;
    for (int i = sizePower; i <= maxPowerPossible; i++) {
        if (buddyConfig.bucketHeads[i] != NULL) {
            smallestBucketPower = i;
            // printf("Found smallest bucket head for %d\n", j);
            break;
        }
    }
    // printf("Smallest bucket power: %d\n", smallestBucketPower);

    if (smallestBucketPower == -1) {
        return NULL;
    }
    
    // printf("Segmentation fault might occur\n");
    // if (smallestBucketPower > maxPowerPossible) {
    //     return NULL;
    // }
    // printf("Finally segmentation fault didn't occur\n");

    BuddyBlock *leftBuddy = removeBucketHeadForAllocation(smallestBucketPower);
    // printf("Bucket popped for %d\n", j);
    int currSize = sizeArray[smallestBucketPower];

    // SPlitting the found block.
    while (smallestBucketPower > sizePower) {
        // printf("Curr power: %d\n", currPower);
        // printf("SizePower : %d\n", sizePower);

        int currSize = sizeArray[smallestBucketPower];
        int halfSize = currSize/2;

        // printf("Left buddy start: %d\n", leftBuddy->start);
        BuddyBlock *rightBuddy = createNewBuddyBlock(leftBuddy->start + halfSize, halfSize, true);
        smallestBucketPower -= 1;
        pushBlockInBucket(smallestBucketPower, rightBuddy);
        leftBuddy->size = halfSize;
    }

    leftBuddy->free = false;

    uint8_t *returnLocPtr = (uint8_t*)setUpConfig.start_of_memory + setUpConfig.header_size + leftBuddy->start;
    leftBuddy->pointingToMemBlock = returnLocPtr;

    // Storing to allocated blocks list.
    leftBuddy->next = buddyConfig.allocatedLLHead;
    buddyConfig.allocatedLLHead = leftBuddy;

    return returnLocPtr;
}

void *slabAllocation(int size) {
    if (size <= 0) return NULL;

    SlabTypeConfig *slabType = getSlabTypeForSize(size);
    if (slabType == NULL) {
        slabType = createNewSlabTypeForSize(size);
    }

    SingleSlabConfig *slab = NULL;
    
    if (slabType->emptySlabList != NULL) {
        slab = slabType->emptySlabList;
    } else {
        slab = createNewSlabForType(slabType);
    }

    if (slab == NULL) {
        // printf("Not able to create slab due to large size found in buddy allocation");
        return NULL;
    }

    int emptyIndx = -1;

    for (int i = 0; i < setUpConfig.n_objs_per_slab; i++) {
        if (slab->bitmap[i] == 0){
            emptyIndx = i;
            // printf("Emoty Index found at %d. Marking it taken\n", emptyIndx);
            break;
        }
    }

    slab->bitmap[emptyIndx] = 1;
    slab->freeSlabObjects--;
    if (slab->freeSlabObjects == 0) {
        // printf("Moved from partial to full slab list for type: %d\n", slabType->eachSlabObjSize);
        removeSlabNode(&slabType->emptySlabList, slab);
        slab->next = slabType->fullSlabList;
        slabType->fullSlabList = slab;
    }

    uint8_t *slabStartAddressBeforeHeader = (uint8_t *)slab->startAddress;
    uint8_t *returnLocPtr = slabStartAddressBeforeHeader + emptyIndx * slab->eachSlabObjSize + setUpConfig.header_size;
    return returnLocPtr;
}

void *my_malloc(int size) {
    if (setUpConfig.type == MALLOC_BUDDY) {
        return buddyAllocation(size);
    } else {
        return slabAllocation(size);
    }
}

void buddyFree(void *ptr) {
    BuddyBlock *blockNode = NULL;

    // Looking for block in allocated list with ptr as pointingToMemBlock.
    BuddyBlock *allocatedLLHead = buddyConfig.allocatedLLHead;
    while (allocatedLLHead != NULL) {
        if (allocatedLLHead->pointingToMemBlock == ptr) {
            blockNode = allocatedLLHead;
            break;
        }
        allocatedLLHead = allocatedLLHead->next;
    }

    if (blockNode == NULL) {
        return;
    }

    removeFromAllocatedList(blockNode);
    blockNode->free = true;

    int i = getPowerFromSize(blockNode->size);

    while (i < maxPowerPossible) {
        int size = sizeArray[i];

        // Looking for previous edge here.
        int doubleSize = 2*size;
        // printf("Looking for size = %d\n", doubleSize);
        int onePowerMoreBlockBase = (blockNode->start / doubleSize) * doubleSize ;
        // printf("One power bigger blocks start address = %d\n", onePowerMoreBlockBase);
        int buddyStart = onePowerMoreBlockBase;
        if (onePowerMoreBlockBase == blockNode->start) {
            // printf("Here1");
            //printf("Right buddy found");
            buddyStart = onePowerMoreBlockBase + size;
        }
        // printf("BuddyStart = %d\n", buddyStart);

        BuddyBlock *buddy = searchBlockUsingStart(i, buddyStart);
        if (buddy == NULL) {
            // printf("Didn't find buddy in free list. So Didn't merge");
            break; 
        }

        removeBlockForNode(i, buddy);

        // printf("Node start = %d, buddyStart = %d\n ", node->start,buddyStart);
        if (blockNode->start > buddyStart) {
            blockNode->start = buddyStart;
        }
        blockNode->size  = blockNode->size*2;
        // printf("Deleting buddy with start: %d\n", buddy->start);
        free(buddy);
        i += 1;

    }

    // printf("Adding merged large block in %d\n", i);
    pushBlockInBucket(i, blockNode);
    blockNode->pointingToMemBlock = NULL;
}

void slabFree(void *ptr) {
    SlabTypeConfig *slabType = NULL;
    int idx = -1;
    SingleSlabConfig *slab = locateSlabUsingPtr(ptr, &slabType, &idx);
    if (slab == NULL || slabType == NULL || idx < 0) {
        return;
    }

    if (slab->bitmap[idx] == 0) {
        return;
    }

    slab->bitmap[idx] = 0;
    slab->freeSlabObjects++;

    if (slab->freeSlabObjects == 1) {
        // full to empty
        removeSlabNode(&slabType->fullSlabList, slab);
        slab->next = slabType->emptySlabList;
        slabType->emptySlabList = slab;
    } else if (slab->freeSlabObjects == setUpConfig.n_objs_per_slab) {
        // In completely empty case we are removing the slab.
        removeSlabNode(&slabType->emptySlabList, slab);
        buddyFree(slab->startAddress);
        free(slab->bitmap);
        free(slab);
        return;
    }
}

void my_free(void *ptr) {
    if (setUpConfig.type == MALLOC_BUDDY) {
        buddyFree(ptr);
    } else {
        slabFree(ptr);
    }
}
