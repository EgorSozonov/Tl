#include "goodString.h"
#include "./aliases.h"
#include "arena.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

/**
 * Allocates a C string literal into an arena. The length of the literal is determined in O(N).
 */
String* allocLit(Arena* a, const char* content) {
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

void printString(String* s) {
    if (s->length == 0) return;
    fwrite(s->content, 1, s->length, stdout);
    printf("\n");
}

void printStringNoLn(String* s) {
    if (s->length == 0) return;
    fwrite(s->content, 1, s->length, stdout);
}

/** Tests if the following several bytes in the input match an array. This is for reserved word detection */
bool testByteSequence(String* inp, int startByte, const byte letters[], int lengthLetters) {
    if (startByte + lengthLetters > inp->length) return false;

    for (int j = (lengthLetters - 1); j > -1; j--) {
        if (inp->content[startByte + j] != letters[j]) return false;
    }
    return true;
}

String empty = { .length = 0 };
