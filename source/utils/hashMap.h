#include "/arena.h"

/** HashMap Int Int with non-negative keys */
typedef struct {
    Arr(int) dict;
    int dictSize;
    int length;    
    Arena* a;
} HashMap;


HashMap createHashMap(int initSize);
void add(int key, int value, HashMap* hm);
bool hasKey(int key, HashMap* hm);
bool hasKeyValue(int key, int value, HashMap* hm);
void remove(int key, HashMap* hm);
