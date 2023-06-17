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
    byte content[];
} String;

String* str(const char* content, Arena* ar);
String* allocateScratchSpace(Arena* ar, uint64_t len);
String* allocateFromSubstring(Arena* ar, char* content, int start, int length);
bool endsWith(String* a, String* b);
bool equal(String* a, String* b);
void printString(String* s);
void printStringNoLn(String* s);

bool isLetter(byte a);
bool isCapitalLetter(byte a);
bool isLowercaseLetter(byte a);
bool isDigit(byte a);
bool isAlphanumeric(byte a);
bool isHexDigit(byte a);
bool isSpace(byte a);

bool testByteSequence(String* inp, int startBt, const byte letters[], int lengthLetters);
bool testForWord(String* inp, int startBt, const byte letters[], int lengthLetters);
extern String empty;

#endif
