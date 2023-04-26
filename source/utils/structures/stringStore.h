#ifndef STRINGSTORE_H
#define STRINGSTORE_H
#include <stdbool.h>
#include "arena.h"
#include "goodString.h"
#include "aliases.h"
#include "stackHeader.h"

/** HashMap String Int that is insert-only */
typedef struct StringStore StringStore;

/** Reference to first occurrence of a string identifier within input text */
typedef struct {
    intt startByte;
    intt length;
} StringRef;

DEFINE_STACK_HEADER(StringRef)

StringStore* createStringStore(int initSize, Arena* a);
intt addStringStore(const byte* text, intt startByte, intt len, StackStringRef* stringTable, StringStore* hm);


#endif
