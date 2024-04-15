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
    { .arity=1, .bytes={ aExclamation, aDot, 0, 0 } },   // !.
    { .arity=2, .bytes={ aExclamation, aEqual, 0, 0 } }, // !=
    { .arity=1, .bytes={ aExclamation, 0, 0, 0 } },      // !
    { .arity=1, .bytes={ aSharp, aSharp, 0, 0 }, .overloadable=true }, // ##
    { .arity=1, .bytes={ aDollar, 0, 0, 0 } },     // $
    { .arity=2, .bytes={ aPercent, 0, 0, 0 } },    // %
    { .arity=2, .bytes={ aAmp, aAmp, aDot, 0 } },  // &&.
    { .arity=2, .bytes={ aAmp, aAmp, 0, 0 }, .assignable=true }, // &&
    { .arity=1, .bytes={ aAmp, 0, 0, 0 }, .isTypelevel=true },   // &
    { .arity=1, .bytes={ aApostrophe, 0, 0, 0 } }, // '
    { .arity=2, .bytes={ aTimes, aColon, 0, 0}, .assignable = true, .overloadable = true}, // *:
    { .arity=2, .bytes={ aTimes, 0, 0, 0 }, .assignable = true, .overloadable = true},     // *
    { .arity=2, .bytes={ aPlus, aColon, 0, 0 }, .assignable = true, .overloadable = true}, // +:
    { .arity=2, .bytes={ aPlus, 0, 0, 0 }, .assignable = true, .overloadable = true},      // +
    { .arity=2, .bytes={ aMinus, aColon, 0, 0}, .assignable = true, .overloadable = true}, // -:
    { .arity=2, .bytes={ aMinus, 0, 0, 0}, .assignable = true, .overloadable = true },     // -
    { .arity=2, .bytes={ aDivBy, aColon, 0, 0}, .assignable = true, .overloadable = true}, // /:
    { .arity=2, .bytes={ aDivBy, aPipe, 0, 0}, .isTypelevel = true}, // /|
    { .arity=2, .bytes={ aDivBy, 0, 0, 0}, .assignable = true, .overloadable = true}, // /
    { .arity=2, .bytes={ aLT, aLT, aDot, 0} },     // <<.
    { .arity=2, .bytes={ aLT, aLT,    0, 0}, .overloadable = true }, // <<
    { .arity=2, .bytes={ aLT, aEqual, 0, 0} },     // <=
    { .arity=2, .bytes={ aLT, aGT, 0, 0} },        // <>
    { .arity=2, .bytes={ aLT, 0, 0, 0 } },         // <
    { .arity=2, .bytes={ aEqual, aEqual, aEqual, 0 } }, // ===
    { .arity=2, .bytes={ aEqual, aEqual, 0, 0 } }, // ==
    { .arity=3, .bytes={ aGT, aEqual, aLT, aEqual } }, // >=<=
    { .arity=3, .bytes={ aGT, aLT, aEqual, 0 } },  // ><=
    { .arity=3, .bytes={ aGT, aEqual, aLT, 0 } },  // >=<
    { .arity=2, .bytes={ aGT, aGT, aDot, 0}, .assignable=true, .overloadable = true}, // >>.
    { .arity=3, .bytes={ aGT, aLT, 0, 0 } },       // ><
    { .arity=2, .bytes={ aGT, aEqual, 0, 0 } },    // >=
    { .arity=2, .bytes={ aGT, aGT, 0, 0 }, .overloadable = true }, // >>
    { .arity=2, .bytes={ aGT, 0, 0, 0 } },         // >
    { .arity=2, .bytes={ aQuestion, aColon, 0, 0 } }, // ?:
    { .arity=1, .bytes={ aQuestion, 0, 0, 0 }, .isTypelevel=true }, // ?
    { .arity=2, .bytes={ aAt, 0, 0, 0}, .overloadable = true}, // @
    { .arity=2, .bytes={ aCaret, aDot, 0, 0} },    // ^.
    { .arity=2, .bytes={ aCaret, 0, 0, 0}, .assignable=true, .overloadable = true}, // ^
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

testable Arena* mkArena(void) { //:mkArena
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

private size_t calculateChunkSize(size_t allocSize) { //:calculateChunkSize
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

testable void* allocateOnArena(size_t allocSize, Arena* ar) { //:allocateOnArena
// Allocate memory in the arena, malloc'ing a new chunk if needed
    if ((size_t)ar->currInd + allocSize >= ar->currChunk->size) {
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
    if (allocSize % 4 != 0)  {
        ar->currInd += (4 - (allocSize % 4));
    }
    return result;
}


testable void deleteArena(Arena* ar) { //:deleteArena
    ArenaChunk* curr = ar->firstChunk;
    while (curr != null) {
        ArenaChunk* nextToFree = curr->next;
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


#ifdef TEST
private void printStackNode(StackNode*, Arena*);
#endif

void pushBulk(Int startInd, StackNode* scr, StackSourceLoc* locs, Compiler* cm) { //:pushBulk
// Pushes the tail of scratch space (from a specified index onward) into the main node list
    const Int pushCount = scr->len - startInd;
    if (pushCount == 0)  {
        return;
    }
    if (cm->nodes.len + pushCount + 1 < cm->nodes.cap) {
        memcpy((Node*)(cm->nodes.cont) + (cm->nodes.len), scr->cont + startInd,
                pushCount*sizeof(Node));
        memcpy((SourceLoc*)(cm->sourceLocs->cont) + (cm->sourceLocs->len), locs->cont + startInd,
                pushCount*sizeof(SourceLoc));
    } else {
        Int newCap = 2*(cm->nodes.cap) + pushCount;
        Arr(Node) newContent = allocateOnArena(newCap*sizeof(Node), cm->a);
        memcpy(newContent,                  cm->nodes.cont + startInd, cm->nodes.len*sizeof(Node));
        memcpy((Node*)(newContent) + (cm->nodes.len), scr->cont + startInd, pushCount*sizeof(Node));
        cm->nodes.cap = newCap;
        cm->nodes.cont = newContent;

        Arr(SourceLoc) newLocs = allocateOnArena(newCap*sizeof(SourceLoc), cm->a);
        memcpy(newLocs, cm->sourceLocs->cont + startInd, pushCount*sizeof(SourceLoc));
        memcpy((SourceLoc*)(newLocs) + (cm->sourceLocs->len), locs->cont + startInd,
                pushCount*sizeof(SourceLoc));
        cm->sourceLocs->cap = newCap;
        cm->sourceLocs->cont = newLocs;
    }
    cm->nodes.len += pushCount;
    cm->sourceLocs->len += pushCount;
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
    testable void pushIn##fieldName(T newItem, Compiler* cm) {\
        if (cm->fieldName.len < cm->fieldName.cap) {\
            memcpy((T*)(cm->fieldName.cont) + (cm->fieldName.len), &newItem, sizeof(T));\
        } else {\
            T* newContent = allocateOnArena(2*(cm->fieldName.cap)*sizeof(T), cm->aName);\
            memcpy(newContent, cm->fieldName.cont, cm->fieldName.len*sizeof(T));\
            memcpy((T*)(newContent) + (cm->fieldName.len), &newItem, sizeof(T));\
            cm->fieldName.cap *= 2;\
            cm->fieldName.cont = newContent;\
        }\
        cm->fieldName.len++;\
    }
//}}}
//{{{ AssocList

void tPrint(Int typeId, Compiler* cm);

MultiAssocList* createMultiAssocList(Arena* a) { //:createMultiAssocList
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


private Int multiListFindFree(Int neededCap, MultiAssocList* ml) { //:multiListFindFree
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
//:multiListReallocToEnd
    ml->cont[ml->len] = listLen;
    ml->cont[ml->len + 1] = neededCap;
    memcpy(ml->cont + ml->len + 2, ml->cont + listInd + 2, listLen*4);
    ml->len += neededCap + 2;
}


private void multiListDoubleCap(MultiAssocList* ml) {
//:multiListDoubleCap
    Int newMultiCap = ml->cap*2;
    Arr(Int) newAlloc = allocateOnArena(newMultiCap*4, ml->a);
    memcpy(newAlloc, ml->cont, ml->len*4);
    ml->cap = newMultiCap;
    ml->cont = newAlloc;
}


testable Int addMultiAssocList(Int newKey, Int newVal, Int listInd, MultiAssocList* ml) {
//:addMultiAssocList Add a new key-value pair to a particular list within the MultiAssocList.
// Returns the new index for this list in case it had to be reallocated, -1 if not. Throws exception
// if key already exists
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
//:listAddMultiAssocList Adds a new list to the MultiAssocList and populates it with one key-value
// pair. Returns its index
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
//:searchMultiAssocList Search for a key in a particular list within the MultiAssocList. Returns
// the value if found, -1 otherwise
    Int len = ml->cont[listInd]/2;
    Int endInd = listInd + 2 + len;
    for (Int j = listInd + 2; j < endInd; j++) {
        if (ml->cont[j] == searchKey) {
            return ml->cont[j + len];
        }
    }
    return -1;
}


private MultiAssocList* copyMultiAssocList(MultiAssocList* ml, Arena* a) { //:copyMultiAssocList
    MultiAssocList* result = allocateOnArena(sizeof(MultiAssocList), a);
    Arr(Int) cont = allocateOnArena(4*ml->cap, a);
    memcpy(cont, ml->cont, 4*ml->cap);
    (*result) = (MultiAssocList){
        .len = ml->len, .cap = ml->cap, .freeList = ml->freeList, .cont = cont, .a = a
    };
    return result;
}

#ifdef TEST

void printRawOverload(Int listInd, Compiler* cm) { //:printRawOverload
    MultiAssocList* ml = cm->rawOverloads;
    Int len = ml->cont[listInd]/2;
    printf("[");
    for (Int j = 0; j < len; j++) {
        printf("%d: %d ", ml->cont[listInd + 2 + 2*j], ml->cont[listInd + 2*j + 3]);
    }
    print("]");
    printf("types: ");
    for (Int j = 0; j < len; j++) {
        tPrint(ml->cont[listInd + 2 + 2*j], cm);
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
DEFINE_STACK(TypeFrame)
DEFINE_STACK(SourceLoc)

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
testable String* str(const char* content, Arena* a) { //:str
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

testable bool endsWith(String* a, String* b) { //:endsWith
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


private bool equal(String* a, String* b) { //:equal
    if (a->len != b->len) {
        return false;
    }

    int cmpResult = memcmp(a->cont, b->cont, b->len);
    return cmpResult == 0;
}


private Int stringLenOfInt(Int n) { //:stringLenOfInt
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

private String* stringOfInt(Int i, Arena* a) { //:stringOfInt
    Int stringLen = stringLenOfInt(i);
    String* result = allocateOnArena(sizeof(String) + stringLen + 1, a);
    result->len = stringLen;
    sprintf((char* restrict)result->cont, "%d", i);
    return result;
}

testable void printString(String* s) { //:printString
    if (s->len == 0) return;
    fwrite(s->cont, 1, s->len, stdout);
    printf("\n");
}


testable void printStringNoLn(String* s) { //:printStringNoLn
    if (s->len == 0) return;
    fwrite(s->cont, 1, s->len, stdout);
}

private bool isLetter(Byte a) { //:isLetter
    return ((a >= aALower && a <= aZLower) || (a >= aAUpper && a <= aZUpper));
}

private bool isCapitalLetter(Byte a) { //:isCapitalLetter
    return a >= aAUpper && a <= aZUpper;
}

private bool isLowercaseLetter(Byte a) {
    return a >= aALower && a <= aZLower;
}

private bool isDigit(Byte a) {
    return a >= aDigit0 && a <= aDigit9;
}

private bool isAlphanumeric(Byte a) { //:isAlphanumeric
    return isLetter(a) || isDigit(a);
}

private bool isHexDigit(Byte a) { //:isHexDigit
    return isDigit(a) || (a >= aALower && a <= aFLower) || (a >= aAUpper && a <= aFUpper);
}

private bool isSpace(Byte a) { //:isSpace
    return a == aSpace || a == aNewline;
}


String empty = { .len = 0 };

//}}}
//{{{ Int Hashmap

testable IntMap* createIntMap(int initSize, Arena* a) { //:createIntMap
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


testable void addIntMap(int key, int value, IntMap* hm) { //:addIntMap
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


testable bool hasKeyIntMap(int key, IntMap* hm) { //:hasKeyIntMap
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


private int getIntMap(int key, int* value, IntMap* hm) { //:getIntMap
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


private int getUnsafeIntMap(int key, IntMap* hm) { //:getUnsafeIntMap
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


private bool hasKeyValueIntMap(int key, int value, IntMap* hm) { //:hasKeyValueIntMap
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


testable Int addStringDict(Byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable,
                           StringDict* hm) { //:addStringDict
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
        untt newName = ((untt)(lenBts) << 24) + (untt)startBt;
        push(newName, stringTable);

        *firstElem = (StringValue){.hash = hash, .indString = newIndString };
        *(hm->dict + hashOffset) = newBucket;
    } else {
        int lenBucket = (bu->capAndLen & 0xFFFF);
        for (int i = 0; i < lenBucket; i++) {
            StringValue strVal = bu->cont[i];
            if (strVal.hash == hash &&
                  memcmp(text + (stringTable->cont[strVal.indString] & LOWER24BITS),
                         text + startBt,
                         lenBts) == 0) {
                // key already present
                return strVal.indString;
            }
        }

        newIndString = stringTable->len;
        untt newName = ((untt)(lenBts) << 24) + (untt)startBt;
        push(newName, stringTable);
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
                          text + (stringTable->cont[stringValues[i].indString] & LOWER24BITS),
                          lenBts) == 0) {
                return stringValues[i].indString;
            }
        }
        return -1;
    }
}

//}}}
//{{{ Algorithms

testable void sortPairsDisjoint(Int startInd, Int endInd, Arr(Int) arr) { //:sortPairsDisjoint
// Performs a "twin" ASC sort for faraway (Struct-of-arrays) pairs: for every swap of keys, the
// same swap on values is also performed.
// Example of such a swap: [type1 type2 type3 entity1 entity2 entity3] ->
//                         [type3 type2 type1 entity3 entity2 entity1]
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
//:sortPairsDistant Performs a "twin" ASC sort for faraway (Struct-of-arrays) pairs: for every
// swap of keys, the same swap on values is also performed.
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


testable void sortPairs(Int startInd, Int endInd, Arr(Int) arr) { //:sortPairs
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


private void sortStackInts(StackInt* st) { //:sortStackInts
// Performs an ASC sort
    const Int len = st->len;
    if (len == 2) return;
    Arr(Int) arr = st->cont;
    for (Int i = 0; i < len; i++) {
        Int minValue = arr[i];
        Int minInd = i;
        for (Int j = i + 1; j < len; j++) {
            if (arr[j] < minValue) {
                minValue = arr[j];
                minInd = j;
            }
        }
        if (minInd != i)  {
            Int tmp = arr[i];
            arr[i] = arr[minInd];
            arr[minInd] = tmp;
        }
    }
}


testable bool verifyUniquenessPairsDisjoint(Int startInd, Int endInd, Arr(Int) arr) {
//:verifyUniquenessPairsDisjoint For disjoint (struct-of-arrays) pairs, makes sure the keys are
// unique and sorted ascending
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


private Int binarySearch(Int key, Int start, Int end, Arr(Int) arr) { //:binarySearch
    if (end <= start) {
        return -1;
    }
    Int i = start;
    Int j = end - 1;
    if (arr[start] == key) {
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


private void removeDuplicatesInStack(StackInt* list) { //:removeDuplicatesInStack
// [55 55 55 56] => [55 56]
// Precondition: the stack must be sorted
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


private void removeDuplicatesInList(InListInt* list) { //:removeDuplicatesInList
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
                                           // generic's tyrity
#define iErrorInconsistentTypeExpr      11 // Reduced type expression has != 1 elements
#define iErrorNotAFunction              12 // Expected to find a function type here

//}}}
//{{{ Syntax errors

const char errNonAscii[]                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!";
const char errPrematureEndOfInput[]        = "Premature end of input";
const char errUnrecognizedByte[]           = "Unrecognized Byte in source code!";
const char errWordChunkStart[]             = "In an identifier, each word piece must start with a letter. Tilde may come only after an identifier";
const char errWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!";
const char errWordLengthExceeded[]         = "I don't know why you want an identifier of more than 255 chars, but they aren't supported";
const char errWordTilde[]                  = "Mutable var definitions should look like `asdf~` with no spaces in between";
const char errWordFreeFloatingFieldAcc[]   = "Free-floating field accessor";
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
const char errPunctuationParamList[]       = "The param list separator `;;` must be directly in a statement";
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
const char errPrematureEndOfTokens[]       = "Premature end of tokens";
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
const char errTypeDefCountNames[]          = "Wrong count of names in a type definition!";
const char errFnNameAndParams[]            = "Function signature must look like this: `{x Type1 y Type 2 ->  ReturnType => body...}`";
const char errFnDuplicateParams[]          = "Duplicate parameter names in a function are not allowed";
const char errFnMissingBody[]              = "Function definition must contain a body which must be a Scope immediately following its parameter list!";
const char errLoopSyntaxError[]            = "A loop should look like `for x = 0; x < 101; ( loopBody ) `";
const char errLoopHeader[]                 = "A loop header should contain 1 or 2 items: the condition and, optionally, the var declarations";
const char errLoopEmptyBody[]              = "Empty loop body!";
const char errBreakContinueTooComplex[]    = "This statement is too complex! Continues and breaks may contain one thing only: the postitive number of enclosing loops to continue/break!";
const char errBreakContinueInvalidDepth[]  = "Invalid depth of break/continue! It must be a positive 32-bit integer!";
const char errDuplicateFunction[]          = "Duplicate function declaration: a function with same name and arity already exists in this scope!";
const char errExpressionError[]             = "Cannot parse expression!";
const char errExpressionCannotContain[]     = "Expressions cannot contain scopes or statements!";
const char errExpressionFunctionless[]      = "Functionless expression!";
const char errTypeDefCannotContain[]        = "Type declarations may only contain types (like Int), type params (like A), type constructors (like List) and parentheses!";
const char errTypeDefError[]                = "Cannot parse type declaration!";
const char errTypeDefParamsError[]          = "Error parsing type params. Should look like this: [T U/2]";
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
const char errTemp[]                        = "Not implemented yet";

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

private void exprCopyFromScratch(Int startNodeInd, Compiler* cm);
private Int isFunction(TypeId typeId, Compiler* cm);
private void addRawOverload(NameId nameId, TypeId typeId, EntityId entityId, Compiler* cm);
testable void typeAddHeader(TypeHeader hdr, Compiler* cm);
testable TypeHeader typeReadHeader(TypeId typeId, Compiler* cm);
testable void typeAddTypeParam(Int paramInd, Int arity, Compiler* cm);
private Int typeEncodeTag(untt sort, Int depth, Int arity, Compiler* cm);
private FirstArgTypeId getFirstParamType(TypeId funcTypeId, Compiler* cm);
private TypeId getFunctionReturnType(TypeId funcTypeId, Compiler* cm);
private bool isFunctionWithParams(TypeId typeId, Compiler* cm);
private OuterTypeId typeGetOuter(FirstArgTypeId typeId, Compiler* cm);
private Int typeGetTyrity(TypeId typeId, Compiler* cm);
testable Int typeCheckBigExpr(Int indExpr, Int sentinel, Compiler* cm);
private TypeId tDefinition(StateForTypes* st, Int sentinel, Compiler* cm);
private TypeId tGetIndexOfFnFirstParam(TypeId fnType, Compiler* cm);
#ifdef TEST
void printParser(Compiler* cm, Arena* a);
void tPrint(TypeId typeId, Compiler* cm);
private void printStackInt(StackInt* st);
void tPrintTypeFrames(StackTypeFrame* st);
#endif

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


const int operatorStartSymbols[14] = {
    aExclamation, aSharp, aDollar, aPercent, aAmp, aApostrophe, aTimes, aPlus,
    aDivBy, aLT, aGT, aQuestion, aAt, aCaret
    // Symbols an operator may start with. "-" is absent because it's handled by lexMinus,
    // "=" - by lexEqual, "|" by lexPipe, "/" by "lexDivBy"
};
const char standardText[] = "aliasassertbreakcatchcontinueeacheielsefalsefinallyfor"
                            "ifimplimportifacematchpubreturntruetry"
                            // reserved words end here
                            "IntLongDoubleBoolStrVoidFLArrayDRecEnumTuPromiselencapf1f2printprintErr"
                            "math:pimath:eTU"
#ifdef TEST
                            "foobarinner"
#endif
                            ;
// The :standardText prepended to all source code inputs and the hash table to provide a built-in
// string set. Tl's reserved words must be at the start and sorted lexicographically.
// Also they must agree with the "standardStr" in tl.internal.h

const Int standardStringLens[] = {
     5, 6, 5, 5, 8, // continue
     4, 2, 4, 5, 7, // finally
     3, 2, 4, 6, 5, // iface
     5, 3, 6, 4, 3, // try
     // reserved words end here
     3, 4, 6, 4, 3, // Str(ing)
     4, 1, 1, 5, 1, // D
     3, 4, 2, 7, 3, // len
     3, 2, 2, 5, 8, // printErr
     7, 6, 1, 1, // U
#ifdef TEST
     3, 3, 5
#endif
};

const Int standardKeywords[] = {
    tokAlias,     tokAssert,  keywBreak,  tokCatch,   keywContinue,
    tokEach,      tokElseIf,  tokElse,    keywFalse,  tokDefer,
    tokFor,       tokIf,      tokImpl,    tokImport,  tokTrait,
    tokMatch,     tokMisc,    tokReturn,  keywTrue,   tokTry
};


StandardText getStandardTextLength(void) { //:getStandardTextLength
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


private String* readSourceFile(const Arr(char) fName, Arena* a) { //:readSourceFile
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


testable String* prepareInput(const char* content, Arena* a) { //:prepareInput
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

private untt nameOfToken(Token tk) { //:nameOfToken
    return ((untt)tk.lenBts << 24) + (untt)tk.pl2;
}


testable untt nameOfStandard(Int strId) { //:nameOfStandard
// Builds a name (nameId + length) for a standardString after "strFirstNonreserved"
    Int length = standardStringLens[strId];
    Int nameId = strId + countOperators;
    return ((untt)length << 24) + (untt)(nameId);
}


private void skipSpaces(Arr(Byte) source, Compiler* lx) { //:skipSpaces
    while (lx->i < lx->stats.inpLength) {
        Byte currBt = CURR_BT;
        if (!isSpace(currBt)) {
            return;
        }
        lx->i += 1;
    }
}


_Noreturn private void throwExcInternal0(Int errInd, Int lineNumber, Compiler* cm) {
    cm->stats.wasError = true;
    printf("Internal error %d at %d\n", errInd, lineNumber);
    cm->stats.errMsg = stringOfInt(errInd, cm->a);
    printString(cm->stats.errMsg);
    longjmp(excBuf, 1);
}

#define throwExcInternal(errInd, cm) throwExcInternal0(errInd, __LINE__, cm)

_Noreturn private void throwExcLexer0(const char errMsg[], Int lineNumber, Compiler* lx) {
// Sets i to beyond input's length to communicate to callers that lexing is over
    lx->stats.wasLexerError = true;
#ifdef TRACE
    printf("Error on code line %d, i = %d: %s\n", lineNumber, IND_BT, errMsg);
#endif
    lx->stats.errMsg = str(errMsg, lx->a);
    longjmp(excBuf, 1);
}

#define throwExcLexer(msg) throwExcLexer0(msg, __LINE__, lx)

//}}}
//{{{ Lexer proper

private void checkPrematureEnd(Int requiredSymbols, Compiler* lx) { //:checkPrematureEnd
// Checks that there are at least 'requiredSymbols' symbols left in the input
    VALIDATEL(lx->i + requiredSymbols <= lx->stats.inpLength, errPrematureEndOfInput)
}


private void setSpanLengthLexer(Int tokenInd, Compiler* lx) { //:setSpanLengthLexer
// Finds the top-level punctuation opener by its index, and sets its lengths.
// Called when the matching closer is lexed. Does not pop anything from the "lexBtrack"
    lx->tokens.cont[tokenInd].lenBts = lx->i - lx->tokens.cont[tokenInd].startBt + 1;
    lx->tokens.cont[tokenInd].pl2 = lx->tokens.len - tokenInd - 1;
}


private BtToken getLexContext(Compiler* lx) { //:getLexContext
    if (!hasValues(lx->lexBtrack)) {
        return (BtToken) { .tp = tokInt, .tokenInd = -1, .spanLevel = 0 };
    }
    return peek(lx->lexBtrack);
}


private void setStmtSpanLength(Int tokenInd, Compiler* lx) { //:setStmtSpanLength
// Correctly calculates the lenBts for a single-line, statement-type span
// This is for correctly calculating lengths of statements when they are ended by parens in
// case of a gap before "}", for example:
//  ` { asdf    <- statement actually ended here
//    }`        <- but we are in this position now
    Token lastToken = lx->tokens.cont[lx->tokens.len - 1];
    Int byteAfterLastToken = lastToken.startBt + lastToken.lenBts;
    Int byteAfterLastPunct = lx->stats.lastClosingPunctInd + 1;
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
//:wrapInAStatementStarting
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel == slScope) {
            addStatementSpan(tokStmt, startBt, lx);
        }
    } else {
        addStatementSpan(tokStmt, startBt, lx);
    }
}


private void wrapInAStatement(Arr(Byte) source, Compiler* lx) { //:wrapInAStatement
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel == slScope) {
            addStatementSpan(tokStmt, lx->i, lx);
        }
    } else {
        addStatementSpan(tokStmt, lx->i, lx);
    }
}


private int64_t calcIntegerWithinLimits(Compiler* lx) { //:calcIntegerWithinLimits
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


private bool integerWithinDigits(const Byte* b, Int bLength, Compiler* lx) { //:integerWithinDigits
// Is the current numeric <= b if they are regarded as arrays of decimal digits (0 to 9)?
    if (lx->numeric.len != bLength) return (lx->numeric.len < bLength);
    for (Int j = 0; j < lx->numeric.len; j++) {
        if (lx->numeric.cont[j] < b[j]) return true;
        if (lx->numeric.cont[j] > b[j]) return false;
    }
    return true;
}


private Int calcInteger(int64_t* result, Compiler* lx) { //:calcInteger
    if (lx->numeric.len > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lx)) return -1;
    *result = calcIntegerWithinLimits(lx);
    return 0;
}


private int64_t calcHexNumber(Compiler* lx) { //:calcHexNumber
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


private void hexNumber(Arr(Byte) source, Compiler* lx) { //:hexNumber
// Lexes a hexadecimal numeric literal (integer or floating-point)
// Examples of accepted expressions: 0xCAFE'BABE, 0xdeadbeef, 0x123'45A
// Examples of NOT accepted expressions: 0xCAFE'babe, 0x'deadbeef, 0x123'
// Checks that the input fits into a signed 64-bit fixnum.
// TODO add floating-pointt literals like 0x12FA
    checkPrematureEnd(2, lx);
    lx->numeric.len = 0;
    Int j = lx->i + 2;
    while (j < lx->stats.inpLength) {
        Byte cByte = source[j];
        if (isDigit(cByte)) {
            pushInnumeric(cByte - aDigit0, lx);
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            pushInnumeric(cByte - aALower + 10, lx);
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            pushInnumeric(cByte - aAUpper + 10, lx);
        } else if (cByte == aUnderscore
                    && (j == lx->stats.inpLength - 1 || isHexDigit(source[j + 1]))) {
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
//:calcFloating Parses the floating-point numbers using just the "fast path" of David Gay's
// "strtod" function, extended to 16 digits.
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


testable int64_t longOfDoubleBits(double d) { //:longOfDoubleBits
    FloatingBits un = {.d = d};
    return un.i;
}


private double doubleOfLongBits(int64_t i) { //:doubleOfLongBits
    FloatingBits un = {.i = i};
    return un.d;
}


private void decNumber(bool isNegative, Arr(Byte) source, Compiler* lx) { //:decNumber
// Lexes a decimal numeric literal (integer or floating-point). Adds a token.
// TODO: add support for the '1.23E4' format
    Int j = (isNegative) ? (lx->i + 1) : lx->i;
    Int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
    bool metDot = false;
    bool metNonzero = false;
    Int maximumInd = (lx->i + 40 > lx->stats.inpLength) ? (lx->i + 40) : lx->stats.inpLength;
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
        } else if (cByte == aUnderscore) {
            VALIDATEL(j != (lx->stats.inpLength - 1) && isDigit(source[j + 1]),
                      errNumericEndUnderscore)
        } else if (cByte == aDot) {
            VALIDATEL(!metDot, errNumericMultipleDots)
            metDot = true;
        } else {
            break;
        }
        j++;
    }

    VALIDATEL(j >= lx->stats.inpLength || !isDigit(source[j]), errNumericWidthExceeded)

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


private void lexNumber(Arr(Byte) source, Compiler* lx) { //:lexNumber
    wrapInAStatement(source, lx);
    Byte cByte = CURR_BT;
    if (lx->i == lx->stats.inpLength - 1 && isDigit(cByte)) {
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
//:openPunctuation Adds a token which serves punctuation purposes, i.e. either a ( or  a [
// These tokens are used to define the structure, that is, nesting within the AST.
// Upon addition, they are saved to the backtracking stack to be updated with their length
// once it is known. Consumes no bytes
    push(((BtToken){ .tp = tType, .tokenInd = lx->tokens.len, .spanLevel = spanLevel}),
            lx->lexBtrack);
    pushIntokens((Token) {.tp = tType, .pl1 = (tType < firstScopeTokenType) ? 0 : spanLevel,
                          .startBt = startBt }, lx);
}


private void lexSyntaxForm(untt reservedWordType, Int startBt, Int lenBts,
                             Arr(Byte) source, Compiler* lx) { //:lexSyntaxForm
// Lexer action for a paren-type or statement-type syntax form.
// Precondition: we are looking at the character immediately after the keyword
    StackBtToken* bt = lx->lexBtrack;
    if (reservedWordType >= firstScopeTokenType) {
        skipSpaces(source, lx);
        VALIDATEL(lx->i < lx->stats.inpLength && CURR_BT == aCurlyLeft, errCoreFormTooShort);
        lx->i += 1; // CONSUME the "{"
        if (hasValues(lx->lexBtrack)) {
            BtToken top = peek(lx->lexBtrack);
            VALIDATEL(top.spanLevel == slScope && top.tp != tokStmt, errPunctuationScope)
        }
        push(((BtToken){ .tp = reservedWordType, .tokenInd = lx->tokens.len,
                    .spanLevel = slScope }), lx->lexBtrack);
        pushIntokens((Token){ .tp = reservedWordType, .pl1 = slScope,
            .startBt = startBt, .lenBts = lenBts }, lx);
    } else if (reservedWordType >= firstSpanTokenType) {
        VALIDATEL(!hasValues(bt) || peek(bt).spanLevel == slScope, errCoreNotInsideStmt)
        addStatementSpan(reservedWordType, startBt, lx);
    }
}


private bool wordChunk(Arr(Byte) source, Compiler* lx) { //:wordChunk
// Lexes a single chunk of a word, i.e. the characters between two minuses (or the whole word
// if there are no minuses). Returns True if the lexed chunk was capitalized
    bool result = false;
    checkPrematureEnd(1, lx);

    Byte currBt = CURR_BT;
    if (isCapitalLetter(currBt)) {
        result = true;
    } else VALIDATEL(isLowercaseLetter(currBt), errWordChunkStart)

    lx->i++; // CONSUME the first letter of the word
    while (lx->i < lx->stats.inpLength && isAlphanumeric(CURR_BT)) {
        lx->i++; // CONSUME alphanumeric characters
    }
    return result;
}


private void closeStatement(Compiler* lx) { //:closeStatement
// Closes the current statement. Consumes no tokens
    BtToken top = peek(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errPunctuationOnlyInMultiline)
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->lexBtrack);
    }
}


private void wordNormal(untt wordType, Int uniqueStringId, Int startBt, Int realStartBt,
                        bool wasCapitalized, Arr(Byte) source, Compiler* lx) { //:wordNormal
    Int lenBts = lx->i - realStartBt;
    Token newToken = (Token){ .tp = wordType, .pl2 = uniqueStringId,
                             .startBt = realStartBt, .lenBts = lenBts };
    if (lx->i < lx->stats.inpLength && CURR_BT == aParenLeft && wordType == tokWord
            && wasCapitalized) {
        newToken.tp = tokDataAlloc;
        newToken.pl1 = uniqueStringId;
        newToken.pl2 = 0;
        push(((BtToken){ .tp = tokDataAlloc, .tokenInd = lx->tokens.len, .spanLevel = slSubexpr }),
             lx->lexBtrack);
        ++lx->i; // CONSUME the opening "(" of the call
    } else if (wordType == tokWord) {
        wrapInAStatementStarting(startBt, source, lx);
        newToken.tp = (wasCapitalized ? tokTypeName : tokWord);
    } else if (wordType == tokFieldAcc || wordType == tokKwArg) {
        wrapInAStatementStarting(startBt, source, lx);
    }
    pushIntokens(newToken, lx);
}


private void wordReserved(untt wordType, Int wordId, Int startBt, Int realStartBt,
                          Arr(Byte) source, Compiler* lx) { //:wordReserved
    Int lenBts = lx->i - startBt;
    Int keywordTp = standardKeywords[wordId];
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
            pushIntokens((Token){.tp=tokBreakCont, .pl1=0, .pl2=0,
                          .startBt=realStartBt, .lenBts=5}, lx);
        } else if (keywordTp == keywContinue) {
            wrapInAStatementStarting(startBt, source, lx);
            pushIntokens((Token){.tp=tokBreakCont, .pl1=1, .pl2=0,
                         .startBt=realStartBt, .lenBts=5}, lx);
        }
    } else {
        lexSyntaxForm(keywordTp, realStartBt, lenBts, source, lx);
    }
}


private void wordInternal(untt wordType, Arr(Byte) source, Compiler* lx) { //:wordInternal
// Lexes a word (both reserved and identifier) according to Tl's rules.
// Precondition: we are pointing at the first letter character of the word (i.e. past the possible
// "." or ":")
// Examples of acceptable words: A:B:c:d, asdf123, ab:cd45
// Examples of unacceptable words: 1asdf23, ab:cd_45
    Int startBt = lx->i;
    bool wasCapitalized = wordChunk(source, lx);

    while (lx->i < (lx->stats.inpLength - 1)) {
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

private void lexWord(Arr(Byte) source, Compiler* lx) { //:lexWord
    wordInternal(tokWord, source, lx);
}


private void lexDot(Arr(Byte) source, Compiler* lx) { //:lexDot
// The dot is a start of a field accessor or a function call
    VALIDATEL(lx->i < lx->stats.inpLength - 1 && isLetter(NEXT_BT) && lx->tokens.len > 0,
        errUnexpectedToken);
    Token prevTok = lx->tokens.cont[lx->tokens.len - 1];
    ++lx->i; // CONSUME the dot
    VALIDATEL(prevTok.tp == tokWord && lx->i == (prevTok.startBt + prevTok.lenBts + 1),
            errWordFreeFloatingFieldAcc);
    wordInternal(tokFieldAcc, source, lx);
}


private void processAssignment(Int opType, Compiler* lx) { //:processAssignment
// Params: opType is the operator for mutations (like `*=`), -1 for other assignments.
// Handles the "=", and "+=" tokens. Changes existing stmt
// token into tokAssignLeft and opens up a new tokAssignRight span. Doesn't consume anything
    BtToken currSpan = peek(lx->lexBtrack);
    VALIDATEL(currSpan.tp == tokStmt, errOperatorAssignmentPunct);

    Int assignmentStartInd = currSpan.tokenInd;
    Token* tok = (lx->tokens.cont + assignmentStartInd);
    tok->tp = tokAssignLeft;
    if (opType == -1) {
        if (assignmentStartInd == (lx->tokens.len - 2)
            && lx->tokens.cont[lx->tokens.len - 1].pl1 == 0)  { // only one token and no ~
            Token wordTk = lx->tokens.cont[assignmentStartInd + 1];
            VALIDATEL(wordTk.tp == tokWord, errAssignment);
            tok->pl1 = wordTk.pl2; // nameId
            tok->startBt = wordTk.startBt;
            tok->lenBts = wordTk.lenBts;
            lx->tokens.len--; // delete the word token because its data now lives in tokAssignLeft
        } else if (lx->tokens.cont[assignmentStartInd + 1].tp == tokTypeName){
            tok->pl1 = assiType;
        }
    } else {
        tok->pl1 = BIG + opType;
    }
    setStmtSpanLength(assignmentStartInd, lx);
    lx->lexBtrack->cont[lx->lexBtrack->len - 1] = (BtToken){ .tp = tokAssignRight, .spanLevel = slStmt,
        .tokenInd = lx->tokens.len };
    pushIntokens((Token){ .tp = tokAssignRight, .startBt = lx->i }, lx);
}


private void lexColon(Arr(Byte) source, Compiler* lx) { //:lexColon
// Handles keyword arguments ":asdf" and param lists "{{x Int : ...}}"
    if (lx->i < lx->stats.inpLength - 1 && isLowercaseLetter(NEXT_BT)) {
        lx->i += 1; // CONSUME the ":"
        wordInternal(tokKwArg, source, lx);
    } else {
        VALIDATEL(hasValues(lx->lexBtrack), errPunctuationOnlyInMultiline)
        BtToken top = peek(lx->lexBtrack);
        VALIDATEL(top.spanLevel == slStmt, errPunctuationParamList)

        setStmtSpanLength(top.tokenInd, lx);
        lx->tokens.cont[top.tokenInd].tp = tokIntro;
        pop(lx->lexBtrack);

        lx->i += 1; // CONSUME the ":"
    }
}


private void lexSemicolon(Arr(Byte) source, Compiler* lx) { //:lexSemicolon
    VALIDATEL(hasValues(lx->lexBtrack), errPunctuationOnlyInMultiline)
    BtToken top = peek(lx->lexBtrack);
    VALIDATEL(top.spanLevel == slStmt, errPunctuationOnlyInMultiline);
    closeStatement(lx);
    lx->i += 1;  // CONSUME the ";"
}


private void lexTilde(Arr(Byte) source, Compiler* lx) { //:lexTilde
    const Int lastInd = lx->tokens.len - 1;
    VALIDATEL(lx->tokens.len > 0, errWordTilde)
    Token lastTk = lx->tokens.cont[lastInd];
    VALIDATEL(lx->tokens.cont[lastInd].tp == tokWord && (lastTk.startBt + lastTk.lenBts == lx->i),
              errWordTilde)
    lx->tokens.cont[lastInd].pl1 = 1;
    lx->tokens.cont[lastInd].lenBts = lastTk.lenBts + 1;
    lx->i += 1;  // CONSUME the ";"
}


private void lexOperator(Arr(Byte) source, Compiler* lx) { //:lexOperator
    wrapInAStatement(source, lx);

    Byte firstSymbol = CURR_BT;
    Byte secondSymbol = (lx->stats.inpLength > lx->i + 1) ? source[lx->i + 1] : 0;
    Byte thirdSymbol = (lx->stats.inpLength > lx->i + 2) ? source[lx->i + 2] : 0;
    Byte fourthSymbol = (lx->stats.inpLength > lx->i + 3) ? source[lx->i + 3] : 0;
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
    if (opDef.assignable && j < lx->stats.inpLength && source[j] == aEqual) {
        isAssignment = true;
        j++;
    }
    if (isAssignment) { // mutation operators like "*=" or "*.="
        processAssignment(opType, lx);
    } else {
        pushIntokens((Token){ .tp = tokOperator, .pl1 = opType,
                    .pl2 = 0, .startBt = lx->i, .lenBts = j - lx->i}, lx);
    }
    lx->i = j; // CONSUME the operator
}


private void lexEqual(Arr(Byte) source, Compiler* lx) { //:lexEqual
// The humble "=" can be the definition statement, the marker "=>" which ends a parameter list,
// or a comparison "=="
    checkPrematureEnd(2, lx);
    Byte nextBt = NEXT_BT;
    if (nextBt == aEqual) {
        lexOperator(source, lx); // ==
    } else if (nextBt == aGT) {
        VALIDATEL(hasValues(lx->lexBtrack), errPunctuationParamList)
        BtToken top = peek(lx->lexBtrack);
        VALIDATEL(top.spanLevel == slStmt, errPunctuationParamList)
        setStmtSpanLength(top.tokenInd, lx);
        lx->tokens.cont[top.tokenInd].tp = tokIntro;
        pop(lx->lexBtrack);
        lx->i += 2; // CONSUME the `=>`
    } else {
        processAssignment(-1, lx);
        lx->i++; // CONSUME the =
    }
}


private void lexUnderscore(Arr(Byte) source, Compiler* lx) { //:lexUnderscore
    if ((lx->i < lx->stats.inpLength - 1) && NEXT_BT == aUnderscore) {
        pushIntokens((Token){ .tp = tokMisc, .pl1 = miscUnderscore, .pl2 = 2,
                     .startBt = lx->i - 1, .lenBts = 2 }, lx);
        lx->i += 2; // CONSUME the "__"
    } else {
        pushIntokens((Token){ .tp = tokMisc, .pl1 = miscUnderscore, .pl2 = 1,
                     .startBt = lx->i - 1, .lenBts = 2 }, lx);
        ++lx->i; // CONSUME the "_"
    }
}


private void lexNewline(Arr(Byte) source, Compiler* lx) { //:lexNewline
    pushInnewlines(lx->i, lx);

    lx->i++;     // CONSUME the LF
    while (lx->i < lx->stats.inpLength) {
        if (!isSpace(CURR_BT)) {
            break;
        }
        lx->i++; // CONSUME a space or tab
    }
}


private void lexComment(Arr(Byte) source, Compiler* lx) { //:lexComment
// Tl separates between documentation comments (which live in meta info and are
// spelt as "meta(`comment`)") and comments for, well, eliding text from code;
// Elision comments are of the "//" form.
    lx->i += 2; // CONSUME the "//"

    for (;lx->i < lx->stats.inpLength - 1 && CURR_BT != aNewline; lx->i += 1) {
        // CONSUME the comment
    }
}


private void lexMinus(Arr(Byte) source, Compiler* lx) { //:lexMinus
// Handles the binary operator as well as the unary negation operator
    if (lx->i == lx->stats.inpLength - 1) {
        lexOperator(source, lx);
    } else {
        Byte nextBt = NEXT_BT;
        if (isDigit(nextBt)) {
            wrapInAStatement(source, lx);
            decNumber(true, source, lx);
            lx->numeric.len = 0;
        } else if (nextBt == aGT)  {
            pushIntokens((Token){ .tp = tokMisc, .pl1 = miscArrow, .startBt = lx->i, .lenBts = 2 }, lx);
            lx->i += 2;  // CONSUME the arrow "->"
        } else {
            lexOperator(source, lx);
        }
    }
}


private void lexDivBy(Arr(Byte) source, Compiler* lx) { //:lexDivBy
// Handles the binary operator as well as the comments
    if (lx->i + 1 < lx->stats.inpLength && NEXT_BT == aDivBy) {
        lexComment(source, lx);
    } else {
        lexOperator(source, lx);
    }
}


private void lexParenLeft(Arr(Byte) source, Compiler* lx) { //:lexParenLeft
    Int j = lx->i + 1;
    VALIDATEL(j < lx->stats.inpLength, errPunctuationUnmatched)
    wrapInAStatement(source, lx);
    openPunctuation(tokParens, slSubexpr, lx->i, lx);
    ++lx->i; // CONSUME the left parenthesis
}


private void lexParenRight(Arr(Byte) source, Compiler* lx) {
    Int startInd = lx->i;

    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);
    VALIDATEL(top.spanLevel == slSubexpr, errPunctuationUnmatched)

    setSpanLengthLexer(top.tokenInd, lx);
    lx->i++; // CONSUME the closing ")"
    lx->stats.lastClosingPunctInd = startInd;
}


private void lexCurlyLeft(Arr(Byte) source, Compiler* lx) { //:lexCurlyLeft
// Handles scopes "{}" and functions "{{...}}"
    Int j = lx->i + 1;
    VALIDATEL(j < lx->stats.inpLength, errPunctuationUnmatched)
    if (source[j] == aCurlyLeft)  {
        openPunctuation(tokFn, slScope, lx->i, lx);
        lx->i += 2; // CONSUME the `{{`
    } else {
        closeStatement(lx);
        openPunctuation(tokScope, slScope, lx->i, lx);
        lx->i += 1; // CONSUME the left bracket
    }
}


private void lexCurlyRight(Arr(Byte) source, Compiler* lx) {
// A closing curly brace may close the following configurations of lexer backtrack:
// 1. [scope stmt] - if it's just a scope nested within another scope or a function
// 2. [coreForm stmt] - eg. if it's closing the function body
// 3. [coreForm scope stmt] - eg. if it's closing the {} part of an "if"
    const Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);

    // since a closing curly is closing something with statements inside it, like a lex scope
    // or a function, we need to close that statement before closing its parent
    if (top.spanLevel == slStmt) {
        VALIDATEL(hasValues(bt) && bt->cont[bt->len - 1].spanLevel == slScope,
                  errPunctuationUnmatched);
        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }

    if (lx->i + 1 < lx->stats.inpLength && NEXT_BT == aCurlyRight) { // function end "}}"
        VALIDATEL(top.tp == tokFn, errPunctuationUnmatched);
        lx->i += 1; // CONSUME the first "}"
        setSpanLengthLexer(top.tokenInd, lx);
        lx->i += 1; // CONSUME the second "}"
    } else { // scope end
        VALIDATEL(top.spanLevel == slScope, errPunctuationUnmatched);
        setSpanLengthLexer(top.tokenInd, lx);
        lx->i += 1; // CONSUME the closing "}"
    }
    if (hasValues(bt)) { lx->stats.lastClosingPunctInd = startInd; }
}


private void lexBracketLeft(Arr(Byte) source, Compiler* lx) { //:lexBracketLeft
    wrapInAStatement(source, lx);
    push(((BtToken){ .tp = tokTypeCall, .tokenInd = lx->tokens.len, .spanLevel = slSubexpr }),
            lx->lexBtrack);
    pushIntokens((Token){ .tp = tokTypeCall, .pl1 = slSubexpr, .startBt = lx->i, .lenBts = 0 }, lx);
    lx->i += 1; // CONSUME the `[`
}


private void lexBracketRight(Arr(Byte) source, Compiler* lx) { //:lexBracketRight
    Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);

    // since a closing bracket is closing a function, it may have statements inside.
    // So we need to close that statement before closing its parent
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
        top = pop(bt);
    }
    setSpanLengthLexer(top.tokenInd, lx);

    if (hasValues(bt)) { lx->stats.lastClosingPunctInd = startInd; }
    lx->i += 1; // CONSUME the closing "]"
}


private void lexPipe(Arr(Byte) source, Compiler* lx) { //:lexPipe
// Closes the current statement and changes its type to tokIntro
    Int j = lx->i + 1;
    if (j < lx->stats.inpLength && NEXT_BT == aPipe) {
        lexOperator(source, lx);
    } else {
        BtToken top = peek(lx->lexBtrack);
        VALIDATEL(top.spanLevel == slStmt, errPunctuationParamList)
        setStmtSpanLength(top.tokenInd, lx);
        lx->tokens.cont[top.tokenInd].tp = tokIntro;
        pop(lx->lexBtrack);
        lx->i++; // CONSUME the `|`
    }
}


private void lexSpace(Arr(Byte) source, Compiler* lx) { //:lexSpace
    lx->i++; // CONSUME the space
    while (lx->i < lx->stats.inpLength && isSpace(CURR_BT)) {
        lx->i++; // CONSUME a space
    }
}


private void lexStringLiteral(Arr(Byte) source, Compiler* lx) { //:lexStringLiteral
    wrapInAStatement(source, lx);
    Int j = lx->i + 1;
    for (; j < lx->stats.inpLength && source[j] != aBacktick; j++);
    VALIDATEL(j != lx->stats.inpLength, errPrematureEndOfInput)
    pushIntokens((Token){.tp=tokString, .startBt=(lx->i), .lenBts=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


private void lexUnexpectedSymbol(Arr(Byte) source, Compiler* lx) { //:lexUnexpectedSymbol
    throwExcLexer(errUnrecognizedByte);
}

private void lexNonAsciiError(Arr(Byte) source, Compiler* lx) { //:lexNonAsciiError
    throwExcLexer(errNonAscii);
}


private LexerFunc (*tabulateDispatch(Arena* a))[256] { //:tabulateDispatch
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
    p[aUnderscore] = &lexUnderscore;

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
    p[aPipe] = &lexPipe;
    p[aDivBy] = &lexDivBy; // to handle the comments "//"
    p[aColon] = &lexColon; // to handle keyword args ":a"
    p[aSemicolon] = &lexSemicolon; // the statement terminator
    p[aTilde] = &lexTilde;

    p[aSpace] = &lexSpace;
    p[aNewline] = &lexNewline; // to make the newline a statement terminator sometimes
    p[aBacktick] = &lexStringLiteral;
    return result;
}


private void setOperatorsLengths() { //:setOperatorsLengths
// The set of operators in the language. Must agree in order with tl.internal.h
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
#define P_CT Arr(Token) toks, Compiler* cm // parser context signature fragment
#define P_C toks, cm // parser context args fragment

private Int exprUpTo(Int sentinelToken, Int startBt, Int lenBts, P_CT);
private void subexClose(StateForExprs* s, Compiler* cm);
private void addBinding(int nameId, int bindingId, Compiler* cm);
private void maybeCloseSpans(Compiler* cm);
private void popScopeFrame(Compiler* cm);
private EntityId importActivateEntity(Entity ent, Compiler* cm);
private void createBuiltins(Compiler* cm);
testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a);
private void exprLinearize(Int sentinel, P_CT);
private TypeId exprHeadless(Int sentinel, Int startBt, Int lenBts, P_CT);
private void pExpr(Token tk, P_CT);
private Int pExprWorker(Token tk, P_CT);


_Noreturn private void throwExcParser0(const char errMsg[], Int lineNumber, Compiler* cm) {
    cm->stats.wasError = true;
#ifdef TRACE
    printf("Error on i = %d line %d\n", cm->i, lineNumber);
#endif
    cm->stats.errMsg = str(errMsg, cm->a);
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


private EntityId createEntity(untt name, Byte class, Compiler* cm) { //:createEntity
// Validates a new binding (that it is unique), creates an entity for it,
// and adds it to the current scope
    Int nameId = name & LOWER24BITS;
    Int mbBinding = cm->activeBindings[nameId];
    VALIDATEP(mbBinding < 0, errAssignmentShadowing)
    // if it's a binding, it should be -1, and if overload, < -1

    Int newEntityId = cm->entities.len;
    pushInentities(((Entity){ .name = name, .class = class }), cm);

    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (cm->scopeStack->len > 0) {
            addBinding(nameId, newEntityId, cm); // adds it to the ScopeStack
        }
        cm->activeBindings[nameId] = newEntityId; // makes it active
    }
    return newEntityId;
}


private Int createEntityWithType(untt name, TypeId typeId, Byte class, Compiler* cm) {
//:createEntityWithType
    EntityId newEntityId = createEntity(name, class, cm);
    cm->entities.cont[newEntityId].typeId = typeId;
    return newEntityId;
}

private Int calcSentinel(Token tok, Int tokInd) { //:calcSentinel
// Calculates the sentinel token for a token at a specific index
    return (tok.tp >= firstSpanTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}



void addNode(Node node, Int startBt, Int lenBts, Compiler* cm) { //:addNode
    pushInnodes(node, cm);
    push(((SourceLoc) { .startBt = startBt, .lenBts = lenBts }), cm->sourceLocs);
}

//}}}
//{{{ Forward decls

testable void pushLexScope(ScopeStack* scopeStack);
testable Int pTypeDef(P_CT);
private void parseFnDef(Token tok, P_CT);
testable bool findOverload(FirstArgTypeId typeId, Int start, EntityId* entityId, Compiler* cm);

#ifdef TEST
void printIntArrayOff(Int startInd, Int count, Arr(Int) arr);
#endif

//}}}

private void addParsedScope(Int sentinelToken, Int startBt, Int lenBts, Compiler* cm) {
//:addParsedScope Performs coordinated insertions to start a scope within the parser
    push(((ParseFrame){
            .tp = nodScope, .startNodeInd = cm->nodes.len, .sentinelToken = sentinelToken }),
        cm->backtrack);
    addNode((Node){.tp = nodScope}, startBt, lenBts, cm);
    pushLexScope(cm->scopeStack);
}

private void parseMisc(Token tok, P_CT) {
}


private void pScope(Token tok, P_CT) { //:pScope
    addParsedScope(cm->i + tok.pl2, tok.startBt, tok.lenBts, cm);
}

private void parseTry(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void ifLeftSide(Token tok, P_CT) { //:ifLeftSide
// Precondition: we are 1 past the "stmt" token, which is the first parameter
    Int leftSentinel = calcSentinel(tok, cm->i - 1);
    VALIDATEP(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errIfLeft)

    VALIDATEP(leftSentinel + 1 < cm->stats.inpLength, errPrematureEndOfTokens)
    VALIDATEP(toks[leftSentinel].tp == tokMisc && toks[leftSentinel].pl1 == miscArrow,
            errIfMalformed)
    Int typeLeft = pExprWorker(tok, P_C);
    VALIDATEP(typeLeft == tokBool, errTypeMustBeBool)
}


private void pIf(Token tok, P_CT) { //:pIf
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = cm->nodes.len,
            .sentinelToken = cm->i + tok.pl2 };
    push(newParseFrame, cm->backtrack);
    addNode((Node){.tp = nodIf, .pl1 = tok.pl1 }, tok.startBt, tok.lenBts, cm);

    Token stmtTok = toks[cm->i];
    ++cm->i; // CONSUME the stmt token
    ifLeftSide(stmtTok, P_C);
}


private void assignmentComplexLeftSide(Int start, Int sentinel, P_CT) { //:assignmentComplexLeftSide
// A left side with more than one token must consist of a known var with a series of accessors.
// It gets transformed like this:
// arr@i@(j*2)@(k + 3) ==> arr i getElemPtr j 2 *(2) getElemPtr k 3 +(2) getElemPtr
    Token firstTk = toks[start];
    VALIDATEP(firstTk.tp == tokWord, errAssignmentLeftSide)
    EntityId leftEntityId = getActiveVar(firstTk.pl2, cm);
    Int j = start + 1;

    StackInt* sc = cm->stateForExprs->exp;
    clearint32_t(sc);

    while (j < sentinel) {
        Token accessorTk = toks[j];
        VALIDATEP(accessorTk.tp == tokFieldAcc, errAssignmentLeftSide)
        j++;
        push(j, sc);
    }

    // Add the "@ @ @" part using the stack of accessor operators
    j = sc->len - 1;
    while (j > -1) {
        Token accessorTk = toks[sc->cont[j]];
        addNode((Node){ .tp = nodFieldAcc, .pl1 = accessorTk.pl1},
                            accessorTk.startBt, accessorTk.lenBts, cm);
    }

    // The collection name part
    addNode((Node){ .tp = nodId, .pl1 = leftEntityId, .pl2 = firstTk.pl2},
                        firstTk.startBt, firstTk.lenBts, cm);
    // The accessor expression parts
    while (j < sentinel) {
        Token accessorTk = toks[j];
        if (accessorTk.tp == tokFieldAcc) {
            j++;
        } else { // tkAccArray
            Token accessTk = toks[j + 1];
            if (accessTk.tp == tokParens) {
                cm->i = j + 2; // CONSUME up to the subexpression start
                Int subexSentinel = cm->i + accessTk.pl2 + 1;
                exprHeadless(subexSentinel, accessTk.startBt, accessTk.lenBts, P_C);
                j = cm->i;
            } else if (accessTk.tp == tokInt || accessTk.tp == tokString) {
                addNode(((Node){ .tp = accessTk.tp, .pl1 = accessTk.pl1, .pl2 = accessTk.pl2}),
                        0, 0, cm);
                j++;
            } else { // tokWord
                Int accessEntityId = getActiveVar(accessTk.pl2, cm);
                addNode((Node){ .tp = nodId, .pl1 = accessEntityId, .pl2 = accessTk.pl2},
                                    accessTk.startBt, accessTk.lenBts, cm);
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
        pTypeDef(P_C);
        return;
    } else if (countLeftSide > 1) {
        complexLeft = true;
        VALIDATEP(tok.pl1 == assiDefinition || tok.pl1 >= BIG, errAssignmentLeftSide)
        assignmentComplexLeftSide(cm->i, cm->i + countLeftSide, P_C);
    } else if (tok.pl2 == 0) { // a new var being defined
        untt newName = ((untt)tok.lenBts << 24) + (untt)tok.pl1; // nameId + lenBts
        mbNewBinding = createEntity(newName, tok.pl1 == 1 ? classMutable : classImmutable, cm);
        addNode((Node){ .tp = nodAssignLeft, .pl1 = mbNewBinding, .pl2 = 0},
                tok.startBt, tok.lenBts, cm);
    } else if (isMutation) {
        addNode((Node){ .tp = nodAssignLeft, .pl1 = assiDefinition, .pl2 = 1},
                tok.startBt, tok.lenBts, cm);
        mbOldBinding = toks[cm->i];
        mbOldBinding.pl1 = getActiveVar(mbOldBinding.pl2, cm);
        addNode((Node){ .tp = nodBinding, .pl1 = mbOldBinding.pl1, .pl2 = 0},
                mbOldBinding.startBt, mbOldBinding.lenBts, cm);
    }
    cm->i += countLeftSide + 1; // CONSUME the left side of an assignment
    Int sentinel = cm->i + rightTk.pl2;
    push(((ParseFrame){.tp = nodExpr, .startNodeInd = cm->nodes.len,
                       .sentinelToken = cm->i + rightTk.pl2 }), cm->backtrack);
    addNode((Node){.tp = nodExpr}, rightTk.startBt, rightTk.lenBts, cm);

    //Token firstInRightTk = toks[cm->i]; // first token inside the tokAssignmentRight
    //Int startBt = firstInRightTk.startBt;
    //Int lenBts = rightTk.startBt + rightTk.lenBts - startBt;
    if (isMutation) {
        StateForExprs* stEx = cm->stateForExprs;

        Int startNodeInd = cm->nodes.len;
        addNode((Node){ .tp = nodId, .pl1 = mbOldBinding.pl1, .pl2 = 0},
                mbOldBinding.startBt, mbOldBinding.lenBts, cm);

        exprLinearize(sentinel, P_C);

        TypeId leftSideType = cm->entities.cont[mbOldBinding.pl1].typeId;
        Int operatorEntity = -1;
        Int opType = tok.pl1 - BIG;
        Int ovInd = -cm->activeBindings[opType] - 2;
        bool foundOv = findOverload(leftSideType, ovInd, &operatorEntity, cm);
        VALIDATEP(foundOv, errTypeNoMatchingOverload)
        push(((Node){ .tp = nodCall, .pl1 = operatorEntity, .pl2 = 2 }), stEx->calls);
        push(((SourceLoc){ .startBt = rightTk.startBt, .lenBts = TL_OPERATORS[opType].lenBts}),
                    stEx->locsCalls);
        TypeId rightType = typeCheckBigExpr(startNodeInd, cm->nodes.len, cm);
        VALIDATEP(rightType == leftSideType, errTypeMismatch)
        subexClose(stEx, cm);
    } else if (rightTk.tp == tokFn) {
        parseFnDef(rightTk, P_C);
    } else {
        Int startNodeInd = cm->nodes.len - 1; // index of the nodExpr for the right side

        exprLinearize(sentinel, P_C);
        exprCopyFromScratch(startNodeInd, cm);
        Int rightType = typeCheckBigExpr(startNodeInd, cm->nodes.len, cm);
        VALIDATEP(rightType != -2, errAssignment)

        if (tok.pl1 == 0 && rightType > -1) {
            cm->entities.cont[mbNewBinding].typeId = rightType;
        }
    }
}


private void pFor(Token forTk, P_CT) { //:pFor
// For loops. Look like "for x = 0; x < 100; x++ { ... }"
    ++cm->stats.loopCounter;
    Int sentinel = cm->i + forTk.pl2;

    Int condInd = cm->i;
    Token condTk = toks[condInd];
    VALIDATEP(condTk.tp == tokStmt, errLoopSyntaxError)

    Int condSent = calcSentinel(condTk, condInd);
    Int startBtScope = toks[condSent].startBt;

    push(((ParseFrame){ .tp = nodFor, .startNodeInd = cm->nodes.len, .sentinelToken = sentinel,
                        .typeId = cm->stats.loopCounter }), cm->backtrack);
    addNode((Node){.tp = nodFor},  forTk.startBt, forTk.lenBts, cm);

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
    addNode((Node){.tp = nodForCond, .pl1 = slStmt, .pl2 = condSent - condInd },
                       condTk.startBt, condTk.lenBts, cm);
    cm->i = condInd + 1; // + 1 because the expression parser needs to be 1 past the expr/token

    Int condTypeId = pExprWorker(toks[condInd], P_C);
    VALIDATEP(condTypeId == tokBool, errTypeMustBeBool)

    cm->i = indBody; // CONSUME the for token, its condition and variable inits (if any)
}


private void setSpanLengthParser(Int nodeInd, Compiler* cm) { //:setSpanLengthParser
// Finds the top-level punctuation opener by its index, and sets its node length.
// Called when the parsing of a span is finished
    cm->nodes.cont[nodeInd].pl2 = cm->nodes.len - nodeInd - 1;
}


private void parseErrorBareAtom(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private ParseFrame popFrame(Compiler* cm) { //:popFrame
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
        addNode((Node){.tp = nodId, .pl1 = varId, .pl2 = tk.pl2},
                 tk.startBt, tk.lenBts, cm);
    } else if (tk.tp == tokOperator) {
        Int operBindingId = tk.pl1;
        OpDef operDefinition = TL_OPERATORS[operBindingId];
        VALIDATEP(operDefinition.arity == 1, errOperatorWrongArity)
        addNode((Node){ .tp = nodId, .pl1 = operBindingId}, tk.startBt, tk.lenBts, cm);
        // TODO add the type when we support first-class functions
    } else if (tk.tp <= topVerbatimType) {
        addNode((Node){.tp = tk.tp, .pl1 = tk.pl1, .pl2 = tk.pl2},
                           tk.startBt, tk.lenBts, cm);
        typeId = tk.tp;
    } else {
        throwExcParser(errUnexpectedToken);
    }
    return typeId;
}


private void exprSubex(Int sentinelToken, Int* arity, P_CT) { //:exprSubex
// Parses a subexpression within an expression.
// Precondition: the current token must be 1 past the opening paren / statement token
// Examples: `foo(5 a)`
//           `5 + ##a`

// Consumes 1 or 2 tokens
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

        addNode((Node){.tp = nodCall, .pl1 = mbBnd, .pl2 = *arity},
                firstTk.startBt, firstTk.lenBts, cm);
        *arity = 0;
        cm->i++; // CONSUME the function or operator call token
    }
}


private void subexDataAllocation(ExprFrame frame, StateForExprs* stEx, Compiler* cm) {
//:subexDataAllocation Creates an assignment in main. Then walks over the data allocator nodes
// and counts elements that are subexpressions. Then copies the nodes from scratch to main,
// careful to wrap subexpressions in a nodExpr. Finally, replaces the copied nodes in scr with
// an id linked to the new entity
    StackNode* scr = stEx->scr;  // ((ind in scr) (count of nodes in subexpr))
    Node rawNd = pop(stEx->calls);
    SourceLoc rawLoc = pop(stEx->locsCalls);

    EntityId newEntityId = cm->entities.len;
    pushInentities(((Entity) {.class = classImmutable, .typeId = rawNd.pl1 }), cm);

    Int countElements = 0;
    Int countNodes = scr->len - frame.startNode;
    for (Int j = frame.startNode; j < scr->len; ++j)  {
        Node nd = scr->cont[j];
        ++countElements;

        if (nd.tp == nodExpr) {
            j += nd.pl2;
        }
    }

    addNode((Node){.tp = nodAssignLeft, .pl1 = newEntityId, .pl2 = 0},
            rawLoc.startBt, rawLoc.lenBts, cm);
    addNode((Node){.tp = nodDataAlloc, .pl1 = rawNd.pl1, .pl2 = countNodes, .pl3 = countElements },
            rawLoc.startBt, rawLoc.lenBts, cm);

    pushBulk(frame.startNode, scr, stEx->locsScr, cm);

    scr->len = frame.startNode + 1;
    stEx->locsScr->len = frame.startNode + 1;
    scr->cont[frame.startNode] = (Node){ .tp = nodId, .pl1 = newEntityId, .pl2 = -1 };
}


private void subexClose(StateForExprs* stEx, Compiler* cm) { //:subexClose
// Flushes the finished subexpr frames from the top of the funcall stack. Handles data allocations
    StackNode* scr = stEx->scr;
    while (stEx->frames->len > 0 && cm->i == peek(stEx->frames).sentinel) {
        ExprFrame frame = pop(stEx->frames);
        if (frame.tp == exfrCall)  {
            Node call = pop(stEx->calls);
            call.pl2 = frame.argCount;
            if (frame.startNode > -1 && scr->cont[frame.startNode].tp == nodExpr) {
                // a prefix call inside a data allocator - need to set the nodExpr length
                scr->cont[frame.startNode].pl2 = scr->len - frame.startNode - 1;
            }
            push(call, scr);
            push(pop(stEx->locsCalls), stEx->locsScr);
        } else {
            if (frame.tp == exfrDataAlloc)  {
                subexDataAllocation(frame, stEx, cm);
            } else if (frame.tp == exfrParen && scr->cont[frame.startNode].tp == nodExpr) {
                // a parens inside a data allocator - need to set the nodExpr length
                scr->cont[frame.startNode].pl2 = scr->len - frame.startNode - 1;
            }
        }
    }
}


private void exprCopyFromScratch(Int startNodeInd, Compiler* cm) { //:exprCopyFromScratch
    StateForExprs* stEx = cm->stateForExprs;
    StackNode* scr = stEx->scr;
    StackSourceLoc* locs = stEx->locsScr;
    if (stEx->metAnAllocation)  {
        cm->nodes.cont[startNodeInd].pl1 = 1;
    }

    if (cm->nodes.len + scr->len + 1 < cm->nodes.cap) {
        memcpy((Node*)(cm->nodes.cont) + (cm->nodes.len), scr->cont, scr->len*sizeof(Node));
        memcpy((SourceLoc*)(cm->sourceLocs->cont) + (cm->sourceLocs->len), locs->cont,
                locs->len*sizeof(SourceLoc));

    } else {
        Int newCap = 2*(cm->nodes.cap) + scr->len;
        Arr(Node) newContent = allocateOnArena(newCap*sizeof(Node), cm->a);
        memcpy(newContent, cm->nodes.cont, cm->nodes.len*sizeof(Node));
        memcpy((Node*)(newContent) + (cm->nodes.len), scr->cont, scr->len*sizeof(Node));
        cm->nodes.cap = newCap;
        cm->nodes.cont = newContent;

        Arr(SourceLoc) newLocs = allocateOnArena(newCap*sizeof(SourceLoc), cm->a);
        memcpy(newLocs, cm->sourceLocs->cont, cm->sourceLocs->len*sizeof(SourceLoc));
        memcpy((SourceLoc*)(newLocs) + (cm->sourceLocs->len), locs->cont,
                locs->len*sizeof(SourceLoc));
        cm->sourceLocs->cap = newCap;
        cm->sourceLocs->cont = newLocs;
    }
    cm->nodes.len += scr->len;
    cm->sourceLocs->len += scr->len;
}


private void exprParensLinearize(Token cTk, Int sentinelToken, ExprFrame* parent, P_CT) {
//:exprParensLinearize Parens inside expressions
    StateForExprs* stEx = cm->stateForExprs;
    StackNode* scr = stEx->scr;
    StackNode* calls = cm->stateForExprs->calls;
    StackSourceLoc* locsScr = cm->stateForExprs->locsScr;
    StackSourceLoc* locsCalls = cm->stateForExprs->locsCalls;
    StackExprFrame* frames = cm->stateForExprs->frames;
    if (parent->tp == exfrCall || parent->tp == exfrDataAlloc) {
        ++parent->argCount;
    }
    Int parensSentinel = calcSentinel(cTk, cm->i);

    if (parensSentinel == cm->i + 1) { // parens like "(foo)" or "(+)"
        Token oneAndOnlyTk = toks[cm->i + 1];
        if (oneAndOnlyTk.tp == tokWord) {
            EntityId funcVarId = getActiveVar(cTk.pl2, cm);
            addNode(((Node) { .tp = nodCall, .pl1 = funcVarId, .pl2 = 0}),
                    cTk.startBt, cTk.lenBts, cm);
        } else if (oneAndOnlyTk.tp == tokOperator) {
            addNode(((Node) { .tp = nodId, .pl1 = -1, .pl2 = oneAndOnlyTk.pl1}),
                    oneAndOnlyTk.startBt, oneAndOnlyTk.lenBts, cm);
        } else {
            throwExcParser(errUnexpectedToken);
        }
        cm->i += 1; // CONSUME the "(" so the end of iteration consumes the contents
    } else {
        push(((ExprFrame){
                .tp = exfrParen, .startNode = scr->len, .sentinel = parensSentinel }), frames);

        if (parent->tp == exfrDataAlloc) { // inside a data allocator, subexpressions need to
                                           // be marked with nodExpr for t-checking & codegen
            push(((Node){ .tp = nodExpr, .pl1 = 0 }), scr);
            push(((SourceLoc){ .startBt = cTk.startBt, .lenBts = cTk.lenBts }), locsScr);
        }

        if (cm->i + 1 < sentinelToken)  {
            Token firstTk = toks[cm->i + 1];
            if (firstTk.tp == tokWord) { // words in initial position are calls
                push(
                    ((ExprFrame) {.tp = exfrCall, .sentinel = parensSentinel, .argCount = 0 }),
                    frames);
                push(((Node) { .tp = nodCall }), calls);
                push(((SourceLoc) { .startBt = firstTk.startBt, .lenBts = firstTk.lenBts }),
                        locsCalls);
                ++cm->i; // CONSUME the parens token
            }
        }
    }
}


private void exprLinearize(Int sentinelToken, P_CT) { //:exprLinearize
// The core code of the general, long expression parse. "startNodeInd" is the index of the
// opening nodExpr, "sentinelToken" is the index where to stop reading tokens.
// Produces a linear sequence of operands and operators with arg counts in Reverse Polish Notation
    StateForExprs* stEx = cm->stateForExprs;
    stEx->metAnAllocation = false;
    StackNode* scr = stEx->scr;
    StackNode* calls = cm->stateForExprs->calls;
    StackSourceLoc* locsScr = cm->stateForExprs->locsScr;
    StackSourceLoc* locsCalls = cm->stateForExprs->locsCalls;
    StackExprFrame* frames = cm->stateForExprs->frames;
    frames->len = 0;
    scr->len = 0;
    locsScr->len = 0;
    locsCalls->len = 0;

    push(((ExprFrame){ .tp = exfrParen, .sentinel = sentinelToken}), frames);
    while (cm->i < sentinelToken) {
        subexClose(stEx, cm);
        Token cTk = toks[cm->i];
        untt tokType = cTk.tp;

        ExprFrame* parent = frames->cont + (frames->len - 1);
        if (tokType <= topVerbatimTokenVariant || tokType == tokWord) {
            if (parent->tp == exfrCall || parent->tp == exfrDataAlloc) {
                ++parent->argCount;
            }
            if (tokType == tokWord)  {
                EntityId varId = getActiveVar(cTk.pl2, cm);
                push(((Node){ .tp = nodId, .pl1 = varId, .pl2 = cTk.pl2 }), scr);
            } else {
                push(((Node){ .tp = cTk.tp, .pl1 = cTk.pl1, .pl2 = cTk.pl2 }), scr);
            }
            push(((SourceLoc){ .startBt = cTk.startBt, .lenBts = cTk.lenBts }), locsScr);
        } else if (tokType == tokParens) {
            exprParensLinearize(cTk, sentinelToken, parent, P_C);
        } else if (tokType == tokOperator) {
            if (TL_OPERATORS[cTk.pl2].arity == 1)  {
                push(((Node) { .tp = nodCall, .pl1 = cTk.pl1, .pl2 = 1, }), scr);
                push(((SourceLoc) { .startBt = cTk.startBt, .lenBts = cTk.lenBts }), locsScr);
            } else {
                if (parent->tp == exfrCall) {
                    ExprFrame callFrame = pop(frames);
                    Node call = pop(calls);
                    call.pl2 = callFrame.argCount;
                    push(call, scr);
                    parent = frames->cont + (frames->len - 1);
                }
                push(((ExprFrame) { .tp = exfrCall, .startNode = -1, .sentinel = parent->sentinel,
                                    .argCount = 1 }), frames); // argCount = 1 because infix
                push(((Node) { .tp = nodCall, .pl1 = cTk.pl1 }), calls);
                push(((SourceLoc) { .startBt = cTk.startBt, .lenBts = cTk.lenBts }), locsCalls);
            }
        } else if (tokType == tokTypeCall) { // not really a type call, but a data alloc "L(1 2 3)"
            stEx->metAnAllocation = true;
            // push to a frame
            if (parent->tp == exfrCall || parent->tp == exfrDataAlloc)  {
                ++parent->argCount;
            }
            push(
                ((ExprFrame) { .tp = exfrDataAlloc, .startNode = scr->len,
                               .sentinel = calcSentinel(cTk, cm->i)
                               }), frames);
            push(((Node) { .tp = nodDataAlloc, .pl1 = cm->activeBindings[cTk.pl1] }), calls);
            push(((SourceLoc) { .startBt = cTk.startBt, .lenBts = cTk.lenBts }), locsCalls);
        } else {
            throwExcParser(errExpressionCannotContain);
        }
        cm->i++; // CONSUME any token
    }
    subexClose(stEx, cm);
}


private TypeId exprUpTo(Int sentinelToken, Int startBt, Int lenBts, P_CT) { //:exprUpTo
// The main "big" expression parser. Parses an expression whether there is a token or not.
// Starts from cm->i and goes up to the sentinel token. Returns the expression's type
// Precondition: we are looking 1 past the tokExpr or tokParens
    Int startNodeInd = cm->nodes.len;
    push(((ParseFrame){.tp = nodExpr, .startNodeInd = startNodeInd,
                .sentinelToken = sentinelToken }), cm->backtrack);
    addNode((Node){ .tp = nodExpr}, startBt, lenBts, cm);

    exprLinearize(sentinelToken, P_C);
    exprCopyFromScratch(startNodeInd, cm);
    Int exprType = typeCheckBigExpr(startNodeInd, cm->nodes.len, cm);
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
        Token currTok = toks[cm->i];
        cm->i++;
        ((*cm->langDef->parserTable)[currTok.tp])(currTok, P_C);

        maybeCloseSpans(cm);
    }
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
    addNode((Node){.tp = nodBreakCont, .pl1 = loopId}, tok.startBt, tok.lenBts, cm);
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
    addNode((Node){.tp = nodReturn}, tok.startBt, tok.lenBts, cm);

    Token rTk = toks[cm->i];
    Int typeId = exprHeadless(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, P_C);
    VALIDATEP(typeId > -1, errReturn)
    if (typeId > -1) {
        typecheckFnReturn(typeId, cm);
    }
}


private EntityId importActivateEntity(Entity ent, Compiler* cm) { //:importActivateEntity
// Adds an import to the entities table, activates it and, if function, adds its overload
    Int nameId = ent.name & LOWER24BITS;
    Int existingBinding = cm->activeBindings[nameId];
    Int isAFunc = isFunction(ent.typeId, cm);
    VALIDATEP(existingBinding == -1 || isAFunc > -1, errAssignmentShadowing)

    Int newEntityId = cm->entities.len;
    pushInentities(ent, cm);

    if (isAFunc > -1) {
        addRawOverload(nameId, ent.typeId, newEntityId, cm);
        pushInimportNames(nameId, cm);
    } else {
        cm->activeBindings[nameId] = newEntityId;
    }
    return newEntityId;
}


testable void importEntities(Arr(EntityImport) impts, Int countEntities,
                             Compiler* cm) { //:importEntities
    for (int j = 0; j < countEntities; j++) {
        EntityImport impt = impts[j];
        importActivateEntity(
                (Entity){.name = impt.name, .class = classImmutable, .typeId = impt.typeId }, cm);
    }
    cm->stats.countNonparsedEntities = cm->entities.len;
}


private ParserFunc (*tabulateParserDispatch(Arena* a))[countSyntaxForms] { //:tabulateParserDispatch
    ParserFunc (*res)[countSyntaxForms] = allocateOnArena(countSyntaxForms*sizeof(ParserFunc), a);
    ParserFunc* p = *res;
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
    p[tokDefer]       = &pAlias;
    p[tokTrait]       = &pAlias;
    p[tokImport]      = &pAlias;
    p[tokReturn]      = &pReturn;
    p[tokTry]         = &pAlias;

    p[tokIf]          = &pIf;
    p[tokFor]         = &pFor;
    p[tokElse]        = &pAlias;
    return res;
}


testable LanguageDefinition* buildLanguageDefinitions(Arena* a) { //:buildLanguageDefinitions
// Definition of the operators, lexer dispatch and parser dispatch tables for the compiler
    setOperatorsLengths();
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    (*result) = (LanguageDefinition) {
        .dispatchTable = tabulateDispatch(a),
        .parserTable = tabulateParserDispatch(a)
    };
    return result;
}


private Stackint32_t* copyStringTable(Stackint32_t* table, Arena* a) { //:copyStringTable
    Stackint32_t* result = createStackint32_t(table->cap, a);
    result->len = table->len;
    result->cap = table->cap;
    memcpy(result->cont, table->cont, table->len*4);
    return result;
}


private StringDict* copyStringDict(StringDict* from, Arena* a) { //:copyStringDict
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
    const Int inpLength = lx->stats.inpLength;
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


testable void pushLexScope(ScopeStack* scopeStack) { //:pushLexScope
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
        ++cm->stats.countOverloadedNames;
    } else {
        Int updatedListId = addMultiAssocList(firstParamType, entityId, mbListId,
                                              cm->rawOverloads);
        if (updatedListId != -1) {
            cm->activeBindings[nameId] = -updatedListId - 2;
        }
    }
    ++cm->stats.countOverloads;
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
        (TypeHeader){.sort = sorFunction, .tyrity = 0, .arity = arity, .nameAndLen = -1}, cm);
    for (Int k = 0; k <= arity; k++) { // <= because there are (arity + 1) elts - the return type!
        pushIntypes(paramsAndReturn[k], cm);
    }
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
    for (Int i = 0; i < strSentinel; i++) {
        addStringDict(lx->sourceCode->cont, startBt, standardStringLens[i],
                      lx->stringTable, lx->stringDict);
        startBt += standardStringLens[i];
    }
}

testable NameId stToNameId(Int a) { //:stToNameId
// Converts a standard string to its nameId. Doesn't work for reserved words, obviously
    return a + countOperators;
}


private untt stToFullName(Int sta, Compiler* cm) { //:stToFullName
// Converts a standard string to its nameId. Doesn't work for reserved words, obviously
    return cm->stringTable->cont[sta + countOperators];
}


private void buildPreludeTypes(Compiler* cm) { //:buildPreludeTypes
// Creates the built-in types in the proto compiler
    for (int i = strInt; i <= strVoid; i++) {
        cm->activeBindings[stToNameId(i)] = i - strInt;
        pushIntypes(0, cm);
    }
    // List
    Int typeIndL = cm->types.len;
    print("typeIndL %d storing %d", typeIndL, stToNameId(strL))
    pushIntypes(7, cm); // 2 for the header, 1 for param tyrity, and 4 for the field names & types
    typeAddHeader((TypeHeader){.sort = sorRecord, .nameAndLen = stToFullName(strL, cm),
                  .arity = 2, .tyrity = 1}, cm);
    pushIntypes(0, cm); // tyrity of the type param
    pushIntypes(stToNameId(strLen), cm);
    pushIntypes(stToNameId(strCap), cm);
    pushIntypes(stToNameId(strInt), cm);
    pushIntypes(stToNameId(strInt), cm);
    cm->activeBindings[stToNameId(strL)] = typeIndL;

    // Array
    Int typeIndA = cm->types.len;
    pushIntypes(5, cm);
    typeAddHeader((TypeHeader){.sort = sorRecord, .nameAndLen = stToFullName(strArray, cm),
                                 .arity = 1, .tyrity = 1}, cm);
    pushIntypes(0, cm); // the arity of the type param
    pushIntypes(stToNameId(strLen), cm);
    pushIntypes(stToNameId(strInt), cm);
    cm->activeBindings[stToNameId(strArray)] = typeIndA;

    // Tuple
    Int typeIndTu = cm->types.len;
    pushIntypes(8, cm);
    typeAddHeader((TypeHeader){.sort = sorRecord, .nameAndLen = stToFullName(strTu, cm),
                                 .arity = 2, .tyrity = 2}, cm);
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
    buildOperator(opPlusExt,      strOfStrStr, cm); // dummy
    buildOperator(opPlus,         intOfIntInt, cm);
    buildOperator(opPlus,         flOfFlFl, cm);
    buildOperator(opPlus,         strOfStrStr, cm);
    buildOperator(opMinusExt,     intOfIntInt, cm); // dummy
    buildOperator(opMinus,        intOfIntInt, cm);
    buildOperator(opMinus,        flOfFlFl, cm);
    buildOperator(opDivByExt,     intOfIntInt, cm); // dummy
    buildOperator(opIntersect,    intOfIntInt, cm); // dummy
    buildOperator(opDivBy,        intOfIntInt, cm);
    buildOperator(opDivBy,        flOfFlFl, cm);
    buildOperator(opBitShiftL,    intOfFlFl, cm);  // dummy
    buildOperator(opShiftL,       intOfIntInt, cm); // dummy
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
    buildOperator(opShiftR,       intOfIntInt, cm);
    buildOperator(opGreaterTh,    boolOfIntInt, cm);
    buildOperator(opGreaterTh,    boolOfFlFl, cm);
    buildOperator(opGreaterTh,    boolOfStrStr, cm);
    buildOperator(opNullCoalesce, intOfIntInt, cm); // dummy
    buildOperator(opQuestionMark, intOfIntInt, cm); // dummy
    buildOperator(opBitwiseXor,   intOfIntInt, cm); // dummy
    buildOperator(opExponent,     intOfIntInt, cm);
    buildOperator(opExponent,     flOfFlFl, cm);
    buildOperator(opAccessor,     flOfFlFl, cm); // a dummy type. @ will be handled separately
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
//    TypeId intToDoub = addConcrFnType(1, (Int[]){ tokInt, tokDouble}, cm);
//    TypeId doubToInt = addConcrFnType(1, (Int[]){ tokDouble, tokInt}, cm);

    EntityImport imports[6] =  {
        (EntityImport) { .name = nameOfStandard(strMathPi), .typeId = tokDouble},
        (EntityImport) { .name = nameOfStandard(strMathE), .typeId = tokDouble},
        (EntityImport) { .name = nameOfStandard(strPrint), .typeId = strToVoid},
        (EntityImport) { .name = nameOfStandard(strPrint), .typeId = intToVoid},
        (EntityImport) { .name = nameOfStandard(strPrint), .typeId = floatToVoid},
        (EntityImport) { .name = nameOfStandard(strPrintErr), .typeId = strToVoid}
           // TODO functions for casting (int, double, unsigned)
    };
    // These base types occupy the first places in the stringTable and in the types table.
    // So for them nameId == typeId, unlike type funcs like L(ist) and A(rray)
    for (Int j = strInt; j <= strVoid; j++) {
        cm->activeBindings[j - strInt + countOperators] = j - strInt;
    }

    importEntities(imports, sizeof(imports)/sizeof(EntityImport), cm);
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
        .tokens = createInListToken(LEXER_INIT_SIZE, a),
        .metas = createInListToken(100, a),
        .newlines = createInListInt(500, a),
        .numeric = createInListInt(50, aTmp),
        .lexBtrack = createStackBtToken(16, aTmp),
        .stringTable = copyStringTable(proto->stringTable, a),
        .stringDict = copyStringDict(proto->stringDict, a),
        .a = a, .aTmp = aTmp
    };
    lx->stats = (CompStats){
        .inpLength = sourceCode->len + sizeof(standardText) - 1,
        .countOverloads = proto->stats.countOverloads,
        .countOverloadedNames = proto->stats.countOverloadedNames,
        .wasLexerError = false, .wasError = false, .errMsg = &empty
    };
    return lx;
}


testable void initializeParser(Compiler* lx, Compiler* proto, Arena* a) { //:initializeParser
// Turns a lexer into a parser. Initializes all the parser & typer stuff after lexing is done
    if (lx->stats.wasLexerError) {
        return;
    }

    Compiler* cm = lx;
    Int initNodeCap = lx->tokens.len > 64 ? lx->tokens.len : 64;
    cm->scopeStack = createScopeStack();
    cm->backtrack = createStackParseFrame(16, lx->aTmp);
    cm->i = 0;
    cm->stats.loopCounter = 0;
    cm->nodes = createInListNode(initNodeCap, a);
    cm->sourceLocs = createStackSourceLoc(initNodeCap, a);
    cm->monoCode = createInListNode(initNodeCap, a);
    cm->monoIds = createMultiAssocList(a);

    StateForExprs* stForExprs = allocateOnArena(sizeof(StateForExprs), a);
    (*stForExprs) = (StateForExprs) {
        .exp = createStackint32_t(16, cm->aTmp),
        .frames = createStackExprFrame(16*sizeof(ExprFrame), a),
        .scr = createStackNode(16*sizeof(Node), a),
        .calls = createStackNode(16*sizeof(Node), a),
        .locsScr = createStackSourceLoc(16*sizeof(SourceLoc), a),
        .locsCalls = createStackSourceLoc(16*sizeof(SourceLoc), a)
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
    cm->toplevels = createInListToplevel(8, lx->a);

    cm->scopeStack = createScopeStack();

    cm->stateForTypes = allocateOnArena(sizeof(StateForTypes), a);
    (*cm->stateForTypes) = (StateForTypes) {
        .exp = createStackint32_t(16, cm->aTmp),
        .frames = createStackTypeFrame(16*sizeof(TypeFrame), cm->aTmp),
        .params = createStackint32_t(16, cm->aTmp),
        .subParams = createStackint32_t(16, cm->aTmp),
        .paramRenumberings = createStackint32_t(16, cm->aTmp),
        .names = createStackint32_t(16, cm->aTmp),
        .tmp = createStackint32_t(16, cm->aTmp)
    };

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
// :validateNameOverloads Validates the overloads for a name don't intersect via their outer types
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
            Int outerArityNegated = -typeGetTyrity(ov[k], cm) - 1;
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


testable Int createNameOverloads(NameId nameId, Compiler* cm) { //:createNameOverloads
// Creates a final subtable in {overloads} for a name and returns the index of said subtable
// Precondition: {rawOverloads} contain twoples of (typeId ref)
// (typeId = the full type of a function)(ref = entityId or monoId)(yes, "twople" = tuple of two)
// Postcondition: {overloads} will contain a subtable of length(outerTypeIds)(refs)
    Arr(Int) raw = cm->rawOverloads->cont;
    const Int listId = -cm->activeBindings[nameId] - 2;
    const Int rawStart = listId + 2;

#if defined(SAFETY) || defined(TEST)
    VALIDATEI(rawStart != -1, iErrorImportedFunctionNotInScope)
#endif

    const Int countOverloads = raw[listId]/2;
    const Int rawSentinel = rawStart + raw[listId];

    Arr(Int) ov = cm->overloads.cont;
    const Int newInd = cm->overloads.len;
    ov[newInd] = 2*countOverloads; // length of the subtable for this name
    cm->overloads.len += 2*countOverloads + 1;

    for (Int j = rawStart, k = newInd + 1; j < rawSentinel; j += 2, k++) {
        const FirstArgTypeId firstParamType = raw[j];
        if (firstParamType > -1) {
            OuterTypeId outerType = typeGetOuter(firstParamType, cm);
            ov[k] = outerType;
        } else {
            ov[k] = -1;
        }
        ov[k + countOverloads] = raw[j + 1]; // entityId
    }

    sortPairsDistant(newInd + 1, newInd + 1 + 2*countOverloads, countOverloads, ov);
    validateNameOverloads(newInd, countOverloads, cm);
    return newInd;
}


testable void createOverloads(Compiler* cm) { //:createOverloads
// Fills {overloads} from {rawOverloads}. Replaces all indices in
// {activeBindings} to point to the new overloads table (they pointed to {rawOverloads} previously)
    cm->overloads.cont = allocateOnArena(
            cm->stats.countOverloads*8 + cm->stats.countOverloadedNames*4, cm->a);
    // Each overload requires 2x4 = 8 bytes for the pair of (outerType entityId).
    // Plus you need an int per overloaded name to hold the length of the overloads for that name

    cm->overloads.len = 0;
    for (Int j = 0; j < countOperators; j++) {
        Int newIndex = createNameOverloads(j, cm);
        cm->activeBindings[j] = -newIndex - 2;
    }
    removeDuplicatesInList(&(cm->importNames));
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


private void pToplevelTypes(Compiler* cm) { //:pToplevelTypes
// Parses top-level types but not functions. Writes them to the types table and adds
// their bindings to the scope
    cm->i = 0;
    Arr(Token) toks = cm->tokens.cont;
    const Int len = cm->tokens.len;
    while (cm->i < len) {
        Token tok = toks[cm->i];
        if (tok.tp == tokAssignLeft && toks[cm->i + 1].tp == tokTypeName) {
            ++cm->i; // CONSUME the assignment token
            pTypeDef(P_C);
        } else {
            cm->i += (tok.pl2 + 1);
        }
    }
}


private void pToplevelConstants(Compiler* cm) { //:pToplevelConstants
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
// Precondition: we are 1 past the tokIntro
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


testable void pFnSignature(Token fnDef, bool isToplevel, untt name, Int voidToVoid,
                               Compiler* cm) { //:pFnSignature
// Parses a function signature. Emits no nodes, adds data to @toplevels, @functions, @overloads.
// Pre-condition: we are 1 token past the tokFn
    Int fnStartTokenId = cm->i - 1;
    Int fnSentinelToken = fnStartTokenId + fnDef.pl2 + 1;

    NameId fnNameId = name & LOWER24BITS;

    pushIntypes(0, cm); // will overwrite it with the type's length once we know it

    Arr(Token) toks = cm->tokens.cont;
    TypeId newFnTypeId = voidToVoid; // default for nullary functions
    StateForTypes* st = cm->stateForTypes;
    if (toks[cm->i].tp == tokIntro) {
        Token paramListTk = toks[cm->i];
        Int sentinel = calcSentinel(paramListTk, cm->i);
        cm->i++; // CONSUME the paramList
        st->frames->len = 0;
        push(((TypeFrame) { .tp = sorFunction, .sentinel = sentinel }), st->frames);
        newFnTypeId = tDefinition(st, sentinel, cm);
        print("fn has received type %d", newFnTypeId)
    }

    EntityId newFnEntityId = cm->entities.len;
    pushInentities(((Entity){ .class = classImmutable, .typeId = newFnTypeId }), cm);
    addRawOverload(fnNameId, newFnTypeId, newFnEntityId, cm);

    pushIntoplevels((Toplevel){.indToken = fnStartTokenId, .sentinelToken = fnSentinelToken,
            .name = name, .entityId = newFnEntityId }, cm);
}


private void pToplevelBody(Toplevel toplevelSignature, Arr(Token) toks, Compiler* cm) {
//:pToplevelBody Parses a top-level function. The result is the AST
//L( FnDef ParamList body... )
// TODO pass concrete type to handle the fact that all types must be monomorphized here
    Int fnStartInd = toplevelSignature.indToken;
    Int fnSentinel = toplevelSignature.sentinelToken;
    EntityId fnEntity = toplevelSignature.entityId;
    TypeId fnType = cm->entities.cont[fnEntity].typeId;
    print("at start, node count is %d", cm->nodes.len);

    print("toplevel body startInd %d sentinel %d", fnStartInd, fnSentinel);

    cm->i = fnStartInd; // tokFn
    Token fnTk = toks[cm->i];

    // the fnDef scope & node
    push(((ParseFrame){ .tp = nodFnDef, .startNodeInd = cm->nodes.len, .sentinelToken = fnSentinel,
                        .typeId = fnType }), cm->backtrack);
    //addParsedScope(fnSentinel, fnTk.startBt, fnTk.lenBts, cm);
    addNode((Node){ .tp = nodFnDef, .pl1 = fnEntity, .pl3 = (toplevelSignature.name & LOWER24BITS)},
            fnTk.startBt, fnTk.lenBts, cm);

    cm->i += 1; // CONSUME the parens token for the param list
    Token mbParamsTk = toks[cm->i];
    if (mbParamsTk.tp == tokIntro) {
        const Int paramsSentinel = cm->i + mbParamsTk.pl2 + 1;
        cm->i += 1; // CONSUME the tokIntro
        print("fn type:");
        tPrint(fnType, cm);
        print("paramsSentinel %d", paramsSentinel)
        TypeId paramType = tGetIndexOfFnFirstParam(fnType, cm);
        while (cm->i < paramsSentinel) {
            // need to get params type from the function type we got, not
            // from tokens (where they may be generic)
            Token paramName = toks[cm->i];
            if (paramName.tp == tokMisc) {
                break;
            }
            print("here token type at %d is %d, type is %d", cm->i, paramName.tp, cm->types.cont[paramType])
            Int newEntityId = createEntityWithType(
                    nameOfToken(paramName), paramType,
                    paramName.pl1 == 1 ? classMutable : classImmutable, cm);
            addNode(((Node){.tp = nodBinding, .pl1 = newEntityId, .pl2 = 0}),
                    paramName.startBt, paramName.lenBts, cm);

            ++cm->i; // CONSUME the param name
            cm->i = calcSentinel(toks[cm->i], cm->i); // CONSUME the tokens of param type
            paramType += 1;
        }
    }

    ++cm->i; // CONSUME the "=" sign
    print("here i %d sent %d", cm->i, fnSentinel)
    parseUpTo(fnSentinel, P_C);
}


private void pFunctionBodies(Arr(Token) toks, Compiler* cm) { //:pFunctionBodies
// Parses top-level function params and bodies
    for (int j = 0; j < cm->toplevels.len; j++) {
        cm->stats.loopCounter = 0;
        pToplevelBody(cm->toplevels.cont[j], P_C);
    }
}


private void pToplevelSignatures(Compiler* cm) { //:pToplevelSignatures
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
        pFnSignature(rightTk, true, name, voidToVoid, cm);
    }
}


// Must agree in order with node types in tl.internal.h
const char* nodeNames[] = {
    "Int", "Long", "Double", "Bool", "String", "_", "misc",
    "id", "call", "binding", ".fld", "GEP", "GElem",
    "{}", "Expr", "...=", "data()",
    "alias", "assert", "breakCont", "catch", "defer",
    "import", "{{fn}}", "trait", "return", "try",
    "for", "forCond", "forStep", "if", "ei", "impl", "match"
};


testable Compiler* parseMain(Compiler* cm, Arena* a) { //:parseMain
    if (setjmp(excBuf) == 0) {
        pToplevelTypes(cm);
        // This gives the complete overloads & overloadIds tables + list of toplevel functions
        pToplevelSignatures(cm);
        createOverloads(cm);

        pToplevelConstants(cm);

#ifdef SAFETY
        validateOverloadsFull(cm);
#endif
        // The main parse (all top-level function bodies)
        pFunctionBodies(cm->tokens.cont, cm);
    }
    return cm;
}


testable Compiler* parse(Compiler* cm, Compiler* proto, Arena* a) { //:parse
// Parses a single file in 4 passes, see docs/parser.txt
    initializeParser(cm, proto, a);
    return parseMain(cm, a);
}

//}}}
//{{{ Types
//{{{ Type utils

#define TYPE_PREFIX_LEN 3 // sizeof(TypeHeader)/4 + 1. Length (in ints) of the prefix in type repr
#define TYPE_DEFINE_EXP const StackInt* exp = st->exp

private Int typeEncodeTag(untt sort, Int depth, Int arity, Compiler* cm) {
    return (Int)((untt)(sort << 16) + (depth << 8) + arity);
}


testable void typeAddHeader(TypeHeader hdr, Compiler* cm) { //:typeAddHeader
// Writes the bytes for the type header to the tail of the cm->types table.
// Adds two 4-byte elements
    pushIntypes((Int)((untt)((untt)hdr.sort << 16) + ((untt)hdr.arity << 8) + hdr.tyrity), cm);
    pushIntypes((Int)hdr.nameAndLen, cm);
}


testable TypeHeader typeReadHeader(TypeId typeId, Compiler* cm) { //:typeReadHeader
// Reads a type header from the type array
    Int tag = cm->types.cont[typeId + 1];
    return (TypeHeader){ .sort = ((untt)tag >> 16) & LOWER16BITS,
            .arity = (tag >> 8) & 0xFF, .tyrity = tag & 0xFF,
            .nameAndLen = (untt)cm->types.cont[typeId + 2] };
}


private Int typeGetTyrity(TypeId typeId, Compiler* cm) { //:typeGetTyrity
    if (cm->types.cont[typeId] == 0) {
        return 0;
    }
    Int tag = cm->types.cont[typeId + 1];
    return tag & 0xFF;
}


private OuterTypeId typeGetOuter(FirstArgTypeId typeId, Compiler* cm) { //:typeGetOuter
// A         => A  (concrete types)
// A(B)      => A  (concrete generic types)
// A + A(B)  => -2 (param generic types)
// F(A -> B) => F(A -> B)
    if (typeId <= topVerbatimType) {
        return typeId;
    }
    TypeHeader hdr = typeReadHeader(typeId, cm);
    if (hdr.sort <= sorFunction) {
        return typeId;
    } else if (hdr.sort == sorTypeCall) {
        Int genElt = cm->types.cont[typeId + hdr.tyrity + 3];
        if ((genElt >> 24) & 0xFF == 0xFF) {
            return -(genElt & 0xFF) - 1; // a param type in outer position,
                                         // so we return its (-arity -1)
        } else {
            return genElt & LOWER24BITS;
        }
    } else {
        return cm->types.cont[typeId + hdr.tyrity + 3] & LOWER24BITS;
    }
}


private TypeId tGetIndexOfFnFirstParam(TypeId fnType, Compiler* cm) { //:tGetIndexOfFnFirstParam
    TypeHeader hdr = typeReadHeader(fnType, cm);
#ifdef SAFETY
    VALIDATEI(hdr.sort == sorFunction, iErrorNotAFunction);
#endif
    return fnType + TYPE_PREFIX_LEN + hdr.tyrity;

}


private Int isFunction(TypeId typeId, Compiler* cm) { //:isFunction
// Returns the function's arity if the type is a function type,
// -1 otherwise
    if (typeId < topVerbatimType) {
        return -1;
    }
    TypeHeader hdr = typeReadHeader(typeId, cm);
    return (hdr.sort == sorFunction) ? hdr.arity : -1;
}

//}}}
//{{{ Parsing type names

testable void typeAddTypeParam(Int paramInd, Int tyrity, Compiler* cm) { //:typeAddTypeParam
// Adds a type param to a TypeCall-sort type. Tyrity > 0 means the param is a type call
    pushIntypes((0xFF << 24) + (paramInd << 8) + tyrity, cm);
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


testable Int typeParamBinarySearch(Int nameIdToFind, Compiler* cm) { //:typeParamBinarySearch
// Performs a binary search of the binary params in {typeParams}. Returns index of found type param,
// or -1 if nothing is found
    StackInt* params = cm->stateForTypes->params;
    if (params->len == 0) {
        return -1;
    }
    Arr(Int) st = params->cont;
    Int i = 0;
    Int j = params->len - 2;
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

private Int tSubexValidateNamesUnique(StateForTypes* st, Int start, Compiler* cm) {
//:tSubexValidateNamesUnique Validates that the names in a record or function sign are unique.
// Returns function/record's arity
    const Int end = st->names->len;
    if (end == 0) {
        return 0;
    }
    StackInt* names = st->names;
    StackInt* tmp = st->tmp;
    // copy from names to tmp
    if (tmp->cap < names->len) {
        Arr(Int) arr = allocateOnArena(names->len*4, cm->aTmp);
        tmp->cont = arr;
        tmp->cap = names->len;
    }
    memcpy(tmp->cont, names->cont, names->len);
    tmp->len = names->len;

    sortStackInts(tmp);
    NameId prev = tmp->cont[0];
    for (Int j = 1; j < tmp->len; j++) {
        if (tmp->cont[j] == prev)  {
            throwExcParser(errFnDuplicateParams);
        }
    }
    return names->len;
}


/*
private TypeId typeCreateRecord(StateForTypes* st, Int startInd, untt nameAndLen,
                                Compiler* cm) { //:typeCreateRecord
// Creates/merges a new record type from a sequence of pairs in @exp and a list of type params
// in @params. The sequence must be flat, i.e. not include any nested structs, and be in the
// final position of @exp. "nameAndLen" may be -1 if it's an anonymous record.
// Returns the typeId of the new/existing type
    TYPE_DEFINE_EXP;
    tSubexValidateNamesUnique(st, startInd, exp->len, cm);
    Int tentativeTypeId = cm->types.len;
    pushIntypes(0, cm);
    Int sentinel = exp->len;

#ifdef SAFETY
    VALIDATEP((sentinel - startInd) % 4 == 0, "typeCreateStruct err not divisible by 4")
#endif
    Int countFields = (sentinel - startInd)/4;
    typeAddHeader((TypeHeader){
        .sort = sorRecord, .tyrity = st->params->len/2, .arity = countFields,
        .nameAndLen = nameAndLen }, cm);
    for (Int j = 1; j < st->params->len; j += 2) {
        pushIntypes(st->params->cont[j], cm);
    }

    for (Int j = startInd + 1; j < sentinel; j += 4) {
        // names of fields
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
*/


private TypeId tCreateTypeCall(StateForTypes* st, Int startInd, TypeFrame frame,
                                  Compiler* cm) { //:tCreateTypeCall
// Creates/merges a new type call from a sequence of pairs in @exp and a list of type params
// in @params. Handles ordinary type calls and function types.
// Returns the typeId of the new type
    TypeId typeId = frame.nameId;
    VALIDATEP(typeGetTyrity(typeId, cm) == frame.countArgs, errTypeConstructorWrongArity)

    TYPE_DEFINE_EXP;
    const Int tentativeTypeId = cm->types.len;
    pushIntypes(0, cm);
    const Int sentinel = exp->len;

    typeAddHeader((TypeHeader){
        .sort = sorTypeCall, .tyrity = frame.countArgs, .arity = 0, .nameAndLen = typeId}, cm);
    for (Int j = startInd; j < sentinel; j += 1) {
        pushIntypes(exp->cont[j], cm);
    }

    cm->types.cont[tentativeTypeId] = cm->types.len - tentativeTypeId - 1;
    print("\nTypeCall result:", startInd, sentinel);
    printIntArrayOff(tentativeTypeId, cm->types.len - tentativeTypeId, cm->types.cont);
    return mergeType(tentativeTypeId, cm);
}


private StackInt* tDetermineUniqueParamsInDef(const StateForTypes* st, Int startInd,
                                                 Compiler* cm) {
//:tDetermineUniqueParamsInDef Counts how many unique type parameters are used in a type def
// TODO take care of nesting of typecalls
    TYPE_DEFINE_EXP;
    Int paramsSentinel = exp->len;
    st->subParams->len = 0;
    for (Int j = startInd; j < paramsSentinel; j += 2) { // +2 to skip the name stuff

        Int cType = exp->cont[j];

        if (cType < 0) {
            push(-cType - 1, st->subParams);
        } else {

            TypeId typeId = exp->cont[j];
            TypeHeader tcHeader = typeReadHeader(typeId, cm);
            if (tcHeader.sort != sorTypeCall) {
                continue;
            }
            TypeId sentOfType = typeId + cm->types.cont[typeId] + 1;
            for (Int k = typeId + TYPE_PREFIX_LEN; k < sentOfType; k++) {
                Int typeArg = cm->types.cont[k];
                if (typeArg < 0) {
                    push(-1*(typeArg + 1), st->subParams);
                }
            }
        }
    }
    sortStackInts(st->subParams);
    removeDuplicatesInStack(st->subParams);

    return st->subParams;
}


private TypeId tCreateFnSignature(StateForTypes* st, Int startInd,
                                     untt fnNameAndLen, Compiler* cm) { //:tCreateFnSignature
// Creates/merges a new struct type from a sequence of pairs in "exp" and a list of type params
// in "params". The sequence must be flat, i.e. not include any nested structs.
// Returns the typeId of the new type
// Example input: exp () params ()
    TYPE_DEFINE_EXP;
    const Int sentinel = exp->len;

    const Int depth = exp->len - startInd;
    const Int arity = depth - 1;
    const Int countNames = tSubexValidateNamesUnique(st, startInd, cm);
    const StackInt* subParams = tDetermineUniqueParamsInDef(st, startInd, cm);

    VALIDATEP(arity == countNames, errTypeDefCountNames);
    Int tyrity = subParams->len;

    const Int tentativeTypeId = cm->types.len;
    pushIntypes(0, cm);

    typeAddHeader((TypeHeader){
        .sort = sorFunction, .tyrity = tyrity, .arity = arity, .nameAndLen = fnNameAndLen}, cm);
    for (Int j = 0; j < tyrity; j += 1) {
        pushIntypes(st->params->cont[st->subParams->cont[j]], cm); // tyrities of the params used in
                                                                   // this subexpression
    }

    for (Int j = startInd; j < sentinel; j += 1) {
        // types of fields
        pushIntypes(exp->cont[j], cm);
    }
    cm->types.cont[tentativeTypeId] = cm->types.len - tentativeTypeId - 1; // set the type length
    return mergeType(tentativeTypeId, cm);
}


#define maxTypeParams 254


private void tSubexClose(StateForTypes* st, Compiler* cm) { //:tSubexClose
// Flushes the finished subexpr frames from the top of the funcall stack. Handles data allocations
    StackInt* exp = st->exp;
    StackTypeFrame* frames = st->frames;
    while (frames->len > 0 && peek(frames).sentinel == cm->i) {
        TypeFrame frame = pop(frames);

        Int startInd = exp->len - frame.countArgs;
        Int newTypeId = -1;
        if (frame.tp == sorFunction) {
            newTypeId = tCreateFnSignature(st, startInd, -1, cm);
            tPrint(newTypeId, cm);

        } else if (frame.tp == sorRecord) {
            throwExcParser(errTemp);
            //typeCreateRecord(st, startInd, -1, cm);
        } else if (frame.tp == sorTypeCall) {
            newTypeId = tCreateTypeCall(st, startInd, frame, cm);
        } else { // tyeParamCall, a call of a type which is a parameter
            // TODO
            throwExcParser(errTemp);
            //VALIDATEP(cm->typeParams->cont[frame.nameId + 1] == frame.countArgs,
            //          errTypeConstructorWrongArity)
        }
//~        if (frame.tp != tyeName && hasValues(frames) && peek(frames).tp == tyeName)  {
//~            frames->cont[frames->len - 1].countArgs += 1;
//~        }
        exp->cont[startInd] = newTypeId;
        exp->len = startInd + 1;
    }
}


private TypeId tDefinition(StateForTypes* st, Int sentinel, Compiler* cm) { //:tDefinition
// Parses a type definition like `Rec(id Int name Str)` or `a Double b Bool -> String?`.
// Precondition: we are looking at the first token in actual type definition.
// {typeStack} may be non-empty (for example, when parsing a function signature,
// it will contain one element with .tp = sorFunction). Produces a linear, RPN sequence.
    StackInt* exp = st->exp;
    StackInt* params = st->params;
    StackTypeFrame* frames = st->frames;
    exp->len = 0;
    params->len = 0;
    Bool metArrow = false;
    Bool isFnSignature = hasValues(frames) && peek(frames).tp == sorFunction;

    while (cm->i < sentinel) {
        tSubexClose(st, cm);

        Token cTk = cm->tokens.cont[cm->i];
        ++cm->i; // CONSUME the current token

        VALIDATEP(hasValues(frames), errTypeDefError)
        if (cTk.tp == tokWord) {
            VALIDATEP(cm->i < sentinel, errTypeDefError)
            Int ctxType = peek(frames).tp;
            VALIDATEP(ctxType == sorRecord || ctxType == sorFunction, errTypeDefError)

            Token nextTk = cm->tokens.cont[cm->i];
            VALIDATEP(nextTk.tp == tokTypeName || nextTk.tp == tokTypeCall, errTypeDefError)
            push(cTk.pl2, st->names);
            continue;
        } else if (cTk.tp == tokMisc) { // "->", function return type marker
            VALIDATEP(cTk.pl1 == miscArrow && peek(frames).tp == sorFunction, errTypeDefError);
            VALIDATEP(!metArrow, errTypeFnSingleReturnType)
            metArrow = true;

            if (cm->i < sentinel) { // no return type specified = void type
                untt nextTp = cm->tokens.cont[cm->i].tp;
                VALIDATEP(nextTp == tokTypeName || nextTp == tokTypeCall, errTypeDefError)
            } else {
                push((cm->activeBindings[stToNameId(strVoid)]), exp);
                frames->cont[frames->len - 1].countArgs++;
            }

            tPrintTypeFrames(frames);
            continue;
        }

        frames->cont[frames->len - 1].countArgs++;

        if (cTk.tp == tokTypeName) {
            // arg count
            Int mbParamId = typeParamBinarySearch(cTk.pl2, cm);
            if (mbParamId == -1) {
                tPrintTypeFrames(frames);
                push(cm->activeBindings[cTk.pl2], exp);

                print("EXP::")
                printStackInt(exp);
            } else {
                push(-mbParamId - 1, exp); // index of this param in @params
            }
        } else if (cTk.tp == tokTypeCall) {
            VALIDATEP(cm->i < sentinel, errTypeDefError)
            Token typeFuncTk = cm->tokens.cont[cm->i];

            Int nameId = typeFuncTk.pl2 & LOWER24BITS;

            Int mbParamId = typeParamBinarySearch(nameId, cm);
            Int newSent = calcSentinel(cTk, cm->i - 1);
            cm->i += 1; // CONSUME the type function

            if (mbParamId == -1) {
                if (nameId == stToNameId(strF)) { // F(...)
                    push(((TypeFrame){ .tp = sorFunction, .sentinel = newSent}), frames);
                } else if (nameId == stToNameId(strRec)) { // inline types  `(id Int name String)`
                    push(((TypeFrame){ .tp = sorRecord, .sentinel = newSent}), frames);
                } else { // ordinary type call
                    Int typeId = cm->activeBindings[nameId];
                    VALIDATEP(typeId > -1, errUnknownTypeConstructor)
                    push(((TypeFrame){ .tp = sorTypeCall, .nameId = typeId, .sentinel = newSent}),
                         frames);
                }
            } else {
                push(((TypeFrame){ .tp = tyeParam, .nameId = mbParamId, .sentinel = newSent}),
                      frames);
            }
        } else {
            print("erroneous type %d", cTk.tp)
            throwExcParser(errTypeDefError);
        }
    }

    VALIDATEP(metArrow || !isFnSignature, errTypeDefParamsError) // func sign must contain the "->"

    print("result before final close");
    printStackInt(exp);

    tSubexClose(st, cm);

    VALIDATEI(exp->len == 1, iErrorInconsistentTypeExpr);
    return exp->cont[0];
}


private void typeNameNewType(TypeId newTypeId, untt name, Compiler* cm) { //:typeNameNewType
    cm->activeBindings[(name & LOWER24BITS)] = newTypeId;
    cm->types.cont[newTypeId + 1] = name;
}


testable Int pTypeDef(P_CT) { //:pTypeDef
// Builds a type expression from a type definition or a function signature.
// Example 1: `Foo = id Int name String`
// Example 2: `a Double b Bool => String`
//
// Accepts a name or -1 for nameless type exprs (like function signatures).
// Uses cm->exp to build a "type expression" and cm->params for the type parameters
// Produces no AST nodes, but potentially lots of new types
// Consumes the whole type assignment right side, or the whole function signature
// Data format: see "Type expression data format"
// Precondition: we are 1 past the tokAssignmentRight token, or tokIntro token
    //StackInt* params = cm->stateForTypes->params;
    cm->stateForTypes->frames->len = 0;

    Int sentinel = cm->i + toks[cm->i - 1].pl2; // we get the length from the tokAssignmentRight
    Token nameTk = toks[cm->i];
    cm->i += 2; // CONSUME the type name and the tokAssignmentRight

    VALIDATEP(cm->i < sentinel, errTypeDefError)
    Int newTypeId = tDefinition(cm->stateForTypes, sentinel, cm);
    typeNameNewType(newTypeId, nameOfToken(nameTk), cm);

    return newTypeId;
}

//}}}
//{{{ Overloads, type check & resolve

/*
testable StackInt* typeSatisfiesGeneric(Int typeId, Int genericId, Compiler* cm);
*/

private FirstArgTypeId getFirstParamType(TypeId funcTypeId, Compiler* cm) { //:getFirstParamType
// Gets the type of the first param of a function. Returns -1 iff it's zero arity
    TypeHeader hdr = typeReadHeader(funcTypeId, cm);
    if (hdr.arity == 0) {
        return -1;
    }
    return cm->types.cont[funcTypeId + 3 + hdr.tyrity]; // +3 skips the type length, type tag & name
}

private Int getFirstParamInd(TypeId funcTypeId, Compiler* cm) { //:getFirstParamInd
// Gets the ind of the first param of a function. Precondition: function has a non-zero arity!
    TypeHeader hdr = typeReadHeader(funcTypeId, cm);
    return funcTypeId + 3 + hdr.tyrity; // +3 skips the type length, type tag & name
}


private TypeId getFunctionReturnType(TypeId funcTypeId, Compiler* cm) { //:getFunctionReturnType
    TypeHeader hdr = typeReadHeader(funcTypeId, cm);
    return cm->types.cont[funcTypeId + 3 + hdr.tyrity + hdr.arity];
}


private bool isFunctionWithParams(TypeId typeId, Compiler* cm) { //:isFunctionWithParams
    return cm->types.cont[typeId] > 1;
}


testable bool findOverload(FirstArgTypeId typeId, Int ovInd, OUT EntityId* entityId, Compiler* cm) {
//:findOverload Params: typeId = type of the first function parameter, or -1 if it's 0-arity
//         ovInd = ind in [overloads], which is found via [activeBindings]
//         entityId = address where to store the result, if successful
// We have 4 scenarios here, sorted from left to right in the outerType part of [overloads]:
// 1. outerType < -1: non-function generic types with outer generic, e.g. "U(Int)" => -2
// 2. outerType = -1: 0-arity function
// 3. outerType >=< 0 BIG: non-function types with outer concrete, e.g. "L(U)" => ind of L
// 4. outerType >= BIG: function types (generic or concrete), e.g. "F(Int => String)" => BIG + 1
    const Int start = ovInd + 1;
    Arr(Int) overs = cm->overloads.cont;
    const Int countOverloads = overs[ovInd]/2;
    const Int sentinel = ovInd + countOverloads + 1;
    if (typeId == -1) { // scenario 2
        Int j = ovInd + 1;
        while (j < sentinel && overs[j] < 0) {
            if (overs[j] == -1) {
                (*entityId) = overs[j + countOverloads];
                return true;
            }
            ++j;
        }
        throwExcParser(errTypeNoMatchingOverload);
    }

    const Int outerType = typeGetOuter(typeId, cm);
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
        Int ind = binarySearch(outerType, firstNonneg, k + 1, overs);
        if (ind == -1) {
            return false;
        }
        (*entityId) = overs[ind + countOverloads];
        return true;
    }
    return false;
}



private void shiftStackLeft(Int startInd, Int byHowMany, Compiler* cm) { //:shiftStackLeft
// Shifts ints from start and until the end to the left.
// E.g. the call with args (4, 2) takes the stack from [x x x x 1 2 3] to [x x 1 2 3]
// startInd and byHowMany are measured in units of Int
    Int from = startInd + byHowMany;
    Int to = startInd;
    StackInt* exp = cm->stateForExprs->exp;
    Int sentinel = exp->len;
    while (from < sentinel) {
        Int pieceSize = MIN(byHowMany, sentinel - from);
        memcpy(exp->cont + to, exp->cont + from, pieceSize*4);
        from += pieceSize;
        to += pieceSize;
    }
    exp->len -= byHowMany;
}


private void populateExp(Int indExpr, Int sentinelNode, Compiler* cm) {
//:populateExp Populates the expression's type stack with the operands and functions of an
// expression
    StackInt* exp = cm->stateForExprs->exp;
    exp->len = 0;
    for (Int j = indExpr + 1; j < sentinelNode; ++j) {
        Node nd = cm->nodes.cont[j];
        if (nd.tp <= tokString) {
            push((Int)nd.tp, exp);
        } else if (nd.tp == nodCall) {
            Int argCount = nd.pl2;
            push(BIG + argCount, exp); // signifies that it's a call, and its arity
            push((argCount > 0 ? -nd.pl1 - 2 : nd.pl1), exp);
            // index into overloadIds, or entityId for 0-arity fns

        } else if (nd.pl1 > -1) { // entityId
            push(cm->entities.cont[nd.pl1].typeId, exp);
        } else { // overloadId
            push(nd.pl1, exp); // overloadId
        }
    }
}


testable void typeReduceExpr(StackInt* exp, Int indExpr, Compiler* cm) {
//:typeReduceExpr Runs the typechecking "evaluation" on a pure expression, i.e. one that doesn't
// contain any nested subexpressions (i.e. data allocations or lambdas)
    // We go from left to right: resolving the calls, typechecking & collapsing args, and replacing
    // calls with their return types
    Int expSentinel = exp->len;
    Arr(Int) cont = exp->cont;
    Int currAhead = 1; // 1 for the extra "BIG" element before the call in "st"

    for (Int j = 0; j < expSentinel; ++j) {
        if (cont[j] < BIG) { // it's not a function call because function call indicators
                             // have BIG in them
            continue;
        }

        // A function call. cont[j] contains the argument count, cont[j + 1] index in {overloads}
        const Int argCount = cont[j] - BIG;
        const Int o = cont[j + 1];
        if (argCount == 0) {
            VALIDATEP(o > -1, errTypeOverloadsOnlyOneZero)

            Int entityId;
            Int indOverl = -cm->activeBindings[o] - 2;
            bool ovFound = findOverload(-1, indOverl, &entityId, cm);

#if defined(TRACE) && defined(TEST) //{{{
            if (!ovFound) {
                printStackInt(exp);
            }
#endif //}}}
            VALIDATEP(ovFound, errTypeNoMatchingOverload)
            cm->nodes.cont[j + indExpr + currAhead].pl1 = entityId;

            cont[j] = getFunctionReturnType(cm->entities.cont[entityId].typeId, cm);
            shiftStackLeft(j + 1, 1, cm);
            --expSentinel;

            // the function returns nothing, so there's no return type to write
        } else {
            const Int tpFstArg = cont[j - argCount];

            VALIDATEP(tpFstArg > -1, errTypeUnknownFirstArg)
            Int entityId;
            Int indOverl = -cm->activeBindings[-o - 2] - 2;
            const Bool ovFound = findOverload(tpFstArg, indOverl, &entityId, cm);
#if defined(TRACE) && defined(TEST) //{{{
            if (!ovFound) {
                printStackInt(exp);
            }
#endif //}}}
            VALIDATEP(ovFound, errTypeNoMatchingOverload)

            Int typeOfFunc = cm->entities.cont[entityId].typeId;
            VALIDATEP(typeReadHeader(typeOfFunc, cm).arity == argCount, errTypeNoMatchingOverload)
            // first parm matches, but not arity
            Int firstParamInd = getFirstParamInd(typeOfFunc, cm);
            for (int k = j - argCount, l = firstParamInd; k < j; k++, l++) {
                // We know the type of the function, now to validate arg types against param types
                if (cont[k] > -1) { // type of arg is known
                    VALIDATEP(cont[k] == cm->types.cont[l],
                              errTypeWrongArgumentType)
                } else { // it's not known, so we fill it in
                    Int argBindingId = cm->nodes.cont[indExpr + k + (currAhead)].pl1;
                    cm->entities.cont[argBindingId].typeId = cm->types.cont[l];
                }
            }


            cm->nodes.cont[j + indExpr + (currAhead)].pl1 = entityId;
            // the type-resolved function of the call

            j -= argCount;
            currAhead += argCount;
            shiftStackLeft(j, argCount + 1, cm);

            cont[j] = getFunctionReturnType(typeOfFunc, cm); // the function return type
            expSentinel -= (argCount + 1);
        }
    }
}


testable TypeId typeCheckBigExpr(Int indExpr, Int sentinelNode, Compiler* cm) {
//:typeCheckBigExpr Typechecks and resolves overloads in a single expression. "Big" refers to
// the fact that this expr may contain sub-assignments for data allocation.
// "indExpr" is the index of nodExpr or nodAssignmentRight
    StackInt* exp = cm->stateForExprs->exp;

    populateExp(indExpr, sentinelNode, cm);

    typeReduceExpr(exp, indExpr, cm);

    if (exp->len == 1) {
        return exp->cont[0]; // the last remaining element is the type of the whole expression
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

/*
testable bool typeGenericsIntersect(Int type1, Int type2, Compiler* cm) {
// Returns true iff two generic types intersect (i.e. a concrete type may satisfy both of them)
// Warning: this function assumes that both types have sort = Partial
    TypeHeader hdr1 = typeReadHeader(type1, cm);
    TypeHeader hdr2 = typeReadHeader(type2, cm);
    Int i1 = type1 + 3 + hdr1.tyrity; // +3 to skip the type length, type tag and name
    Int i2 = type2 + 3 + hdr2.tyrity;

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
*/


private Int typeMergeTypeCall(Int startInd, Int len, Compiler* cm) {
// Takes a single call from within a concrete type, and merges it into the type dictionary
// as a whole, separate type. Returns the unique typeId
    return -1;
}

//}}}
//}}}
//{{{ Utils for tests & debugging

#ifdef TEST

//{{{ General utils

void printIntArray(Int count, Arr(Int) arr) { //:printIntArray
    printf("[");
    for (Int k = 0; k < count; k++) {
        printf("%d ", arr[k]);
    }
    printf("]\n");
}


void printIntArrayOff(Int startInd, Int count, Arr(Int) arr) { //:printIntArrayOff
    printf("[...");
    for (Int k = 0; k < count; k++) {
        printf("%d ", arr[startInd + k]);
    }
    printf("...]\n");
}

private void printStackInt(StackInt* st) { //:printStackInt
    printIntArray(st->len, st->cont);
}


void printNameAndLen(untt unsign, Compiler* cm) { //:printNameAndLen
    Int startBt = unsign & LOWER24BITS;
    Int len = (unsign >> 24) & 0xFF;
    fwrite(cm->sourceCode->cont + startBt, 1, len, stdout);
}


void printName(NameId nameId, Compiler* cm) { //:printName
    untt unsign = cm->stringTable->cont[nameId];
    printNameAndLen(unsign, cm);
    printf("\n");
}

//}}}
//{{{ Lexer testing


// Must agree in order with token types in tl.internal.h
const char* tokNames[] = {
    "Int", "Long", "Double", "Bool", "String", "misc",
    "word", "Type", ":kwarg", "operator", "@acc",
    "stmt", "()", "[]", "intro:", "data",
    "...=", "=...", "alias", "assert", "breakCont",
    "iface", "import", "return",
    "{}", "{{fn}}", "try{", "catch{", "finally{",
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


int equalityLexer(Compiler a, Compiler b) { //:equalityLexer
// Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first
// differing token otherwise
    if (a.stats.wasLexerError != b.stats.wasLexerError
            || !endsWith(a.stats.errMsg, b.stats.errMsg)) {
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


testable void printLexer(Compiler* lx) { //:printLexer
    if (lx->stats.wasLexerError) {
        printf("Error: ");
        printString(lx->stats.errMsg);
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

testable void printParser(Compiler* cm, Arena* a) { //:printParser
    if (cm->stats.wasError) {
        printf("Error: ");
        printString(cm->stats.errMsg);
    }
    Int indent = 0;
    Stackint32_t* sentinels = createStackint32_t(16, a);
    StandardText stText = getStandardTextLength();
    for (int i = 0; i < cm->nodes.len; i++) {
        Node nod = cm->nodes.cont[i];
        SourceLoc loc = cm->sourceLocs->cont[i];
        for (int m = sentinels->len - 1; m > -1 && sentinels->cont[m] == i; m--) {
            popint32_t(sentinels);
            indent--;
        }

        if (i < 10) printf(" ");
        printf("%d: ", i);
        for (int j = 0; j < indent; j++) {
            printf("  ");
        }
        Int startBt = loc.startBt - stText.len;
        if (nod.tp == nodCall) {
            printf("Call %d [%d; %d] type = \n", nod.pl1, startBt, loc.lenBts);
            //printType(cm->entities.cont[nod.pl1].typeId, cm);
        } else if (nod.pl1 != 0 || nod.pl2 != 0) {
            if (nod.pl3 != 0)  {
                printf("%s %d %d %d [%d; %d]\n", nodeNames[nod.tp], nod.pl1, nod.pl2, nod.pl3,
                        startBt, loc.lenBts);
            } else {
                printf("%s %d %d [%d; %d]\n", nodeNames[nod.tp], nod.pl1, nod.pl2,
                        startBt, loc.lenBts);
            }
        } else {
            printf("%s [%d; %d]\n", nodeNames[nod.tp], startBt, loc.lenBts);
        }
        if (nod.tp >= nodScope && nod.pl2 > 0) {
            pushint32_t(i + nod.pl2 + 1, sentinels);
            indent++;
        }
    }
}

private void printStackNode(StackNode* st, Arena* a) { //:printStackNode
    Int indent = 0;
    Stackint32_t* sentinels = createStackint32_t(16, a);
    for (int i = 0; i < st->len; i++) {
        Node nod = st->cont[i];
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
            printf("Call %d type = \n", nod.pl1);
            //printType(cm->entities.cont[nod.pl1].typeId, cm);
        } else if (nod.pl1 != 0 || nod.pl2 != 0) {
            if (nod.pl3 != 0)  {
                printf("%s %d %d %d\n", nodeNames[nod.tp], nod.pl1, nod.pl2, nod.pl3);
            } else {
                printf("%s %d %d [%d; %d]\n", nodeNames[nod.tp], nod.pl1, nod.pl2);
            }
        } else {
            printf("%s\n", nodeNames[nod.tp]);
        }
        if (nod.tp >= nodScope && nod.pl2 > 0) {
            pushint32_t(i + nod.pl2 + 1, sentinels);
            indent++;
        }
    }
}

//}}}
//{{{ Types testing

private void tPrintGenElt(Int v) { //:tPrintGenElt
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

typedef struct {
    TypeId currPos;
    TypeId sentinel;
} TypeLoc;

DEFINE_STACK_HEADER(TypeLoc)
DEFINE_STACK(TypeLoc)

void tPrintOuter(TypeId currT, Compiler* cm) { //:tPrintOuter
// Print the name of the outer type. `[Tu Int Double]` -> `Tu`
    Arr(Int) t = cm->types.cont;
    TypeHeader currHdr = typeReadHeader(currT, cm);
    if (currHdr.sort == sorFunction) {
        printf("[F ");
    } else if (currHdr.sort == sorTypeCall) {
//~        print("TypeCall print %d", currT)
//~        printIntArrayOff(currT, t[currT] + 1, t);
        const TypeId genericTypeId = currHdr.nameAndLen;
        const TypeHeader genericHdr = typeReadHeader(genericTypeId, cm);
        printf("[");
        printNameAndLen(genericHdr.nameAndLen, cm);
        printf(" ");
    } else {
        printName(t[currT + TYPE_PREFIX_LEN], cm);
    }
}


void tPrint(Int typeId, Compiler* cm) { //:tPrint
    //printf("Printing the type [ind = %d, len = %d]\n", typeId, cm->types.cont[typeId]);
    printIntArrayOff(typeId, 6, cm->types.cont);

    StackTypeLoc* st = createStackTypeLoc(16, cm->aTmp);
    TypeLoc* top = nullptr;

    Int sentinel = typeId + cm->types.cont[typeId] + 1;
    tPrintOuter(typeId, cm);
    pushTypeLoc(((TypeLoc){ .currPos = typeId + TYPE_PREFIX_LEN, .sentinel = sentinel }), st);
    top = st->cont;

    Int countIters = 0;
    while (top != nullptr && countIters < 10)  {
        Int currT = cm->types.cont[top->currPos];
    //    print("printLoop currT %d currSentinel = %d", currT, top->sentinel)
        top->currPos += 1;
        if (currT > -1) {
            if (currT <= topVerbatimType)  {
                printf("%s ", currT != tokMisc ? nodeNames[currT] : "Void");
            } else {
                Int newSentinel = currT + cm->types.cont[currT] + 1;
                pushTypeLoc(
                    ((TypeLoc){ .currPos = currT + TYPE_PREFIX_LEN, .sentinel = newSentinel}), st);
                top = st->cont + (st->len - 1);
                tPrintOuter(currT, cm);
            }
        } else { // type param
            printf("%d ", -currT - 1);
        }
        while (top != nullptr && top->currPos == top->sentinel) {
            popTypeLoc(st);
            top = hasValuesTypeLoc(st) ? st->cont + (st->len - 1) : nullptr;
            printf("]");
        }

        countIters += 1;
    }
    printf("\n");
   /*
    if (hdr.sort == sorRecord) {
        printf("[Record]");
    } else if (hdr.sort == sorEnum) {
        printf("[Enum]");
    } else if (hdr.sort == sorFunction) {
        printf("[Fn]");
    } else if (hdr.sort == sorTypeCall) {
        printf("[TypeCall]");
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

    if (hdr.sort == sorRecord) {
        if (hdr.tyrity > 0) {
            printf("[Params: ");
            for (Int j = 0; j < hdr.tyrity; j++) {
                printf("%d ", cm->types.cont[j + typeId + 3]);
            }
            printf("]");
        }
        printf("(");
        Int fieldsStart = typeId + hdr.tyrity + 3;
        Int fieldsSentinel = fieldsStart + hdr.arity;
        for (Int j = fieldsStart; j < fieldsSentinel; j++) {
            printf("name %d ", cm->types.cont[j]);
        }
        fieldsStart += hdr.arity;
        fieldsSentinel += hdr.arity;
        for (Int j = fieldsStart; j < fieldsSentinel; j++) {
            tPrintGenElt(cm->types.cont[j]);
        }

        printf("\n)");
    } else if (hdr.sort == sorFunction) {
        if (hdr.tyrity > 0) {
            printf("[Params: ");
            for (Int j = 0; j < hdr.tyrity; j++) {
                printf("%d ", cm->types.cont[j + typeId + 3]);
            }
            printf("]");
        }
        printf("(");
        Int fnParamsStart = typeId + hdr.tyrity + 3;
        Int fnParamsSentinel = fnParamsStart + hdr.arity - 1;
        for (Int j = fnParamsStart; j < fnParamsSentinel; j++) {
            tPrintGenElt(cm->types.cont[j]);
        }
        // now for the return type
        printf("=> ");
        tPrintGenElt(cm->types.cont[fnParamsSentinel]);

        printf("\n");

    } else if (hdr.sort == sorTypeCall) {
        printf("(\n    ");
        for (Int j = typeId + hdr.tyrity + 3; j < sentinel; j++) {
            tPrintGenElt(cm->types.cont[j]);
        }
        printf("\n");
    }
    print(")");
    */
}

void tPrintTypeFrames(StackTypeFrame* st) { //:tPrintTypeFrames
    print(">>> Type frames cnt %d", st->len);
    for (Int j = 0; j < st->len; j += 1) {
        TypeFrame fr = st->cont[j];
        if (fr.tp == sorFunction) {
            printf("Func ");
        } else if (fr.tp == sorTypeCall) {
            printf("TypeCall ");
        }
        printf("countArgs %d ", fr.countArgs);
        printf("sent %d \n", fr.sentinel);
    }
    printf(">>>\n\n");
}


void printOverloads(Int nameId, Compiler* cm) { //:printOverloads
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
