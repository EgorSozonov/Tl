#include "goodString.h"
#include "./aliases.h"
#include "arena.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define aALower       97
#define aFLower      102
#define aZLower      122
#define aFUpper      127
#define aAUpper       65
#define aZUpper       90
#define aDigit0       48
#define aDigit9       57
#define aSpace        32
#define aNewline      10
#define aCarrReturn   13

/**
 * Allocates a C string literal into an arena. The length of the literal is determined in O(N).
 */
String* str(const char* content, Arena* a) {
    if (content == NULL) return NULL;
    const char* ind = content;
    int len = 0;
    for (; *ind != '\0'; ind++)
        len++;

    String* result = allocateOnArena(len + 1 + sizeof(String), a);
    result->length = len;
    memcpy(result->content, content, len + 1);
    return result;
}

/**
 * Allocates a C string of set length to use as scratch space.
 * WARNING: does not zero out its contents.
 */
String* allocateScratchSpace(Arena* a, uint64_t len) {
    String* result = allocateOnArena(len + 1 + sizeof(String), a);
    result->length = len;
    return result;
}

/**
 * Allocates a C string literal into an arena from a piece of another string.
 */
String* allocateFromSubstring(Arena* a, char* content, int start, int length) {
    if (length <= 0 || start < 0) return NULL;
    int i = 0;
    while (content[i] != '\0') {
        ++i;
    }
    if (i < start) return NULL;
    int realLength = MIN(i - start, length);

    String* result = allocateOnArena(realLength + 1 + sizeof(String), a);
    result->length = realLength;
    memcpy(result->content, content, realLength);
    result->content[realLength] = '\0';
    return result;
}

/** Does string "a" end with string "b"? */
bool endsWith(String* a, String* b) {
    if (a->length < b->length) {
        return false;
    } else if (b->length == 0) {
        return true;
    }
    
    int shift = a->length - b->length;
    int cmpResult = memcmp(a->content + shift, b->content, b->length);
    return cmpResult == 0;
}


bool equal(String* a, String* b) {
    if (a->length != b->length) {
        return false;
    }
    
    int cmpResult = memcmp(a->content, b->content, b->length);
    return cmpResult == 0;
}


void printString(String* s) {
    if (s->length == 0) return;
    fwrite(s->content, 1, s->length, stdout);
    printf("\n");
}


void printStringNoLn(String* s) {
    if (s->length == 0) return;
    fwrite(s->content, 1, s->length, stdout);
}

bool isLetter(byte a) {
    return ((a >= aALower && a <= aZLower) || (a >= aAUpper && a <= aZUpper));
}

bool isCapitalLetter(byte a) {
    return a >= aAUpper && a <= aZUpper;
}

bool isLowercaseLetter(byte a) {
    return a >= aALower && a <= aZLower;
}

bool isDigit(byte a) {
    return a >= aDigit0 && a <= aDigit9;
}

bool isAlphanumeric(byte a) {
    return isLetter(a) || isDigit(a);
}

bool isHexDigit(byte a) {
    return isDigit(a) || (a >= aALower && a <= aFLower) || (a >= aAUpper && a <= aFUpper);
}

bool isSpace(byte a) {
    return a == aSpace || a == aNewline || a == aCarrReturn;
}

/** Tests if the following several bytes in the input match an array */
bool testByteSequence(String* inp, int startBt, const byte letters[], int lengthLetters) {
    if (startBt + lengthLetters > inp->length) return false;

    for (int j = (lengthLetters - 1); j > -1; j--) {
        if (inp->content[startBt + j] != letters[j]) return false;
    }
    return true;
}

/** Tests if the following several bytes in the input match a word. Tests also that it is the whole word */
bool testForWord(String* inp, int startBt, const byte letters[], int lengthLetters) {
    Int j = startBt + lengthLetters;
    if (j > inp->length) return false;
    
    if (j < inp->length && isAlphanumeric(inp->content[j])) {
        print("j %d", j)
        return false;        
    }

    for (j = (lengthLetters - 1); j > -1; j--) {
        if (inp->content[startBt + j] != letters[j]) return false;
    }
    return true;
}

String empty = { .length = 0 };
