#pragma once

#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_POWER 30

typedef struct {
    enum malloc_type type;
    int memory_size;
    void *start_of_memory;
    int header_size;        
    int min_mem_chunk_size; 
    int n_objs_per_slab;
} SetUpConfig;

extern SetUpConfig setUpConfig;

extern int minPowerPossible;
extern int maxPowerPossible;

extern int sizeArray[MAX_POWER+1];

typedef struct BuddyBlock {
    int start;     
    int size;
    bool free;                    
    uint8_t *pointingToMemBlock; // We used this for freeing ptr purpose.
    struct BuddyBlock *next;
} BuddyBlock;

typedef struct {
    BuddyBlock *bucketHeads[MAX_POWER];
    BuddyBlock *allocatedLLHead;
} BuddyConfig;

extern BuddyConfig buddyConfig;

typedef struct SingleSlabConfig {
    void *startAddress;
    int eachSlabObjSize;
    int freeSlabObjects;
    uint8_t *bitmap;
    struct SingleSlabConfig *next;
} SingleSlabConfig;

typedef struct SlabTypeConfig {
    int eachSlabObjSize;
    SingleSlabConfig *emptySlabList;
    SingleSlabConfig *fullSlabList;
    struct SlabTypeConfig *next;
} SlabTypeConfig;

typedef struct {
    SlabTypeConfig *slabTypes;
} SlabAllocator;

extern SlabAllocator slabAllocator;

void *buddyAllocation(int req_size);
int  getPowerFromSize(int size);
BuddyBlock *createNewBuddyBlock(int start, int size, bool isFree);
void pushBlockInBucket(int power, BuddyBlock *node);
BuddyBlock *removeBucketHeadForAllocation(int power);
BuddyBlock *searchBlockUsingStart(int power, int start);
void removeBlockForNode(int power, BuddyBlock *node);
void removeFromAllocatedList(BuddyBlock *node);
void removeSlabNode(SingleSlabConfig **head, SingleSlabConfig *slabNode);
int requiredSizeFor(int size);
void buddyFree(void *ptr);
SlabTypeConfig *getSlabTypeForSize(int slabObjSize);
SlabTypeConfig *createNewSlabTypeForSize(int slabObjSize);
SingleSlabConfig *createNewSlabForType(SlabTypeConfig *slabType);
SingleSlabConfig *locateSlabUsingPtr(void *ptr, SlabTypeConfig **outSlabType, int *outIndex);