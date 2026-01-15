#include "my_memory.h" 
#include <stdio.h>

int requiredSizeFor(int size) {
    int requiredBlockSize = 1;
    for (int i=0; i<=MAX_POWER; i++) {
        if (sizeArray[i] >= size) {
            return sizeArray[i];
        }
    }
    return sizeArray[MAX_POWER];
}

int getPowerFromSize(int size) {
    int low = 0, high = MAX_POWER, ans = high;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (sizeArray[mid] >= size) {
            ans = mid;
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return ans;
}

BuddyBlock *createNewBuddyBlock(int start, int size, bool isFree) {
    BuddyBlock *newBuddyBLock = (BuddyBlock*)malloc(sizeof(BuddyBlock));
    newBuddyBLock->start = start;
    newBuddyBLock->size = size;
    newBuddyBLock->free = isFree;
    newBuddyBLock->pointingToMemBlock = NULL;
    newBuddyBLock->next= NULL;
    return newBuddyBLock;
}

void pushBlockInBucket(int power, BuddyBlock *block) {
    block->free = true;
    block->next = NULL;
    BuddyBlock **head = &buddyConfig.bucketHeads[power];

    if (*head == NULL || block->start < (*head)->start) {
        block->next = *head;
        *head = block;
        return;
    }

    BuddyBlock *curr = *head;
    while (curr->next && curr->next->start < block->start) {
        curr = curr->next;
    }

    block->next = curr->next;
    curr->next = block;
}

BuddyBlock *removeBucketHeadForAllocation(int power) {
    BuddyBlock *bucketHead = buddyConfig.bucketHeads[power];
    if (bucketHead == NULL) {
        // printf("Bucket head is null here1\n");
        return NULL;
    }

    buddyConfig.bucketHeads[power] = bucketHead->next;
    bucketHead->next = NULL;
    bucketHead->free = false;
    return bucketHead;
}

BuddyBlock *searchBlockUsingStart(int power, int start) {
    BuddyBlock *curr = buddyConfig.bucketHeads[power];
    if (curr == NULL) {
        // printf("Curr is null here2 for power: %d \n", power);
        return NULL;
    }

    while (curr) {
        if (curr->start == start) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void removeBlockForNode(int power, BuddyBlock *block) {
    BuddyBlock **head = &buddyConfig.bucketHeads[power];
    if (*head == NULL) {
        // printf("Head is null for power : %d\n", power);
        return;
    }
    
    if (*head == block) {
        *head = block->next;
        block->next = NULL;
        return;
    }
    BuddyBlock *curr = *head;

    while (curr->next && curr->next != block) {
        curr = curr->next;
    }
    if (curr->next == block) {
        curr->next = block->next;
        block->next = NULL;
    }
}

void removeFromAllocatedList(BuddyBlock *block) {
    BuddyBlock **allocatedLLHead = &buddyConfig.allocatedLLHead;
    if (!*allocatedLLHead) {
        // printf("Nothing in allocated list\n");
        return;
    }

    if (*allocatedLLHead == block) {
        *allocatedLLHead = block->next;
        block->next = NULL;
        return;
    }

    BuddyBlock *curr = *allocatedLLHead;
    while (curr->next && curr->next != block) {
        curr = curr->next;
    }
    if (curr->next == block) {
        curr->next = block->next;
        block->next = NULL;
    }
}

// SLAB METHODS

void removeSlabNode(SingleSlabConfig **head, SingleSlabConfig *slabNode) {
    if (*head == NULL) {
        return;
    }

    if (*head == slabNode) { 
        *head = slabNode->next; 
        slabNode->next = NULL; 
        return; 
    }

    SingleSlabConfig *curr = *head;
    while (curr->next && curr->next != slabNode) {
        curr = curr->next;
    }

    if (curr->next == slabNode) { 
        curr->next = slabNode->next; 
        slabNode->next = NULL; 
    }
}

SlabTypeConfig *getSlabTypeForSize(int slabObjSize) {
    SlabTypeConfig *slabTypeListHeadPtr = slabAllocator.slabTypes;
    while (slabTypeListHeadPtr) {
        //cur = cur->next;
        if (slabTypeListHeadPtr->eachSlabObjSize == setUpConfig.header_size + slabObjSize) {
            return slabTypeListHeadPtr;
        }
        slabTypeListHeadPtr = slabTypeListHeadPtr->next;
    }

    return NULL;
}

SlabTypeConfig *createNewSlabTypeForSize(int slabObjSize) {
    SlabTypeConfig *slabType = (SlabTypeConfig*)malloc(sizeof(SlabTypeConfig));
    slabType->eachSlabObjSize = setUpConfig.header_size + slabObjSize;
    slabType->emptySlabList = NULL;
    slabType->fullSlabList = NULL;
    slabType->next = slabAllocator.slabTypes;
    slabAllocator.slabTypes = slabType;
    // printf("New SLab type added\n");
    return slabType;
}

SingleSlabConfig *createNewSlabForType(SlabTypeConfig *slabType) {
    int sizeForNSlabs = setUpConfig.n_objs_per_slab * slabType->eachSlabObjSize;
    int completeSlabSize = setUpConfig.header_size + sizeForNSlabs;
    int slabSizeRequired  = requiredSizeFor(completeSlabSize);

    uint8_t *startAddress = buddyAllocation(slabSizeRequired);

    if (startAddress == NULL) {
        // printf("Buddy allocation failed for slab size: %d\n", slabSizeRequired);
        return NULL;
    }     

    SingleSlabConfig *slab = (SingleSlabConfig*)malloc(sizeof(SingleSlabConfig));
    slab->startAddress = startAddress;
    slab->eachSlabObjSize = slabType->eachSlabObjSize;
    slab->freeSlabObjects = setUpConfig.n_objs_per_slab;
    slab->bitmap = (uint8_t *)calloc(setUpConfig.n_objs_per_slab, 1);
    slab->next = slabType->emptySlabList;
    slabType->emptySlabList = slab;
    return slab;
}

SingleSlabConfig *locatePtrIn(SingleSlabConfig *s, uint8_t *ptr, int *outIndex){
    while (s) {
        uint8_t *startAddress = s->startAddress;
        int total_payload = setUpConfig.n_objs_per_slab * s->eachSlabObjSize;
        if (ptr >= startAddress && ptr <  startAddress + total_payload) {
            int byteDifference = (uint8_t *)ptr - startAddress;
            int rel = byteDifference - setUpConfig.header_size;
            int idx = (int)(rel/s->eachSlabObjSize);
            *outIndex = idx;
            return s;
        }

        s = s->next;
    }

    return NULL;
}

SingleSlabConfig *locateSlabUsingPtr(void *ptr, SlabTypeConfig **outSlabType, int *outIndex) {
    SlabTypeConfig *slabType = slabAllocator.slabTypes;
    while (slabType) {
        // printf("Locating in type with size: %d\n", slabType->eachSlabObjSize);
        SingleSlabConfig *s = slabType->fullSlabList;
        SingleSlabConfig *returnS = locatePtrIn(s, ptr, outIndex);
        if (returnS) {
            *outSlabType = slabType;
            return returnS;
        }
        
        s = slabType->emptySlabList;
        returnS = locatePtrIn(s, ptr, outIndex);
        if (returnS) {
            *outSlabType = slabType;
            return returnS;
        }

        slabType = slabType->next;
    }

    *outSlabType = NULL;
    *outIndex = -1;
    return NULL;
}