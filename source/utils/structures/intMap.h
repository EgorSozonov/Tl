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
} IntMap;


IntMap* createIntMap(int initSize, Arena* a);
void addIntMap(int key, int value, IntMap* hm);
bool hasKeyIntMap(int key, IntMap* hm);
bool hasKeyValueIntMap(int key, int value, IntMap* hm);
int getIntMap(int key, int* value, IntMap* hm);
int getUnsafeIntMap(int key, IntMap* hm);

#endif
