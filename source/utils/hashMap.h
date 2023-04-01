#ifndef HASHMAP_H
#define HASHMAP_H
#include "./arena.h"
#include <stdbool.h>
#include "./aliases.h"



/** HashMap Int Int with non-negative keys */
typedef struct {
    Arr(int*) dict;
    int dictSize;
    int length;    
    Arena* a;
} HashMap;


HashMap* createHashMap(int initSize, Arena* a);
void add(int key, int value, HashMap* hm);
bool hasKey(int key, HashMap* hm);
bool hasKeyValue(int key, int value, HashMap* hm);
void removeKey(int key, HashMap*);

#endif
