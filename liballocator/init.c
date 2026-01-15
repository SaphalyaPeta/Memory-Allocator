#include "api.h"
#include "my_memory.h"
    
SetUpConfig setUpConfig;
BuddyConfig buddyConfig;
int minPowerPossible;
int maxPowerPossible;
SlabAllocator slabAllocator;
int sizeArray[MAX_POWER+1];

void my_setup(enum malloc_type type, int memory_size, void *start_of_memory,
              int header_size, int min_mem_chunk_size, int n_objs_per_slab) {
    setUpConfig.type = type;
    setUpConfig.memory_size = memory_size;
    setUpConfig.start_of_memory = start_of_memory;
    setUpConfig.header_size = header_size;
    setUpConfig.min_mem_chunk_size = min_mem_chunk_size;
    setUpConfig.n_objs_per_slab = n_objs_per_slab;

    int size = 1;
    for (int i = 0; i <= MAX_POWER; i++) {
        sizeArray[i] = size;
        size = size*2;
    }
        
    minPowerPossible = getPowerFromSize(setUpConfig.min_mem_chunk_size);
    maxPowerPossible = getPowerFromSize(setUpConfig.memory_size);

    for (int i = 0; i < MAX_POWER; i++) {
        buddyConfig.bucketHeads[i] = NULL;
    }

    BuddyBlock *firstBlock = createNewBuddyBlock(0, setUpConfig.memory_size, true);
    // printf("Created first block of size: %d\n",setUpConfig.memory_size);
    pushBlockInBucket(maxPowerPossible, firstBlock);
    // printf("Pushed from init\n");
    buddyConfig.allocatedLLHead = NULL;

    slabAllocator.slabTypes = NULL;

}

void slab_cleanup(void) {
    SlabTypeConfig *slabType = slabAllocator.slabTypes;
    while (slabType) {
        SingleSlabConfig *s = slabType->fullSlabList;
        while (s) {
            SingleSlabConfig *next = s->next;
            if (s->startAddress) {
                buddyFree(s->startAddress);
            }
            free(s->bitmap);
            free(s);
            s = next;
        }

        s = slabType->emptySlabList;
        while (s) {
            SingleSlabConfig *next = s->next;
            if (s->startAddress) {
                buddyFree(s->startAddress);
            }
            free(s->bitmap);
            free(s);
            s = next;
        }

        SlabTypeConfig *nextType = slabType->next;
        free(slabType);
        slabType = nextType;
    }
    slabAllocator.slabTypes = NULL;
}

void my_cleanup() {
    // DOing slab related cleaning.
    if (setUpConfig.type == MALLOC_SLAB) {
        slab_cleanup();
    }

    // This clean up is for both case.
    for (int power = 0; power < MAX_POWER; power++) {
        BuddyBlock *curr = buddyConfig.bucketHeads[power];
        while (curr) {
            BuddyBlock *next = curr->next;
            free(curr);
            curr = next;
        }
        buddyConfig.bucketHeads[power] = NULL;
    }

    BuddyBlock *allocatedLLHead = buddyConfig.allocatedLLHead;
    while (allocatedLLHead) {
        BuddyBlock *nxt = allocatedLLHead->next;
        free(allocatedLLHead);
        allocatedLLHead = nxt;
    }
    buddyConfig.allocatedLLHead = NULL;
}
