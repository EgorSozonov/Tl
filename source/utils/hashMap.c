#include "hashMap.h"
#include <stdio.h>
#include <string.h>


HashMap* createHashMap(int initSize, Arena* a) {
    HashMap* result = allocateOnArena(sizeof(HashMap), a);
    int realInitSize = (initSize >= 4 && initSize < 1024) ? initSize : (initSize >= 4 ? 1024 : 4);
    Arr(int*) dict = allocateOnArena(sizeof(int*)*realInitSize, a);
    
    int** d = dict;

    for (int i = 0; i < realInitSize; i++) {
        d[i] = NULL;
    }
    result->dictSize = realInitSize;
    result->dict = dict;

    return result;
}


void add(int key, int value, HashMap* hm) {
    if (key < 0) return;

    int hash = key % (hm->dictSize);
    printf("when adding, hash = %d\n", hash);
    if ((*hm->dict + hash) == NULL) {
        Arr(int) newBucket = allocateOnArena(9*sizeof(int), hm->a);
        printf("here\n");
        newBucket[0] = (8 << 16) + 1; // u16 = capacity, u16 = length
        newBucket[1] = key;
        newBucket[2] = value;
        (*hm->dict)[hash] = newBucket;
    } else {
        int* p = (*hm->dict)[hash];
        int maxInd = 2*((*p) & 0xFFFF) + 1;
        for (int i = 1; i < maxInd; i += 2) {
            if (p[i] == key) {
                return; // key already present
            }
        }
        int capacity = (uint)(*p) >> 16;
        if (maxInd - 1 < capacity) {
            p[maxInd] = key;
            p[maxInd + 1] = value;
            p[0] = (capacity << 16) + (maxInd + 1)/2;
        } else {
            // TODO handle the case where we've overflowing the 16 bits of capacity
            Arr(int) newBucket = allocateOnArena((4*capacity + 1)*sizeof(int), hm->a);
            memcpy(newBucket + 1, p + 1, capacity*2);
            newBucket[0] = ((2*capacity) << 16) + capacity;
            newBucket[2*capacity + 1] = key;
            newBucket[2*capacity + 2] = value;
            (*hm->dict)[hash] = newBucket;
        }
    }
}


bool hasKey(int key, HashMap* hm) {
    if (key < 0) return false;
    
    int hash = key % hm->dictSize;
    printf("when searching, hash = %d\n", hash);
    if (hm->dict[hash] == NULL) { return false; }
    int* p = (*hm->dict)[hash];
    int maxInd = 2*((*p) & 0xFFFF) + 1;
    for (int i = 1; i < maxInd; i += 2) {
        if (p[i] == key) {
            return true; // key already present
        }
    }
    return false;
}


bool hasKeyValue(int key, int value, HashMap* hm) {
    return false;
}


void removeKey(int key, HashMap* hm) {
    return;
}
