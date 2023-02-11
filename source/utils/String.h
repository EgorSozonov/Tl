#ifndef STRING_H
#define STRING_H
#include "Arena.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * \0-terminated C-string carrying its own length with it.
 */
typedef struct {
    int length;
    char content[];
} String;

String* allocateLiteral(Arena* ar, const char* content);
String* allocateScratchSpace(Arena* ar, uint64_t len);
String* allocateFromSubstring(Arena* ar, char* content, int start, int length);
bool endsWith(String* a, String* b);
void printString(String* s);
extern String empty;

#endif
