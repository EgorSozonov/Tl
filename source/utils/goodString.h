#ifndef STRING_H
#define STRING_H
#include "arena.h"
#include "aliases.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * \0-terminated C-string carrying its own length with it.
 */
typedef struct {
    int length;
    unsigned char content[];
} String;

String* allocLit(Arena* ar, const char* content);
String* allocateScratchSpace(Arena* ar, uint64_t len);
String* allocateFromSubstring(Arena* ar, char* content, int start, int length);
bool endsWith(String* a, String* b);
void printString(String* s);
void printStringNoLn(String* s);
bool testByteSequence(String* inp, int startByte, const byte letters[], int lengthLetters);
extern String empty;

#endif
