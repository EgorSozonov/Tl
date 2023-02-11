#include "String.h"
#include "Arena.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

///
/// \brief allocateString allocates a C string literal into an arena. The length of the literal has to be provided in an arg.
/// It should NOT include +1 for the \0 character, as that will be added automatically.
/// \param ar
/// \param length
/// \param content
/// \return
///
String* allocateLiteral(Arena* ar, char* content) {
    if (content == NULL) return NULL;
    char* ind = content;
    int len = 0;
    for (; *ind != '\0'; ind++)
        len++;

    String* result = allocateOnArena(ar, len + 1 + sizeof(String));
    result->length = len;
    memcpy(result->content, content, len + 1);
    return result;
}

///
/// \brief allocateScratchSpace allocates a C string of set length to use as scratch space.
/// WARNING: does not zero out its contents.
///
String* allocateScratchSpace(Arena* ar, uint64_t len) {
    String* result = allocateOnArena(ar, len + 1 + sizeof(String));
    result->length = len;
    return result;
}


///
/// \brief allocateFromSubstring allocates a C string literal into an arena from a piece of another string.
///
String* allocateFromSubstring(Arena* ar, char* content, int start, int length) {
    if (length <= 0 || start < 0) return NULL;
    int i = 0;
    while (content[i] != '\0') {
        ++i;
    }
    if (i < start) return NULL;
    int realLength = MIN(i - start, length);

    String* result = allocateOnArena(ar, realLength + 1 + sizeof(String));
    result->length = realLength;
    strncpy(result->content, content, realLength);
    result->content[realLength] = '\0';
    return result;
}

void printString(String* s) {
    fwrite(s->content, 1, s->length, stdout);
    printf("\n");
}
