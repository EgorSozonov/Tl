#include "hashMap.h"
#include "arena.h"


HashMap* createHashMap(int initSize, Arena* a) {
    HashMap* result = allocateOnArena(sizeof(HashMap), a);
    int realInitSize = (initSize >= 4 && initSize < 1024) ? initSize : (initSize >= 4 ? 1024 : 4);
    Arr(Arr(int)) dict = allocateOnArena(sizeof(int*)*realInitSize, a);
    for (int i = 0; i < realInitSize; i++) {
        dict[i] = NULL;
    }
    result->dict = dict;
    return result;
}

void add(int key, int value, HashMap* hm) {
    if (key < 0) return;
    
    int hash = key % hm->dictSize;
    if (hm->dict[hash] == NULL) {
        Arr(int) newBucket = allocateOnArena(9*sizeof(int), hm->a);
        newBucket[8] = -1; // sentinel value
        newBucket[0] = key;
        newBucket[1] = value;
        hm->dict[hash] = newBucket;
    } else {
        int* p = hm->dict[hash];
        while (*p > -1) {
            if (*p == key) {
                return; // key already present
            }
        }
    }
    // hash
    // 
}

bool hasKey(int key, HashMap* hm) {
    
}

bool hasKeyValue(int key, int value, HashMap* hm) {
    
}

void remove(int key, HashMap* hm) {
    
}
