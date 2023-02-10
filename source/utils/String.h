#ifndef STRING_H
#define STRING_H
#include "Arena.h"
#include <stdint.h>
/**
 * \0-terminated C-string carrying its own length with it.
 */
typedef struct {
    int length;
    char content[];
} String;

String* allocateLiteral(Arena* ar, char* content);
String* allocateScratchSpace(Arena* ar, uint64_t len);
String* allocateFromSubstring(Arena* ar, char* content, int start, int length);
void printString(String* s);

#endif
