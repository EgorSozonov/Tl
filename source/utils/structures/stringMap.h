#ifndef STRINGMAP_H
#define STRINGMAP_H
#include <stdbool.h>
#include "arena.h"
#include "goodString.h"
#include "aliases.h"


/** HashMap String Int that is insert-only */
typedef struct StringMap StringMap;


StringMap* createStringMap(int initSize, Arena* a);
void addStringMap(String* key, int value, StringMap* hm);
bool hasKeyStringMap(String* key, StringMap* hm);
int getStringMap(String* key, int* result, StringMap* hm);
int getUnsafeStringMap(String* key, StringMap* hm);


#endif
