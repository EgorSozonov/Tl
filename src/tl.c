//{{{ Includes
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <setjmp.h>
#include "tl.internal.h"
//}}}
//{{{ Language definition
//{{{ Text encoding

#define aALower       97
#define aFLower      102
#define aILower      105
#define aWLower      119
#define aXLower      120
#define aYLower      121
#define aZLower      122
#define aAUpper       65
#define aFUpper       70
#define aZUpper       90
#define aDigit0       48
#define aDigit9       57

#define aPlus         43
#define aMinus        45
#define aTimes        42
#define aDivBy        47
#define aDot          46
#define aPercent      37

#define aParenLeft    40
#define aParenRight   41
#define aBracketLeft  91
#define aBracketRight 93
#define aCurlyLeft   123
#define aCurlyRight  125
#define aPipe        124
#define aAmp          38
#define aTilde       126
#define aBackslash    92

#define aSpace        32
#define aNewline      10
#define aCarrReturn   13
#define aTab           9

#define aApostrophe   39
#define aBacktick     96
#define aSharp        35
#define aDollar       36
#define aUnderscore   95
#define aCaret        94
#define aAt           64
#define aColon        58
#define aSemicolon    59
#define aExclamation  33
#define aQuestion     63
#define aComma        44
#define aEqual        61
#define aLT           60
#define aGT           62

//}}}

static OpDef TL_OPERATORS[countOperators] = {
    { .arity=1, .bytes={ aExclamation, aDot, 0, 0 } }, // !.
    { .arity=2, .bytes={ aExclamation, aEqual, 0, 0 } }, // !=
    { .arity=1, .bytes={ aExclamation, 0, 0, 0 } }, // !
    { .arity=1, .bytes={ aSharp, aSharp, 0, 0 }, .overloadable=true }, // ##
    { .arity=1, .bytes={ aDollar, 0, 0, 0 } }, // $
    { .arity=2, .bytes={ aPercent, 0, 0, 0 } }, // %
    { .arity=2, .bytes={ aAmp, aAmp, aDot, 0 } }, // &&.
    { .arity=2, .bytes={ aAmp, aAmp, 0, 0 }, .assignable=true }, // &&
    { .arity=1, .bytes={ aAmp, 0, 0, 0 }, .isTypelevel=true }, // &
    { .arity=1, .bytes={ aApostrophe, 0, 0, 0 } }, // '
    { .arity=2, .bytes={ aTimes, aColon, 0, 0}, .assignable = true, .overloadable = true}, // *:
    { .arity=2, .bytes={ aTimes, 0, 0, 0 }, .assignable = true, .overloadable = true}, // *
    { .arity=1, .bytes={ aPlus, aPlus, 0, 0 }, .assignable = false, .overloadable = true}, // ++
    { .arity=2, .bytes={ aPlus, aColon, 0, 0 }, .assignable = true, .overloadable = true}, // +:
    { .arity=2, .bytes={ aPlus, 0, 0, 0 }, .assignable = true, .overloadable = true}, // +
    { .arity=1, .bytes={ aMinus, aMinus, 0, 0 }, .assignable = false, .overloadable = true}, // --
    { .arity=2, .bytes={ aMinus, aColon, 0, 0}, .assignable = true, .overloadable = true}, // -:
    { .arity=2, .bytes={ aMinus, 0, 0, 0}, .assignable = true, .overloadable = true }, // -
    { .arity=2, .bytes={ aDivBy, aColon, 0, 0}, .assignable = true, .overloadable = true}, // /:
    { .arity=2, .bytes={ aDivBy, aPipe, 0, 0}, .isTypelevel = true}, // /|
    { .arity=2, .bytes={ aDivBy, 0, 0, 0}, .assignable = true, .overloadable = true}, // /
    { .arity=2, .bytes={ aLT, aLT, aDot, 0} }, // <<.
    { .arity=2, .bytes={ aLT, aEqual, 0, 0} }, // <=
    { .arity=2, .bytes={ aLT, aGT, 0, 0} }, // <>
    { .arity=2, .bytes={ aLT, 0, 0, 0 } }, // <
    { .arity=2, .bytes={ aEqual, aEqual, aEqual, 0 } }, // ===
    { .arity=2, .bytes={ aEqual, aEqual, 0, 0 } }, // ==
    { .arity=3, .bytes={ aGT, aEqual, aLT, aEqual } }, // >=<=
    { .arity=3, .bytes={ aGT, aLT, aEqual, 0 } }, // ><=
    { .arity=3, .bytes={ aGT, aEqual, aLT, 0 } }, // >=<
    { .arity=2, .bytes={ aGT, aGT, aDot, 0}, .assignable=true, .overloadable = true}, // >>.
    { .arity=3, .bytes={ aGT, aLT, 0, 0 } }, // ><
    { .arity=2, .bytes={ aGT, aEqual, 0, 0 } }, // >=
    { .arity=2, .bytes={ aGT, 0, 0, 0 } }, // >
    { .arity=2, .bytes={ aQuestion, aColon, 0, 0 } }, // ?:
    { .arity=1, .bytes={ aQuestion, 0, 0, 0 }, .isTypelevel=true }, // ?
    { .arity=2, .bytes={ aCaret, aDot, 0, 0} }, // ^.
    { .arity=2, .bytes={ aCaret, 0, 0, 0}, .assignable=true, .overloadable = true}, // ^
    { .arity=2, .bytes={ aUnderscore, 0, 0, 0}, .overloadable = true}, // _
    { .arity=2, .bytes={ aPipe, aPipe, aDot, 0} }, // ||.
    { .arity=2, .bytes={ aPipe, aPipe, 0, 0}, .assignable=true } // ||
};

//}}}
//{{{ Utils

jmp_buf excBuf;

//{{{ Arena

#define CHUNK_QUANT 32768

private size_t minChunkSize(void) {
    return (size_t)(CHUNK_QUANT - 32);
}

testable Arena* mkArena(void) {
    Arena* result = malloc(sizeof(Arena));

    size_t firstChunkSize = minChunkSize();
    ArenaChunk* firstChunk = malloc(firstChunkSize);
    firstChunk->size = firstChunkSize - sizeof(ArenaChunk);
    firstChunk->next = null;

    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->currInd = 0;

    return result;
}

private size_t calculateChunkSize(size_t allocSize) {
// Calculates memory for a new chunk. Memory is quantized and is always 32 bytes less
// 32 for any possible padding malloc might use internally,
// so that the total allocation size is a good even number of OS memory pages
    size_t fullMemory = sizeof(ArenaChunk) + allocSize + 32;
    // struct header + main memory chunk + space for malloc bookkeep

    int mallocMemory = fullMemory < CHUNK_QUANT
                        ? CHUNK_QUANT
                        : (fullMemory % CHUNK_QUANT > 0
                           ? (fullMemory/CHUNK_QUANT + 1)*CHUNK_QUANT
                           : fullMemory);

    return mallocMemory - 32;
}

testable void* allocateOnArena(size_t allocSize, Arena* ar) {
// Allocate memory in the arena, malloc'ing a new chunk if needed
    if (ar->currInd + allocSize >= ar->currChunk->size) {
        size_t newSize = calculateChunkSize(allocSize);

        ArenaChunk* newChunk = malloc(newSize);
        if (!newChunk) {
            perror("malloc error when allocating arena chunk");
            exit(EXIT_FAILURE);
        };
        // sizeof counts everything but the flexible array member, that's why we subtract it
        newChunk->size = newSize - sizeof(ArenaChunk);
        newChunk->next = null;

        ar->currChunk->next = newChunk;
        ar->currChunk = newChunk;
        ar->currInd = 0;
    }
    void* result = (void*)(ar->currChunk->memory + (ar->currInd));
    ar->currInd += allocSize;
    return result;
}


testable void deleteArena(Arena* ar) {
    ArenaChunk* curr = ar->firstChunk;
    ArenaChunk* nextToFree = curr;
    while (curr != null) {
        nextToFree = curr->next;
        free(curr);
        curr = nextToFree;
    }
    free(ar);
}

//}}}
//{{{ Stack
#define DEFINE_STACK(T)\
    testable Stack##T * createStack##T (int initCapacity, Arena* a) {\
        int capacity = initCapacity < 4 ? 4 : initCapacity;\
        Stack##T * result = allocateOnArena(sizeof(Stack##T), a);\
        result->cap = capacity;\
        result->len = 0;\
        result->arena = a;\
        T* arr = allocateOnArena(capacity*sizeof(T), a);\
        result->cont = arr;\
        return result;\
    }\
    testable bool hasValues ## T (Stack ## T * st) {\
        return st->len > 0;\
    }\
    testable T pop##T (Stack ## T * st) {\
        st->len--;\
        return st->cont[st->len];\
    }\
    testable T peek##T(Stack##T * st) {\
        return st->cont[st->len - 1];\
    }\
    testable T penultimate##T(Stack##T * st) {\
        return st->cont[st->len - 2];\
    }\
    testable void push##T (T newItem, Stack ## T * st) {\
        if (st->len < st->cap) {\
            memcpy((T*)(st->cont) + (st->len), &newItem, sizeof(T));\
        } else {\
            T* newContent = allocateOnArena(2*(st->cap)*sizeof(T), st->arena);\
            memcpy(newContent, st->cont, st->len*sizeof(T));\
            memcpy((T*)(newContent) + (st->len), &newItem, sizeof(T));\
            st->cap *= 2;\
            st->cont = newContent;\
        }\
        ++st->len;\
    }\
    testable void clear##T (Stack##T * st) {\
        st->len = 0;\
    }
//}}}
//{{{ Internal list

#define DEFINE_INTERNAL_LIST_CONSTRUCTOR(T)                 \
testable InList##T createInList##T(Int initCap, Arena* a) { \
    return (InList##T){                                     \
        .cont = allocateOnArena(initCap*sizeof(T), a),   \
        .len = 0, .cap = initCap };                 \
}

#define DEFINE_INTERNAL_LIST(fieldName, T, aName)            \
    testable bool hasValuesIn##fieldName(Compiler* cm) {     \
        return cm->fieldName.len > 0;                        \
    }                                                        \
    testable T popIn##fieldName(Compiler* cm) {              \
        cm->fieldName.len--;                              \
        return cm->fieldName.cont[cm->fieldName.len];        \
    }                                                        \
    testable T peekIn##fieldName(Compiler* cm) {             \
        return cm->fieldName.cont[cm->fieldName.len - 1];    \
    }                                                          \
    testable void pushIn##fieldName(T newItem, Compiler* cm) { \
        if (cm->fieldName.len < cm->fieldName.cap) {    \
            memcpy((T*)(cm->fieldName.cont) + (cm->fieldName.len), &newItem, sizeof(T)); \
        } else {                                                                               \
            T* newContent = allocateOnArena(2*(cm->fieldName.cap)*sizeof(T), cm->aName);  \
            memcpy(newContent, cm->fieldName.cont, cm->fieldName.len*sizeof(T));         \
            memcpy((T*)(newContent) + (cm->fieldName.len), &newItem, sizeof(T));            \
            cm->fieldName.cap *= 2;                                                       \
            cm->fieldName.cont = newContent;                                                \
        }                                                                                      \
        cm->fieldName.len++;                                                                \
    }
//}}}
//{{{ AssocList

void typePrint(Int typeId, Compiler* cm);

MultiAssocList* createMultiAssocList(Arena* a) {
    MultiAssocList* ml = allocateOnArena(sizeof(MultiAssocList), a);
    Arr(Int) content = allocateOnArena(12*4, a);
    (*ml) = (MultiAssocList) {
        .len = 0,
        .cap = 12,
        .cont = content,
        .freeList = -1,
        .a = a,
    };
    return ml;
}


private Int multiListFindFree(Int neededCap, MultiAssocList* ml) {
    Int freeInd = ml->freeList;
    Int prevFreeInd = -1;
    Int freeStep = 0;
    while (freeInd > -1 && freeStep < 10) {
        Int freeCap = ml->cont[freeInd + 1];
        if (freeCap == neededCap) {
            if (prevFreeInd > -1) {
                ml->cont[prevFreeInd] = ml->cont[freeInd]; // remove this node from the free list
            } else {
                ml->freeList = -1;
            }
            return freeInd;
        }
        prevFreeInd = freeInd;
        freeInd = ml->cont[freeInd];
        ++freeStep;
    }
    return -1;
}


private void multiListReallocToEnd(Int listInd, Int listLen, Int neededCap, MultiAssocList* ml) {
    ml->cont[ml->len] = listLen;
    ml->cont[ml->len + 1] = neededCap;
    memcpy(ml->cont + ml->len + 2, ml->cont + listInd + 2, listLen*4);
    ml->len += neededCap + 2;
}


private void multiListDoubleCap(MultiAssocList* ml) {
    Int newMultiCap = ml->cap*2;
    Arr(Int) newAlloc = allocateOnArena(newMultiCap*4, ml->a);
    memcpy(newAlloc, ml->cont, ml->len*4);
    ml->cap = newMultiCap;
    ml->cont = newAlloc;
}


testable Int addMultiAssocList(Int newKey, Int newVal, Int listInd, MultiAssocList* ml) {
// Add a new key-value pair to a particular list within the MultiAssocList. Returns the new index
// for this list in case it had to be reallocated, -1 if not. Throws exception if key already exists
    Int listLen = ml->cont[listInd];
    Int listCap = ml->cont[listInd + 1];
    ml->cont[listInd + listLen + 2] = newKey;
    ml->cont[listInd + listLen + 3] = newVal;
    listLen += 2;
    Int newListInd = -1;
    if (listLen == listCap) { // look in the freelist, but not more than 10 steps
                              // to not waste much time
        Int neededCap = listCap*2;
        Int freeInd = multiListFindFree(neededCap, ml);
        if (freeInd > -1) {
            ml->cont[freeInd] = listLen;
            memcpy(ml->cont + freeInd + 2, ml->cont + listInd + 2, listLen);
            newListInd = freeInd;
        } else if (ml->len + neededCap + 2 < ml->cap) {
            newListInd = ml->len;
            multiListReallocToEnd(listInd, listLen, neededCap, ml);
        } else {
            newListInd = ml->len;
            multiListDoubleCap(ml);
            multiListReallocToEnd(listInd, listLen, neededCap, ml);
        }

        // add this newly freed sector to the freelist
        ml->cont[listInd] = ml->freeList;
        ml->freeList = listInd;
    } else {
        ml->cont[listInd] = listLen;
    }
    return newListInd;
}


testable Int listAddMultiAssocList(Int newKey, Int newVal, MultiAssocList* ml) {
// Adds a new list to the MultiAssocList and populates it with one key-value pair.
// Returns its index
    Int initCap = 8;
    Int newInd = multiListFindFree(initCap, ml);
    if (newInd == -1) {
        if (ml->len + initCap + 2 >= ml->cap) {
             multiListDoubleCap(ml);
        }
        newInd = ml->len;
        ml->len += (initCap + 2);
    }

    ml->cont[newInd] = 2;
    ml->cont[newInd + 1] = initCap;
    ml->cont[newInd + 2] = newKey;
    ml->cont[newInd + 3] = newVal;
    return newInd;
}


testable Int searchMultiAssocList(Int searchKey, Int listInd, MultiAssocList* ml) {
// Search for a key in a particular list within the MultiAssocList. Returns the value if found,
// -1 otherwise
    Int len = ml->cont[listInd]/2;
    Int endInd = listInd + 2 + len;
    for (Int j = listInd + 2; j < endInd; j++) {
        if (ml->cont[j] == searchKey) {
            return ml->cont[j + len];
        }
    }
    return -1;
}

private MultiAssocList* copyMultiAssocList(MultiAssocList* ml, Arena* a) {
    MultiAssocList* result = allocateOnArena(sizeof(MultiAssocList), a);
    Arr(Int) cont = allocateOnArena(4*ml->cap, a);
    memcpy(cont, ml->cont, 4*ml->cap);
    (*result) = (MultiAssocList){
        .len = ml->len, .cap = ml->cap, .freeList = ml->freeList, .cont = cont, .a = a
    };
    return result;
}

#ifdef TEST

void printRawOverload(Int listInd, Compiler* cm) {
    MultiAssocList* ml = cm->rawOverloads;
    Int len = ml->cont[listInd]/2;
    printf("[");
    for (Int j = 0; j < len; j++) {
        printf("%d: %d ", ml->cont[listInd + 2 + 2*j], ml->cont[listInd + 2*j + 3]);
    }
    print("]");
    printf("types: ");
    for (Int j = 0; j < len; j++) {
        typePrint(ml->cont[listInd + 2 + 2*j], cm);
        printf("\n");
    }
}

#endif

//}}}
//{{{ Datatypes a la carte

DEFINE_STACK(int32_t)
DEFINE_STACK(BtToken)
DEFINE_STACK(ParseFrame)
DEFINE_STACK(ExprFrame)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Token)
DEFINE_INTERNAL_LIST(tokens, Token, a)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Toplevel)
DEFINE_INTERNAL_LIST(toplevels, Toplevel, a)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Node)
DEFINE_INTERNAL_LIST(nodes, Node, a)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Entity)
DEFINE_INTERNAL_LIST(entities, Entity, a)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Int)
DEFINE_INTERNAL_LIST(newlines, Int, a)
DEFINE_INTERNAL_LIST(numeric, Int, a)
DEFINE_INTERNAL_LIST(importNames, Int, a)
DEFINE_INTERNAL_LIST(overloads, Int, a)
DEFINE_INTERNAL_LIST(types, Int, a)

//}}}
//{{{ Strings

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
testable String* str(const char* content, Arena* a) {
// Allocates a C string literal into an arena. The length of the literal is determined in O(N)
    if (content == null) return null;
    const char* ind = content;
    int len = 0;
    for (; *ind != '\0'; ind++)
        len++;

    String* result = allocateOnArena(len + 1 + sizeof(String), a);
    result->len = len;
    memcpy(result->cont, content, len + 1);
    return result;
}

testable bool endsWith(String* a, String* b) {
// Does string "a" end with string "b"?
    if (a->len < b->len) {
        return false;
    } else if (b->len == 0) {
        return true;
    }

    int shift = a->len - b->len;
    int cmpResult = memcmp(a->cont + shift, b->cont, b->len);
    return cmpResult == 0;
}


private bool equal(String* a, String* b) {
    if (a->len != b->len) {
        return false;
    }

    int cmpResult = memcmp(a->cont, b->cont, b->len);
    return cmpResult == 0;
}

private Int stringLenOfInt(Int n) {
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
    if (n < 10) return 1;
    if (n < 100) return 2;
    if (n < 1000) return 3;
    if (n < 10000) return 4;
    if (n < 100000) return 5;
    if (n < 1000000) return 6;
    if (n < 10000000) return 7;
    if (n < 100000000) return 8;
    if (n < 1000000000) return 9;
    return 10;
}

private String* stringOfInt(Int i, Arena* a) {
    Int stringLen = stringLenOfInt(i);
    String* result = allocateOnArena(sizeof(String) + stringLen + 1, a);
    result->len = stringLen;
    sprintf((char* restrict)result->cont, "%d", i);
    return result;
}

testable void printString(String* s) {
    if (s->len == 0) return;
    fwrite(s->cont, 1, s->len, stdout);
    printf("\n");
}


testable void printStringNoLn(String* s) {
    if (s->len == 0) return;
    fwrite(s->cont, 1, s->len, stdout);
}

private bool isLetter(Byte a) {
    return ((a >= aALower && a <= aZLower) || (a >= aAUpper && a <= aZUpper));
}

private bool isCapitalLetter(Byte a) {
    return a >= aAUpper && a <= aZUpper;
}

private bool isLowercaseLetter(Byte a) {
    return a >= aALower && a <= aZLower;
}

private bool isDigit(Byte a) {
    return a >= aDigit0 && a <= aDigit9;
}

private bool isAlphanumeric(Byte a) {
    return isLetter(a) || isDigit(a);
}

private bool isHexDigit(Byte a) {
    return isDigit(a) || (a >= aALower && a <= aFLower) || (a >= aAUpper && a <= aFUpper);
}

private bool isSpace(Byte a) {
    return a == aSpace || a == aNewline || a == aCarrReturn;
}


String empty = { .len = 0 };
//}}}
//{{{ Int Hashmap

testable IntMap* createIntMap(int initSize, Arena* a) {
    IntMap* result = allocateOnArena(sizeof(IntMap), a);
    int realInitSize = (initSize >= 4 && initSize < 1024) ? initSize : (initSize >= 4 ? 1024 : 4);
    Arr(int*) dict = allocateOnArena(sizeof(int*)*realInitSize, a);

    result->a = a;

    int** d = dict;

    for (int i = 0; i < realInitSize; i++) {
        d[i] = null;
    }
    result->dictSize = realInitSize;
    result->dict = dict;

    return result;
}


testable void addIntMap(int key, int value, IntMap* hm) {
    if (key < 0) return;

    int hash = key % (hm->dictSize);
    if (*(hm->dict + hash) == null) {
        Arr(int) newBucket = allocateOnArena(9*sizeof(int), hm->a);
        newBucket[0] = (8 << 16) + 1; // left u16 = capacity, right u16 = length
        newBucket[1] = key;
        newBucket[2] = value;
        *(hm->dict + hash) = newBucket;
    } else {
        Int* p = *(hm->dict + hash);
        Int maxInd = 2*((*p) & 0xFFFF) + 1;
        for (Int i = 1; i < maxInd; i += 2) {
            if (p[i] == key) { // key already present
                return;
            }
        }
        Int capacity = (untt)(*p) >> 16;
        if (maxInd - 1 < capacity) {
            p[maxInd] = key;
            p[maxInd + 1] = value;
            p[0] = (capacity << 16) + (maxInd + 1)/2;
        } else {
            // TODO handle the case where we've overflowing the 16 bits of capacity
            Arr(int) newBucket = allocateOnArena((4*capacity + 1)*sizeof(int), hm->a);
            memcpy(newBucket + 1, p + 1, capacity*2*sizeof(int));
            newBucket[0] = ((2*capacity) << 16) + capacity;
            newBucket[2*capacity + 1] = key;
            newBucket[2*capacity + 2] = value;
            *(hm->dict + hash) = newBucket;
        }
    }
}


testable bool hasKeyIntMap(int key, IntMap* hm) {
    if (key < 0) return false;

    int hash = key % hm->dictSize;
    printf("when searching, hash = %d\n", hash);
    if (hm->dict[hash] == null) { return false; }
    int* p = *(hm->dict + hash);
    int maxInd = 2*((*p) & 0xFFFF) + 1;
    for (int i = 1; i < maxInd; i += 2) {
        if (p[i] == key) {
            return true; // key already present
        }
    }
    return false;
}


private int getIntMap(int key, int* value, IntMap* hm) {
    int hash = key % (hm->dictSize);
    if (*(hm->dict + hash) == null) {
        return 1;
    }

    int* p = *(hm->dict + hash);
    int maxInd = 2*((*p) & 0xFFFF) + 1;
    for (int i = 1; i < maxInd; i += 2) {
        if (p[i] == key) { // key already present
            *value = p[i + 1];
            return 0;
        }
    }
    return 1;
}

private int getUnsafeIntMap(int key, IntMap* hm) {
// Throws an exception when key is absent
    int hash = key % (hm->dictSize);
    if (*(hm->dict + hash) == null) {
        longjmp(excBuf, 1);
    }

    int* p = *(hm->dict + hash);
    int maxInd = 2*((*p) & 0xFFFF) + 1;
    for (int i = 1; i < maxInd; i += 2) {
        if (p[i] == key) { // key already present
            return p[i + 1];
        }
    }
    longjmp(excBuf, 1);
}


private bool hasKeyValueIntMap(int key, int value, IntMap* hm) {
    return false;
}


//}}}
//{{{ String Hashmap

#define initBucketSize 8

private StringDict* createStringDict(int initSize, Arena* a) {
    StringDict* result = allocateOnArena(sizeof(StringDict), a);
    int realInitSize = (initSize >= initBucketSize && initSize < 2048)
        ? initSize
        : (initSize >= initBucketSize ? 2048 : initBucketSize);
    Arr(Bucket*) dict = allocateOnArena(sizeof(Bucket*)*realInitSize, a);

    result->a = a;

    Arr(Bucket*) d = dict;

    for (int i = 0; i < realInitSize; i++) {
        d[i] = null;
    }
    result->dictSize = realInitSize;
    result->dict = dict;

    return result;
}


private untt hashCode(Byte* start, Int len) {
    untt result = 5381;
    Byte* p = start;
    for (Int i = 0; i < len; i++) {
        result = ((result << 5) + result) + p[i]; // hash*33 + c
    }

    return result;
}


private void addValueToBucket(Bucket** ptrToBucket, Int newIndString, untt hash, Arena* a) {
    Bucket* p = *ptrToBucket;
    Int capacity = (p->capAndLen) >> 16;
    Int lenBucket = (p->capAndLen & 0xFFFF);
    if (lenBucket + 1 < capacity) {
        *(p->cont + lenBucket) = (StringValue){.hash = hash, .indString = newIndString};
        ++(p->capAndLen);
        if (hash == 177649) {
        }
    } else {
        // TODO handle the case when we're overflowing the 16 bits of capacity
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + 2*capacity*sizeof(StringValue), a);
        memcpy(newBucket->cont, p->cont, capacity*sizeof(StringValue));

        Arr(StringValue) newValues = (StringValue*)newBucket->cont;
        newValues[capacity] = (StringValue){.indString = newIndString, .hash = hash};
        *ptrToBucket = newBucket;
        newBucket->capAndLen = ((2*capacity) << 16) + capacity + 1;
    }
}

testable Int addStringDict(Byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable, StringDict* hm) {
// Unique'ing of symbols within source code
    untt hash = hashCode(text + startBt, lenBts);
    Int hashOffset = hash % (hm->dictSize);
    Int newIndString;
    Bucket* bu = *(hm->dict + hashOffset);

    if (bu == null) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = cap, right u16 = len
        StringValue* firstElem = (StringValue*)newBucket->cont;

        newIndString = stringTable->len;
        push(startBt, stringTable);

        *firstElem = (StringValue){.hash = hash, .indString = newIndString };
        *(hm->dict + hashOffset) = newBucket;
    } else {
        int lenBucket = (bu->capAndLen & 0xFFFF);
        for (int i = 0; i < lenBucket; i++) {
            StringValue strVal = bu->cont[i];
            if (strVal.hash == hash &&
                  memcmp(text + stringTable->cont[strVal.indString], text + startBt, lenBts) == 0) {
                // key already present
                return strVal.indString;
            }
        }

        newIndString = stringTable->len;
        push(startBt, stringTable);
        addValueToBucket(hm->dict + hashOffset, newIndString, hash, hm->a);
    }
    return newIndString;
}

testable Int getStringDict(Byte* text, String* strToSearch, Stackint32_t* stringTable, StringDict* hm) {
// Returns the index of a string within the string table, or -1 if it's not present
    Int lenBts = strToSearch->len;
    untt hash = hashCode(strToSearch->cont, lenBts);
    Int hashOffset = hash % (hm->dictSize);
    if (*(hm->dict + hashOffset) == null) {
        return -1;
    } else {
        Bucket* p = *(hm->dict + hashOffset);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->cont;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].hash == hash
                && memcmp(strToSearch->cont,
                          text + stringTable->cont[stringValues[i].indString], lenBts) == 0) {
                return stringValues[i].indString;
            }
        }
        return -1;
    }
}

//}}}
//{{{ Algorithms

testable void sortPairsDisjoint(Int startInd, Int endInd, Arr(Int) arr) {
// Performs a "twin" ASC sort for faraway (Struct-of-arrays) pairs: for every swap of keys, the
// same swap on values is also performed.
// Example of such a swap: [type1 type2 type3 entity1 entity2 entity3] ->
//                         [type3 type1 type2 entity3 entity1 entity2]
// Sorts only the concrete group of overloads, doesn't touch the generic part.
// Params: startInd = inclusive
//         endInd = exclusive
    Int countPairs = (endInd - startInd)/2;
    if (countPairs == 2) return;
    Int keyEnd = startInd + countPairs;

    for (Int i = startInd; i < keyEnd; i++) {
        Int minValue = arr[i];
        Int minInd = i;
        for (Int j = i + 1; j < keyEnd; j++) {
            if (arr[j] < minValue) {
                minValue = arr[j];
                minInd = j;
            }
        }
        if (minInd == i) {
            continue;
        }

        // swap the keys
        Int tmp = arr[i];
        arr[i] = arr[minInd];
        arr[minInd] = tmp;
        // swap the corresponding values
        tmp = arr[i + countPairs];
        arr[i + countPairs] = arr[minInd + countPairs];
        arr[minInd + countPairs] = tmp;
    }
}


testable void sortPairsDistant(Int startInd, Int endInd, Int distance, Arr(Int) arr) {
// Performs a "twin" ASC sort for faraway (Struct-of-arrays) pairs: for every swap of keys, the
// same swap on values is also performed.
// Example of such a swap: [type1 type2 type3 ... entity1 entity2 entity3 ...] ->
//                         [type3 type1 type2 ... entity3 entity1 entity2 ...]
// The ... is the same number of elements in both halves that don't participate in sorting
// Sorts only the concrete group of overloads, doesn't touch the generic part.
// Params: startInd = inclusive
//         endInd = exclusive
    Int countPairs = (endInd - startInd)/2;
    if (countPairs == 2) return;
    Int keyEnd = startInd + countPairs;

    for (Int i = startInd; i < keyEnd; i++) {
        Int minValue = arr[i];
        Int minInd = i;
        for (Int j = i + 1; j < keyEnd; j++) {
            if (arr[j] < minValue) {
                minValue = arr[j];
                minInd = j;
            }
        }
        if (minInd == i) {
            continue;
        }

        // swap the keys
        Int tmp = arr[i];
        arr[i] = arr[minInd];
        arr[minInd] = tmp;
        // swap the corresponding values
        tmp = arr[i + distance];
        arr[i + distance] = arr[minInd + distance];
        arr[minInd + distance] = tmp;
    }
}


testable void sortPairs(Int startInd, Int endInd, Arr(Int) arr) {
// Performs an ASC sort for compact pairs (array-of-structs) by key:
// Params: startInd = the first index of the overload (the one with the count of concrete overloads)
//        endInd = the last index belonging to the overload (the one with the last entityId)
    Int countPairs = (endInd - startInd)/2;
    if (countPairs == 2) return;

    for (Int i = startInd; i < endInd; i += 2) {
        Int minValue = arr[i];
        Int minInd = i;
        for (Int j = i + 2; j < endInd; j += 2) {
            if (arr[j] < minValue) {
                minValue = arr[j];
                minInd = j;
            }
        }
        if (minInd == i) {
            continue;
        }

        // swap the keys
        Int tmp = arr[i];
        arr[i] = arr[minInd];
        arr[minInd] = tmp;
        // swap the values
        tmp = arr[i + 1];
        arr[i + 1] = arr[minInd + 1];
        arr[minInd + 1] = tmp;
    }
}


testable bool verifyUniquenessPairsDisjoint(Int startInd, Int endInd, Arr(Int) arr) {
// For disjoint (struct-of-arrays) pairs, makes sure the keys are unique and sorted ascending
// Params: startInd = inclusive
//         endInd = exclusive
// Returns: true iff all's OK
    Int currKey = arr[startInd];
    Int i = startInd + 1;
    while (i < endInd) {
        if (arr[i] <= currKey) {
            return false;
        }
        currKey = arr[i];
        ++i;
    }
    return true;
}


private bool binarySearch(Int key, Int start, Int end, Arr(Int) arr) {
    if (end <= start) {
        return -1;
    }
    Int i = start;
    Int j = end - 1;
    if (arr[i] == key) {
        return i;
    } else if (arr[j] == key) {
        return j;
    }

    while (i < j) {
        if (j - i == 1) {
            return -1;
        }
        Int midInd = (i + j)/2;
        Int mid = arr[midInd];
        if (mid > key) {
            j = midInd;
        } else if (mid < key) {
            i = midInd;
        } else {
            return midInd;
        }
    }
    return -1;
}


private void removeDuplicatesInList(InListInt* list) {
// [55 55 55 56] => [55 56]
// Precondition: the list must be sorted
    Int initLen = list->len;
    if (initLen < 2) {
        return;
    }
    Int prevInd = 0;
    Int prevVal = list->cont[0];


    for (Int i = 1; i < initLen; i++) {
        Int currVal = list->cont[i];
        if (currVal != prevVal) {
            prevInd++;
            list->cont[prevInd] = currVal;
        }
    }
    list->len = prevInd + 1;
}

//}}}

//}}}
//{{{ Errors

//{{{ Internal errors

#define iErrorInconsistentSpans          1 // Inconsistent span length / structure of token scopes!
#define iErrorImportedFunctionNotInScope 2 // There is a -1 or something else in the activeBindings
                                           // table for an imported function
#define iErrorParsedFunctionNotInScope   3 // There is a -1 or something else in the activeBindings
                                           // table for a parsed function
#define iErrorOverloadsOverflow          4 // There were more overloads for a function than what
                                           // was allocated
#define iErrorOverloadsNotFull           5 // There were fewer overloads for a function than what
                                           // was allocated
#define iErrorOverloadsIncoherent        6 // The overloads table is incoherent
#define iErrorExpressionIsNotAnExpr      7 // What is supposed to be an expression in the AST is
                                           // not a nodExpr
#define iErrorZeroArityFuncWrongEmit     8 // A 0-arity function has a wrong "emit" (should be one
                                           // of the prefix ones)
#define iErrorGenericTypesInconsistent   9 // Two generic types have inconsistent layout (premature
                                           // end of type)
#define iErrorGenericTypesParamOutOfBou 10 // A type contains a paramId that is out-of-bounds of the
                                           // generic's arity

//}}}
//{{{ Syntax errors

const char errNonAscii[]                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!";
const char errPrematureEndOfInput[]        = "Premature end of input";
const char errUnrecognizedByte[]           = "Unrecognized Byte in source code!";
const char errWordChunkStart[]             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!";
const char errWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!";
const char errWordLengthExceeded[]         = "I don't know why you want an identifier of more than 255 chars, but they aren't supported";
const char errWordWrongCall[]              = "Unsupported kind of call. Only 3 types of call are supported: prefix `foo()`, infix `a .foo` and type calls `L(Int)`";
const char errWordReservedWithDot[]        = "Reserved words may not be called like functions!";
const char errWordInMeta[]                 = "Only ordinary words are allowed inside meta blocks!";
const char errNumericEndUnderscore[]       = "Numeric literal cannot end with underscore!";
const char errNumericWidthExceeded[]       = "Numeric literal width is exceeded!";
const char errNumericBinWidthExceeded[]    = "Integer literals cannot exceed 64 bit!";
const char errNumericFloatWidthExceeded[]  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!";
const char errNumericEmpty[]               = "Could not lex a numeric literal, empty sequence!";
const char errNumericMultipleDots[]        = "Multiple dots in numeric literals are not allowed!";
const char errNumericIntWidthExceeded[]    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!";
const char errPunctuationExtraOpening[]    = "Extra opening punctuation";
const char errPunctuationExtraClosing[]    = "Extra closing punctuation";
const char errPunctuationOnlyInMultiline[] = "The statement separator is not allowed inside expressions!";
const char errPunctuationParamList[]       = "The param list separator `|` must be directly in a statement";
const char errPunctuationUnmatched[]       = "Unmatched closing punctuation";
const char errPunctuationScope[]           = "Scopes may only be opened in multi-line syntax forms";
const char errOperatorUnknown[]            = "Unknown operator";
const char errOperatorAssignmentPunct[]    = "Incorrect assignment operator: must be directly inside an ordinary statement, after the binding name(s) or l-value!";
const char errToplevelAssignment[]         = "Toplevel assignments must have only single word on the left!";
const char errOperatorTypeDeclPunct[]      = "Incorrect type declaration operator placement: must be the first in a statement!";
const char errCoreNotInsideStmt[]          = "Core form must be directly inside statement";
const char errCoreMisplacedElse[]          = "The else statement must be inside an if, ifEq, ifPr or match form";
const char errCoreMissingParen[]           = "Core form requires opening parenthesis/curly brace immediately after keyword!";
const char errBareAtom[]                   = "Malformed token stream (atoms and parentheses must not be bare)";
const char errImportsNonUnique[]           = "Import names must be unique!";
const char errCannotMutateImmutable[]      = "Immutable variables cannot be reassigned to!";
const char errLengthOverflow[]             = "AST nodes length overflow";
const char errPrematureEndOfTokens[]       = "Premature end of input";
const char errUnexpectedToken[]            = "Unexpected token";
const char errCoreFormTooShort[]           = "Core syntax form too short";
const char errCoreFormUnexpected[]         = "Unexpected core form";
const char errCoreFormAssignment[]         = "A core form may not contain any assignments!";
const char errCoreFormInappropriate[]      = "Inappropriate reserved word!";
const char errIfLeft[]                     = "A left-hand clause in an if can only contain variables, boolean literals and expressions!";
const char errIfRight[]                    = "A right-hand clause in an if can only contain atoms, expressions, scopes and some core forms!";
const char errIfEmpty[]                    = "Empty `if` expression";
const char errIfMalformed[]                = "Malformed `if` expression, should look like (if pred => `true case` else `default`)";
const char errIfElseMustBeLast[]           = "An `else` subexpression must be the last thing in an `if`";
const char errFnNameAndParams[]            = "Function signature must look like this: `^{x Type1 y Type 2 =>  ReturnType | body...}`";
const char errFnMissingBody[]              = "Function definition must contain a body which must be a Scope immediately following its parameter list!";
const char errLoopSyntaxError[]            = "A loop should look like `for x = 0; x < 101 { loopBody } `";
const char errLoopHeader[]                 = "A loop header should contain 1 or 2 items: the condition and, optionally, the var declarations";
const char errLoopEmptyBody[]              = "Empty loop body!";
const char errBreakContinueTooComplex[]    = "This statement is too complex! Continues and breaks may contain one thing only: the postitive number of enclosing loops to continue/break!";
const char errBreakContinueInvalidDepth[]  = "Invalid depth of break/continue! It must be a positive 32-bit integer!";
const char errDuplicateFunction[]          = "Duplicate function declaration: a function with same name and arity already exists in this scope!";
const char errExpressionInfixNotSecond[]   = "An infix expression must have the infix operator in second position (not counting possible prefix operators)!";
const char errExpressionError[]             = "Cannot parse expression!";
const char errExpressionCannotContain[]     = "Expressions cannot contain scopes or statements!";
const char errExpressionFunctionless[]      = "Functionless expression!";
const char errTypeDefCannotContain[]       = "Type declarations may only contain types (like Int), type params (like A), type constructors (like List) and parentheses!";
const char errTypeDefError[]               = "Cannot parse type declaration!";
const char errTypeDefParamsError[]         = "Error parsing type params. Should look like this: [T U/2]";
const char errOperatorWrongArity[]          = "Wrong number of arguments for operator!";
const char errUnknownBinding[]              = "Unknown binding!";
const char errUnknownFunction[]             = "Unknown function!";
const char errOperatorUsedInappropriately[] = "Operator used in an inappropriate location!";
const char errAssignment[]                  = "Cannot parse assignment, it must look like `freshIdentifier` = `expression`";
const char errMutation[]                    = "Cannot parse mutation, it must look like `freshIdentifier` += `expression`";
const char errAssignmentShadowing[]         = "Assignment error: existing identifier is being shadowed";
const char errAssignmentToplevelFn[]        = "Assignment of top-level functions must be immutable";
const char errAssignmentLeftSide[]          = "Assignment error: left side must be a var name, a type name, or an existing var with one or more accessors";
const char errAssignmentAccessOnToplevel[]  = "Accessor on the left side of an assignment at toplevel";
const char errReturn[]                      = "Cannot parse return statement, it must look like `return ` {expression}";
const char errScope[]                       = "A scope may consist only of expressions, assignments, function definitions and other scopes!";
const char errLoopBreakOutside[]            = "The break keyword can only be used inside a loop scope!";
const char errTemp[]                        = "Temporary, delete it when finished";

//}}}
//{{{ Type errors

const char errUnknownType[]                 = "Unknown type";
const char errUnexpectedType[]              = "Unexpected to find a type here";
const char errExpectedType[]                = "Expected to find a type here";
const char errUnknownTypeConstructor[]      = "Unknown type constructor";
const char errTypeUnknownFirstArg[]         = "The type of first argument to a call must be known, otherwise I can't resolve the function overload!";
const char errTypeOverloadsIntersect[]      = "Two or more overloads of a single function intersect (impossible to choose one over the other)";
const char errTypeOverloadsOnlyOneZero[]    = "Only one 0-arity function version is possible, otherwise I can't disambiguate the overloads!";
const char errTypeNoMatchingOverload[]      = "No matching function overload was found";
const char errTypeWrongArgumentType[]       = "Wrong argument type";
const char errTypeWrongReturnType[]         = "Wrong return type";
const char errTypeMismatch[]                = "Declared type doesn't match actual type";
const char errTypeMustBeBool[]              = "Expression must have the Bool type";
const char errTypeConstructorWrongArity[]   = "Wrong arity for the type constructor";
const char errTypeOnlyTypesArity[]          = "Only type constructors (i.e. capitalized words) may have arity specification";
const char errTypeTooManyParameters[]       = "Only up to 254 type parameters are supported";
const char errTypeFnSingleReturnType[]      = "More than one return type in a function type";

//}}}

//}}}
//{{{ Forward decls

testable void typeAddHeader(TypeHeader hdr, Compiler* cm);
testable void typeAddTypeParam(Int paramInd, Int arity, Compiler* cm);
private Int typeEncodeTag(untt sort, Int depth, Int arity, Compiler* cm);
private FirstArgTypeId getFirstParamType(TypeId funcTypeId, Compiler* cm);
private bool isFunctionWithParams(TypeId typeId, Compiler* cm);
private OuterTypeId typeGetOuter(FirstArgTypeId typeId, Compiler* cm);
private Int typeGetArity(TypeId typeId, Compiler* cm);
testable Int typeCheckResolveExpr(Int indExpr, Int sentinel, Compiler* cm);

//}}}
//{{{ Lexer
//{{{ LexerConstants

const Byte maxInt[19] = {
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
};
// The ASCII notation for the highest signed 64-bit integer abs value, 9_223_372_036_854_775_807

const Byte maximumPreciselyRepresentedFloatingInt[16] = {
    9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2 };
// 2**53


const int operatorStartSymbols[12] = {
    aExclamation, aSharp, aDollar, aPercent, aAmp, aApostrophe, aTimes, aPlus,
    aDivBy, aGT, aQuestion, aUnderscore
    // Symbols an operator may start with. "-" is absent because it's handled by lexMinus,
    // "=" - by lexEqual, "<" by lexLT, "^" by lexCaret, "|" by lexPipe
};

const char standardText[] = "aliasassertbreakcatchcontinueeacheielsefalsefinallyfor"
                            "ifimplimportifacematchpubreturntruetry"
                            // reserved words end here
                            "IntLongDoubleBoolStringVoidFLADTulencapf1f2printprintErr"
                            "math:pimath:eTU";
// The :standardText prepended to all source code inputs and the hash table to provide a built-in
// string set. Tl's reserved words must be at the start and sorted lexicographically.
// Also they must agree with the "standardStr" in tl.internal.h

const Int standardStringLens[] = {
     5, 6, 5, 5, 8, // continue
     4, 2, 4, 5, 7, // finally
     3, 2, 4, 6, 5, // iface
     5, 3, 6, 4, 3, // try
     // reserved words end here
     3, 4, 6, 4, 6, // String
     4, 1, 1, 1, 1, // D
     2, 3, 3, 2, 2, // f2
     5, 8, 7, 6, 1, // T
     1 // U
};

const Int standardKeywords[] = {
    tokAlias,     tokAssert,  keywBreak,  tokCatch,   keywContinue,
    tokEach,      tokElseIf,  tokElse,    keywFalse,  tokFinally,
    tokFor,       tokIf,      tokImpl,    tokImport,  tokIface,
    tokMatch,     tokMisc,    tokReturn,  keywTrue,   tokTry
};


StandardText getStandardTextLength(void) {
    return (StandardText){
        .len = sizeof(standardText) - 1,
        .firstParsed = (strSentinel + countOperators),
        .firstBuiltin = countOperators };
}

//}}}
//{{{ LexerUtils

#define CURR_BT source[lx->i]
#define NEXT_BT source[lx->i + 1]
#define IND_BT (lx->i - getStandardTextLength().len)
#define VALIDATEI(cond, errInd) if (!(cond)) { throwExcInternal0(errInd, __LINE__, cm); }
#define VALIDATEL(cond, errMsg) if (!(cond)) { throwExcLexer0(errMsg, __LINE__, lx); }


#ifdef TEST

Int pos(Compiler* lx);
void printLexBtrack(Compiler* lx);

#endif

typedef union {
    uint64_t i;
    double   d;
} FloatingBits;


private String* readSourceFile(const Arr(char) fName, Arena* a) {
    String* result = null;
    FILE *file = fopen(fName, "r");
    if (file == null) {
        goto cleanup;
    }
    // Go to the end of the file
    if (fseek(file, 0L, SEEK_END) != 0) {
        goto cleanup;
    }
    long fileSize = ftell(file);
    if (fileSize == -1) {
        goto cleanup;
    }
    Int lenStandard = sizeof(standardText);
    // Allocate our buffer to that size, with space for the standard text in front of it
    result = allocateOnArena(lenStandard + fileSize + 1 + sizeof(String), a);

    // Go back to the start of the file
    if (fseek(file, 0L, SEEK_SET) != 0) {
        goto cleanup;
    }

    memcpy(result->cont, standardText, lenStandard);
    // Read the entire file into memory
    size_t newLen = fread(result->cont + lenStandard, 1, fileSize, file);
    if (ferror(file) != 0 ) {
        fputs("Error reading file", stderr);
    } else {
        result->cont[newLen] = '\0'; // Just to be safe
    }
    result->len = newLen;
    cleanup:
    fclose(file);
    return result;
}

testable String* prepareInput(const char* content, Arena* a) {
// Allocates a source code into an arena after prepending it with the standardText
    if (content == null) return null;
    const char* ind = content;
    Int lenStandard = sizeof(standardText) - 1; // -1 for the invisible \0 char at end
    Int len = 0;
    for (; *ind != '\0'; ind++) {
        len++;
    }

    String* result = allocateOnArena(lenStandard + len + 1 + sizeof(String), a); // +1 for the \0
    result->len = lenStandard + len;
    memcpy(result->cont, standardText, lenStandard);
    memcpy(result->cont + lenStandard, content, len + 1); // + 1 to copy the \0
    return result;
}

private untt nameOfToken(Token tk) {
    return ((untt)tk.lenBts << 24) + (untt)tk.pl2;
}


private untt nameOfStandard(Int strId) {
// Builds a name (id + length) for a standardString after "strFirstNonreserved"
    Int length = standardStringLens[strId];
    Int nameId = strId - strFirstNonReserved + countOperators;
    return ((untt)length << 24) + (untt)(nameId);
}


_Noreturn private void throwExcInternal0(Int errInd, Int lineNumber, Compiler* cm) {
    cm->wasError = true;
    printf("Internal error %d at %d\n", errInd, lineNumber);
    cm->errMsg = stringOfInt(errInd, cm->a);
    printString(cm->errMsg);
    longjmp(excBuf, 1);
}

#define throwExcInternal(errInd, cm) throwExcInternal0(errInd, __LINE__, cm)

_Noreturn private void throwExcLexer0(const char errMsg[], Int lineNumber, Compiler* lx) {
// Sets i to beyond input's length to communicate to callers that lexing is over
    lx->wasError = true;
#ifdef TRACE
    printf("Error on code line %d, i = %d: %s\n", lineNumber, IND_BT, errMsg);
#endif
    lx->errMsg = str(errMsg, lx->a);
    longjmp(excBuf, 1);
}

#define throwExcLexer(msg) throwExcLexer0(msg, __LINE__, lx)

//}}}
//{{{ Lexer proper

private void checkPrematureEnd(Int requiredSymbols, Compiler* lx) {
// Checks that there are at least 'requiredSymbols' symbols left in the input
    VALIDATEL(lx->i + requiredSymbols <= lx->inpLength, errPrematureEndOfInput)
}


private void setSpanLengthLexer(Int tokenInd, Compiler* lx) {
// Finds the top-level punctuation opener by its index, and sets its lengths.
// Called when the matching closer is lexed. Does not pop anything from the "lexBtrack"
    lx->tokens.cont[tokenInd].lenBts = lx->i - lx->tokens.cont[tokenInd].startBt + 1;
    lx->tokens.cont[tokenInd].pl2 = lx->tokens.len - tokenInd - 1;
}


private BtToken getLexContext(Compiler* lx) {
    if (!hasValues(lx->lexBtrack)) {
        return (BtToken) { .tp = tokInt, .tokenInd = -1, .spanLevel = 0 };
    }
    return peek(lx->lexBtrack);
}


private void setStmtSpanLength(Int tokenInd, Compiler* lx) {
// Correctly calculates the lenBts for a single-line, statement-type span
// This is for correctly calculating lengths of statements when they are ended by parens in
// case of a gap before "}", for example:
//  ` { asdf    <- statement actually ended here
//    }`        <- but we are in this position now
    Token lastToken = lx->tokens.cont[lx->tokens.len - 1];
    Int byteAfterLastToken = lastToken.startBt + lastToken.lenBts;
    Int byteAfterLastPunct = lx->lastClosingPunctInd + 1;
    Int lenBts = (byteAfterLastPunct > byteAfterLastToken ? byteAfterLastPunct : byteAfterLastToken)
                    - lx->tokens.cont[tokenInd].startBt;
    lx->tokens.cont[tokenInd].lenBts = lenBts;
    lx->tokens.cont[tokenInd].pl2 = lx->tokens.len - tokenInd - 1;
}


private void addStatementSpan(untt stmtType, Int startBt, Compiler* lx) {
    push(((BtToken){ .tp = stmtType, .tokenInd = lx->tokens.len, .spanLevel = slStmt }),
                    lx->lexBtrack);
    pushIntokens((Token){ .tp = stmtType, .startBt = startBt, .lenBts = 0 }, lx);
}


private void wrapInAStatementStarting(Int startBt, Arr(Byte) source, Compiler* lx) {
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel <= slDoubleScope) {
            addStatementSpan(tokStmt, startBt, lx);
        }
    } else {
        addStatementSpan(tokStmt, startBt, lx);
    }
}


private void wrapInAStatement(Arr(Byte) source, Compiler* lx) {
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel <= slDoubleScope) {
            addStatementSpan(tokStmt, lx->i, lx);
        }
    } else {
        addStatementSpan(tokStmt, lx->i, lx);
    }
}

private void maybeBreakStatement(Compiler* lx) {
// If the lexer is in a statement, we need to close it
    BtToken ctx = getLexContext(lx);
    if(ctx.spanLevel == slStmt) {
        setStmtSpanLength(ctx.tokenInd, lx);
        pop(lx->lexBtrack);
    }
}


private int64_t calcIntegerWithinLimits(Compiler* lx) {
    int64_t powerOfTen = (int64_t)1;
    int64_t result = 0;
    Int j = lx->numeric.len - 1;

    Int loopLimit = -1;
    while (j > loopLimit) {
        result += powerOfTen*lx->numeric.cont[j];
        powerOfTen *= 10;
        j--;
    }
    return result;
}

private bool integerWithinDigits(const Byte* b, Int bLength, Compiler* lx){
// Is the current numeric <= b if they are regarded as arrays of decimal digits (0 to 9)?
    if (lx->numeric.len != bLength) return (lx->numeric.len < bLength);
    for (Int j = 0; j < lx->numeric.len; j++) {
        if (lx->numeric.cont[j] < b[j]) return true;
        if (lx->numeric.cont[j] > b[j]) return false;
    }
    return true;
}


private Int calcInteger(int64_t* result, Compiler* lx) {
    if (lx->numeric.len > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lx)) return -1;
    *result = calcIntegerWithinLimits(lx);
    return 0;
}


private int64_t calcHexNumber(Compiler* lx) {
    int64_t result = 0;
    int64_t powerOfSixteen = 1;
    Int j = lx->numeric.len - 1;

    // If the literal is full 16 bits long, then its upper sign contains the sign bit
    Int loopLimit = -1;
    while (j > loopLimit) {
        result += powerOfSixteen*lx->numeric.cont[j];
        powerOfSixteen = powerOfSixteen << 4;
        j--;
    }
    return result;
}


private void hexNumber(Arr(Byte) source, Compiler* lx) {
// Lexes a hexadecimal numeric literal (integer or floating-point)
// Examples of accepted expressions: 0xCAFE_BABE, 0xdeadbeef, 0x123_45A
// Examples of NOT accepted expressions: 0xCAFE_babe, 0x_deadbeef, 0x123_
// Checks that the input fits into a signed 64-bit fixnum.
// TODO add floating-pointt literals like 0x12FA
    checkPrematureEnd(2, lx);
    lx->numeric.len = 0;
    Int j = lx->i + 2;
    while (j < lx->inpLength) {
        Byte cByte = source[j];
        if (isDigit(cByte)) {
            pushInnumeric(cByte - aDigit0, lx);
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            pushInnumeric(cByte - aALower + 10, lx);
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            pushInnumeric(cByte - aAUpper + 10, lx);
        } else if (cByte == aApostrophe && (j == lx->inpLength - 1 || isHexDigit(source[j + 1]))) {
            throwExcLexer(errNumericEndUnderscore);
        } else {
            break;
        }
        VALIDATEL(lx->numeric.len <= 16, errNumericBinWidthExceeded)
        j++;
    }
    int64_t resultValue = calcHexNumber(lx);
    pushIntokens((Token){ .tp = tokInt, .pl1 = resultValue >> 32, .pl2 = resultValue & LOWER32BITS,
                .startBt = lx->i, .lenBts = j - lx->i }, lx);
    lx->numeric.cont = 0;
    lx->i = j; // CONSUME the hex number
}


private Int calcFloating(double* result, Int powerOfTen, Arr(Byte) source, Compiler* lx) {
// Parses the floating-point numbers using just the "fast path" of David Gay's "strtod" function,
// extended to 16 digits.
// I.e. it handles only numbers with 15 digits or 16 digits with the first digit not 9,
// and decimal powers within [-22; 22]. Parsing the rest of numbers exactly is a huge and pretty
// useless effort. Nobody needs these floating literals in text form: they are better input in
// binary, or at least text-hex or text-binary.
// Input: array of bytes that are digits (without leading zeroes), and the negative power of ten.
// So for '0.5' the input would be (5 -1), and for '1.23000' (123000 -5).
// Example, for input text '1.23' this function would get the args: ([1 2 3] 1)
// Output: a 64-bit floating-pointt number, encoded as a long (same bits)
    Int indTrailingZeroes = lx->numeric.len - 1;
    Int ind = lx->numeric.len;
    while (indTrailingZeroes > -1 && lx->numeric.cont[indTrailingZeroes] == 0) {
        indTrailingZeroes--;
    }

    // how many powers of 10 need to be knocked off the significand to make it fit
    Int significandNeeds = ind - 16 >= 0 ? ind - 16 : 0;
    // how many power of 10 significand can knock off (these are just trailing zeroes)
    Int significantCan = ind - indTrailingZeroes - 1;
    // how many powers of 10 need to be added to the exponent to make it fit
    Int exponentNeeds = -22 - powerOfTen;
    // how many power of 10 at maximum can be added to the exponent
    Int exponentCanAccept = 22 - powerOfTen;

    if (significantCan < significandNeeds
        || significantCan < exponentNeeds
        || significandNeeds > exponentCanAccept) {
        return -1;
    }

    // Transfer of decimal powers from significand to exponent to make them both fit within their
    // respective limits
    // (10000 -6) -> (1 -2); (10000 -3) -> (10 0)
    Int transfer = (significandNeeds >= exponentNeeds) ? (
             (ind - significandNeeds == 16 && significandNeeds < significantCan
              && significandNeeds + 1 <= exponentCanAccept) ?
                    (significandNeeds + 1) : significandNeeds
        ) : exponentNeeds;
    lx->numeric.len -= transfer;
    Int finalPowerTen = powerOfTen + transfer;

    if (!integerWithinDigits(maximumPreciselyRepresentedFloatingInt,
                            sizeof(maximumPreciselyRepresentedFloatingInt), lx)) {
        return -1;
    }

    int64_t significandInt = calcIntegerWithinLimits(lx);
    double significand = (double)significandInt; // precise
    double exponent = pow(10.0, (double)(abs(finalPowerTen)));

    *result = (finalPowerTen > 0) ? (significand*exponent) : (significand/exponent);
    return 0;
}


testable int64_t longOfDoubleBits(double d) {
    FloatingBits un = {.d = d};
    return un.i;
}


private double doubleOfLongBits(int64_t i) {
    FloatingBits un = {.i = i};
    return un.d;
}

private void decNumber(bool isNegative, Arr(Byte) source, Compiler* lx) {
// Lexes a decimal numeric literal (integer or floating-point). Adds a token.
// TODO: add support for the '1.23E4' format
    Int j = (isNegative) ? (lx->i + 1) : lx->i;
    Int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
    bool metDot = false;
    bool metNonzero = false;
    Int maximumInd = (lx->i + 40 > lx->inpLength) ? (lx->i + 40) : lx->inpLength;
    while (j < maximumInd) {
        Byte cByte = source[j];

        if (isDigit(cByte)) {
            if (metNonzero) {
                pushInnumeric(cByte - aDigit0, lx);
            } else if (cByte != aDigit0) {
                metNonzero = true;
                pushInnumeric(cByte - aDigit0, lx);
            }
            if (metDot) {
                ++digitsAfterDot;
            }
        } else if (cByte == aApostrophe) {
            VALIDATEL(j != (lx->inpLength - 1) && isDigit(source[j + 1]), errNumericEndUnderscore)
        } else if (cByte == aDot) {
            VALIDATEL(!metDot, errNumericMultipleDots)
            metDot = true;
        } else {
            break;
        }
        j++;
    }

    VALIDATEL(j >= lx->inpLength || !isDigit(source[j]), errNumericWidthExceeded)

    if (metDot) {
        double resultValue = 0;
        Int errorCode = calcFloating(&resultValue, -digitsAfterDot, source, lx);
        VALIDATEL(errorCode == 0, errNumericFloatWidthExceeded)

        int64_t bitsOfFloat = longOfDoubleBits((isNegative) ? (-resultValue) : resultValue);
        pushIntokens((Token){ .tp = tokDouble, .pl1 = (bitsOfFloat >> 32),
                    .pl2 = (bitsOfFloat & LOWER32BITS), .startBt = lx->i, .lenBts = j - lx->i}, lx);
    } else {
        int64_t resultValue = 0;
        Int errorCode = calcInteger(&resultValue, lx);
        VALIDATEL(errorCode == 0, errNumericIntWidthExceeded)

        if (isNegative) resultValue = -resultValue;
        pushIntokens((Token){ .tp = tokInt, .pl1 = resultValue >> 32, .pl2 = resultValue & LOWER32BITS,
                .startBt = lx->i, .lenBts = j - lx->i }, lx);
    }
    lx->i = j; // CONSUME the decimal number
}


private void lexNumber(Arr(Byte) source, Compiler* lx) {
    wrapInAStatement(source, lx);
    Byte cByte = CURR_BT;
    if (lx->i == lx->inpLength - 1 && isDigit(cByte)) {
        pushIntokens((Token){ .tp = tokInt, .pl2 = cByte - aDigit0, .startBt = lx->i, .lenBts = 1 }, lx);
        lx->i++; // CONSUME the single-digit number
        return;
    }

    Byte nByte = NEXT_BT;
    if (nByte == aXLower) {
        hexNumber(source, lx);
    } else {
        decNumber(false, source, lx);
    }
    lx->numeric.len = 0;
}


private void openPunctuation(untt tType, untt spanLevel, Int startBt, Compiler* lx) {
// Adds a token which serves punctuation purposes, i.e. either a ( or  a [
// These tokens are used to define the structure, that is, nesting within the AST.
// Upon addition, they are saved to the backtracking stack to be updated with their length
// once it is known. Consumes no bytes
    push(((BtToken){ .tp = tType, .tokenInd = lx->tokens.len, .spanLevel = spanLevel}),
            lx->lexBtrack);
    pushIntokens((Token) {.tp = tType, .pl1 = (tType < firstScopeTokenType) ? 0 : spanLevel,
                          .startBt = startBt }, lx);
}


private void lexReservedWord(untt reservedWordType, Int startBt, Int lenBts,
                             Arr(Byte) source, Compiler* lx) {
// Lexer action for a paren-type or statement-type reserved word.
// Precondition: we are looking at the character immediately after the keyword
    StackBtToken* bt = lx->lexBtrack;
    if (reservedWordType >= firstScopeTokenType) {
        if (hasValues(lx->lexBtrack)) {
            BtToken top = peek(lx->lexBtrack);
            VALIDATEL(top.spanLevel == slScope && top.tp != tokStmt, errPunctuationScope)
        }
        push(((BtToken){ .tp = reservedWordType, .tokenInd = lx->tokens.len,
                    .spanLevel = slDoubleScope }), lx->lexBtrack);
        pushIntokens((Token){ .tp = reservedWordType, .pl1 = slDoubleScope,
            .startBt = startBt, .lenBts = lenBts }, lx);
    } else if (reservedWordType >= firstSpanTokenType) {
        VALIDATEL(!hasValues(bt) || peek(bt).spanLevel == slScope, errCoreNotInsideStmt)
        addStatementSpan(reservedWordType, startBt, lx);
    }
}


private bool wordChunk(Arr(Byte) source, Compiler* lx) {
// Lexes a single chunk of a word, i.e. the characters between two minuses (or the whole word
// if there are no minuses). Returns True if the lexed chunk was capitalized
    bool result = false;
    checkPrematureEnd(1, lx);

    Byte currBt = CURR_BT;
    if (isCapitalLetter(currBt)) {
        result = true;
    } else VALIDATEL(isLowercaseLetter(currBt), errWordChunkStart)

    lx->i++; // CONSUME the first letter of the word
    while (lx->i < lx->inpLength && isAlphanumeric(CURR_BT)) {
        lx->i++; // CONSUME alphanumeric characters
    }
    return result;
}


private void closeStatement(Compiler* lx) {
// Closes the current statement. Consumes no tokens
    BtToken top = peek(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errPunctuationOnlyInMultiline)
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->lexBtrack);
    }
}


private void wordNormal(untt wordType, Int uniqueStringId, Int startBt, Int realStartBt,
                        bool wasCapitalized, Arr(Byte) source, Compiler* lx) {
    Int lenBts = lx->i - realStartBt;
    Token newToken = (Token){ .tp = wordType, .pl2 = uniqueStringId,
                             .startBt = realStartBt, .lenBts = lenBts };

    if (lx->i < lx->inpLength && CURR_BT == aParenLeft && wordType == tokWord) {
        // A prefix function call or a type call/constructor (only the parser can distinguish)
        newToken.tp = wasCapitalized ? tokTypeCall : tokCall;
        newToken.pl1 = uniqueStringId;
        newToken.pl2 = 0;
        push(((BtToken){ .tp = newToken.tp, .tokenInd = lx->tokens.len, .spanLevel = slSubexpr }),
             lx->lexBtrack);
        ++lx->i; // CONSUME the opening "(" of the call
    } else if (wordType == tokWord) {
        wrapInAStatementStarting(startBt, source, lx);
        newToken.tp = (wasCapitalized ? tokTypeName : tokWord);
    } else if (wordType == tokCall)  {
        wrapInAStatementStarting(startBt, source, lx);
        newToken.pl1 = uniqueStringId;
        newToken.pl2 = 0;
    } else if (wordType == tokFieldAcc) {
        wrapInAStatementStarting(startBt, source, lx);
    }
    pushIntokens(newToken, lx);
}


private void wordReserved(untt wordType, Int wordId, Int startBt, Int realStartBt,
                          Arr(Byte) source, Compiler* lx) {
    Int lenBts = lx->i - startBt;
    Int keywordTp = standardKeywords[wordId];
    VALIDATEL(wordType != tokFieldAcc, errWordReservedWithDot)
    if (keywordTp < firstSpanTokenType) {
        if (keywordTp == keywTrue) {
            wrapInAStatementStarting(startBt, source, lx);
            pushIntokens((Token){.tp=tokBool, .pl2=1, .startBt=realStartBt, .lenBts=4}, lx);
        } else if (keywordTp == keywFalse) {
            //VALIDATEL(!hasValues(bt) || peek(bt).spanLevel == slScope, errCoreNotInsideStmt)
            wrapInAStatementStarting(startBt, source, lx);
            pushIntokens((Token){.tp=tokBool, .pl2=0, .startBt=realStartBt, .lenBts=5}, lx);
        } else if (keywordTp == keywBreak) {
            wrapInAStatementStarting(startBt, source, lx);
            pushIntokens((Token){.tp=tokBreakCont, .pl1=0, .pl2=0, .startBt=realStartBt, .lenBts=5}, lx);
        } else if (keywordTp == keywContinue) {
            wrapInAStatementStarting(startBt, source, lx);
            pushIntokens((Token){.tp=tokBreakCont, .pl1=1, .pl2=0, .startBt=realStartBt, .lenBts=5}, lx);
        }
    } else {
        lexReservedWord(keywordTp, realStartBt, lenBts, source, lx);
    }
}


private void wordInternal(untt wordType, Arr(Byte) source, Compiler* lx) {
// Lexes a word (both reserved and identifier) according to Tl's rules.
// Precondition: we are pointing at the first letter character of the word (i.e. past the possible
// "." or ":")
// Examples of acceptable words: A:B:c:d, asdf123, ab:cd45
// Examples of unacceptable words: 1asdf23, ab:cd_45
    Int startBt = lx->i;
    bool wasCapitalized = wordChunk(source, lx);

    while (lx->i < (lx->inpLength - 1)) {
        Byte currBt = CURR_BT;
        if (currBt == aColon) {
            Byte nextBt = NEXT_BT;
            if (isLetter(nextBt)) {
                lx->i++; // CONSUME the minus
                bool isCurrCapitalized = wordChunk(source, lx);
                VALIDATEL(!wasCapitalized, errWordCapitalizationOrder)
                wasCapitalized = isCurrCapitalized;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    Int realStartBt = (wordType == tokWord) ? startBt : (startBt - 1);
    // accounting for the initial ".", ":" or other symbol
    Int lenString = lx->i - startBt;
    VALIDATEL(lenString <= maxWordLength, errWordLengthExceeded)
    Int uniqueStringId = addStringDict(source, startBt, lenString, lx->stringTable, lx->stringDict);
    if (uniqueStringId - countOperators < strFirstNonReserved)  {
        wordReserved(wordType, uniqueStringId - countOperators, startBt, realStartBt, source, lx);
    } else {
        wrapInAStatementStarting(startBt, source, lx);
        wordNormal(wordType, uniqueStringId, startBt, realStartBt, wasCapitalized, source, lx);
    }
}

private void lexWord(Arr(Byte) source, Compiler* lx) {
    wordInternal(tokWord, source, lx);
}


private void lexDot(Arr(Byte) source, Compiler* lx) {
// The dot is a start of a field accessor or a function call

    VALIDATEL(lx->i < lx->inpLength - 1 && isLetter(NEXT_BT) && lx->tokens.len > 0,
        errUnexpectedToken);
    Token prevTok = lx->tokens.cont[lx->tokens.len - 1];
    ++lx->i; // CONSUME the dot
    if (prevTok.tp == tokWord && lx->i == (prevTok.startBt + prevTok.lenBts + 1))  {
        wordInternal(tokFieldAcc, source, lx);
    } else {
        wordInternal(tokCall, source, lx);
    }

}


private void processAssignment(Int mutType, untt opType, Compiler* lx) {
// Params: Handles the "=", "<-" and "+=" tokens. Changes existing stmt
// token into tokAssignLeft and opens up a new tokAssignRight span. Doesn't consume anything
    BtToken currSpan = peek(lx->lexBtrack);
    VALIDATEL(currSpan.tp == tokStmt, errOperatorAssignmentPunct);

    Int assignmentStartInd = currSpan.tokenInd;
    Token* tok = (lx->tokens.cont + assignmentStartInd);
    tok->tp = tokAssignLeft;
    if (assignmentStartInd == (lx->tokens.len - 2) && mutType == 0)  { // only one token on the left
        Token wordTk = lx->tokens.cont[assignmentStartInd + 1];
        VALIDATEL(wordTk.tp == tokWord, errAssignment);
        tok->pl1 = wordTk.pl2; // nameId
        tok->startBt = wordTk.startBt;
        tok->lenBts = wordTk.lenBts;
        lx->tokens.len--; // delete the word token because its data now lives in tokAssignLeft
    } else {
        if (lx->tokens.cont[assignmentStartInd + 1].tp == tokTypeName){
            tok->pl1 = assiType;
        } else if (mutType == 1) {
            tok->pl1 = assiReassign;
        } else if (mutType == 2) {
            tok->pl1 = BIG + opType;
        }
    }
    setStmtSpanLength(assignmentStartInd, lx);
    lx->lexBtrack->cont[lx->lexBtrack->len - 1] = (BtToken){ .tp = tokAssignRight, .spanLevel = slStmt,
        .tokenInd = lx->tokens.len };
    pushIntokens((Token){ .tp = tokAssignRight, .startBt = lx->i }, lx);
}


private void lexColon(Arr(Byte) source, Compiler* lx) { //:lexColon
// Handles keyword arguments ":asdf"
    if (lx->i < lx->inpLength - 1) {
        Byte nextBt = NEXT_BT;
        if (isLowercaseLetter(nextBt)) {
            ++lx->i; // CONSUME the ":"
            wordInternal(tokKwArg, source, lx);
            return;
        }
    }
    throwExcLexer(errOperatorUnknown);
}


private void lexSemicolon(Arr(Byte) source, Compiler* lx) {
    if (hasValues(lx->lexBtrack) && peek(lx->lexBtrack).spanLevel != slScope) {
        closeStatement(lx);
        ++lx->i;  // CONSUME the ;
    }
}


private void lexOperator(Arr(Byte) source, Compiler* lx) { //:lexOperator
    wrapInAStatement(source, lx);

    Byte firstSymbol = CURR_BT;
    Byte secondSymbol = (lx->inpLength > lx->i + 1) ? source[lx->i + 1] : 0;
    Byte thirdSymbol = (lx->inpLength > lx->i + 2) ? source[lx->i + 2] : 0;
    Byte fourthSymbol = (lx->inpLength > lx->i + 3) ? source[lx->i + 3] : 0;
    Int k = 0;
    Int opType = -1; // corresponds to the op... operator types
    //OpDef (*operators)[countOperators] = lx->langDef->operators;
    while (k < countOperators && TL_OPERATORS[k].bytes[0] < firstSymbol) {
        k++;
    }
    while (k < countOperators && TL_OPERATORS[k].bytes[0] == firstSymbol) {
        OpDef opDef = TL_OPERATORS[k];
        Byte secondTentative = opDef.bytes[1];
        if (secondTentative != 0 && secondTentative != secondSymbol) {
            k++;
            continue;
        }
        Byte thirdTentative = opDef.bytes[2];
        if (thirdTentative != 0 && thirdTentative != thirdSymbol) {
            k++;
            continue;
        }
        Byte fourthTentative = opDef.bytes[3];
        if (fourthTentative != 0 && fourthTentative != fourthSymbol) {
            k++;
            continue;
        }
        opType = k;
        break;
    }
    VALIDATEL(opType > -1, errOperatorUnknown)

    OpDef opDef = TL_OPERATORS[opType];
    bool isAssignment = false;

    Int lengthOfBaseOper = (opDef.bytes[1] == 0) ? 1
        : (opDef.bytes[2] == 0 ? 2 : (opDef.bytes[3] == 0 ? 3 : 4));
    Int j = lx->i + lengthOfBaseOper;
    if (opDef.assignable && j < lx->inpLength && source[j] == aEqual) {
        isAssignment = true;
        j++;
    }
    if (isAssignment) { // mutation operators like "*=" or "*.="
        processAssignment(2, opType, lx);
    } else {
        if (j < lx->inpLength && source[j] == aParenLeft) {
            push(((BtToken){ .tp = tokOperator, .tokenInd = lx->tokens.len,
                        .spanLevel = slSubexpr }), lx->lexBtrack);
            pushIntokens((Token){ .tp = tokOperator, .pl1 = opType, .startBt = lx->i }, lx);
            ++j; // CONSUME the opening "(" of the operator call
        } else {
            pushIntokens((Token){ .tp = tokOperator, .pl1 = opType, .startBt = lx->i,
                         .lenBts = j - lx->i}, lx);
        }
    }
    lx->i = j; // CONSUME the operator
}

private bool isOperatorTypeful(Int opType) {
    return false;
}


private void lexEqual(Arr(Byte) source, Compiler* lx) { //:lexEqual
// The humble "=" can be the definition statement, a marker that separates signature from definition,
// or an arrow "=>"
    checkPrematureEnd(2, lx);
    Byte nextBt = NEXT_BT;
    if (nextBt == aEqual) {
        lexOperator(source, lx); // ==
    } else if (nextBt == aGT) {
        pushIntokens((Token){ .tp = tokMisc, .pl1 = miscArrow, .startBt = lx->i, .lenBts = 2 }, lx);
        lx->i += 2;  // CONSUME the arrow "=>"
    } else {
        processAssignment(0, 0, lx);
        lx->i++; // CONSUME the =
    }
}


private void lexLT(Arr(Byte) source, Compiler* lx) { //:lexLT
// Handles "<-" (reassignment) as well as operators
    if ((lx->i + 1 < lx->inpLength) && NEXT_BT == aMinus) {
        processAssignment(1, 0, lx);
        lx->i += 2; // CONSUME the "<-"
    } else {
        lexOperator(source, lx);
    }
}

private void lexTilde(Arr(Byte) source, Compiler* lx) { //:lexTilde
    if ((lx->i < lx->inpLength - 1) && NEXT_BT == aTilde) {
        pushIntokens((Token){ .tp = tokTilde, .pl1 = 2, .startBt = lx->i - 1, .lenBts = 2 }, lx);
        lx->i += 2; // CONSUME the "~~"
    } else {
        pushIntokens((Token){ .tp = tokTilde, .pl1 = 1, .startBt = lx->i - 1, .lenBts = 2 }, lx);
        ++lx->i; // CONSUME the "~"
    }
}

private void lexNewline(Arr(Byte) source, Compiler* lx) { //:lexNewline
// Tl is not indentation-sensitive, but it is newline-sensitive. Thus, a newline charactor closes
// the current statement unless it's inside an inline span (i.e. parens or accessor parens)
    pushInnewlines(lx->i, lx);
    maybeBreakStatement(lx);

    lx->i++;     // CONSUME the LF
    while (lx->i < lx->inpLength) {
        if (CURR_BT != aSpace && CURR_BT != aTab && CURR_BT != aCarrReturn) {
            break;
        }
        lx->i++; // CONSUME a space or tab
    }
}

private void lexElision(Arr(Byte) source, Compiler* lx) {
// Tl separates between comments for documentation (which live in meta info and are
// spelt as [`comment`]) and elisions for, well, temporarily eliding code
// Elisions are of the / * comm * / form like in old-school C, except they are also nestable
    lx->i += 2; // CONSUME the "/ *"
    Int elisionLevel = 1;

    Int j = lx->i;
    while (j < lx->inpLength - 1) {
        Byte cByte = source[j];
        if (cByte == aTimes && NEXT_BT == aDivBy) {
            --elisionLevel;
            if (elisionLevel == 0)  {
                lx->i = j + 2; // CONSUME the elision
                return;
            }
        } else if (cByte == aDivBy && NEXT_BT == aTimes) {
            ++elisionLevel;
        } else {
            j++;
        }
    }
    throwExcLexer(errPrematureEndOfInput);
}


private void lexMinus(Arr(Byte) source, Compiler* lx) {
// Handles the binary operator as well as the unary negation operator
    if (lx->i == lx->inpLength - 1) {
        lexOperator(source, lx);
    } else {
        Byte nextBt = NEXT_BT;
        if (isDigit(nextBt)) {
            wrapInAStatement(source, lx);
            decNumber(true, source, lx);
            lx->numeric.len = 0;
        } else {
            lexOperator(source, lx);
        }
    }
}


private void lexDivBy(Arr(Byte) source, Compiler* lx) {
// Handles the binary operator as well as the elisions
    if (lx->i + 1 < lx->inpLength && NEXT_BT == aTimes) {
        lexElision(source, lx);
    } else {
        lexOperator(source, lx);
    }
}


private void lexParenLeft(Arr(Byte) source, Compiler* lx) {
    Int j = lx->i + 1;
    VALIDATEL(j < lx->inpLength, errPunctuationUnmatched)
    wrapInAStatement(source, lx);
    openPunctuation(tokParens, slSubexpr, lx->i, lx);
    ++lx->i; // CONSUME the left parenthesis
}


private void lexParenRight(Arr(Byte) source, Compiler* lx) {
    Int startInd = lx->i;

    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);

    setSpanLengthLexer(top.tokenInd, lx);
    lx->i++; // CONSUME the closing ")"

    lx->lastClosingPunctInd = startInd;
}


private void lexCurlyLeft(Arr(Byte) source, Compiler* lx) {
    Int j = lx->i + 1;
    VALIDATEL(j < lx->inpLength, errPunctuationUnmatched)
    closeStatement(lx);
    openPunctuation(tokScope, slScope, lx->i, lx);
    ++lx->i; // CONSUME the left bracket
}


private void lexCurlyRight(Arr(Byte) source, Compiler* lx) {
// A closing curly brace may close the following configurations of lexer backtrack:
// 1. [scope stmt] - if it's just a scope nested within another scope or a function
// 2. [coreForm stmt] - eg. if it's closing the function body
// 3. [coreForm scope stmt] - eg. if it's closing the {} part of an "if"
    Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);

    // since a closing curly is closing something with statements inside it, like a lex scope
    // or a core syntax form, we need to close that statement before closing its parent
    if (top.spanLevel == slStmt && bt->cont[bt->len - 1].spanLevel <= slDoubleScope) {
        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }
    setSpanLengthLexer(top.tokenInd, lx);

    if (top.spanLevel == slScope && hasValues(bt) && peek(bt).spanLevel == slDoubleScope)  {
        top = pop(bt);
        setSpanLengthLexer(top.tokenInd, lx);
    }

    if (hasValues(bt)) { lx->lastClosingPunctInd = startInd; }
    lx->i++; // CONSUME the closing "}"
}


private void lexBracketLeft(Arr(Byte) source, Compiler* lx) {
    wrapInAStatement(source, lx);
    // TODO openPunctuation(tokMeta, slSubexpr, lx->i, lx);
    lx->i++; // CONSUME the `[`
}


private void lexBracketRight(Arr(Byte) source, Compiler* lx) {
    // TODO
    ++lx->i;
    return;
   /*
    Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);
    if (top.tp != tokMeta) {
        throwExcLexer(errPunctuationUnmatched);
    }
    setSpanLengthLexer(top.tokenInd, lx);
    lx->i++; // CONSUME the `]`
    if (hasValues(lx->lexBtrack)) { lx->lastClosingPunctInd = startInd; }
   */
}


private void lexCaret(Arr(Byte) source, Compiler* lx) {
    Int j = lx->i + 1;
    if (j < lx->inpLength && NEXT_BT == aCurlyLeft) {
        push(((BtToken){ .tp = tokFn, .tokenInd = lx->tokens.len, .spanLevel = slScope }),
                lx->lexBtrack);
        pushIntokens((Token){ .tp = tokFn, .pl1 = slScope, .startBt = lx->i, .lenBts = 0 }, lx);
        lx->i += 2; // CONSUME the `^{`
    } else {
        lexOperator(source, lx);
    }
}


private void lexComma(Arr(Byte) source, Compiler* lx) {
    pushIntokens((Token){ .tp = tokMisc, .pl1 = miscComma, .startBt = lx->i, .lenBts = 1 }, lx);
    lx->i++; // CONSUME the `,`
}


private void lexPipe(Arr(Byte) source, Compiler* lx) {
// Closes the current statement and changes its type to tokParamList
    Int j = lx->i + 1;
    if (j < lx->inpLength && NEXT_BT == aPipe) {
        lexOperator(source, lx);
    } else {
        BtToken top = peek(lx->lexBtrack);
        VALIDATEL(top.spanLevel == slStmt, errPunctuationParamList)
        setStmtSpanLength(top.tokenInd, lx);
        lx->tokens.cont[top.tokenInd].tp = tokParamList;
        pop(lx->lexBtrack);
        lx->i++; // CONSUME the `|`
    }
}


private void lexSpace(Arr(Byte) source, Compiler* lx) {
    lx->i++; // CONSUME the space
    while (lx->i < lx->inpLength && CURR_BT == aSpace) {
        lx->i++; // CONSUME a space
    }
}


private void lexStringLiteral(Arr(Byte) source, Compiler* lx) {
    wrapInAStatement(source, lx);
    Int j = lx->i + 1;
    for (; j < lx->inpLength && source[j] != aBacktick; j++);
    VALIDATEL(j != lx->inpLength, errPrematureEndOfInput)
    pushIntokens((Token){.tp=tokString, .startBt=(lx->i), .lenBts=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


private void lexUnexpectedSymbol(Arr(Byte) source, Compiler* lx) {
    throwExcLexer(errUnrecognizedByte);
}

private void lexNonAsciiError(Arr(Byte) source, Compiler* lx) {
    throwExcLexer(errNonAscii);
}


private LexerFunc (*tabulateDispatch(Arena* a))[256] {
    LexerFunc (*result)[256] = allocateOnArena(256*sizeof(LexerFunc), a);
    LexerFunc* p = *result;
    for (Int i = 0; i < 128; i++) {
        p[i] = &lexUnexpectedSymbol;
    }
    for (Int i = 128; i < 256; i++) {
        p[i] = &lexNonAsciiError;
    }
    for (Int i = aDigit0; i <= aDigit9; i++) {
        p[i] = &lexNumber;
    }

    for (Int i = aALower; i <= aZLower; i++) {
        p[i] = &lexWord;
    }
    for (Int i = aAUpper; i <= aZUpper; i++) {
        p[i] = &lexWord;
    }
    p[aDot] = &lexDot;
    p[aEqual] = &lexEqual;
    p[aLT] = &lexLT; // to handle the reassignment `<-`
    p[aTilde] = &lexTilde;

    for (Int i = sizeof(operatorStartSymbols)/4 - 1; i > -1; i--) {
        p[operatorStartSymbols[i]] = &lexOperator;
    }
    p[aMinus] = &lexMinus; // to handle literal negation
    p[aParenLeft] = &lexParenLeft;
    p[aParenRight] = &lexParenRight;
    p[aCurlyLeft] = &lexCurlyLeft;
    p[aCurlyRight] = &lexCurlyRight;
    p[aBracketLeft] = &lexBracketLeft;
    p[aBracketRight] = &lexBracketRight;
    p[aCaret] = &lexCaret; // to handle the functions `^{}`
    p[aComma] = &lexComma;
    p[aPipe] = &lexPipe;
    p[aDivBy] = &lexDivBy; // to handle the comments `/ *`
    p[aColon] = &lexColon; // to handle keyword args `:a`
    p[aSemicolon] = &lexSemicolon; // the statement terminator

    p[aSpace] = &lexSpace;
    p[aCarrReturn] = &lexSpace;
    p[aNewline] = &lexNewline; // to make the newline a statement terminator sometimes
    p[aBacktick] = &lexStringLiteral;
    return result;
}


private void tabulateOperators() {
// The set of operators in the language. Must agree in order with tl.internal.h
    /*
    OpDef (*result)[countOperators] = allocateOnArena(countOperators*sizeof(OpDef), a);
    OpDef* p = *result;

    // This is an array of 4-Byte arrays containing operator Byte sequences.
    // Sorted: 1) by first Byte ASC 2) by second Byte DESC 3) third Byte DESC 4) fourth Byte DESC.
    // Used to lex operators. Indices must equal the :OperatorType constants
    p[ 0] = (OpDef){ .arity=1, .bytes={ aExclamation, aDot, 0, 0 } }; // !.
    p[ 1] = (OpDef){ .arity=2, .bytes={ aExclamation, aEqual, 0, 0 } }; // !=
    p[ 2] = (OpDef){ .arity=1, .bytes={ aExclamation, 0, 0, 0 } }; // !
    p[ 3] = (OpDef){ .arity=1, .bytes={ aSharp, aSharp, 0, 0 }, .overloadable=true }; // ##
    p[ 4] = (OpDef){ .arity=1, .bytes={ aDollar, 0, 0, 0 } }; // $
    p[ 5] = (OpDef){ .arity=2, .bytes={ aPercent, 0, 0, 0 } }; // %
    p[ 6] = (OpDef){ .arity=2, .bytes={ aAmp, aAmp, aDot, 0 } }; // &&.
    p[ 7] = (OpDef){ .arity=2, .bytes={ aAmp, aAmp, 0, 0 }, .assignable=true }; // &&
    p[ 8] = (OpDef){ .arity=1, .bytes={ aAmp, 0, 0, 0 }, .isTypelevel=true }; // &
    p[ 9] = (OpDef){ .arity=1, .bytes={ aApostrophe, 0, 0, 0 } }; // '
    p[10] = (OpDef){ .arity=2, .bytes={ aTimes, aColon, 0, 0},
                     .assignable = true, .overloadable = true}; // *:
    p[11] = (OpDef){ .arity=2, .bytes={ aTimes, 0, 0, 0 },
                     .assignable = true, .overloadable = true}; // *
    p[12] = (OpDef){ .arity=1, .bytes={ aPlus, aPlus, 0, 0 },
                     .assignable = false, .overloadable = true}; // ++
    p[13] = (OpDef){ .arity=2, .bytes={ aPlus, aColon, 0, 0 },
                     .assignable = true, .overloadable = true}; // +:
    p[14] = (OpDef){ .arity=2, .bytes={ aPlus, 0, 0, 0 },
                     .assignable = true, .overloadable = true}; // +
    p[15] = (OpDef){ .arity=1, .bytes={ aMinus, aMinus, 0, 0 },
                     .assignable = false, .overloadable = true}; // --
    p[16] = (OpDef){ .arity=2, .bytes={ aMinus, aColon, 0, 0},
                     .assignable = true, .overloadable = true}; // -:
    p[17] = (OpDef){ .arity=2, .bytes={ aMinus, 0, 0, 0},
                     .assignable = true, .overloadable = true }; // -
    p[18] = (OpDef){ .arity=2, .bytes={ aDivBy, aColon, 0, 0},
                     .assignable = true, .overloadable = true}; // /:
    p[19] = (OpDef){ .arity=2, .bytes={ aDivBy, aPipe, 0, 0}, .isTypelevel = true}; // /|
    p[20] = (OpDef){ .arity=2, .bytes={ aDivBy, 0, 0, 0},
                     .assignable = true, .overloadable = true}; // /
    p[21] = (OpDef){ .arity=2, .bytes={ aLT, aLT, aDot, 0} }; // <<.
    p[22] = (OpDef){ .arity=2, .bytes={ aLT, aEqual, 0, 0} }; // <=
    p[23] = (OpDef){ .arity=2, .bytes={ aLT, aGT, 0, 0} }; // <>
    p[24] = (OpDef){ .arity=2, .bytes={ aLT, 0, 0, 0 } }; // <
    p[25] = (OpDef){ .arity=2, .bytes={ aEqual, aEqual, aEqual, 0 } }; // ===
    p[26] = (OpDef){ .arity=2, .bytes={ aEqual, aEqual, 0, 0 } }; // ==
    p[27] = (OpDef){ .arity=3, .bytes={ aGT, aEqual, aLT, aEqual } }; // >=<=
    p[28] = (OpDef){ .arity=3, .bytes={ aGT, aLT, aEqual, 0 } }; // ><=
    p[29] = (OpDef){ .arity=3, .bytes={ aGT, aEqual, aLT, 0 } }; // >=<
    p[30] = (OpDef){ .arity=2, .bytes={ aGT, aGT, aDot, 0}, .assignable=true, .overloadable = true}; // >>.
    p[31] = (OpDef){ .arity=3, .bytes={ aGT, aLT, 0, 0 } }; // ><
    p[32] = (OpDef){ .arity=2, .bytes={ aGT, aEqual, 0, 0 } }; // >=
    p[33] = (OpDef){ .arity=2, .bytes={ aGT, 0, 0, 0 } }; // >
    p[34] = (OpDef){ .arity=2, .bytes={ aQuestion, aColon, 0, 0 } }; // ?:
    p[35] = (OpDef){ .arity=1, .bytes={ aQuestion, 0, 0, 0 }, .isTypelevel=true }; // ?
    p[36] = (OpDef){ .arity=2, .bytes={ aCaret, aDot, 0, 0} }; // ^.
    p[37] = (OpDef){ .arity=2, .bytes={ aCaret, 0, 0, 0},
                     .assignable=true, .overloadable = true}; // ^
    p[38] = (OpDef){ .arity=2, .bytes={ aPipe, aPipe, aDot, 0} }; // ||.
    p[39] = (OpDef){ .arity=2, .bytes={ aPipe, aPipe, 0, 0}, .assignable=true }; // ||
   */
    for (Int k = 0; k < countOperators; k++) {
        Int m = 0;
        for (; m < 4 && TL_OPERATORS[k].bytes[m] > 0; m++) {}
        TL_OPERATORS[k].lenBts = m;
    }
}

//}}}

//}}}
//{{{ Parser
//{{{ Parser utils

#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser0(errMsg, __LINE__, cm); }
#define P_CT Arr(Token) toks, Compiler* cm // parser context type fragment
#define P_C toks, cm // parser context args fragment

private Int exprUpTo(Int sentinelToken, Int startBt, Int lenBts, P_CT);
private void subexprClose(P_CT);
private void addBinding(int nameId, int bindingId, Compiler* cm);
private void maybeCloseSpans(Compiler* cm);
private void popScopeFrame(Compiler* cm);
private EntityId importActivateEntity(Entity ent, Compiler* cm);
private void createBuiltins(Compiler* cm);
testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a);
private void exprCore(Int sentinel, Int startBt, Int lenBts, P_CT);
private TypeId exprHeadless(Int sentinel, Int startBt, Int lenBts, P_CT);
private void pExpr(Token tk, P_CT);
private Int pExprWorker(Token tk, P_CT);


_Noreturn private void throwExcParser0(const char errMsg[], Int lineNumber, Compiler* cm) {
    cm->wasError = true;
#ifdef TRACE
    printf("Error on i = %d line %d\n", cm->i, lineNumber);
#endif
    cm->errMsg = str(errMsg, cm->a);
    longjmp(excBuf, 1);
}

#define throwExcParser(errMsg) throwExcParser0(errMsg, __LINE__, cm)


private EntityId getActiveVar(Int nameId, Compiler* cm) { //:getActiveVar
// Resolves an active binding, throws if it's not active
    Int rawValue = cm->activeBindings[nameId];
    VALIDATEP(rawValue > -1 && rawValue < BIG, errUnknownBinding)
    return rawValue;
}

private Int getTypeOfVar(Int varId, Compiler* cm) {
    return cm->entities.cont[varId].typeId;
}


private Int createEntity(untt name, Compiler* cm) {
// Validates a new binding (that it is unique), creates an entity for it,
// and adds it to the current scope
    Int nameId = name & LOWER24BITS;
    Int mbBinding = cm->activeBindings[nameId];
    VALIDATEP(mbBinding < 0, errAssignmentShadowing)
    // if it's a binding, it should be -1, and if overload, < -1

    Int newEntityId = cm->entities.len;
    pushInentities(((Entity){ .name = name, .class = classMutableGuaranteed }), cm);

    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (cm->scopeStack->len > 0) {
            addBinding(nameId, newEntityId, cm); // adds it to the ScopeStack
        }
        cm->activeBindings[nameId] = newEntityId; // makes it active
    }
    return newEntityId;
}

private Int calcSentinel(Token tok, Int tokInd) { //:calcSentinel
// Calculates the sentinel token for a token at a specific index
    return (tok.tp >= firstSpanTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}


testable void printName(Int name, Compiler* cm) {
    untt unsign = (untt)name;
    Int nameId = unsign & LOWER24BITS;
    Int len = (unsign >> 24) & 0xFF;
    Int startBt = cm->stringTable->cont[nameId];
    fwrite(cm->sourceCode->cont + startBt, 1, len, stdout);
    printf("\n");
}

Node trivialNode(untt tp, Token tk) {
    return
        (Node){ .tp = tp, .pl1 = tk.pl1, .pl2 = tk.pl2, .startBt = tk.startBt, .lenBts = tk.pl2 };
}

//}}}
//{{{ Forward decls

testable void pushLexScope(ScopeStack* scopeStack);
private Int parseLiteralOrIdentifier(Token tok, Compiler* cm);
testable Int typeDef(Token assignTk, bool isFunction, Arr(Token) toks, Compiler* cm);
private void parseFnDef(Token tok, P_CT);
testable bool findOverload(FirstArgTypeId typeId, Int start, EntityId* entityId, Compiler* cm);

#ifdef TEST
void printIntArrayOff(Int startInd, Int count, Arr(Int) arr);
#endif

//}}}

private void addParsedScope(Int sentinelToken, Int startBt, Int lenBts, Compiler* cm) {
// Performs coordinated insertions to start a scope within the parser
    push(((ParseFrame){
            .tp = nodScope, .startNodeInd = cm->nodes.len, .sentinelToken = sentinelToken }),
        cm->backtrack);
    pushInnodes((Node){.tp = nodScope, .startBt = startBt, .lenBts = lenBts}, cm);
    pushLexScope(cm->scopeStack);
}

private void parseMisc(Token tok, P_CT) {
}


private void pScope(Token tok, P_CT) {
    addParsedScope(cm->i + tok.pl2, tok.startBt, tok.lenBts, cm);
}

private void parseTry(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseYield(Token tok, P_CT) {
    throwExcParser(errTemp);
}

private void ifLeftSide(Token tok, P_CT) {
// Precondition: we are 1 past the "stmt" token, which is the first parameter
    Int leftSentinel = calcSentinel(tok, cm->i - 1);
    VALIDATEP(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errIfLeft)

    VALIDATEP(leftSentinel + 1 < cm->inpLength, errPrematureEndOfTokens)
    VALIDATEP(toks[leftSentinel].tp == tokMisc && toks[leftSentinel].pl1 == miscArrow,
            errIfMalformed)
    Int typeLeft = pExprWorker(tok, P_C);
    VALIDATEP(typeLeft == tokBool, errTypeMustBeBool)
}


private void pIf(Token tok, P_CT) { //:pIf
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = cm->nodes.len,
            .sentinelToken = cm->i + tok.pl2 };
    push(newParseFrame, cm->backtrack);
    pushInnodes((Node){.tp = nodIf, .pl1 = tok.pl1, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);

    Token stmtTok = toks[cm->i];
    ++cm->i; // CONSUME the stmt token
    ifLeftSide(stmtTok, P_C);
}


private void assignmentComplexLeftSide(Int start, Int sentinel, P_CT) {
// A left side with more than one token must consist of a known var with a series of accessors.
// It gets transformed like this:
// arr_i_(j*2)_(k + 3) ==> _ _ _ arr i (* j 2) (+ k 3)
    Token firstTk = toks[start];
    VALIDATEP(firstTk.tp == tokWord, errAssignmentLeftSide)
    EntityId leftEntityId = getActiveVar(firstTk.pl2, cm);
    Int j = start + 1;

    StackInt* sc = cm->tempStack;
    clearint32_t(sc);

    while (j < sentinel) {
        Token accessorTk = toks[j];
        VALIDATEP(accessorTk.tp == tokFieldAcc, errAssignmentLeftSide)
        j++;
        push(j, sc);
    }

    // Add the "_ _ _" part using the stack of accessor operators
    j = sc->len - 1;
    while (j > -1) {
        Token accessorTk = toks[sc->cont[j]];
        pushInnodes((Node){ .tp = nodFieldAcc, .pl1 = accessorTk.pl1,
                            .startBt = accessorTk.startBt, .lenBts = accessorTk.lenBts }, cm);
    }

    // The collection name part
    pushInnodes((Node){ .tp = nodId, .pl1 = leftEntityId, .pl2 = firstTk.pl2,
                        .startBt = firstTk.startBt, .lenBts = firstTk.lenBts }, cm);
    // The accessor expression parts
    while (j < sentinel) {
        Token accessorTk = toks[j];
        if (accessorTk.tp == tokFieldAcc) {
            j++;
        } else { // tkAccArray
            Token accessTk = toks[j + 1];
            if (accessTk.tp == tokParens) {
                cm->i = j + 2; // CONSUME up to the subexpression start
                Int subexprSentinel = cm->i + accessTk.pl2 + 1;
                exprHeadless(subexprSentinel, accessTk.startBt, accessTk.lenBts, P_C);
                j = cm->i;
            } else if (accessTk.tp == tokInt || accessTk.tp == tokString) {
                pushInnodes(trivialNode(accessTk.tp, accessTk), cm);
                j++;
            } else { // tokWord
                Int accessEntityId = getActiveVar(accessTk.pl2, cm);
                pushInnodes((Node){ .tp = nodId, .pl1 = accessEntityId, .pl2 = accessTk.pl2,
                                    .startBt = accessTk.startBt, .lenBts = accessTk.lenBts }, cm);
                j++;
            }
        }
    }
    clearint32_t(sc);
}


private void pAssignment(Token tok, P_CT) { //:pAssignment
    Int countLeftSide = tok.pl2;
    Token rightTk = toks[cm->i + countLeftSide];

#ifdef SAFETY
    VALIDATEI(rightTk.tp == tokAssignRight, iErrorInconsistentSpans)
#endif

    Bool complexLeft = false;
    Int mbNewBinding = -1;
    Token mbOldBinding = (Token){.tp = 0 };
    Bool isMutation = tok.pl1 >= BIG;
    if (tok.pl1 == assiType) {
        VALIDATEP(countLeftSide == 1, errAssignmentLeftSide)
        typeDef(tok, false, P_C);
        return;
    } else if (countLeftSide > 1) {
        complexLeft = true;
        VALIDATEP(tok.pl1 == assiDefinition || tok.pl1 >= BIG, errAssignmentLeftSide)
        assignmentComplexLeftSide(cm->i, cm->i + countLeftSide, P_C);
    } else if (tok.pl2 == 0) { // a new var being defined
        untt newName = ((untt)tok.lenBts << 24) + (untt)tok.pl1;
        mbNewBinding = createEntity(newName, cm);
        pushInnodes((Node){ .tp = nodAssignLeft, .pl1 = mbNewBinding, .pl2 = 0,
                .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    } else if (tok.pl1 == assiReassign || isMutation) {
        pushInnodes((Node){ .tp = nodAssignLeft, .pl1 = assiReassign, .pl2 = 1,
                .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
        mbOldBinding = toks[cm->i];
        mbOldBinding.pl1 = getActiveVar(mbOldBinding.pl2, cm);
        pushInnodes((Node){ .tp = nodBinding, .pl1 = mbOldBinding.pl1, .pl2 = 0,
                .startBt = mbOldBinding.startBt, .lenBts = mbOldBinding.lenBts}, cm);
    }
    cm->i += countLeftSide + 1; // CONSUME the left side of an assignment
    Int sentinel = cm->i + rightTk.pl2;
    push(((ParseFrame){.tp = nodAssignRight, .startNodeInd = cm->nodes.len,
                       .sentinelToken = cm->i + rightTk.pl2 }), cm->backtrack);
    pushInnodes((Node){.tp = nodAssignRight, .startBt = rightTk.startBt, .lenBts = rightTk.lenBts},
                cm);

    Int lenBytes = tok.lenBts - rightTk.startBt + tok.startBt;
    if (isMutation) {
        StackExprFrame* exprFrames = cm->stateForExprs->exprFrames;
        StackNode* scr = cm->stateForExprs->scratchCode;
        StackNode* calls = cm->stateForExprs->calls;

        Int startNodeInd = cm->nodes.len;
        pushInnodes((Node){ .tp = nodId, .pl1 = mbOldBinding.pl1, .pl2 = 0,
                .startBt = mbOldBinding.startBt, .lenBts = mbOldBinding.lenBts}, cm);

        exprCore(sentinel, rightTk.startBt, lenBytes, P_C);

        TypeId leftSideType = cm->entities.cont[mbOldBinding.pl1].typeId;
        Int operatorEntity = -1;
        Int opType = tok.pl1 - BIG;
        Int ovInd = -cm->activeBindings[opType] - 2;
        bool foundOv = findOverload(leftSideType, ovInd, &operatorEntity, cm);
        VALIDATEP(foundOv, errTypeNoMatchingOverload)
        push(((Node){ .tp = nodCall, .pl1 = operatorEntity, .pl2 = 2,
                .startBt = rightTk.startBt,
                .lenBts = TL_OPERATORS[opType].lenBts}), scr);
        TypeId rightType = typeCheckResolveExpr(startNodeInd, cm->nodes.len, cm);
        VALIDATEP(rightType == leftSideType, errTypeMismatch)
        subexprClose(P_C);
    } else if (rightTk.tp == tokFn) {
        parseFnDef(rightTk, P_C);
    } else {
        Int rightType = exprHeadless(sentinel, rightTk.startBt, lenBytes, P_C);
        VALIDATEP(rightType != -2, errAssignment)

        if (tok.pl1 == 0 && rightType > -1) {
            cm->entities.cont[mbNewBinding].typeId = rightType;
        }
    }
}


private void pFor(Token forTk, P_CT) { //:pFor
// For loops. Look like "for x = 0; x < 100; x++ { ... }"
    ++cm->loopCounter;
    Int sentinel = cm->i + forTk.pl2;

    Int condInd = cm->i;
    Token condTk = toks[condInd];
    VALIDATEP(condTk.tp == tokStmt, errLoopSyntaxError)

    Int condSent = calcSentinel(condTk, condInd);
    Int startBtScope = toks[condSent].startBt;

    push(((ParseFrame){ .tp = nodFor, .startNodeInd = cm->nodes.len, .sentinelToken = sentinel,
                        .typeId = cm->loopCounter }), cm->backtrack);
    pushInnodes((Node){.tp = nodFor,  .startBt = forTk.startBt, .lenBts = forTk.lenBts}, cm);

    addParsedScope(sentinel, startBtScope, forTk.lenBts - startBtScope + forTk.startBt, cm);

    // variable initializations, if any

    cm->i = condSent;
    while (cm->i < sentinel) {
        Token tok = toks[cm->i];
        if (tok.tp != tokAssignLeft) {
            break;
        }
        ++cm->i; // CONSUME the assignment span marker
        pAssignment(tok, P_C);
        maybeCloseSpans(cm);
    }
    Int indBody = cm->i;
    VALIDATEP(indBody < sentinel, errLoopEmptyBody);

    // loop condition
    push(((ParseFrame){.tp = nodForCond, .startNodeInd = cm->nodes.len,
                .sentinelToken = condSent }), cm->backtrack);
    pushInnodes((Node){.tp = nodForCond, .pl1 = slStmt, .pl2 = condSent - condInd,
                       .startBt = condTk.startBt, .lenBts = condTk.lenBts}, cm);
    cm->i = condInd + 1; // + 1 because the expression parser needs to be 1 past the expr/token

    Int condTypeId = pExprWorker(toks[condInd], P_C);
    VALIDATEP(condTypeId == tokBool, errTypeMustBeBool)

    cm->i = indBody; // CONSUME the for token, its condition and variable inits (if any)
}


private void setSpanLengthParser(Int nodeInd, Compiler* cm) {
// Finds the top-level punctuation opener by its index, and sets its node length.
// Called when the parsing of a span is finished
    cm->nodes.cont[nodeInd].pl2 = cm->nodes.len - nodeInd - 1;
}


private void parseVerbatim(Token tok, Compiler* cm) {
    pushInnodes((Node){
        .tp = tok.tp, .startBt = tok.startBt, .lenBts = tok.lenBts, .pl1 = tok.pl1, .pl2 = tok.pl2}, cm);
}

private void parseErrorBareAtom(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private ParseFrame popFrame(Compiler* cm) {
    ParseFrame frame = pop(cm->backtrack);
    if (frame.tp == nodScope) {
        popScopeFrame(cm);
    }
    setSpanLengthParser(frame.startNodeInd, cm);
    return frame;
}


private Int exprSingleItem(Token tk, Compiler* cm) { //:exprSingleItem
// A single-item expression, like "foo". Consumes no tokens.
// Pre-condition: we are 1 token past the token we're parsing.
// Returns the type of the single item
    Int typeId = -1;
    if (tk.tp == tokWord) {
        Int varId = getActiveVar(tk.pl2, cm);
        typeId = getTypeOfVar(varId, cm);
        pushInnodes((Node){.tp = nodId, .pl1 = varId, .pl2 = tk.pl2,
                           .startBt = tk.startBt, .lenBts = tk.lenBts}, cm);
    } else if (tk.tp == tokOperator) {
        Int operBindingId = tk.pl1;
        OpDef operDefinition = TL_OPERATORS[operBindingId];
        VALIDATEP(operDefinition.arity == 1, errOperatorWrongArity)
        pushInnodes((Node){ .tp = nodId, .pl1 = operBindingId, .startBt = tk.startBt,
                .lenBts = tk.lenBts}, cm);
        // TODO add the type when we support first-class functions
    } else if (tk.tp <= topVerbatimType) {
        pushInnodes((Node){.tp = tk.tp, .pl1 = tk.pl1, .pl2 = tk.pl2,
                           .startBt = tk.startBt, .lenBts = tk.lenBts }, cm);
        typeId = tk.tp;
    } else {
        throwExcParser(errUnexpectedToken);
    }
    return typeId;
}


private void exprSubexpr(Int sentinelToken, Int* arity, P_CT) { //:exprSubexpr
// Parses a subexpression within an expression.
// Precondition: the current token must be 1 past the opening paren / statement token
// Examples: `foo(5 a)`
//           `5 + ##a`

// Consumes 1 or 2 tokens
//TODO: allow for core forms (but not scopes!)
    Token firstTk = toks[cm->i];

    if (firstTk.tp == tokWord || firstTk.tp == tokOperator) {
        Int mbBnd = -1;
        if (firstTk.tp == tokWord) {
            mbBnd = cm->activeBindings[firstTk.pl2];
        } else if (firstTk.tp == tokOperator) {
            VALIDATEP(*arity == (TL_OPERATORS)[firstTk.pl1].arity, errOperatorWrongArity)
            mbBnd = -firstTk.pl1 - 2;
        }
        VALIDATEP(mbBnd != -1, errUnknownFunction)

        pushInnodes((Node){.tp = nodCall, .pl1 = mbBnd, .pl2 = *arity,
                           .startBt = firstTk.startBt, .lenBts = firstTk.lenBts}, cm);
        *arity = 0;
        cm->i++; // CONSUME the function or operator call token
    }
}


private void subexprClose(P_CT) { //:subexprClose
// Flushes the finished subexpr frames from the top of the funcall stack
    StackExprFrame* frames = cm->stateForExprs->exprFrames;
    while (frames->len > 0 && cm->i == peek(frames).sentinel) {
        ExprFrame eFr = pop(frames);
        while (frames->len > 0 && peek(frames).isPrefix) {
            ExprFrame prefixFrame = pop(frames);
            push((pop(cm->stateForExpr->calls)), cm->stateForExpr->scratchCode);
        }
    }
}


private ExprFrame exprGetTopSubexpr(StackExprFrame* scr) { //:exprGetTopSubexpr
    Int i = scr->len - 1;
    //while (i > -1 && scr->cont[i].tp != exfrParens) { i--; }
    //VALIDATEI(i > -1, iErrorInconsistentSpans)
    return scr->cont[i];
}


private void exprCopyFromScratch(StackNode* scr, Compiler* cm) {
    // ensure enough space
    // copy
}


private void exprCore(Int sentinelToken, Int startBt, Int lenBts, P_CT) { //:exprCore
// The core code of the general, long expression parser
    Int arity = 0;
    StackNode* scr = cm->stateForExprs->scratchCode;
    StackNode* calls = cm->stateForExprs->calls;
    StackExprFrame* frames = cm->stateForExprs->exprFrames;
    frames->len = 0;
    scr->len = 0;

    push((ExprFrame){ .sentinel = sentinelToken}, frames);
    scr->len = 0;
    while (cm->i < sentinelToken) {
        subexprClose(P_C);
        Token cTk = toks[cm->i];
        untt tokType = cTk.tp;

        if (tokType <= topVerbatimTokenVariant) {
            push(((Node){ .tp = cTk.tp, .pl1 = cTk.pl1, .pl2 = cTk.pl2,
                                .startBt = cTk.startBt, .lenBts = cTk.lenBts }), scr);
            cm->i++; // CONSUME the verbatim token
        } else  if (tokType == tokWord) {
            EntityId varId = getActiveVar(cTk.pl2, cm);
            push(((Node){ .tp = nodId, .pl1 = varId, .pl2 = cTk.pl2,
                                .startBt = cTk.startBt, .lenBts = cTk.lenBts}), scr);
        } else if (tokType == tokParens) {
            cm->i++; // CONSUME the parens token
            Int parensSentinel = cm->i + cTk.pl2;
            push(((ExprFrame){ .sentinel = parensSentinel }), frames);
            if (cm->i < sentinelToken)  {
                Token firstTk = toks[cm->i];
                if ((firstTk.tp == tokCall || firstTk.tp == tokOper) && firstTk.pl2 == 0) {
                    // `(.call 1 2)`
                    push(
                        ((ExprFrame) { .sentinel = parensSentinel, .argCount = 0,
                    .isPrefix = (firstTk.tp == tokOper && TL_OPERATORS[firstTk.pl1].arity == 1) },
                     frames);
                    push(((Node)) { .tp = nodCall,
                            .startBt = firstTk.startBt, .lenBts = firstTk.lenBts }, calls);
                    ++cm->i; // CONSUME the parens-initial call node
                }
            }
        } else if (tokType == tokOperator) {
            Int bindingId = cTk.pl1;
            OpDef operDefinition = TL_OPERATORS[bindingId];
            Bool isPrefix = TL_OPERATORS[bindingId].arity == 1;
            push(((ExprFrame){
                    .isPrefix = isPrefix, .sentinel = -1, .argCount = isPrefix ? 0 : 1
            }), frames);
        } else if (tokType == tokCall) {
            if (cTk.pl2 > 0) { // prefix calls like `foo()`
                push(
                    ((ExprFrame) { .sentinel = calcSentinel(cTk, cm->i), .argCount = 0,
                                   .isPrefix = false }, frames);
                push(((Node)) { .tp = nodCall,
                        .startBt = firstTk.startBt, .lenBts = firstTk.lenBts }, calls);
            } else { // infix calls like ` .foo`
                push(
                    ((ExprFrame) { .sentinel = -1, .argCount = 1,
                                   .isPrefix = false }, frames);
                push(((Node)) { .tp = nodCall,
                        .startBt = firstTk.startBt, .lenBts = firstTk.lenBts }, calls);
            }
        } else {
            throwExcParser(errExpressionCannotContain);
        }
        cm->i++; // CONSUME any token
    }
    exprCopyFromScratch(scr, cm);
}


private TypeId exprUpTo(Int sentinelToken, Int startBt, Int lenBts, P_CT) { //:exprUpTo
// The main "big" expression parser. Parses an expression whether there is a token or not.
// Starts from cm->i and goes up to the sentinel token. Returns the expression's type
// Precondition: we are looking 1 past the tokExpr or tokParens
    Int startNodeInd = cm->nodes.len;
    push(((ParseFrame){.tp = nodExpr, .startNodeInd = startNodeInd,
                .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){ .tp = nodExpr, .startBt = startBt, .lenBts = lenBts }, cm);

    exprCore(sentinelToken, startBt, lenBts, P_C);

    subexprClose(P_C);

    Int exprType = typeCheckResolveExpr(startNodeInd, cm->nodes.len, cm);
    return exprType;
}


private TypeId exprHeadless(Int sentinel, Int startBt, Int lenBts, P_CT) { //:exprHeadless
// Precondition: we are looking at the first token of expr which does not have a
// tokStmt/tokParens header. If "omitSpan" is set, this function will not emit a nodExpr nor
// create a ParseFrame.
// Consumes 1 or more tokens. Returns the type of parsed expression
    if (cm->i + 1 == sentinel) { // the [stmt 1, tokInt] case
        Token singleToken = toks[cm->i];
        if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) {
            cm->i += 1; // CONSUME the single literal
            return exprSingleItem(singleToken, cm);
        }
    }
    return exprUpTo(sentinel, startBt, lenBts, P_C);
}


private Int pExprWorker(Token tok, P_CT) { //:pExprWorker
// Precondition: we are looking 1 past the first token of expr, which is the first parameter.
// Consumes 1
// or more tokens. Handles single items also Returns the type of parsed expression
    if (tok.tp == tokStmt || tok.tp == tokParens) {
        if (tok.pl2 == 1) {
            Token singleToken = toks[cm->i];
            if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) {
                // [stmt 1, tokInt]
                ++cm->i; // CONSUME the single literal token
                return exprSingleItem(singleToken, cm);
            }
        }
        return exprUpTo(cm->i + tok.pl2, tok.startBt, tok.lenBts, P_C);
    } else {
        return exprSingleItem(tok, cm);
    }
}


private void pExpr(Token tok, P_CT) {
    pExprWorker(tok, P_C);
}


private void maybeCloseSpans(Compiler* cm) { //:maybeCloseSpans
// When we are at the end of a function parsing a parse frame, we might be at the end of said frame
// (otherwise => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
// in which case this function handles all the corresponding stack poppin'.
// It also always handles updating all inner frames with consumed tokens
// This is safe to call anywhere, pretty much
    while (hasValues(cm->backtrack)) { // loop over subscopes and expressions inside FunctionDef
        ParseFrame frame = peek(cm->backtrack);
        if (cm->i < frame.sentinelToken) {
            return;
        }
#ifdef SAFETY
        if (cm->i != frame.sentinelToken) {
            print("Span inconsistency i %d  frame.sentinelToken %d frametp %d startInd %d",
                cm->i, frame.sentinelToken, frame.tp, frame.startNodeInd);
        }
        VALIDATEI(cm->i == frame.sentinelToken, iErrorInconsistentSpans)
#endif
        popFrame(cm);
    }
}

private void parseUpTo(Int sentinelToken, P_CT) { //:parseUpTo
// Parses anything from current cm->i to "sentinelToken"
    while (cm->i < sentinelToken) {
        print("parsing tok i %d", cm->i)
        Token currTok = toks[cm->i];
        cm->i++;
        ((*cm->langDef->parserTable)[currTok.tp])(currTok, P_C);

        maybeCloseSpans(cm);
    }
}

private Int parseLiteralOrIdentifier(Token tok, Compiler* cm) {
// Consumes 0 or 1 tokens. Returns the type of what it parsed, including -1 if unknown.
// Returns -2 if didn't parse
    Int typeId = -1;
    if (tok.tp <= topVerbatimTokenVariant) {
        parseVerbatim(tok, cm);
        typeId = tok.tp;
    } else if (tok.tp == tokWord) {
        Int nameId = tok.pl2;
        Int mbBinding = cm->activeBindings[nameId];
        pushInnodes((Node){.tp = nodId, .pl1 = mbBinding, .pl2 = nameId, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
        if (mbBinding > -1) {
            typeId = getTypeOfVar(mbBinding, cm);
        }
    } else {
        return -2;
    }
    cm->i++; // CONSUME the literal or ident token
    return typeId;
}


private void setClassToMutated(Int bindingId, Compiler* cm) { //:setClassToMutated
// Changes a mutable variable to mutated. Throws an exception for an immutable one
    Int class = cm->entities.cont[bindingId].class;
    VALIDATEP(class < classImmutable, errCannotMutateImmutable);
    if (class % 2 == 0) {
        ++cm->entities.cont[bindingId].class;
    }
}


private void pAlias(Token tok, P_CT) { //:pAlias
    throwExcParser(errTemp);
}

private void parseAssert(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseAssertDbg(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseAwait(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private Int breakContinue(Token tok, Int* sentinel, P_CT) { //:breakContinue
    VALIDATEP(tok.pl2 <= 1, errBreakContinueTooComplex);
    Int unwindLevel = 1;
    *sentinel = cm->i;
    if (tok.pl2 == 1) {
        Token nextTok = toks[cm->i];
        VALIDATEP(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errBreakContinueInvalidDepth)

        unwindLevel = nextTok.pl2;
        ++(*sentinel); // CONSUME the Int after the `break`
    }
    if (unwindLevel == 1) {
        return -1;
    }

    Int j = cm->backtrack->len;
    while (j > -1) {
        Int pfType = cm->backtrack->cont[j].tp;
        if (pfType == nodFor) {
            --unwindLevel;
            if (unwindLevel == 0) {
                ParseFrame loopFrame = cm->backtrack->cont[j];
                Int loopId = loopFrame.typeId;
                cm->nodes.cont[loopFrame.startNodeInd].pl1 = loopId;
                return unwindLevel == 1 ? -1 : loopId;
            }
        }
        --j;
    }
    throwExcParser(errBreakContinueInvalidDepth);

}


private void pBreakCont(Token tok, P_CT) { //:pBreakCont
    Int sentinel = cm->i;
    Int loopId = breakContinue(tok, &sentinel, P_C);
    if (tok.pl1 > 0) { // continue
        loopId += BIG;
    }
    pushInnodes((Node){.tp = nodBreakCont, .pl1 = loopId,
                       .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    cm->i = sentinel; // CONSUME the whole break statement
}


private void parseCatch(Token tok, P_CT) {
    throwExcParser(errTemp);
}

private void parseDefer(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseDispose(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseExposePrivates(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseFnDef(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseInterface(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseImpl(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseLambda(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseLambda1(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseLambda2(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parseLambda3(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void parsePackage(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void typecheckFnReturn(Int typeId, Compiler* cm) { //:typecheckFnReturn
    Int j = cm->backtrack->len - 1;
    while (j > -1 && cm->backtrack->cont[j].tp != nodFnDef) {
        --j;
    }
    Int fnTypeInd = cm->backtrack->cont[j].typeId;
    VALIDATEP(cm->types.cont[fnTypeInd + 1] == typeId, errTypeWrongReturnType)
}


private void pReturn(Token tok, P_CT) { //:pReturn
    Int lenTokens = tok.pl2;
    Int sentinelToken = cm->i + lenTokens;

    push(((ParseFrame){ .tp = nodReturn, .startNodeInd = cm->nodes.len,
                        .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodReturn, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);

    Token rTk = toks[cm->i];
    Int typeId = exprHeadless(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, P_C);
    VALIDATEP(typeId > -1, errReturn)
    if (typeId > -1) {
        typecheckFnReturn(typeId, cm);
    }
}


private Int isFunction(FirstArgTypeId typeId, Compiler* cm);
private void addRawOverload(NameId nameId, TypeId typeId, EntityId entityId, Compiler* cm);


private EntityId importActivateEntity(Entity ent, Compiler* cm) { //:importActivateEntity
// Adds an import to the entities table, activates it and, if function, adds its overload
    Int existingBinding = cm->activeBindings[ent.name & LOWER24BITS];
    Int isAFunc = isFunction(ent.typeId, cm);
    VALIDATEP(existingBinding == -1 || isAFunc > -1, errAssignmentShadowing)

    Int newEntityId = cm->entities.len;
    pushInentities(ent, cm);
    Int nameId = ent.name & LOWER24BITS;

    if (isAFunc > -1) {
        addRawOverload(nameId, ent.typeId, newEntityId, cm);
        pushInimportNames(nameId, cm);
    } else {
        cm->activeBindings[nameId] = newEntityId;
    }
    return newEntityId;
}


testable void importEntities(Arr(EntityImport) impts, Int countEntities, Arr(TypeId) typeIds,
                             Compiler* cm) { //:importEntities
    for (int j = 0; j < countEntities; j++) {
        EntityImport impt = impts[j];
        TypeId typeId = typeIds[impt.typeInd];
        importActivateEntity(
                (Entity){.name = impt.name, .class = classImmutable, .typeId = typeId }, cm);
    }
    cm->countNonparsedEntities = cm->entities.len;
}


private ParserFunc (*tabulateParserDispatch(Arena* a))[countSyntaxForms] {
    ParserFunc (*result)[countSyntaxForms] = allocateOnArena(countSyntaxForms*sizeof(ParserFunc), a);
    ParserFunc* p = *result;
    int i = 0;
    while (i <= firstSpanTokenType) {
        p[i] = &parseErrorBareAtom;
        i++;
    }
    p[tokScope]       = &pScope;
    p[tokStmt]        = &pExpr;
    p[tokParens]      = &parseErrorBareAtom;
    p[tokAssignLeft]  = &pAssignment;
    p[tokMisc]        = &parseMisc;

    p[tokAlias]       = &pAlias;
    p[tokAssert]      = &parseAssert;
    p[tokBreakCont]   = &pBreakCont;
    p[tokCatch]       = &pAlias;
    p[tokFn]          = &pAlias;
    p[tokFinally]     = &pAlias;
    p[tokIface]       = &pAlias;
    p[tokImport]      = &pAlias;
    p[tokReturn]      = &pReturn;
    p[tokTry]         = &pAlias;

    p[tokIf]          = &pIf;
    p[tokFor]         = &pFor;
    p[tokElse]     = &pAlias;
    return result;
}


testable LanguageDefinition* buildLanguageDefinitions(Arena* a) {
// Definition of the operators, lexer dispatch and parser dispatch tables for the compiler
    tabulateOperators();
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    (*result) = (LanguageDefinition) {
        .dispatchTable = tabulateDispatch(a),
        .parserTable = tabulateParserDispatch(a)
    };
    return result;
}


private Stackint32_t* copyStringTable(Stackint32_t* table, Arena* a) {
    Stackint32_t* result = createStackint32_t(table->cap, a);
    result->len = table->len;
    result->cap = table->cap;
    memcpy(result->cont, table->cont, table->len*4);
    return result;
}


private StringDict* copyStringDict(StringDict* from, Arena* a) {
    StringDict* result = allocateOnArena(sizeof(StringDict), a);
    const Int dictSize = from->dictSize;
    Arr(Bucket*) dict = allocateOnArena(sizeof(Bucket*)*dictSize, a);

    result->a = a;
    for (int i = 0; i < dictSize; i++) {
        if (from->dict[i] == null) {
            dict[i] = null;
        } else {
            Bucket* old = from->dict[i];
            Int capacity = old->capAndLen >> 16;
            Int len = old->capAndLen & LOWER16BITS;
            Bucket* new = allocateOnArena(sizeof(Bucket) + capacity*sizeof(StringValue), a);
            new->capAndLen = old->capAndLen;
            memcpy(new->cont, old->cont, len*sizeof(StringValue));
            dict[i] = new;
        }
    }
    result->dictSize = from->dictSize;
    result->dict = dict;
    return result;
}

testable Compiler* createProtoCompiler(Arena* a) { //:createProtoCompiler
// Creates a proto-compiler, which is used not for compilation but as a seed value to be cloned
// for every source code module. The proto-compiler contains the following data:
// - langDef with operators
// - types that are sufficient for the built-in operators
// - entities with the built-in operator entities
// - overloadIds with counts
    Compiler* proto = allocateOnArena(sizeof(Compiler), a);
    (*proto) = (Compiler){
        .langDef = buildLanguageDefinitions(a), .entities = createInListEntity(32, a),
        .sourceCode = str(standardText, a),
        .stringTable = createStackint32_t(16, a), .stringDict = createStringDict(128, a),
        .types = createInListInt(64, a), .typesDict = createStringDict(128, a),
        .activeBindings = allocateOnArena(4*countOperators, a),
        .rawOverloads = createMultiAssocList(a),
        .a = a
    };
    memset(proto->activeBindings, 0xFF, 4*countOperators);
    createBuiltins(proto);
    return proto;
}

private void finalizeLexer(Compiler* lx) { //:finalizeLexer
// Finalizes the lexing of a single input: checks for unclosed scopes, and closes semicolons and
// an open statement, if any
    if (!hasValues(lx->lexBtrack)) {
        return;
    }
    BtToken top = pop(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slScope && !hasValues(lx->lexBtrack), errPunctuationExtraOpening)

    setStmtSpanLength(top.tokenInd, lx);
}


testable Compiler* lexicallyAnalyze(String* sourceCode, Compiler* proto, Arena* a) {
//:lexicallyAnalyze Main lexer function. Precondition: the input Byte array has been prepended with StandardText
    Compiler* lx = createLexerFromProto(sourceCode, proto, a);
    Int inpLength = lx->inpLength;
    Arr(Byte) inp = lx->sourceCode->cont;
    VALIDATEL(inpLength > 0, "Empty input")

    // Check for UTF-8 BOM at start of file
    if ((lx->i + 3) < inpLength
        && (unsigned char)inp[lx->i] == 0xEF
        && (unsigned char)inp[lx->i + 1] == 0xBB
        && (unsigned char)inp[lx->i + 2] == 0xBF) {
        lx->i += 3;
    }
    LexerFunc (*dispatch)[256] = proto->langDef->dispatchTable;
    // Main loop over the input
    if (setjmp(excBuf) == 0) {
        while (lx->i < inpLength) {
            ((*dispatch)[inp[lx->i]])(inp, lx);
        }
        finalizeLexer(lx);
    }
    return lx;
}


struct ScopeStackFrame {
// This frame corresponds either to a lexical scope or a subexpression.
// It contains string ids introduced in the current scope, used not for cleanup after the frame
// is popped. Otherwise, it contains the function call of the subexpression
    int len;                // number of elements in scope stack

    ScopeChunk* previousChunk;
    int previousInd;           // index of the start of previous frame within its chunk

    ScopeChunk* thisChunk;
    int thisInd;               // index of the start of this frame within this chunk
    Arr(int) bindings;         // list of names of bindings introduced in this scope
};



DEFINE_STACK(Node)

private size_t ceiling4(size_t sz) {
    size_t rem = sz % 4;
    return sz + 4 - rem;
}


private size_t floor4(size_t sz) {
    size_t rem = sz % 4;
    return sz - rem;
}

#define CHUNK_SIZE 65536
#define FRESH_CHUNK_LEN floor4(CHUNK_SIZE - sizeof(ScopeChunk))/4


testable ScopeStack* createScopeStack(void) {
// TODO put it into an arena?
    ScopeStack* result = malloc(sizeof(ScopeStack));

    ScopeChunk* firstChunk = malloc(CHUNK_SIZE);

    firstChunk->len = FRESH_CHUNK_LEN;
    firstChunk->next = null;

    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->lastChunk = firstChunk;
    result->topScope = (ScopeStackFrame*)firstChunk->cont;
    result->nextInd = ceiling4(sizeof(ScopeStackFrame))/4;
    Arr(int) firstFrame = (int*)firstChunk->cont + result->nextInd;

    (*result->topScope) = (ScopeStackFrame){.len = 0, .previousChunk = null, .thisChunk = firstChunk,
        .thisInd = result->nextInd, .bindings = firstFrame };

    result->nextInd += 64;

    return result;
}

private void mbNewChunk(ScopeStack* scopeStack) { //:mbNewChunk
    if (scopeStack->currChunk->next != null) {
        return;
    }
    ScopeChunk* newChunk = malloc(CHUNK_SIZE);
    newChunk->len = FRESH_CHUNK_LEN;
    newChunk->next = null;
    scopeStack->currChunk->next = newChunk;
    scopeStack->lastChunk = null;
}

testable void pushLexScope(ScopeStack* scopeStack) {
// Allocates a new scope, either within this chunk or within a pre-existing lastChunk or within
// a brand new chunk. Scopes have a simple size policy: 64 elements at first, then 256, then
// throw exception. This is because only 256 local variables are allowed in one function, and
// transitively in one scope
    int remainingSpace = scopeStack->currChunk->len - scopeStack->nextInd + 1;
    int necessarySpace = ceiling4(sizeof(ScopeStackFrame))/4 + 64;

    ScopeChunk* oldChunk = scopeStack->topScope->thisChunk;
    int oldInd = scopeStack->topScope->thisInd;
    ScopeStackFrame* newScope;
    int newInd;
    if (remainingSpace < necessarySpace) {
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (ScopeStackFrame*)scopeStack->currChunk->cont;
        newInd = 0;
    } else {
        newScope = (ScopeStackFrame*)((int*)scopeStack->currChunk->cont + scopeStack->nextInd);
        newInd = scopeStack->nextInd;
        scopeStack->nextInd += necessarySpace;
    }
    (*newScope) = (ScopeStackFrame){.previousChunk = oldChunk, .previousInd = oldInd, .len = 0,
        .thisChunk = scopeStack->currChunk, .thisInd = newInd,
        .bindings = (int*)newScope + ceiling4(sizeof(ScopeStackFrame))};

    scopeStack->topScope = newScope;
    scopeStack->len++;
}


private void resizeScopeArrayIfNecessary(Int initLength, ScopeStackFrame* topScope,
                                         ScopeStack* scopeStack) { //:resizeScopeArrayIfNecessary
    int newLength = scopeStack->topScope->len + 1;
    if (newLength == initLength) {
        int remainingSpace = scopeStack->currChunk->len - scopeStack->nextInd + 1;
        if (remainingSpace + initLength < 256) {
            mbNewChunk(scopeStack);
            scopeStack->currChunk = scopeStack->currChunk->next;
            Arr(int) newContent = scopeStack->currChunk->cont;

            memcpy(topScope->bindings, newContent, initLength*4);
            topScope->bindings = newContent;
        }
    } else if (newLength == 256) {
        longjmp(excBuf, 1);
    }
}


private void addBinding(int nameId, int bindingId, Compiler* cm) { //:addBinding
    ScopeStackFrame* topScope = cm->scopeStack->topScope;
    resizeScopeArrayIfNecessary(64, topScope, cm->scopeStack);

    topScope->bindings[topScope->len] = nameId;
    topScope->len++;

    cm->activeBindings[nameId] = bindingId;
}


private void popScopeFrame(Compiler* cm) { //:popScopeFrame
// Pops a frame from the ScopeStack. For a scope type of frame, also deactivates its bindings.
// Returns pointer to previous frame (which will be top after this call) or null if there isn't any
    ScopeStackFrame* topScope = cm->scopeStack->topScope;
    ScopeStack* scopeStack = cm->scopeStack;
    if (topScope->bindings) {
        for (int i = 0; i < topScope->len; i++) {
            cm->activeBindings[*(topScope->bindings + i)] = -1;
        }
    }

    if (topScope->previousChunk) {
        scopeStack->currChunk = topScope->previousChunk;
        scopeStack->lastChunk = scopeStack->currChunk->next;
        scopeStack->topScope = (ScopeStackFrame*)(scopeStack->currChunk->cont + topScope->previousInd);
    }

    // if the lastChunk is defined, it will serve as pre-allocated buffer for future frames, but
    // everything after it needs to go
    if (scopeStack->lastChunk) {
        ScopeChunk* ch = scopeStack->lastChunk->next;
        if (ch != null) {
            scopeStack->lastChunk->next = null;
            do {
                ScopeChunk* nextToDelete = ch->next;
                printf("ScopeStack is freeing a chunk of memory at %p next = %p\n", ch, nextToDelete);
                free(ch);

                if (nextToDelete == null) break;
                ch = nextToDelete;

            } while (ch != null);
        }
    }
    scopeStack->len--;
}


private void addRawOverload(NameId nameId, TypeId typeId, EntityId entityId, Compiler* cm) {
//:addRawOverload Adds an overload of a function to the [rawOverloads] and activates it, if needed
    Int mbListId = -cm->activeBindings[nameId] - 2;
    FirstArgTypeId firstParamType = getFirstParamType(typeId, cm);
    if (mbListId == -1) {
        Int newListId = listAddMultiAssocList(firstParamType, entityId, cm->rawOverloads);
        cm->activeBindings[nameId] = -newListId - 2;
        ++cm->countOverloadedNames;
    } else {
        Int updatedListId = addMultiAssocList(firstParamType, entityId, mbListId,
                                              cm->rawOverloads);
        if (updatedListId != -1) {
            cm->activeBindings[nameId] = -updatedListId - 2;
        }
    }
    ++cm->countOverloads;
}


private TypeId mergeTypeWorker(Int startInd, Int lenInts, Compiler* cm) { //:mergeTypeWorker
    Byte* types = (Byte*)cm->types.cont;
    StringDict* hm = cm->typesDict;
    Int startBt = startInd*4;
    Int lenBts = lenInts*4;
    untt theHash = hashCode(types + startBt, lenBts);
    Int hashOffset = theHash % (hm->dictSize);
    if (*(hm->dict + hashOffset) == null) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue),
                hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = cap, right u16 = len
        StringValue* firstElem = (StringValue*)newBucket->cont;
        *firstElem = (StringValue){.hash = theHash, .indString = startBt };
        *(hm->dict + hashOffset) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hashOffset);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->cont;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].hash == theHash
                  && memcmp(types + stringValues[i].indString, types + startBt, lenBts) == 0) {
                // key already present
                cm->types.len -= lenInts;
                return stringValues[i].indString/4;
            }
        }
        addValueToBucket((hm->dict + hashOffset), startBt, lenBts, hm->a);
    }
    ++hm->len;
    return startInd;
}

testable TypeId mergeType(Int startInd, Compiler* cm) { //:mergeType
// Unique'ing of types. Precondition: the type is parked at the end of cm->types, forming its
// tail. Returns the resulting index of this type and updates the length of cm->types if
// appropriate
    Int lenInts = cm->types.cont[startInd] + 1; // +1 for the type length
    return mergeTypeWorker(startInd, lenInts, cm);
}


testable TypeId addConcrFnType(Int arity, Arr(Int) paramsAndReturn, Compiler* cm) {
//:addConcrFnType Function types are stored as: (length, paramType1, paramType2, ..., returnType)
    Int newInd = cm->types.len;
    pushIntypes(arity + 3, cm); // +3 because the header takes 2 ints, 1 more for the return typeId
    typeAddHeader(
        (TypeHeader){.sort = sorFunction, .arity = 0, .depth = arity + 1, .nameAndLen = -1}, cm);
    for (Int k = 0; k < arity; k++) {
        pushIntypes(paramsAndReturn[k], cm);
    }
    pushIntypes(paramsAndReturn[arity], cm);
    return mergeType(newInd, cm);
}


private void buildStandardStrings(Compiler* lx) { //:buildStandardStrings
// Inserts all strings from the standardText into the string table and the hash table
// But first inserts a reservation for every operator symbol ("countOperators" nameIds in all,
// the lx->stringTable contains zeros in those places)
    for (Int j = 0; j < countOperators; j++) {
        push(0, lx->stringTable);
    }
    int startBt = 0;
    for (Int i = strAlias; i < strSentinel; i++) {
        addStringDict(lx->sourceCode->cont, startBt, standardStringLens[i],
                      lx->stringTable, lx->stringDict);
        startBt += standardStringLens[i];
    }
}

testable NameId stToNameId(Int a) { //:stToNameId
// Converts a standard string to its nameId. Doesn't work for reserved words, obviously
    return a - strFirstNonReserved + countOperators;
}


private void buildPreludeTypes(Compiler* cm) { //:buildPreludeTypes
// Creates the built-in types in the proto compiler
    for (int i = strInt; i <= strVoid; i++) {
        pushIntypes(0, cm);
    }
    // List
    Int typeIndL = cm->types.len;
    pushIntypes(6, cm);
    typeAddHeader((TypeHeader){.sort = sorStruct, .nameAndLen = (1 << 24) + stToNameId(strL),
                                 .depth = 2, .arity = 1}, cm);
    pushIntypes(0, cm); // the arity of the type param
    pushIntypes(stToNameId(strLen), cm);
    pushIntypes(stToNameId(strCap), cm);
    pushIntypes(stToNameId(strInt), cm);
    pushIntypes(stToNameId(strInt), cm);
    cm->activeBindings[stToNameId(strL)] = typeIndL;

    // Array
    Int typeIndA = cm->types.len;
    pushIntypes(4, cm);
    typeAddHeader((TypeHeader){.sort = sorStruct, .nameAndLen = (1 << 24) + stToNameId(strA),
                                 .depth = 1, .arity = 1}, cm);
    pushIntypes(0, cm); // the arity of the type param
    pushIntypes(stToNameId(strLen), cm);
    pushIntypes(stToNameId(strInt), cm);
    cm->activeBindings[stToNameId(strA)] = typeIndA;

    // Tuple
    Int typeIndTu = cm->types.len;
    pushIntypes(6, cm);
    typeAddHeader((TypeHeader){.sort = sorStruct, .nameAndLen = (2 << 24) + stToNameId(strTu),
                                 .depth = 2, .arity = 2}, cm);
    pushIntypes(0, cm); // the arities of the type params
    pushIntypes(0, cm);
    pushIntypes(stToNameId(strF1), cm);
    pushIntypes(stToNameId(strF2), cm);
    typeAddTypeParam(0, 0, cm);
    typeAddTypeParam(1, 0, cm);
    cm->activeBindings[stToNameId(strTu)] = typeIndTu;
}


private void buildOperator(Int operId, TypeId typeId, Compiler* cm) { //:buildOperator
// Creates an entity, pushes it to [rawOverloads] and activates its name
    Int newEntityId = cm->entities.len;
    pushInentities((Entity){ .typeId = typeId, .class = classImmutable }, cm);
    addRawOverload(operId, typeId, newEntityId, cm);
}


private void buildOperators(Compiler* cm) { //:buildOperators
// Operators are the first-ever functions to be defined. This function builds their [types],
// [functions] and overload counts. The order must agree with the order of operator
// definitions in tl.internal.h, and every operator must have at least one function defined
    TypeId boolOfIntInt    = addConcrFnType(2, (Int[]){ tokInt, tokInt, tokBool}, cm);
    TypeId boolOfIntIntInt = addConcrFnType(3, (Int[]){ tokInt, tokInt, tokInt, tokBool}, cm);
    TypeId boolOfFlFl      = addConcrFnType(2, (Int[]){ tokDouble, tokDouble, tokBool}, cm);
    TypeId boolOfFlFlFl    = addConcrFnType(3,
                            (Int[]){ tokDouble, tokDouble, tokDouble, tokBool}, cm);
    TypeId boolOfStrStr    = addConcrFnType(2, (Int[]){ tokString, tokString, tokBool}, cm);
    TypeId boolOfBool      = addConcrFnType(1, (Int[]){ tokBool, tokBool}, cm);
    TypeId boolOfBoolBool  = addConcrFnType(2, (Int[]){ tokBool, tokBool, tokBool}, cm);
    TypeId intOfStr        = addConcrFnType(1, (Int[]){ tokString, tokInt}, cm);
    TypeId intOfInt        = addConcrFnType(1, (Int[]){ tokInt, tokInt}, cm);
    TypeId intOfIntInt     = addConcrFnType(2, (Int[]){ tokInt, tokInt, tokInt}, cm);
    TypeId intOfFlFl       = addConcrFnType(2, (Int[]){ tokDouble, tokDouble, tokInt}, cm);
    TypeId intOfStrStr     = addConcrFnType(2, (Int[]){ tokString, tokString, tokInt}, cm);
    TypeId strOfInt        = addConcrFnType(1, (Int[]){ tokInt, tokString}, cm);
    TypeId strOfFloat      = addConcrFnType(1, (Int[]){ tokDouble, tokString}, cm);
    TypeId strOfBool       = addConcrFnType(1, (Int[]){ tokBool, tokString}, cm);
    TypeId strOfStrStr     = addConcrFnType(2, (Int[]){ tokString, tokString, tokString}, cm);
    TypeId flOfFlFl        = addConcrFnType(2, (Int[]){ tokDouble, tokDouble, tokDouble}, cm);
    TypeId flOfFl          = addConcrFnType(1, (Int[]){ tokDouble, tokDouble}, cm);

    buildOperator(opBitwiseNeg,   boolOfBool, cm); // dummy
    buildOperator(opNotEqual,     boolOfIntInt, cm);
    buildOperator(opNotEqual,     boolOfFlFl, cm);
    buildOperator(opNotEqual,     boolOfStrStr, cm);
    buildOperator(opBoolNeg,      boolOfBool, cm);
    buildOperator(opSize,         intOfStr, cm);
    buildOperator(opSize,         intOfInt, cm);
    buildOperator(opToString,     strOfInt, cm);
    buildOperator(opToString,     strOfBool, cm);
    buildOperator(opToString,     strOfFloat, cm);
    buildOperator(opRemainder,    intOfIntInt, cm);
    buildOperator(opBitwiseAnd,   boolOfBoolBool, cm); // dummy
    buildOperator(opBoolAnd,      boolOfBoolBool, cm);
    buildOperator(opPtr,          boolOfBoolBool, cm); // dummy
    buildOperator(opIsNull,       boolOfBoolBool, cm); // dummy
    buildOperator(opTimesExt,     flOfFlFl, cm); // dummy
    buildOperator(opTimes,        intOfIntInt, cm);
    buildOperator(opTimes,        flOfFlFl, cm);
    buildOperator(opIncrement,    intOfIntInt, cm);
    buildOperator(opPlusExt,      strOfStrStr, cm); // dummy
    buildOperator(opPlus,         intOfIntInt, cm);
    buildOperator(opPlus,         flOfFlFl, cm);
    buildOperator(opPlus,         strOfStrStr, cm);
    buildOperator(opDecrement,    intOfIntInt, cm); // dummy
    buildOperator(opMinusExt,     intOfIntInt, cm); // dummy
    buildOperator(opMinus,        intOfIntInt, cm);
    buildOperator(opMinus,        flOfFlFl, cm);
    buildOperator(opDivByExt,     intOfIntInt, cm); // dummy
    buildOperator(opIntersect,    intOfIntInt, cm); // dummy
    buildOperator(opDivBy,        intOfIntInt, cm);
    buildOperator(opDivBy,        flOfFlFl, cm);
    buildOperator(opBitShiftL,    intOfFlFl, cm);  // dummy
    buildOperator(opLTEQ,         boolOfIntInt, cm);
    buildOperator(opLTEQ,         boolOfFlFl, cm);
    buildOperator(opLTEQ,         boolOfStrStr, cm);
    buildOperator(opComparator,   intOfIntInt, cm);  // dummy
    buildOperator(opComparator,   intOfFlFl, cm);  // dummy
    buildOperator(opComparator,   intOfStrStr, cm);  // dummy
    buildOperator(opLessTh,       boolOfIntInt, cm);
    buildOperator(opLessTh,       boolOfFlFl, cm);
    buildOperator(opLessTh,       boolOfStrStr, cm);
    buildOperator(opRefEquality,  boolOfIntInt, cm); // dummy
    buildOperator(opEquality,     boolOfIntInt, cm);
    buildOperator(opIntervalBo,   boolOfIntIntInt, cm); // dummy
    buildOperator(opIntervalBo,   boolOfFlFlFl, cm); // dummy
    buildOperator(opIntervalR,    boolOfIntIntInt, cm); // dummy
    buildOperator(opIntervalR,    boolOfFlFlFl, cm); // dummy
    buildOperator(opIntervalL,    boolOfIntIntInt, cm); // dummy
    buildOperator(opIntervalL,    boolOfFlFlFl, cm); // dummy
    buildOperator(opBitShiftR,    boolOfBoolBool, cm); // dummy
    buildOperator(opIntervalEx,   boolOfIntIntInt, cm); // dummy
    buildOperator(opIntervalEx,   boolOfFlFlFl, cm); // dummy
    buildOperator(opGTEQ,         boolOfIntInt, cm);
    buildOperator(opGTEQ,         boolOfFlFl, cm);
    buildOperator(opGTEQ,         boolOfStrStr, cm);
    buildOperator(opGreaterTh,    boolOfIntInt, cm);
    buildOperator(opGreaterTh,    boolOfFlFl, cm);
    buildOperator(opGreaterTh,    boolOfStrStr, cm);
    buildOperator(opNullCoalesce, intOfIntInt, cm); // dummy
    buildOperator(opQuestionMark, intOfIntInt, cm); // dummy
    buildOperator(opBitwiseXor,   intOfIntInt, cm); // dummy
    buildOperator(opExponent,     intOfIntInt, cm);
    buildOperator(opExponent,     flOfFlFl, cm);
    buildOperator(opBitwiseOr,    intOfIntInt, cm); // dummy
    buildOperator(opBoolOr,       flOfFl, cm); // dummy
}


private void createBuiltins(Compiler* cm) { //:createBuiltins
// Entities and functions for the built-in operators, types and functions
    buildStandardStrings(cm);
    buildOperators(cm);
}


private void importPrelude(Compiler* cm) { //:importPrelude
// Imports the standard, Prelude kind of stuff into the compiler immediately after the lexing phase
    buildPreludeTypes(cm);
    TypeId strToVoid = addConcrFnType(1, (Int[]){ tokString, tokMisc }, cm);
    TypeId intToVoid = addConcrFnType(1, (Int[]){ tokInt, tokMisc }, cm);
    TypeId floatToVoid = addConcrFnType(1, (Int[]){ tokDouble, tokMisc }, cm);
    TypeId intToDoub = addConcrFnType(1, (Int[]){ tokInt, tokDouble}, cm);
    TypeId doubToInt = addConcrFnType(1, (Int[]){ tokDouble, tokInt}, cm);

    EntityImport imports[6] =  {
        (EntityImport) { .name = nameOfStandard(strMathPi), .typeInd = 0},
        (EntityImport) { .name = nameOfStandard(strMathE), .typeInd = 0},
        (EntityImport) { .name = nameOfStandard(strPrint), .typeInd = 1},
        (EntityImport) { .name = nameOfStandard(strPrint), .typeInd = 2},
        (EntityImport) { .name = nameOfStandard(strPrint), .typeInd = 3},
        (EntityImport) { .name = nameOfStandard(strPrintErr), .typeInd = 1}
           // TODO functions for casting (int, double, unsigned)
    };
    // These base types occupy the first places in the stringTable and in the types table.
    // So for them nameId == typeId, unlike type funcs like L(ist) and A(rray)
    for (Int j = strInt; j <= strVoid; j++) {
        cm->activeBindings[j - strInt + countOperators] = j - strInt;
    }

    importEntities(imports, sizeof(imports)/sizeof(EntityImport),
                   ((Int[]){ strDouble - strFirstNonReserved, strToVoid, intToVoid, floatToVoid,
                             intToDoub, doubToInt }),
                   cm);
}


testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a) {
//:createLexerFromProto A proto compiler contains just the built-in definitions and tables. This fn
// copies it and performs initialization. Post-condition: i has been incremented by the
// standardText size
    Compiler* lx = allocateOnArena(sizeof(Compiler), a);
    Arena* aTmp = mkArena();
    (*lx) = (Compiler){
        // this assumes that the source code is prefixed with the "standardText"
        .i = sizeof(standardText) - 1, .langDef = proto->langDef,
        .sourceCode = prepareInput((const char *)sourceCode->cont, a),
        .inpLength = sourceCode->len + sizeof(standardText) - 1,
        .tokens = createInListToken(LEXER_INIT_SIZE, a),
        .metas = createInListToken(100, a),
        .newlines = createInListInt(500, a),
        .numeric = createInListInt(50, aTmp),
        .lexBtrack = createStackBtToken(16, aTmp),
        .stringTable = copyStringTable(proto->stringTable, a),
        .stringDict = copyStringDict(proto->stringDict, a),
        .wasError = false, .errMsg = &empty,
        .a = a, .aTmp = aTmp
    };
    return lx;
}


testable void initializeParser(Compiler* lx, Compiler* proto, Arena* a) { //:initializeParser
// Turns a lexer into a parser. Initializes all the parser & typer stuff after lexing is done
    if (lx->wasError) {
        return;
    }

    Compiler* cm = lx;
    Int initNodeCap = lx->tokens.len > 64 ? lx->tokens.len : 64;
    cm->scopeStack = createScopeStack();
    cm->backtrack = createStackParseFrame(16, lx->aTmp);
    cm->i = 0;
    cm->loopCounter = 0;
    cm->nodes = createInListNode(initNodeCap, a);
    cm->monoCode = createInListNode(initNodeCap, a);
    cm->monoIds = createMultiAssocList(a);

    StateForExprs* stForExprs = allocateOnArena(sizeof(StateForExprs), a);
    (*stForExprs) = (StateForExprs) {
        .exprFrames = createStackExprFrame(16*sizeof(ExprFrame), a),
        .scratchCode = createStackNode(16*sizeof(Node), a),
        .calls = createStackNode(16*sizeof(Node), a)
    };
    cm->stateForExprs = stForExprs;

    cm->rawOverloads = copyMultiAssocList(proto->rawOverloads, cm->aTmp);
    cm->overloads = (InListInt){.len = 0, .cont = null};

    cm->activeBindings = allocateOnArena(4*lx->stringTable->len, lx->aTmp);
    memcpy(cm->activeBindings, proto->activeBindings, 4*countOperators); // operators only

    Int extraActive = lx->stringTable->len - countOperators;
    if (extraActive > 0) {
        memset(cm->activeBindings + countOperators, 0xFF, extraActive*4);
    }
    cm->entities = createInListEntity(proto->entities.cap, a);
    memcpy(cm->entities.cont, proto->entities.cont, proto->entities.len*sizeof(Entity));
    cm->entities.len = proto->entities.len;
    cm->entities.cap = proto->entities.cap;

    cm->types.cont = allocateOnArena(proto->types.cap*8, a);
    memcpy(cm->types.cont, proto->types.cont, proto->types.len*4);
    cm->types.len = proto->types.len;
    cm->types.cap = proto->types.cap*2;

    cm->typesDict = copyStringDict(proto->typesDict, a);

    cm->importNames = createInListInt(8, lx->aTmp);

    cm->expStack = createStackint32_t(16, cm->aTmp);
    cm->typeStack = createStackint32_t(16, cm->aTmp);
    cm->tempStack = createStackint32_t(16, cm->aTmp);
    cm->scopeStack = createScopeStack();
    importPrelude(cm);
}


private void reserveSpaceInList(Int neededSpace, InListInt st, Compiler* cm) { //:reserveSpaceInList
    if (st.len + neededSpace >= st.cap) {
        Arr(Int) newContent = allocateOnArena(8*(st.cap), cm->a);
        memcpy(newContent, st.cont, st.len*4);
        st.cap *= 2;
        st.cont = newContent;
    }
}


private void validateNameOverloads(Int listId, Int countOverloads, Compiler* cm) {
// Validates the overloads for a name for non-intersecting via their outer types
// 1. For parameter outer types, their arities must be unique and not match any other outer types
// 2. A zero-arity function, if any, must be unique
// 3. For concrete outer types, there must be only one ref
// 4. For refs to concrete entities, full types (which are concrete) must be different
    Arr(Int) ov = cm->overloads.cont;
    Int start = listId + 1;
    Int outerSentinel = start + countOverloads;
    Int prevParamOuter = -1;
    Int o = start;
    for (; o < outerSentinel && ov[o] < -1; o++) {
        // These are the param outer types, like the outer of "U | U(Int)". Their values are
        // (-arity - 1)
        VALIDATEP(ov[o] != prevParamOuter, errTypeOverloadsIntersect)
        prevParamOuter = ov[o];
    }
    if (o > start) {
        // Since we have some param outer types, we need to check every positive outer type, and -1
        // The rough simple criterion is they must not have the same arity as any param outer
        for (Int k = o; k < outerSentinel; k++) {
            if (ov[k] == -1) {
                continue;
            }
            Int outerArityNegated = -typeGetArity(ov[k], cm) - 1;
            VALIDATEP(binarySearch(outerArityNegated, start, o, ov) == -1,
                      errTypeOverloadsIntersect)
        }
    }
    if (o + 1 < outerSentinel && ov[o] == -1) {
        VALIDATEP(ov[o + 1] > -1, errTypeOverloadsOnlyOneZero)
        ++o;
    }
    if (o == outerSentinel) {
        return;
    }
    // Validate that all nonnegative outerTypes are distinct
    OuterTypeId prevOuter = ov[o];
    for (Int k = o + 1; k < outerSentinel; k++)  {
        VALIDATEP(ov[k] != prevOuter, errTypeOverloadsIntersect)
        prevOuter = ov[k];
    }
}

#ifdef TEST
void printIntArray(Int count, Arr(Int) arr);
#endif


testable Int createNameOverloads(NameId nameId, Compiler* cm) {
// Creates a final subtable in [overloads] for a name and returns the index of said subtable
// Precondition: [rawOverloads] contain twoples of (typeId ref)
// (typeId = the full type of a function)(ref = entityId or monoId)(yes, "twople" = tuple of two)
// Postcondition: [overloads] will contain a subtable of length(outerTypeIds)(refs)
    Arr(Int) raw = cm->rawOverloads->cont;
    Int listId = -cm->activeBindings[nameId] - 2;
    Int rawStart = listId + 2;

#if defined(SAFETY) || defined(TEST)
    VALIDATEI(rawStart != -1, iErrorImportedFunctionNotInScope)
#endif

    Int countOverloads = raw[listId]/2;
    Int rawSentinel = rawStart + raw[listId];

    Arr(Int) ov = cm->overloads.cont;
    Int newInd = cm->overloads.len;
    ov[newInd] = 2*countOverloads; // length of the subtable for this name
    cm->overloads.len += 2*countOverloads + 1;

    if (nameId == 81)  {
        print("newInd for 81 %d count ov %d", newInd, countOverloads)
    }
    for (Int j = rawStart, k = newInd + 1; j < rawSentinel; j += 2, k++) {
        FirstArgTypeId firstParamType = raw[j];
        if (nameId == 81)  {
            print("first %d its outer %d", firstParamType)
        }
        OuterTypeId outerType = typeGetOuter(firstParamType, cm);
        ov[k] = outerType;
        ov[k + countOverloads] = raw[j + 1]; // entityId
    }

    sortPairsDistant(newInd + 1, newInd + 1 + 2*countOverloads, countOverloads, ov);
    validateNameOverloads(newInd, countOverloads, cm);
    return newInd;
}


testable void createOverloads(Compiler* cm) {
// Fills [overloads] from [rawOverloads]. Replaces all indices in
// [activeBindings] to point to the new overloads table (they pointed to [rawOverloads] previously)
    cm->overloads.cont = allocateOnArena(cm->countOverloads*8 + cm->countOverloadedNames*4,
                                         cm->a);
    // Each overload requires 2x4 = 8 bytes for the pair of (outerType entityId).
    // Plus you need an int per overloaded name to hold the length of the overloads for that name

    cm->overloads.len = cm->countOverloads;
    for (Int j = 0; j < countOperators; j++) {
        Int newIndex = createNameOverloads(j, cm);
        cm->activeBindings[j] = -newIndex - 2;
    }
    removeDuplicatesInList(&(cm->importNames));
    printIntArray(cm->importNames.len, cm->importNames.cont);
    for (Int j = 0; j < cm->importNames.len; j++) {
        Int nameId = cm->importNames.cont[j];
        Int newIndex = createNameOverloads(nameId, cm);
        cm->activeBindings[nameId] = -newIndex - 2;
    }
    for (Int j = 0; j < cm->toplevels.len; j++) {
        Int nameId = cm->toplevels.cont[j].name & LOWER24BITS;
        Int newIndex = createNameOverloads(nameId, cm);
        cm->activeBindings[nameId] = -newIndex - 2;
    }
}


private void pToplevelTypes(Compiler* cm) {
// Parses top-level types but not functions. Writes them to the types table and adds
// their bindings to the scope
    cm->i = 0;
    Arr(Token) toks = cm->tokens.cont;
    const Int len = cm->tokens.len;
    while (cm->i < len) {
        Token tok = toks[cm->i];
        if (tok.tp == tokAssignLeft && toks[cm->i + 1].tp == tokTypeName) {
            ++cm->i; // CONSUME the assignment token
            typeDef(tok, false, P_C);
        } else {
            cm->i += (tok.pl2 + 1);
        }
    }
}


private void pToplevelConstants(Compiler* cm) {
// Parses top-level constants but not functions, and adds their bindings to the scope
    cm->i = 0;
    Arr(Token) toks = cm->tokens.cont;
    const Int len = cm->tokens.len;
    while (cm->i < len) {
        Token tok = toks[cm->i];
        if (tok.tp == tokAssignLeft && tok.pl2 == 0) {
            Token rightTk = toks[cm->i + 2];
            if (rightTk.tp != tokFn) {
                cm->i += 1; // CONSUME the left and right assignment
                pAssignment(tok, P_C);
                maybeCloseSpans(cm);
                //parseUpTo(cm->i + toks[cm->i + 1].pl2 + 1, P_C);
            } else {
                cm->i += (tok.pl2 + 1);
            }
        } else {
            cm->i += (tok.pl2 + 1);
        }
    }
}


#ifdef SAFETY
private void validateOverloadsFull(Compiler* cm) {
/*
    Int lenTypes = cm->types.len; Int lenEntities = cm->entities.len;
    for (Int i = 1; i < cm->overloadIds.len; i++) {
        Int currInd = cm->overloadIds.cont[i - 1];
        Int nextInd = cm->overloadIds.cont[i];

        VALIDATEI((nextInd > currInd + 2) && (nextInd - currInd) % 2 == 1, iErrorOverloadsIncoherent)

        Int countOverloads = (nextInd - currInd - 1)/2;
        Int countConcreteOverloads = cm->overloads.cont[currInd];
        VALIDATEI(countConcreteOverloads <= countOverloads, iErrorOverloadsIncoherent)
        for (Int j = currInd + 1; j < currInd + countOverloads; j++) {
            if (cm->overloads.cont[j] < 0) {
                throwExcInternal(iErrorOverloadsNotFull, cm);
            }
            if (cm->overloads.cont[j] >= lenTypes) {
                throwExcInternal(iErrorOverloadsIncoherent, cm);
            }
        }
        for (Int j = currInd + countOverloads + 1; j < nextInd; j++) {
            if (cm->overloads.cont[j] < 0) {
                print("ERR overload missing entity currInd %d nextInd %d j %d cm->overloads.cont[j] %d", currInd, nextInd,
                    j, cm->overloads.cont[j])
                throwExcInternal(iErrorOverloadsNotFull, cm);
            }
            if (cm->overloads.cont[j] >= lenEntities) {
                throwExcInternal(iErrorOverloadsIncoherent, cm);
            }
        }
    }
*/
}
#endif

/*
 * TO DELETE
private Int parseFnParamList(Token paramListTk, Compiler* cm) {
// Precondition: we are 1 past the tokParamList
// Returns the function's type, interpreted to be `Void => Void` if paramList is absent
    if (cm->tokens.cont[cm->i].tp == tokTypeName) {
        Token fnReturnType = cm->tokens.cont[cm->i];
        Int returnTypeId = cm->activeBindings[fnReturnType.pl2];
        VALIDATEP(returnTypeId > -1, errUnknownType)
        pushIntypes(returnTypeId, cm);

        cm->i++; // CONSUME the function return type token
    } else {
        pushIntypes(tokMisc, cm); // Misc stands for the Void type
    }
    VALIDATEP(cm->tokens.cont[cm->i].tp == tokParens, errFnNameAndParams)

    Int paramsTokenInd = cm->i;
    Token parens = cm->tokens.cont[paramsTokenInd];
    Int paramsSentinel = cm->i + parens.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list

    Int fnEntityId = cm->entities.len;

    Int arity = 0;
    Int firstParamType = -1;
    while (cm->i < paramsSentinel) {
        Token paramName = cm->tokens.cont[cm->i];
        VALIDATEP(paramName.tp == tokWord, errFnNameAndParams)
        ++cm->i; // CONSUME a param name
        ++arity;

        VALIDATEP(cm->i < paramsSentinel, errFnNameAndParams)
        Token paramType = cm->tokens.cont[cm->i];
        VALIDATEP(paramType.tp == tokTypeName, errFnNameAndParams)

        Int paramTypeId = cm->activeBindings[paramType.pl2]; // the binding of this parameter's type
        VALIDATEP(paramTypeId > -1, errUnknownType)
        pushIntypes(paramTypeId, cm);
        if (firstParamType == -1) {
            firstParamType = paramTypeId;
        }

        ++cm->i; // CONSUME the param's type name
    }

    cm->types.cont[tentativeTypeInd] = arity + 1;

    Int uniqueTypeId = mergeType(tentativeTypeInd, cm);

}
*/


testable void parseFnSignature(Token fnDef, bool isToplevel, untt name, Int voidToVoid,
                               Compiler* cm) {
// Parses a function signature. Emits no nodes, adds data to [toplevels], [functions], [overloads].
// Pre-condition: we are 1 token past the tokFn
    Int fnStartTokenId = cm->i - 1;
    Int fnSentinelToken = fnStartTokenId + fnDef.pl2 + 1;

    NameId fnNameId = name & LOWER24BITS;

    pushIntypes(0, cm); // will overwrite it with the type's length once we know it

    Arr(Token) toks = cm->tokens.cont;
    TypeId newFnTypeId = voidToVoid; // default for nullary functions
    if (toks[cm->i].tp == tokParamList) {
        Token paramListTk = toks[cm->i];
        cm->i++;
        newFnTypeId = typeDef(paramListTk, true, P_C);
    }

    EntityId newFnEntityId = cm->entities.len;
    pushInentities(((Entity){ .class = classImmutable, .typeId = newFnTypeId }), cm);
    addRawOverload(fnNameId, newFnTypeId, newFnEntityId, cm);

    pushIntoplevels((Toplevel){.indToken = fnStartTokenId, .sentinelToken = fnSentinelToken,
            .name = name, .entityId = newFnEntityId }, cm);
}


private void pToplevelBody(Toplevel toplevelSignature, Arr(Token) toks, Compiler* cm) {
// Parses a top-level function.
// The result is the AST ([] FnDef EntityName Scope EntityParam1 EntityParam2 ... )
    Int fnStartInd = toplevelSignature.indToken;
    Int fnSentinel = toplevelSignature.sentinelToken;

    cm->i = fnStartInd; // tokFn
    Int startBt = toks[fnStartInd + 1].startBt;
    Token fnTk = toks[cm->i];

    // the fnDef scope & node
    Int entityId = toplevelSignature.entityId;
    push(((ParseFrame){ .tp = nodFnDef, .startNodeInd = cm->nodes.len, .sentinelToken = fnSentinel,
                        .typeId = cm->entities.cont[entityId].typeId}), cm->backtrack);
    pushInnodes(((Node){ .tp = nodFnDef, .startBt = fnTk.startBt, .lenBts = fnTk.lenBts }), cm);
    pushInnodes((Node){ .tp = nodBinding, .pl1 = entityId, .startBt = startBt,
                        .lenBts = (Int)(startBt >> 24) }, cm);

    // the scope for the function's body
    addParsedScope(fnSentinel, fnTk.startBt, fnTk.lenBts, cm);

    if (toks[cm->i + 1].tp == tokParamList) {
        Int paramsSentinel = cm->i + fnTk.pl2 + 1;
        cm->i++; // CONSUME the parens token for the param list
        while (cm->i < paramsSentinel) {
            Token paramName = toks[cm->i];
            Int newEntityId = createEntity(nameOfToken(paramName), cm);
            ++cm->i; // CONSUME the param name
            Int typeId = cm->activeBindings[paramName.pl2];
            cm->entities.cont[newEntityId].typeId = typeId;

            Node paramNode = (Node){.tp = nodBinding, .pl1 = newEntityId,
                                    .startBt = paramName.startBt, .lenBts = paramName.lenBts };

            ++cm->i; // CONSUME the param's type name
            pushInnodes(paramNode, cm);
        }
    }

    ++cm->i; // CONSUME the "=" sign
    parseUpTo(fnSentinel, P_C);
}


private void pFunctionBodies(Arr(Token) toks, Compiler* cm) {
// Parses top-level function params and bodies
    for (int j = 0; j < cm->toplevels.len; j++) {
        cm->loopCounter = 0;
        pToplevelBody(cm->toplevels.cont[j], P_C);
    }
}


private void pToplevelSignatures(Compiler* cm) {
// Walks the top-level functions' signatures (but not bodies). Increments counts of overloads
// Result: the overload counts and the list of toplevel functions to parse. No nodes emitted
    cm->i = 0;
    Arr(Token) toks = cm->tokens.cont;
    Int len = cm->tokens.len;

    Int voidToVoid = addConcrFnType(1, (Int[]){ tokMisc, tokMisc}, cm);
    Int nextI = cm->i;
    for (Token tok = toks[cm->i]; cm->i < len; cm->i = nextI, tok = toks[nextI]) {
        if (tok.tp != tokAssignLeft) {
            nextI = cm->i + tok.pl2 + 1;  // CONSUME the whole span, whatever it is
            continue;
        }
        VALIDATEP(tok.pl2 == 0, errAssignmentToplevelFn)
        nextI = cm->i + toks[cm->i + 1].pl2 + 2; // CONSUME tokAssignmentRight (later)
        Token rightTk = toks[cm->i + 2]; // the token after tokAssignRight
        if (rightTk.tp != tokFn) {
            continue;
        }

        //Int sentinel = calcSentinel(tok, cm->i);
        //parseUpTo(cm->i + tok.pl2, P_C);

        // since this is an immutable definition tokAssignLeft, its pl1 is the nameId and
        // its bytes are same as the var name in source code
        untt name =  ((untt)tok.lenBts << 24) + (untt)tok.pl1;
        cm->i += tok.pl2 + 3; // CONSUME the left side, tokAssignmentRight and tokFn
        parseFnSignature(rightTk, true, name, voidToVoid, cm);
    }
}


// Must agree in order with node types in tl.internal.h
const char* nodeNames[] = {
    "Int", "Long", "Double", "Bool", "String", "~", "misc",
    "id", "call", "binding",
    "{}", "expr", "...=", "=...", "_",
    "alias", "assert", "breakCont", "catch", "defer",
    "import", "^{}", "iface", "[]", "return", "try",
    "for", "forCond", "if", "ei", "impl", "match"
};


private void printType(Int typeInd, Compiler* cm) {
    if (typeInd < 5) {
        printf("%s\n", nodeNames[typeInd]);
        return;
    }
    Int arity = cm->types.cont[typeInd] - 1;
    Int retType = cm->types.cont[typeInd + 1];
    if (retType < 5) {
        printf("%s(", nodeNames[retType]);
    } else {
        printf("Void(");
    }
    for (Int j = typeInd + 2; j < typeInd + arity + 2; j++) {
        Int tp = cm->types.cont[j];
        if (tp < 5) {
            printf("%s ", nodeNames[tp]);
        }
    }
    print(")");
}


testable Compiler* parseMain(Compiler* cm, Arena* a) {
    if (setjmp(excBuf) == 0) {
        pToplevelTypes(cm);
        // This gives us the semi-complete overloads & overloadIds tables (with only the
        // built-ins and imports)
        pToplevelConstants(cm);

        // This gives the complete overloads & overloadIds tables + list of toplevel functions
        pToplevelSignatures(cm);
        createOverloads(cm);

#ifdef SAFETY
        validateOverloadsFull(cm);
#endif
        // The main parse (all top-level function bodies)
        pFunctionBodies(cm->tokens.cont, cm);
    }
    return cm;
}


testable Compiler* parse(Compiler* cm, Compiler* proto, Arena* a) {
// Parses a single file in 4 passes, see docs/parser.txt
    initializeParser(cm, proto, a);
    return parseMain(cm, a);
}

//}}}
//{{{ Types
//{{{ Type utils

private Int typeEncodeTag(untt sort, Int depth, Int arity, Compiler* cm) {
    return (Int)((untt)(sort << 16) + (depth << 8) + arity);
}

testable void typeAddHeader(TypeHeader hdr, Compiler* cm) { //:typeAddHeader
// Writes the bytes for the type header to the tail of the cm->types table.
// Adds two 4-byte elements
    pushIntypes((Int)((untt)((untt)hdr.sort << 16) + ((untt)hdr.depth << 8) + hdr.arity), cm);
    pushIntypes((Int)hdr.nameAndLen, cm);
}

testable TypeHeader typeReadHeader(Int typeId, Compiler* cm) {
// Reads a type header from the type array
    Int tag = cm->types.cont[typeId + 1];
    TypeHeader result = (TypeHeader){ .sort = ((untt)tag >> 16) & LOWER16BITS,
            .depth = (tag >> 8) & 0xFF, .arity = tag & 0xFF,
            .nameAndLen = (untt)cm->types.cont[typeId + 2] };
    return result;
}


private Int typeGetArity(Int typeId, Compiler* cm) { //:typeGetArity
    if (cm->types.cont[typeId] == 0) {
        return 0;
    }
    Int tag = cm->types.cont[typeId + 1];
    return tag & 0xFF;
}


private OuterTypeId typeGetOuter(FirstArgTypeId typeId, Compiler* cm) { //:typeGetOuter
// A         => A  (concrete types)
// A(B)      => A  (concrete generic types)
// A | A(B)  => -2 (param generic types)
// F(A => B) => F(A => B)
    if (typeId <= topVerbatimType) {
        return typeId;
    }
    TypeHeader hdr = typeReadHeader(typeId, cm);
    if (hdr.sort <= sorFunction) {
        return typeId;
    } else if (hdr.sort == sorPartial) {
        Int genElt = cm->types.cont[typeId + hdr.arity + 3];
        if ((genElt >> 24) & 0xFF == 0xFF) {
            return -(genElt & 0xFF) - 1; // a param type in outer position,
                                         // so we return its (-arity -1)
        } else {
            return genElt & LOWER24BITS;
        }
    } else {
        return cm->types.cont[typeId + hdr.arity + 3] & LOWER24BITS;
    }
}


private Int isFunction(FirstArgTypeId typeId, Compiler* cm) {
// Returns the function's depth (count of args) if the type is a function type, -1 otherwise
    if (typeId < topVerbatimType) {
        return -1;
    }
    TypeHeader hdr = typeReadHeader(typeId, cm);
    return (hdr.sort == sorFunction) ? hdr.depth : -1;
}

//}}}
//{{{ Parsing type names

testable void typeAddTypeParam(Int paramInd, Int arity, Compiler* cm) { //:typeAddTypeParam
// Adds a type param to a Concretization-sort type. Arity > 0 means it's a type call
    pushIntypes((0xFF << 24) + (paramInd << 8) + arity, cm);
}

testable void typeAddTypeCall(Int typeInd, Int arity, Compiler* cm) {
// Known type fn call
    pushIntypes((arity << 24) + typeInd, cm);
}


testable Int typeSingleItem(Token tk, Compiler* cm) {
// A single-item type, like "Int". Consumes no tokens.
// Pre-condition: we are 1 token past the token we're parsing.
// Returns the type of the single item, or in case it's a type param, (-nameId - 1)
    Int typeId = -1;
    if (tk.tp == tokTypeName) {
        bool isAParam = tk.pl1 > 0;
        if (isAParam) {
            return -tk.pl1 - 1;
        } else {
            typeId = cm->activeBindings[tk.pl2];
            VALIDATEP(typeId > -1, errUnknownType)
        }
        return typeId;
    } else {
        throwExcParser(errUnexpectedToken);
    }
}


private Int typeCountArgs(Int sentinelToken, P_CT) {
// Counts the arity of a type call. Consumes no tokens.
// Precondition: we are pointing 1 past the type call
    Int arity = 0;
    Int j = cm->i;
    bool metArrow = false;
    while (j < sentinelToken) {
        Token tok = toks[j];

        if (tok.tp == tokMisc && tok.pl1 == miscArrow) {
            metArrow = true;
            ++j;
        } else {
            if (tok.tp == tokTypeCall || tok.tp == tokParens) {
                j += tok.pl2 + 1;
            } else {
                ++j;
            }
            if (metArrow) {
                VALIDATEP(j == sentinelToken, errTypeFnSingleReturnType)
            }
        }
        ++arity;
    }
    return arity;
}

testable Int typeParamBinarySearch(Int nameIdToFind, Compiler* cm) {
// Performs a binary search of the binary params in cm->typeStack. Returns -1 if nothing is found
    if (cm->typeStack->len == 0) {
        return -1;
    }
    Arr(Int) st = cm->typeStack->cont;
    Int i = 0;
    Int j = cm->typeStack->len - 2;
    if (st[i] == nameIdToFind) {
        return i;
    } else if (st[j] == nameIdToFind) {
        return j;
    }

    while (i < j) {
        if (j - i == 2) {
            return -1;
        }
        Int midInd = (i + j)/2;
        if (midInd % 2 == 1) {
            --midInd;
        }
        Int mid = st[midInd];
        if (mid > nameIdToFind) {
            j = midInd;
        } else if (mid < nameIdToFind) {
            i = midInd;
        } else {
            return midInd;
        }
    }
    return -1;
}

//}}}
//{{{ Type expressions

// Type expression data format: First element is the tag (one of the following
// constants), second is payload. Type calls need to have an extra payload, so their tag
// is (8 bits of "tye", 24 bits of typeId). Used in [expStack]
#define tyeStruct     1 // payload: count of fields in the struct
#define tyeSum        2 // payload: count of variants
#define tyeFunction   3 // payload: count of parameters. This is a function signature, not the F(...)
#define tyeFnType     4 // payload: count of parameters. This is the F(...)
#define tyeType       5 // payload: typeId
#define tyeTypeCall   6 // payload: count of args. Payl in tag: nameId
#define tyeParam      7 // payload: paramId
#define tyeParamCall  8 // payload: count of args. Payl in tag: nameId
#define tyeName       9 // payload: nameId. Used for struct fields, function params, sum variants
#define tyeMeta      10 // payload: index of this meta's token
#define tyeRetType   11 // payload: none

private void shiftTypeStackLeft(Int startInd, Int byHowMany, Compiler* cm);


private TypeId typeCreateStruct(StackInt* exp, Int startInd, Int length, StackInt* params,
                             untt nameAndLen, Compiler* cm) {
// Creates/merges a new struct type from a sequence of pairs in "exp" and a list of type params
// in "params". The sequence must be flat, i.e. not include any nested structs.
// Returns the typeId of the new type
    Int tentativeTypeId = cm->types.len;
    pushIntypes(0, cm);
    Int sentinel = startInd + length;
    if (length % 4 == 2 && penultimate(exp) == tyeMeta) {
        sentinel -= 2;
    }
#ifdef SAFETY
    VALIDATEP(sentinel - startInd % 4 == 0, "typeCreateStruct err not divisible by 4")
#endif
    Int countFields = (sentinel - startInd)/4;
    typeAddHeader((TypeHeader){
        .sort = sorStruct, .arity = params->len/2, .depth = countFields,
        .nameAndLen = nameAndLen}, cm);
    for (Int j = 1; j < params->len; j += 2) {
        pushIntypes(params->cont[j], cm);
    }

    for (Int j = startInd + 1; j < sentinel; j += 4) {
        // names of fields
#ifdef SAFETY
/*
        VALIDATEP(exp->cont[j - 1] == tyeField, "not a field")
*/
#endif
        pushIntypes(exp->cont[j], cm);
    }

    for (Int j = startInd + 3; j < sentinel; j += 4) {
        // types of fields
#ifdef SAFETY
        VALIDATEP(exp->cont[j - 1] == tyeType, "not a type")
#endif
        pushIntypes(exp->cont[j], cm);
    }
    cm->types.cont[tentativeTypeId] = cm->types.len - tentativeTypeId - 1;
    return mergeType(tentativeTypeId, cm);
}


private TypeId typeCreateTypeCall(StackInt* exp, Int startInd, Int length, StackInt* params,
                               bool isFunction, Compiler* cm) {
// Creates/merges a new type call from a sequence of pairs in "exp" and a list of type params
// in "params". Handles both ordinary type calls and function types.
// Returns the typeId of the new type
    Int tentativeTypeId = cm->types.len;
    pushIntypes(0, cm);
    Int sentinel = startInd + length;
    Int countFnParams = (sentinel - startInd)/2;
    bool metRetType = exp->len >= 4 && exp->cont[exp->len - 4] == tyeRetType;
    if (metRetType) {
        --countFnParams;
    }
    typeAddHeader((TypeHeader){
        .sort = (isFunction ? sorFunction : sorPartial), .arity = params->len/2, .depth = countFnParams,
        .nameAndLen = 0}, cm);
    for (Int j = 1; j < params->len; j += 2) {
        pushIntypes(params->cont[j], cm);
    }

    for (Int j = startInd + 1; j < sentinel; j += 2) {
        Int tye = exp->cont[j - 1] & LOWER24BITS;
        if (tye == tyeRetType) {
        } else if (tye == tyeType) {
            pushIntypes(exp->cont[j], cm);
        } else {
            throwExcParser("not a type");
        }
    }
    if (!metRetType) {
        pushIntypes(stToNameId(strVoid), cm);
    }

    cm->types.cont[tentativeTypeId] = cm->types.len - tentativeTypeId - 1;
    return mergeType(tentativeTypeId, cm);
}


private TypeId typeCreateFnSignature(StackInt* exp, Int startInd, Int length, StackInt* params,
                                     untt nameAndLen, Compiler* cm) {
// Creates/merges a new struct type from a sequence of pairs in "exp" and a list of type params
// in "params". The sequence must be flat, i.e. not include any nested structs.
// Returns the typeId of the new type
// Example input: exp () params ()
    Int tentativeTypeId = cm->types.len;
    pushIntypes(0, cm);
    Int sentinel = startInd + length;
    if (length % 4 == 2 && penultimate(exp) == tyeMeta) {
        sentinel -= 2;
    }
#ifdef SAFETY
    VALIDATEP(sentinel - startInd % 4 == 0, "typeCreateFnSignature error not divisible by 4")
#endif
    Int countFields = (sentinel - startInd)/4;
    typeAddHeader((TypeHeader){
        .sort = sorFunction, .arity = params->len/2, .depth = countFields,
        .nameAndLen = nameAndLen}, cm);
    for (Int j = 1; j < params->len; j += 2) {
        pushIntypes(params->cont[j], cm);
    }

    for (Int j = startInd + 1; j < sentinel; j += 4) {
        // names of fields
#ifdef SAFETY
/*
        VALIDATEP(exp->cont[j - 1] == tydField, "not a field")
*/
#endif
        pushIntypes(exp->cont[j], cm);
    }

    for (Int j = startInd + 3; j < sentinel; j += 4) {
        // types of fields
#ifdef SAFETY
        VALIDATEP(exp->cont[j - 1] == tyeType, "not a type")
#endif
        pushIntypes(exp->cont[j], cm);
    }
    cm->types.cont[tentativeTypeId] = cm->types.len - tentativeTypeId - 1; // set the type length
    return mergeType(tentativeTypeId, cm);
}


private Int typeEvalExpr(StackInt* exp, StackInt* params, untt nameAndLen, Compiler* cm) {
// Processes the "type expression" produced by "typeDef".
// Returns the typeId of the new typeId
    Int j = exp->len - 2;
#if defined(TEST) && defined(TRACE)
    printIntArray(exp->len, exp->cont);
#endif
    while (j > 0) {
        Int tyeContent = exp->cont[j];
        Int tye = (exp->cont[j] >> 24) & 0xFF;
        if (tye == 0)
            { tye = tyeContent; }
        if (tye == tyeStruct) {
            Int lengthStruct = exp->cont[j + 1]*4; // *4 because for every field there are 4 ints
            Int typeNestedStruct = typeCreateStruct(exp, j + 2, lengthStruct, params, 0, cm);
            exp->cont[j] = tyeType;
            exp->cont[j + 1] = typeNestedStruct;
            shiftTypeStackLeft(j + 2 + lengthStruct, lengthStruct, cm);

#if defined(TEST) && defined(TRACE)
            print("after struct shift:");
            printIntArray(exp->len, exp->cont);
#endif
        } else if (tye == tyeTypeCall || tye == tyeFnType) {
            Int lengthFn = exp->cont[j + 1]*2;
            Int typeFn = typeCreateTypeCall(exp, j + 2, lengthFn, params, tye == tyeFnType, cm);
            exp->cont[j] = tyeType;
            exp->cont[j + 1] = typeFn;
            shiftTypeStackLeft(j + 2 + lengthFn, lengthFn, cm);

#if defined(TEST) && defined(TRACE)
            print("after typeCall/fnType shift:");
            printIntArray(exp->len, exp->cont);
#endif
        } else if (tye == tyeParamCall) {

        }
        j -= 2;
    }
#if defined(TEST) && defined(TRACE)
    print("after processing")
    printIntArray(exp->len, exp->cont);
#endif
    if (nameAndLen > 0) {
        // at this point, exp must contain just a single struct with its fields
        // followed by just typeIds
        Int lengthNewStruct = exp->cont[1]*4; // *4 because for every field there are 4 ints
        return typeCreateStruct(exp, 2, lengthNewStruct, params, nameAndLen, cm);
    } else {
        Int lengthNewSignature = exp->cont[1]*4; // *4 because for every field there are 4 ints
        return typeCreateFnSignature(exp, 2, lengthNewSignature, params, 0, cm);
    }
}

#define maxTypeParams 254


private void typeDefReadParams(Token bracketTk, StackInt* params, Compiler* cm) {
// Precondition: we are pointing 1 past the bracket token
    Int brackSentinel = cm->i + bracketTk.pl2;
    while (cm->i < brackSentinel) {
        Token tk = cm->tokens.cont[cm->i];
        VALIDATEP(tk.tp == tokTypeName, errTypeDefParamsError)
        Int nameId = tk.pl2;
        VALIDATEP(cm->activeBindings[nameId] == -1, errAssignmentShadowing)
        cm->activeBindings[nameId] = -params->len - 2;
        push(tk.pl2, params);
        if (tk.pl1 > 0) {
            VALIDATEP(cm->i + 1 < brackSentinel, errTypeDefParamsError)
            Token arityTk = cm->tokens.cont[cm->i];
            VALIDATEP(arityTk.tp == tokInt && arityTk.pl1 == 0
                      && arityTk.pl2 > 0 && arityTk.pl2 < 255, errTypeDefParamsError)
            push(arityTk.pl2, params);
        } else {
            push(0, params);
        }
        VALIDATEP(params->len <= maxTypeParams, errWordLengthExceeded)
        ++cm->i;
    }
}


private void typeDefClearParams(StackInt* params, Compiler* cm) {
    for (Int j = 0; j < params->len; j += 2) {
        cm->activeBindings[params->cont[j]] = -1;
    }
    params->len = 0;
}


private Int typeCountFieldsInStruct(Int length, Compiler* cm) {
// Returns the number of fields in struct definition. Precond: we are 1 past the parens token
    Int count = 0;
    Int j = cm->i;
    Int sentinel = j + length;
    while (j < sentinel) {
        VALIDATEP(cm->tokens.cont[j].tp == tokWord, errTypeDefCannotContain)
        ++j;
        Token nextTk = cm->tokens.cont[j];
        if (nextTk.tp == tokTypeCall || nextTk.tp == tokParens) {
            j += nextTk.pl2 + 1;
        } else if (nextTk.tp == tokTypeName) {
            ++j;
        } else {
            throwExcParser(errTypeDefCannotContain);
        }
        ++count;
    }
    return count;
}


private bool typeExprIsInside(Int tp, Compiler* cm) {
// Is the current type expression inside e.g. a struct or a function
    return (cm->tempStack->len >= 2
            && penultimate(cm->tempStack) == tp);
}


private void typeBuildExpr(StackInt* exp, Int sentinel, Compiler* cm) {
// Precondition: exp->len == 0, params->len == 0, and we are looking at the first token in actual
// type definition
    StackInt* params = cm->typeStack;
    while (cm->i < sentinel) {
        Token cTk = cm->tokens.cont[cm->i];
        ++cm->i; // CONSUME the current token
        if (cTk.tp == tokParens) {
            // TODO sum types
            Int countFields = typeCountFieldsInStruct(cTk.pl2, cm);
            push(tyeStruct, exp);
            push(countFields, exp);

            push(tyeStruct, cm->tempStack);
            push(cm->i + cTk.pl2, cm->tempStack);  // sentinel
        } else if (cTk.tp == tokTypeName) {
            Int mbParamId = typeParamBinarySearch(cTk.pl2, cm);
            if (mbParamId == -1) {
                push(tyeType, exp);
                push(cTk.pl2, exp);
            } else {
                push(tyeParam, exp);
                push(cTk.pl2, exp);
            }
        } else if (cTk.tp == tokTypeCall) {
            Int countArgs = typeCountArgs(calcSentinel(cTk, cm->i - 1), cm->tokens.cont, cm);
            Int nameId = cTk.pl1 & LOWER24BITS;
            Int mbParamId = typeParamBinarySearch(nameId, cm);
            if (mbParamId == -1) {
                if (nameId == stToNameId(strF)) { // F(...)
                    push(((untt)tyeFnType << 24) + nameId, exp);
                    push(countArgs, exp);

                    push(tyeFnType, cm->tempStack);
                    push(cm->i + cTk.pl2, cm->tempStack);
                } else {
                    Int typeId = cm->activeBindings[nameId];
                    VALIDATEP(typeId > -1, errUnknownTypeConstructor)
                    /* Int typeAr = typeGetArity(typeId, cm); */
                    VALIDATEP(typeGetArity(typeId, cm) == countArgs, errTypeConstructorWrongArity)
                    push(((untt)tyeTypeCall << 24) + nameId, exp);
                    push(countArgs, exp);
                }
            } else {
                VALIDATEP(params->cont[mbParamId + 1] == countArgs, errTypeConstructorWrongArity)
                push(((untt)tyeParamCall << 24) + nameId, exp);
                push(countArgs, exp);
            }
        } else if (cTk.tp == tokWord) {
            VALIDATEP(typeExprIsInside(tyeStruct, cm), errTypeDefError)

            push(tyeName, exp);
            push(cTk.pl2, exp);  // nameId
            Token nextTk = cm->tokens.cont[cm->i];
            VALIDATEP(nextTk.tp == tokTypeName || nextTk.tp == tokTypeCall || nextTk.tp == tokParens,
                      errTypeDefError)
        } else if (cTk.tp == tokMisc && cTk.pl1 == miscArrow) {
            VALIDATEP(cm->tempStack->len >= 2, errTypeDefError)
            Int penult = penultimate(cm->tempStack);
#if defined(TEST) && defined(TRACE)
            printIntArray(cm->tempStack->len, cm->tempStack->cont);
#endif
            VALIDATEP(penult == tyeFunction || penult == tyeFnType, errTypeDefError)

            push(tyeRetType, exp);
            push(0, exp);
        } else {
            print("erroneous type %d", cTk.tp)
            throwExcParser(errTypeDefError);
        }
        while (cm->tempStack->len > 0) {
            if (peek(cm->tempStack) != cm->i) {
                break;
            }
            pop(cm->tempStack);
            pop(cm->tempStack);
        }
    }
}


testable Int typeDef(Token assignTk, bool isFunction, Arr(Token) toks, Compiler* cm) {
// Builds a type expression from a type definition or a function signature.
// Example 1: `Foo = id Int name String`
// Example 2: `a Double b Bool => String`
//
// Accepts a name or -1 for nameless type exprs (like function signatures).
// Uses cm->expStack to build a "type expression" and cm->params for the type parameters
// Produces no AST nodes, but potentially lots of new types
// Consumes the whole type assignment right side, or the whole function signature
// Data format: see "Type expression data format"
// Precondition: we are 1 past the tokAssignmentRight token, or tokParamList token
    StackInt* exp = cm->expStack;
    StackInt* params = cm->typeStack;
    exp->len = 0;
    params->len = 0;

    Int sentinel = cm->i + toks[cm->i + 1].pl2 + 2; // we get the length from the tokAssignmentRight
    Token nameTk = toks[cm->i];
    cm->i += 2; // CONSUME the type name and the tokAssignmentRight

    VALIDATEP(cm->i < sentinel, errTypeDefError)
    if (isFunction) {
        push(tyeFunction, cm->tempStack);
        push(sentinel, cm->tempStack);
    }
    typeBuildExpr(exp, sentinel, cm);

#if defined(TEST) && defined(TRACE)
    printIntArray(exp->len, exp->cont);
#endif

    Int newTypeId = typeEvalExpr(exp, params, ((untt)(nameTk.lenBts) << 24) + nameTk.pl2, cm);
    typeDefClearParams(params, cm);
    return newTypeId;
}

//}}}
//{{{ Overloads, type check & resolve

testable StackInt* typeSatisfiesGeneric(Int typeId, Int genericId, Compiler* cm);


private FirstArgTypeId getFirstParamType(TypeId funcTypeId, Compiler* cm) { //:getFirstParamType
// Gets the type of the first param of a function
    TypeHeader hdr = typeReadHeader(funcTypeId, cm);
    return cm->types.cont[funcTypeId + 3 + hdr.arity]; // +3 skips the type length, type tag & name
}

private bool isFunctionWithParams(TypeId typeId, Compiler* cm) { //:isFunctionWithParams
    return cm->types.cont[typeId] > 1;
}


testable bool findOverload(FirstArgTypeId typeId, Int ovInd, EntityId* entityId, Compiler* cm) {
// Params: typeId = type of the first function parameter, or -1 if it's 0-arity
//         ovInd = ind in [overloads], which is found via [activeBindings]
//         entityId = address where to store the result, if successful
// We have 4 scenarios here, sorted from left to right in the outerType part of [overloads]:
// 1. outerType < -1: non-function generic types with outer generic, e.g. "U(Int)" => -2
// 2. outerType = -1: 0-arity function
// 3. outerType >=< 0 BIG: non-function types with outer concrete, e.g. "L(U)" => ind of L
// 4. outerType >= BIG: function types (generic or concrete), e.g. "F(Int => String)" => BIG + 1
    Int start = ovInd + 1;
    Arr(Int) overs = cm->overloads.cont;
    print("ovInd %d overs %p len %d %d", ovInd, cm->overloads.cont, cm->overloads.len, cm->countOverloads)
    Int countOverloads = overs[ovInd];
    Int sentinel = ovInd + countOverloads + 1;
    if (typeId == -1) { // scenario 2
        Int j = 0;
        while (j < sentinel && overs[j] < 0) {
            if (overs[j] == -1) {
                (*entityId) = overs[j + countOverloads];
                return true;
            }
            ++j;
        }
        throwExcParser(errTypeNoMatchingOverload);
    }

    Int outerType = typeGetOuter(typeId, cm);
    Int mbFuncArity = isFunction(typeId, cm);
    if (mbFuncArity > -1) { // scenario 4
        mbFuncArity += BIG;
        Int j = sentinel - 1;
        for (; j > start && overs[j] > BIG; j--) {
            if (overs[j] == mbFuncArity) {
                (*entityId) = overs[j + countOverloads];
                return true;
            }
        }
    } else { // scenarios 1 or 3
        Int firstNonneg = start;
        for (; firstNonneg < sentinel && overs[firstNonneg] < 0; firstNonneg++); // scenario 1

        Int k = sentinel - 1;
        while (k > firstNonneg && overs[k] >= BIG) {
            --k;
        }
        if (k < firstNonneg) {
            return false;
        }

        // scenario 3
        Int ind = binarySearch(outerType, firstNonneg, k, overs);
        if (ind == -1) {
            return false;
        }

        (*entityId) = overs[ind + countOverloads];
        return true;
    }
    return false;
}

#if defined(TRACE) && defined(TEST)
private void printExpSt(StackInt* st);
#endif

private void shiftTypeStackLeft(Int startInd, Int byHowMany, Compiler* cm) {
// Shifts ints from start and until the end to the left.
// E.g. the call with args (4, 2) takes the stack from [x x x x 1 2 3] to [x x 1 2 3]
// startInd and byHowMany are measured in units of Int
    Int from = startInd;
    Int to = startInd - byHowMany;
    Int sentinel = cm->expStack->len;
    while (from < sentinel) {
        Int pieceSize = MIN(byHowMany, sentinel - from);
        memcpy(cm->expStack->cont + to, cm->expStack->cont + from, pieceSize*4);
        from += pieceSize;
        to += pieceSize;
    }
    cm->expStack->len -= byHowMany;
}

private void populateExpStack(Int indExpr, Int sentinelNode, Int* currAhead, Compiler* cm) {
// Populates the expression's type stack with the operands and functions of an expression
    StackInt* st = cm->expStack;
    st->len = 0;
    for (Int j = indExpr + 1; j < sentinelNode; ++j) {
        Node nd = cm->nodes.cont[j];
        if (nd.tp <= tokString) {
            push((Int)nd.tp, st);
        } else if (nd.tp == nodCall) {
            push(BIG + nd.pl2, st); // signifies that it's a call, and its arity
            push((nd.pl2 > 0 ? -nd.pl1 - 2 : nd.pl1), st);
            // index into overloadIds, or entityId for 0-arity fns

            ++(*currAhead);
        } else if (nd.pl1 > -1) { // entityId
            push(cm->entities.cont[nd.pl1].typeId, st);
        } else { // overloadId
            push(nd.pl1, st); // overloadId
        }
    }
#if defined(TRACE) && defined(TEST)
    printExpSt(st);
#endif
}

testable TypeId typeCheckResolveExpr(Int indExpr, Int sentinelNode, Compiler* cm) {
//:typeCheckResolveExpr Typechecks and resolves overloads in a single expression
    Int currAhead = 0; // how many elements ahead we are compared to the token array (because
                       // of extra call indicators)
    StackInt* st = cm->expStack;

    populateExpStack(indExpr, sentinelNode, &currAhead, cm);
    // now go from back to front: resolving the calls, typechecking & collapsing args, and replacing calls
    // with their return types
    Int j = cm->expStack->len - 1;
    Arr(Int) cont = st->cont;
    while (j > -1) {
        if (cont[j] < BIG) { // it's not a function call because function call indicators
                             // have BIG in them
            --j;
            continue;
        }

        // A function call. cont[j] contains the arity, cont[j + 1] the index into [overloads]
        Int arity = cont[j] - BIG;
        Int o = cont[j + 1];
        if (arity == 0) {
            VALIDATEP(o > -1, errTypeOverloadsOnlyOneZero)

            Int functionTypeInd = cm->entities.cont[o].typeId;
#ifdef SAFETY
            VALIDATEI(cm->types.cont[functionTypeInd] == 1, iErrorOverloadsIncoherent)
#endif

            cont[j] = cm->types.cont[functionTypeInd + 1]; // write the return type
            shiftTypeStackLeft(j + 2, 1, cm);
                // the function returns nothing, so there's no return type to write

            --currAhead;
#if defined(TRACE) && defined(TEST)
            printExpSt(st);
#endif
        } else {
            Int tpFstArg = cont[j + 2];
                // + 1 for the element with the overloadId of the func, +1 for return type

            VALIDATEP(tpFstArg > -1, errTypeUnknownFirstArg)
            Int entityId;

            bool ovFound = findOverload(tpFstArg, o, &entityId, cm);

#if defined(TRACE) && defined(TEST)
            if (!ovFound) {
                printExpSt(st);
            }
#endif
            VALIDATEP(ovFound, errTypeNoMatchingOverload)

            Int typeOfFunc = cm->entities.cont[entityId].typeId;
            VALIDATEP(cm->types.cont[typeOfFunc] - 1 == arity, errTypeNoMatchingOverload)
                // first parm matches, but not arity

            for (int k = j + 3; k < j + arity + 2; k++) {
                // We know the type of the function, now to validate arg types against param types
                // Loop init is not "k = j + 2" because we've already checked the first param
                if (cont[k] > -1) {
                    VALIDATEP(cont[k] == cm->types.cont[typeOfFunc + k - j],
                              errTypeWrongArgumentType)
                } else {
                    Int argBindingId = cm->nodes.cont[indExpr + k - currAhead].pl1;
                    cm->entities.cont[argBindingId].typeId = cm->types.cont[typeOfFunc + k - j];
                }
            }
            --currAhead;
            cm->nodes.cont[j + indExpr + 1 - currAhead].pl1 = entityId;
            // the type-resolved function of the call
            cont[j] = cm->types.cont[typeOfFunc + 1]; // the function return type
            shiftTypeStackLeft(j + arity + 2, arity + 1, cm);
#if defined(TRACE) && defined(TEST)
            printExpSt(st);
#endif
        }
        --j;
    }

    if (st->len == 1) {
        return st->cont[0]; // the last remaining element is the type of the whole expression
    } else {
        return -1;
    }
}

//}}}
//{{{ Generic types

testable void typeSkipNode(Int* ind, Compiler* cm) {
// Completely skips the current node in a type, whether the node is concrete or param,
// a call or not
    Int toSkip = 1;
    while (toSkip > 0) {
        Int cType = cm->types.cont[*ind];
        Int hdr = (cType >> 24) & 0xFF;
        if (hdr == 255) {
            toSkip += cType & 0xFF;
        } else {
            toSkip += hdr;
        }
        ++(*ind);
        --toSkip;
    }
}

#define typeGenTag(val) ((val >> 24) & 0xFF)

private Int typeEltArgcount(Int typeElt) {
// Get type-arg-count from a type element (which may be a type call, concrete type, type param etc)
    untt genericTag = typeGenTag(typeElt);
    if (genericTag == 255) {
        return typeElt & 0xFF;
    } else {
        return genericTag;
    }
}


testable bool typeGenericsIntersect(Int type1, Int type2, Compiler* cm) {
// Returns true iff two generic types intersect (i.e. a concrete type may satisfy both of them)
// Warning: this function assumes that both types have sort = Partial
    TypeHeader hdr1 = typeReadHeader(type1, cm);
    TypeHeader hdr2 = typeReadHeader(type2, cm);
    Int i1 = type1 + 3 + hdr1.arity; // +3 to skip the type length, type tag and name
    Int i2 = type2 + 3 + hdr2.arity;

    Int sentinel1 = type1 + cm->types.cont[type1] + 1;
    Int sentinel2 = type2 + cm->types.cont[type2] + 1;
    untt top1 = cm->types.cont[i1];
    untt top2 = cm->types.cont[i2];
    if (top1 != top2) {
        return false;
    } else if (typeGenTag(top1) == 255 || typeGenTag(top2) == 255) {
        // If any type has a param in top position, they already intersect
        return true;
    }
    ++i1;
    ++i2;
    while (i1 < sentinel1 && i2 < sentinel2) {
        untt nod1 = cm->types.cont[i1];
        untt nod2 = cm->types.cont[i2];
        if (typeGenTag(nod1) < 255 && typeGenTag(nod2) < 255) {
            if (nod1 != nod2) {
                return false;
            }
            ++i1;
            ++i2;
        } else {
            // TODO check the arities
            Int argCount1 = typeEltArgcount(nod1);
            Int argCount2 = typeEltArgcount(nod2);
            if (argCount1 != argCount2) {
                return false;
            }
            typeSkipNode(&i1, cm);
            typeSkipNode(&i2, cm);
        }
    }
#ifdef SAFETY
    VALIDATEI(i1 == sentinel1 && i2 == sentinel2, iErrorGenericTypesInconsistent)
#endif
    return true;
}

private Int typeMergeTypeCall(Int startInd, Int len, Compiler* cm) {
// Takes a single call from within a concrete type, and merges it into the type dictionary
// as a whole, separate type. Returns the unique typeId
    return -1;
}

testable StackInt* typeSatisfiesGeneric(Int typeId, Int genericId, Compiler* cm) {
// Checks whether a concrete type satisfies a generic type. Returns a pointer to
// cm->typeStack with the values of parameters if satisfies, null otherwise.
// Example: for typeId = L(L(Int)) and genericId = [T/1]L(T(Int)) returns Generic[L]
// Warning: assumes that typeId points to a concrete type, and genericId to a partial one
    StackInt* tStack = cm->typeStack;
    tStack->len = 0;
    TypeHeader genericHeader = typeReadHeader(genericId, cm);
    // yet-unknown values of the type parameters
    for (Int j = 0; j < genericHeader.arity; j++) {
        push(-1, cm->typeStack);
    }

    Int i1 = typeId + 3; // +3 to skip the type length, type tag and name
    Int i2 = genericId + 3 + genericHeader.arity;
    Int sentinel1 = typeId + cm->types.cont[typeId] + 1;
    Int sentinel2 = genericId + cm->types.cont[genericId] + 1;
    while (i1 < sentinel1 && i2 < sentinel2) {
        untt nod1 = cm->types.cont[i1];
        untt nod2 = cm->types.cont[i2];
        if (typeGenTag(nod2) < 255) {
            if (nod1 != nod2) {
                return null;
            }
            ++i1;
            ++i2;
        } else {
            Int paramArgcount = nod2 & 0xFF;
            Int paramId = (nod2 >> 8) & 0xFF;
#ifdef SAFETY
            VALIDATEI(paramId <= genericHeader.arity, iErrorGenericTypesParamOutOfBou)
#endif
            Int concreteArgcount = typeEltArgcount(nod1);
            Int concreteElt = nod1 & LOWER24BITS;
            if (paramArgcount > 0) {
                // A param that is being called must correspond to a concrete type being called
                // E.g. for type = L(Int), gen = U(1) V | U(V), param U = L, not L(Int)
                if (paramArgcount != concreteArgcount) {
                    return null;
                }
                if (tStack->cont[paramId] == -1) {
                    tStack->cont[paramId] = concreteElt;
                } else if (tStack->cont[paramId] != concreteElt) {
                    return null;
                }
                ++i1;
                ++i2;
            } else {
                // A param not being called may correspond to a simple type, a type call or a
                // callable type. E.g. for a generic type G = U(1) | U(Int), concrete type G(L),
                // generic type T(1) | G(T): we will have T = L, even though the type param T is
                // not being called in generic type
                Int declaredArity = cm->types.cont[genericId + 3 + paramId];
                Int typeFromConcrete = -1;
                if (concreteArgcount > 0) {
                    typeFromConcrete = typeMergeTypeCall(i1, concreteArgcount, cm);
                    typeSkipNode(&i1, cm);
                } else {
                    if (typeEltArgcount(nod1) != declaredArity) {
                        return null;
                    }
                    typeFromConcrete = concreteElt;
                    ++i1;
                }
                if (tStack->cont[paramId] == -1) {
                    tStack->cont[paramId] = typeFromConcrete;
                } else {
                    if (tStack->cont[paramId] != typeFromConcrete) {
                        return null;
                    }
                }
                ++i2;
            }
        }
    }
#ifdef SAFETY
    VALIDATEI(i1 == sentinel1 && i2 == sentinel2, iErrorGenericTypesInconsistent)
#endif
    return tStack;
}

//}}}
//}}}
//{{{ Interpreter

//}}}
//{{{ Utils for tests & debugging

#ifdef TEST

//{{{ General utils

void printIntArray(Int count, Arr(Int) arr) {
    printf("[");
    for (Int k = 0; k < count; k++) {
        printf("%d ", arr[k]);
    }
    printf("]\n");
}

void printIntArrayOff(Int startInd, Int count, Arr(Int) arr) {
    printf("[...");
    for (Int k = 0; k < count; k++) {
        printf("%d ", arr[startInd + k]);
    }
    printf("...]\n");
}



private void printExpSt(StackInt* st) {
    printIntArray(st->len, st->cont);
}

//}}}
//{{{ Lexer testing


// Must agree in order with token types in tl.internal.h
const char* tokNames[] = {
    "Int", "Long", "Double", "Bool", "String", "~", "misc",
    "word", "Type", ":kwarg", "operator", "_acc",
    "stmt", "()", ".call", "TypeCall", "paramList",
    "...=", "=...", "alias", "assert", "breakCont",
    "iface", "import", "return",
    "{}", "^{}", "try{", "catch{", "finally{",
    "if{", "match{", "ei", "else", "impl{", "for{", "each{"
};

Int pos(Compiler* lx) {
    return lx->i - sizeof(standardText) + 1;
}


Int posInd(Int ind) {
    return ind - sizeof(standardText) + 1;
}


void printLexBtrack(Compiler* lx) {
    printf("[");
    StackBtToken* bt = lx->lexBtrack;

    for (Int k = 0; k < bt->len; k++) {
        printf("%s ", tokNames[bt->cont[k].tp]);
    }
    printf("]\n");
}


int equalityLexer(Compiler a, Compiler b) {
// Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first
// differing token otherwise
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.tokens.len < b.tokens.len ? a.tokens.len : b.tokens.len;
    int i = 0;
    for (; i < commonLength; i++) {
        Token tokA = a.tokens.cont[i];
        Token tokB = b.tokens.cont[i];
        if (tokA.tp != tokB.tp || tokA.lenBts != tokB.lenBts || tokA.startBt != tokB.startBt
            || tokA.pl1 != tokB.pl1 || tokA.pl2 != tokB.pl2) {
            printf("\n\nUNEQUAL RESULTS on token %d\n", i);
            if (tokA.tp != tokB.tp) {
                printf("Diff in tp, %s but was expected %s\n", tokNames[tokA.tp], tokNames[tokB.tp]);
            }
            if (tokA.lenBts != tokB.lenBts) {
                printf("Diff in lenBts, %d but was expected %d\n", tokA.lenBts, tokB.lenBts);
            }
            if (tokA.startBt != tokB.startBt) {
                printf("Diff in startBt, %d but was expected %d\n",
                        posInd(tokA.startBt), posInd(tokB.startBt));
            }
            if (tokA.pl1 != tokB.pl1) {
                printf("Diff in pl1, %d but was expected %d\n", tokA.pl1, tokB.pl1);
            }
            if (tokA.pl2 != tokB.pl2) {
                printf("Diff in pl2, %d but was expected %d\n", tokA.pl2, tokB.pl2);
            }
            return i;
        }
    }
    return (a.tokens.len == b.tokens.len) ? -2 : i;
}


testable void printLexer(Compiler* lx) {
    if (lx->wasError) {
        printf("Error: ");
        printString(lx->errMsg);
    }
    Int indent = 0;
    Arena* a = lx->a;
    Stackint32_t* sentinels = createStackint32_t(16, a);
    for (int i = 0; i < lx->tokens.len; i++) {
        Token tok = lx->tokens.cont[i];
        for (int m = sentinels->len - 1; m > -1 && sentinels->cont[m] == i; m--) {
            popint32_t(sentinels);
            indent--;
        }

        Int realStartBt = tok.startBt - sizeof(standardText) + 1;
        if (i < 10) {
            printf(" ");
        }
        printf("%d: ", i);
        for (int j = 0; j < indent; j++) {
            printf("  ");
        }
        if (tok.pl1 != 0 || tok.pl2 != 0) {
            printf("%s %d %d [%d; %d]\n",
                    tokNames[tok.tp], tok.pl1, tok.pl2, realStartBt, tok.lenBts);
        } else {
            printf("%s [%d; %d]\n", tokNames[tok.tp], realStartBt, tok.lenBts);
        }
        if (tok.tp >= firstSpanTokenType && tok.pl2 > 0) {
            pushint32_t(i + tok.pl2 + 1, sentinels);
            indent++;
        }
    }
}

//}}}
//{{{ Parser testing

testable void printParser(Compiler* cm, Arena* a) {
    print("p0")
    if (cm->wasError) {
        printf("Error: ");
        printString(cm->errMsg);
    }
    Int indent = 0;
    Stackint32_t* sentinels = createStackint32_t(16, a);
    StandardText stText = getStandardTextLength();
    for (int i = 0; i < cm->nodes.len; i++) {
        Node nod = cm->nodes.cont[i];
        for (int m = sentinels->len - 1; m > -1 && sentinels->cont[m] == i; m--) {
            popint32_t(sentinels);
            indent--;
        }

        if (i < 10) printf(" ");
        printf("%d: ", i);
        for (int j = 0; j < indent; j++) {
            printf("  ");
        }
        if (nod.tp == nodCall) {
            printf("Call %d [%d; %d] type = ", nod.pl1, nod.startBt, nod.lenBts);
            //printType(cm->entities.cont[nod.pl1].typeId, cm);
        } else if (nod.pl1 != 0 || nod.pl2 != 0) {
            printf("%s %d %d [%d; %d]\n", nodeNames[nod.tp], nod.pl1, nod.pl2,
                    nod.startBt - stText.len, nod.lenBts);
        } else {
            printf("%s [%d; %d]\n", nodeNames[nod.tp], nod.startBt - stText.len, nod.lenBts);
        }
        if (nod.tp >= nodScope && nod.pl2 > 0) {
            pushint32_t(i + nod.pl2 + 1, sentinels);
            indent++;
        }
    }

    print("p1")
}

//}}}
//{{{ Types testing

private void typePrintGenElt(Int v) {
    Int upper = (v >> 24) & 0xFF;
    Int lower = v & LOWER24BITS;
    if (upper == 0) {
        printf("Type %d, ", lower);
    } else if (upper == 255) {
        Int arity = lower & 0xFF;
        if (arity > 0) {
            printf("ParamCall %d arity = %d, ", (lower >> 8) & 0xFF, lower & 0xFF);
        } else {
            printf("Param %d, ", (lower >> 8) & 0xFF);
        }
    } else {
        printf("TypeCall %d arity = %d, ", lower, upper);
    }
}


void typePrint(Int typeId, Compiler* cm) {
    printf("Type [ind = %d, len = %d]\n", typeId, cm->types.cont[typeId]);
    Int sentinel = typeId + cm->types.cont[typeId] + 1;
    TypeHeader hdr = typeReadHeader(typeId, cm);
    if (hdr.sort == sorStruct) {
        printf("[Struct]");
    } else if (hdr.sort == sorSum) {
        printf("[Sum]");
    } else if (hdr.sort == sorFunction) {
        printf("[Fn]");
    } else if (hdr.sort == sorPartial) {
        printf("[Partial]");
    } else if (hdr.sort == sorConcrete) {
        printf("[Concrete]");
    }

    untt nameParamId = hdr.nameAndLen;
    Int mainNameLen = (nameParamId >> 24) & 0xFF;
    Int mainName = nameParamId & LOWER24BITS;

    if (mainNameLen == 0) {
        printf("[$anonymous]");
    } else if (hdr.sort != sorFunction) {
        printf("{");
        Int startBt = cm->stringTable->cont[mainName];
        fwrite(cm->sourceCode->cont + startBt, 1, mainNameLen, stdout);
        printf("}\n");
    }

    if (hdr.sort == sorStruct) {
        if (hdr.arity > 0) {
            printf("[Params: ");
            for (Int j = 0; j < hdr.arity; j++) {
                printf("%d ", cm->types.cont[j + typeId + 3]);
            }
            printf("]");
        }
        printf("(");
        Int fieldsStart = typeId + hdr.arity + 3;
        Int fieldsSentinel = fieldsStart + hdr.depth;
        for (Int j = fieldsStart; j < fieldsSentinel; j++) {
            printf("name %d ", cm->types.cont[j]);
        }
        fieldsStart += hdr.depth;
        fieldsSentinel += hdr.depth;
        for (Int j = fieldsStart; j < fieldsSentinel; j++) {
            typePrintGenElt(cm->types.cont[j]);
        }

        printf("\n)");
    } else if (hdr.sort == sorFunction) {
        if (hdr.arity > 0) {
            printf("[Params: ");
            for (Int j = 0; j < hdr.arity; j++) {
                printf("%d ", cm->types.cont[j + typeId + 3]);
            }
            printf("]");
        }
        printf("(");
        Int fnParamsStart = typeId + hdr.arity + 3;
        Int fnParamsSentinel = fnParamsStart + hdr.depth - 1;
        for (Int j = fnParamsStart; j < fnParamsSentinel; j++) {
            typePrintGenElt(cm->types.cont[j]);
        }
        // now for the return type
        printf("=> ");
        typePrintGenElt(cm->types.cont[fnParamsSentinel]);

        printf("\n");

    } else if (hdr.sort == sorPartial) {
        printf("(\n    ");
        for (Int j = typeId + hdr.arity + 3; j < sentinel; j++) {
            typePrintGenElt(cm->types.cont[j]);
        }
        printf("\n");
    }
    print(")");
}

void printOverloads(Int nameId, Compiler* cm) {
    Int listId = -cm->activeBindings[nameId] - 2;
    if (listId < 0) {
        print("Overloads for name %d not found", nameId)
        return;
    }
    Arr(Int) overs = cm->overloads.cont;
    Int countOverloads = overs[listId]/2;
    printf("%d overloads:\n", countOverloads);
    printf("[outer types, %d]\n", countOverloads);
    Int sentinel = listId + countOverloads + 1;
    Int j = listId + 1;
    for (; j < sentinel; j++) {
        printf("%d ", overs[j]);
    }
    printf("\n[typeId: ");
    sentinel += countOverloads;
    for (; j < sentinel; j++) {
        printf("%d ", overs[j]);
    }
    printf("]\n[ref = ");
    sentinel += countOverloads;
    for (; j < sentinel; j++) {
        printf("%d ", overs[j]);
    }
    printf("]\n\n");
}
//}}}

#endif

//}}}
//{{{ Main


#ifndef TEST
Int main(int argc, char** argv) {
    Arena* a = mkArena();
    String* sourceCode = readSourceFile("_bin/code.tl", a);
    if (sourceCode == null) {
        goto cleanup;
    }

/*
    Codegen* cg = compile(sourceCode);
    if (cg != null) {
        fwrite(cg->buffer, 1, cg->len, stdout);
    }
*/
    cleanup:
    deleteArena(a);
    return 0;
}
#endif

//}}}
