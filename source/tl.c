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
    firstChunk->next = NULL;

    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->currInd = 0;

    return result;
}

private size_t calculateChunkSize(size_t allocSize) {
    /// Calculates memory for a new chunk. Memory is quantized and is always 32 bytes less
    /// 32 for any possible padding malloc might use internally,
    /// so that the total allocation size is a good even number of OS memory pages.
    size_t fullMemory = sizeof(ArenaChunk) + allocSize + 32; // struct header + main memory chunk + space for malloc bookkeep
    int mallocMemory = fullMemory < CHUNK_QUANT
                        ? CHUNK_QUANT
                        : (fullMemory % CHUNK_QUANT > 0
                           ? (fullMemory/CHUNK_QUANT + 1)*CHUNK_QUANT
                           : fullMemory);

    return mallocMemory - 32;
}

testable void* allocateOnArena(size_t allocSize, Arena* ar) {
    /// Allocate memory in the arena, malloc'ing a new chunk if needed
    if (ar->currInd + allocSize >= ar->currChunk->size) {
        size_t newSize = calculateChunkSize(allocSize);

        ArenaChunk* newChunk = malloc(newSize);
        if (!newChunk) {
            perror("malloc error when allocating arena chunk");
            exit(EXIT_FAILURE);
        };
        // sizeof counts everything but the flexible array member, that's why we subtract it
        newChunk->size = newSize - sizeof(ArenaChunk);
        newChunk->next = NULL;
        //printf("Allocated a new chunk with bookkeep size %zu, array size %zu\n", sizeof(ArenaChunk), newChunk->size);

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
    while (curr != NULL) {
        //printf("freeing a chunk of size %zu\n", curr->size);
        nextToFree = curr->next;
        free(curr);
        curr = nextToFree;
    }
    //printf("freeing arena itself\n");
    free(ar);
}

//}}}
//{{{Stack
#define DEFINE_STACK(T)                                                             \
    testable Stack##T * createStack##T (int initCapacity, Arena* a) {               \
        int capacity = initCapacity < 4 ? 4 : initCapacity;                         \
        Stack##T * result = allocateOnArena(sizeof(Stack##T), a);                   \
        result->capacity = capacity;                                                \
        result->length = 0;                                                         \
        result->arena = a;                                                          \
        T* arr = allocateOnArena(capacity*sizeof(T), a);                            \
        result->content = arr;                                                      \
        return result;                                                              \
    }                                                                               \
    testable bool hasValues ## T (Stack ## T * st) {                                \
        return st->length > 0;                                                      \
    }                                                                               \
    testable T pop##T (Stack ## T * st) {                                           \
        st->length--;                                                               \
        return st->content[st->length];                                             \
    }                                                                               \
    testable T peek##T(Stack##T * st) {                                             \
        return st->content[st->length - 1];                                         \
    }                                                                               \
    testable T penultimate##T(Stack##T * st) {                                      \
        return st->content[st->length - 2];                                         \
    }                                                                               \
    testable void push##T (T newItem, Stack ## T * st) {                            \
        if (st->length < st->capacity) {                                            \
            memcpy((T*)(st->content) + (st->length), &newItem, sizeof(T));          \
        } else {                                                                    \
            T* newContent = allocateOnArena(2*(st->capacity)*sizeof(T), st->arena); \
            memcpy(newContent, st->content, st->length*sizeof(T));                  \
            memcpy((T*)(newContent) + (st->length), &newItem, sizeof(T));           \
            st->capacity *= 2;                                                      \
            st->content = newContent;                                               \
        }                                                                           \
        st->length++;                                                               \
    }                                                                               \
    testable void clear##T (Stack##T * st) {                                        \
        st->length = 0;                                                             \
    }
//}}}
//{{{ Internal list

#define DEFINE_INTERNAL_LIST_CONSTRUCTOR(T)                 \
testable InList##T createInList##T(Int initCap, Arena* a) { \
    return (InList##T){                                     \
        .content = allocateOnArena(initCap*sizeof(T), a),   \
        .length = 0, .capacity = initCap };                 \
}

#define DEFINE_INTERNAL_LIST(fieldName, T, aName)               \
    testable bool hasValuesIn##fieldName(Compiler* cm) {        \
        return cm->fieldName.length > 0;                        \
    }                                                           \
    testable T popIn##fieldName(Compiler* cm) {                 \
        cm->fieldName.length--;                                 \
        return cm->fieldName.content[cm->fieldName.length];     \
    }                                                           \
    testable T peekIn##fieldName(Compiler* cm) {                \
        return cm->fieldName.content[cm->fieldName.length - 1]; \
    }                                                           \
    testable void pushIn##fieldName(T newItem, Compiler* cm) {  \
        if (cm->fieldName.length < cm->fieldName.capacity) {    \
            memcpy((T*)(cm->fieldName.content) + (cm->fieldName.length), &newItem, sizeof(T)); \
        } else {                                                                               \
            T* newContent = allocateOnArena(2*(cm->fieldName.capacity)*sizeof(T), cm->aName);  \
            memcpy(newContent, cm->fieldName.content, cm->fieldName.length*sizeof(T));         \
            memcpy((T*)(newContent) + (cm->fieldName.length), &newItem, sizeof(T));            \
            cm->fieldName.capacity *= 2;                                                       \
            cm->fieldName.content = newContent;                                                \
        }                                                                                      \
        cm->fieldName.length++;                                                                \
    }
//}}}
//{{{ MultiList

MultiList* createMultiList(Arena* a) {
    MultiList* ml = allocateOnArena(sizeof(MultiList), a);
    Arr(Int) content = allocateOnArena(12*4, a);
    (*ml) = (MultiList) {
        .len = 0,
        .cap = 12,
        .content = content,
        .freeList = -1,
        .a = a,
    };
    return ml;
}

private Int multiListFindFree(Int neededCap, MultiList* ml) {
    Int freeInd = ml->freeList;
    Int prevFreeInd = -1;
    Int freeStep = 0;
    while (freeInd > -1 && freeStep < 10) {
        Int freeCap = ml->content[freeInd + 1];
        if (freeCap == neededCap) {
            print("found freeInd %d prevFree %d step %d", freeInd, prevFreeInd, freeStep)
            if (prevFreeInd > -1) {
                ml->content[prevFreeInd] = ml->content[freeInd]; // remove this node from the free list
            } else {
                ml->freeList = -1;
            }
            return freeInd;
        }
        prevFreeInd = freeInd;
        freeInd = ml->content[freeInd];
        ++freeStep;
    }
    return -1;
}


private void multiListReallocToEnd(Int listInd, Int listLen, Int neededCap, MultiList* ml) {
    ml->content[ml->len] = listLen;
    ml->content[ml->len + 1] = neededCap;
    memcpy(ml->content + ml->len + 2, ml->content + listInd + 2, listLen*4);
    ml->len += neededCap + 2;
}


private void multiListDoubleSize(MultiList* ml) {
    Int newMultiCap = ml->cap*2;
    Arr(Int) newAlloc = allocateOnArena(newMultiCap*4, ml->a);
    memcpy(newAlloc, ml->content, ml->len);
    ml->content = newAlloc;
}

testable Int addMultiList(Int newKey, Int newVal, Int listInd, MultiList* ml) {
    /// Add a new key-value pair to a particular list within the MultiList. Returns the new index for this
    /// list in case it had to be reallocated, -1 if not.
    /// Throws an exception if key already exists
    Int listLen = ml->content[listInd];
    Int listCap = ml->content[listInd + 1];
    ml->content[listInd + listLen + 2] = newKey;
    ml->content[listInd + listLen + 3] = newVal;
    listLen += 2;
    Int newListInd = -1;
    if (listLen == listCap) { // look in the freelist, but not more than 10 steps to save time
        Int neededCap = listCap*2;
        Int freeInd = multiListFindFree(neededCap, ml);
        if (freeInd > -1) {
            ml->content[freeInd] = listLen;
            memcpy(ml->content + freeInd + 2, ml->content + listInd + 2, listLen);
            newListInd = freeInd;
        } else if (ml->len + neededCap + 2 < ml->cap) {
            newListInd = ml->len;
            multiListReallocToEnd(listInd, listLen, neededCap, ml);
        } else {
            newListInd = ml->len;
            multiListDoubleSize(ml);
            multiListReallocToEnd(listInd, listLen, neededCap, ml);
        }

        // add this freed sector to the freelist
        ml->content[listInd] = ml->freeList;
        ml->freeList = listInd;
    } else {
        ml->content[listInd] = listLen;
    }
    return newListInd;
}

testable Int listAddMultiList(Int newKey, Int newVal, MultiList* ml) {
    /// Adds a new list to the MultiList and populates it with one key-value pair. Returns its index.
    Int initCap = 8;
    Int newInd = multiListFindFree(initCap, ml);
    if (newInd == -1) {
        if (ml->len + initCap + 2 >= ml->cap) {
             multiListDoubleSize(ml);
        }
        newInd = ml->len;
        ml->len += (initCap + 2);
    }

    ml->content[newInd] = 2;
    ml->content[newInd + 1] = initCap;
    ml->content[newInd + 2] = newKey;
    ml->content[newInd + 3] = newVal;
    return newInd;
}

testable Int searchMultiList(Int searchKey, Int listInd, MultiList* ml) {
    /// Search for a key in a particular list within the MultiList. Returns the value if found,
    /// -1 otherwise
    Int len = ml->content[listInd]/2;
    Int endInd = listInd + 2 + len;
    for (Int j = listInd + 2; j < endInd; j++) {
        if (ml->content[j] == searchKey) {
            return ml->content[j + len];
        }
    }
    return -1;
}

//}}}
//{{{ Datatypes a la carte

DEFINE_STACK(int32_t)
DEFINE_STACK(BtToken)
DEFINE_STACK(ParseFrame)

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
DEFINE_INTERNAL_LIST(imports, Int, a)
DEFINE_INTERNAL_LIST(overloads, Int, a)
DEFINE_INTERNAL_LIST(types, Int, a)

//}}}
//{{{ Strings

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
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

testable String* str(const char* content, Arena* a) {
    /// Allocates a C string literal into an arena. The length of the literal is determined in O(N).
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

testable bool endsWith(String* a, String* b) {
    /// Does string "a" end with string "b"?
    if (a->length < b->length) {
        return false;
    } else if (b->length == 0) {
        return true;
    }

    int shift = a->length - b->length;
    int cmpResult = memcmp(a->content + shift, b->content, b->length);
    return cmpResult == 0;
}


private bool equal(String* a, String* b) {
    if (a->length != b->length) {
        return false;
    }

    int cmpResult = memcmp(a->content, b->content, b->length);
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
    result->length = stringLen;
    sprintf(result->content, "%d", i);
    return result;
}

testable void printString(String* s) {
    if (s->length == 0) return;
    fwrite(s->content, 1, s->length, stdout);
    printf("\n");
}


testable void printStringNoLn(String* s) {
    if (s->length == 0) return;
    fwrite(s->content, 1, s->length, stdout);
}

private bool isLetter(byte a) {
    return ((a >= aALower && a <= aZLower) || (a >= aAUpper && a <= aZUpper));
}

private bool isCapitalLetter(byte a) {
    return a >= aAUpper && a <= aZUpper;
}

private bool isLowercaseLetter(byte a) {
    return a >= aALower && a <= aZLower;
}

private bool isDigit(byte a) {
    return a >= aDigit0 && a <= aDigit9;
}

private bool isAlphanumeric(byte a) {
    return isLetter(a) || isDigit(a);
}

private bool isHexDigit(byte a) {
    return isDigit(a) || (a >= aALower && a <= aFLower) || (a >= aAUpper && a <= aFUpper);
}

private bool isSpace(byte a) {
    return a == aSpace || a == aNewline || a == aCarrReturn;
}


String empty = { .length = 0 };
//}}}
//{{{ Int Hashmap

testable IntMap* createIntMap(int initSize, Arena* a) {
    IntMap* result = allocateOnArena(sizeof(IntMap), a);
    int realInitSize = (initSize >= 4 && initSize < 1024) ? initSize : (initSize >= 4 ? 1024 : 4);
    Arr(int*) dict = allocateOnArena(sizeof(int*)*realInitSize, a);

    result->a = a;

    int** d = dict;

    for (int i = 0; i < realInitSize; i++) {
        d[i] = NULL;
    }
    result->dictSize = realInitSize;
    result->dict = dict;

    return result;
}


testable void addIntMap(int key, int value, IntMap* hm) {
    if (key < 0) return;

    int hash = key % (hm->dictSize);
    if (*(hm->dict + hash) == NULL) {
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
    if (hm->dict[hash] == NULL) { return false; }
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
    if (*(hm->dict + hash) == NULL) {
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
    /// Throws an exception when key is absent
    int hash = key % (hm->dictSize);
    if (*(hm->dict + hash) == NULL) {
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
        d[i] = NULL;
    }
    result->dictSize = realInitSize;
    result->dict = dict;

    return result;
}


private untt hashCode(byte* start, Int len) {
    untt result = 5381;
    byte* p = start;
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
        *(p->content + lenBucket) = (StringValue){.hash = hash, .indString = newIndString};
        ++(p->capAndLen);
        if (hash == 177649) {
        }
    } else {
        // TODO handle the case when we're overflowing the 16 bits of capacity
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + 2*capacity*sizeof(StringValue), a);
        memcpy(newBucket->content, p->content, capacity*sizeof(StringValue));

        Arr(StringValue) newValues = (StringValue*)newBucket->content;
        newValues[capacity] = (StringValue){.indString = newIndString, .hash = hash};
        *ptrToBucket = newBucket;
        newBucket->capAndLen = ((2*capacity) << 16) + capacity + 1;
    }
}

private Int addStringDict(byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable, StringDict* hm) {
    /// Unique'ing of symbols within source code
    untt hash = hashCode(text + startBt, lenBts);
    Int hashOffset = hash % (hm->dictSize);
    Int newIndString;
    Bucket* bu = *(hm->dict + hashOffset);

    if (bu == NULL) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;

        newIndString = stringTable->length;
        push(startBt, stringTable);

        *firstElem = (StringValue){.hash = hash, .indString = newIndString };
        *(hm->dict + hashOffset) = newBucket;
    } else {
        int lenBucket = (bu->capAndLen & 0xFFFF);
        for (int i = 0; i < lenBucket; i++) {
            StringValue strVal = bu->content[i];
            if (strVal.hash == hash &&
                  memcmp(text + stringTable->content[strVal.indString], text + startBt, lenBts) == 0) {
                // key already present
                return strVal.indString;
            }
        }

        newIndString = stringTable->length;
        push(startBt, stringTable);
        addValueToBucket(hm->dict + hashOffset, newIndString, hash, hm->a);
    }
    return newIndString;
}

testable Int getStringDict(byte* text, String* strToSearch, Stackint32_t* stringTable, StringDict* hm) {
    /// Returns the index of a string within the string table, or -1 if it's not present
    Int lenBts = strToSearch->length;
    untt hash = hashCode(strToSearch->content, lenBts);
    Int hashOffset = hash % (hm->dictSize);
    if (*(hm->dict + hashOffset) == NULL) {
        return -1;
    } else {
        Bucket* p = *(hm->dict + hashOffset);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].hash == hash
                && memcmp(strToSearch->content, text + stringTable->content[stringValues[i].indString], lenBts) == 0) {
                return stringValues[i].indString;
            }
        }

        return -1;
    }
}

//}}}
//{{{ Algorithms

testable void sortPairsDisjoint(Int startInd, Int endInd, Arr(Int) arr) {
    /// Performs a "twin" ASC sort for faraway (Struct-of-arrays) pairs: for every swap of keys, the
    /// same swap on values is also performed.
    /// Example of such a swap: [type1 type2 type3 entity1 entity2 entity3] ->
    ///                         [type3 type1 type2 entity3 entity1 entity2]
    /// Sorts only the concrete group of overloads, doesn't touch the generic part.
    /// Params: startInd = inclusive
    ///         endInd = exclusive
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

testable void sortPairs(Int startInd, Int endInd, Arr(Int) arr) {
    /// Performs an ASC sort for compact pairs (array-of-structs) by key:
    /// Params: startInd = the first index of the overload (the one with the count of concrete overloads)
    ///         endInd = the last index belonging to the overload (the one with the last entityId)
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
    /// For disjoint (struct-of-arrays) pairs, makes sure the keys are unique and sorted ascending
    /// Params: startInd = inclusive
    ///         endInd = exclusive
    /// Returns: true iff all's OK
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

//}}}
//}}}
//{{{ Errors

//{{{ Internal errors

#define iErrorInconsistentSpans          1 // Inconsistent span length / structure of token scopes!;
#define iErrorImportedFunctionNotInScope 2 // There is a -1 or something else in the activeBindings table for an imported function
#define iErrorParsedFunctionNotInScope   3 // There is a -1 or something else in the activeBindings table for a parsed function
#define iErrorOverloadsOverflow          4 // There were more overloads for a function than what was allocated
#define iErrorOverloadsNotFull           5 // There were fewer overloads for a function than what was allocated
#define iErrorOverloadsIncoherent        6 // The overloads table is incoherent
#define iErrorExpressionIsNotAnExpr      7 // What is supposed to be an expression in the AST is not a nodExpr
#define iErrorZeroArityFuncWrongEmit     8 // A 0-arity function has a wrong "emit" (should be one of the prefix ones)
#define iErrorGenericTypesInconsistent   9 // Two generic types have inconsistent layout (premature end of type)
#define iErrorGenericTypesParamOutOfBou 10 // A type contains a paramId that is out-of-bounds of the generic's arity

//}}}
//{{{ Syntax errors
const char errNonAscii[]                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!";
const char errPrematureEndOfInput[]        = "Premature end of input";
const char errUnrecognizedByte[]           = "Unrecognized byte in source code!";
const char errWordChunkStart[]             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!";
const char errWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!";
const char errWordUnderscoresOnlyAtStart[] = "Underscores are only allowed at start of word (snake case is forbidden)!";
const char errWordWrongAccessor[]          = "Only regular identifier words may be used for data access with []!";
const char errWordLengthExceeded[]         = "I don't know why you want an identifier of more than 255 chars, but they aren't supported";
const char errWordReservedWithDot[]        = "Reserved words may not be called like functions!";
const char errNumericEndUnderscore[]       = "Numeric literal cannot end with underscore!";
const char errNumericWidthExceeded[]       = "Numeric literal width is exceeded!";
const char errNumericBinWidthExceeded[]    = "Integer literals cannot exceed 64 bit!";
const char errNumericFloatWidthExceeded[]  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!";
const char errNumericEmpty[]               = "Could not lex a numeric literal, empty sequence!";
const char errNumericMultipleDots[]        = "Multiple dots in numeric literals are not allowed!";
const char errNumericIntWidthExceeded[]    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!";
const char errPunctuationExtraOpening[]    = "Extra opening punctuation";
const char errPunctuationExtraClosing[]    = "Extra closing punctuation";
const char errPunctuationOnlyInMultiline[] = "The dot separator is not allowed inside expressions!";
const char errPunctuationUnmatched[]       = "Unmatched closing punctuation";
const char errPunctuationWrongCall[]       = "Wrong call syntax: this opening paren doesn't belong here";
const char errPunctuationScope[]           = "Scopes may only be opened in multi-line syntax forms";
const char errOperatorUnknown[]            = "Unknown operator";
const char errOperatorAssignmentPunct[]    = "Incorrect assignment operator: must be directly inside an ordinary statement, after the binding name!";
const char errToplevelAssignment[]         = "Toplevel assignments must have only single word on the left!";
const char errOperatorTypeDeclPunct[]      = "Incorrect type declaration operator placement: must be the first in a statement!";
const char errOperatorMultipleAssignment[] = "Multiple assignment / type declaration operators within one statement are not allowed!";
const char errCoreNotInsideStmt[]          = "Core form must be directly inside statement";
const char errCoreMisplacedColon[]         = "The colon separator (:) must be inside an if, ifEq, ifPr or match form";
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
const char errFnNameAndParams[]            = "Function signature must look like this: `(:f fnName ReturnType(x Type1 y Type2). body...)`";
const char errFnMissingBody[]              = "Function definition must contain a body which must be a Scope immediately following its parameter list!";
const char errLoopSyntaxError[]            = "A loop should look like `(:loop (< x 101) x = 0. loopBody)`";
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
const char errAssignmentAccessOnToplevel[]  = "Accessor on the left side of an assignment at toplevel";
const char errReturn[]                      = "Cannot parse return statement, it must look like `return ` {expression}";
const char errScope[]                       = "A scope may consist only of expressions, assignments, function definitions and other scopes!";
const char errLoopBreakOutside[]            = "The break keyword can only be used inside a loop scope!";
const char errTemp[]                        = "Temporary, delete it when finished";

//}}}
//{{{ Type errors

const char errUnknownType[]                 = "Unknown type";
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
//{{{ Lexer

//{{{ LexerConstants

/// The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
const byte maxInt[19] = (byte []){
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
};

/// 2**53
const byte maximumPreciselyRepresentedFloatingInt[16] = (byte []){ 9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2 };

/// The indices of reserved words that are stored in token pl2. Must be positive, unique,
/// and below "firstPunctuationTokenType"
#define reservedFalse   2
#define reservedTrue    1
#define reservedAnd     3
#define reservedOr      4


/// Symbols an operator may start with. "-" is absent because it's handled by lexMinus, "=" - by lexEqual
const int operatorStartSymbols[15] = {
    aExclamation, aSharp, aPercent, aAmp, aApostrophe, aTimes, aPlus, aComma, aDivBy,
    aSemicolon, aLT, aGT, aQuestion, aCaret, aPipe
};

///  The standard text prepended to all source code inputs and the hash table to provide a built-in string set.
///  The Tl reserved words must be at the start and be sorted alphabetically on the first letter.
const char standardText[] = "aliasandassertawaitbreakcatchcontinuedeferdoelseembedfalsefnfor"
                            "ififPrimplimportinterfacematchorreturntruetrywhileyield"
                            // reserved words end here
                            "IntLongDoubleBoolStringVoidFLATulencapf1f2TUprintalertmath-pimath-e";

const Int standardStrings[] = {
      0,   5,   8,  14,  19, // break
     24,  29,  37,  42,  44, // else
     48,  53,  58,  60,  63, // if
     65,  69,  73,  79,  88, // match
     93,  95, 101, 105, 108, // while
    113,                     // yield
    118, 121, 125, 131, 135, // String
    141, 145, 146, 147, 148, // Tu
    150, 153, 156, 158, 160, // print
    165, 170, 177, 183
};

const Int standardToks[] = {
    tokAlias,  reservedAnd, tokAssert, tokAwait, tokBreak,
    tokCatch,  tokContinue, tokDefer,  tokScope, tokElse,
    tokEmbed,  reservedFalse, tokFn,   tokFor, tokIf,
    tokIfPr,   tokImpl,     tokImport,  tokIface, tokMatch,
    reservedOr, tokReturn,  reservedTrue, tokTry,
    tokWhile, tokYield
};

#define maxWordLength 255
//}}}
//{{{ LexerUtils

#define CURR_BT source[lx->i]
#define NEXT_BT source[lx->i + 1]
#define VALIDATEI(cond, errInd) if (!(cond)) { throwExcInternal0(errInd, __LINE__, cm); }
#define VALIDATEL(cond, errMsg) if (!(cond)) { throwExcLexer0(errMsg, __LINE__, lx); }


typedef union {
    uint64_t i;
    double   d;
} FloatingBits;


private String* readSourceFile(const Arr(char) fName, Arena* a) {
    String* result = NULL;
    FILE *file = fopen(fName, "r");
    if (file == NULL) {
        goto cleanup;
    }
    /* Go to the end of the file. */
    if (fseek(file, 0L, SEEK_END) != 0) {
        goto cleanup;
    }
    long fileSize = ftell(file);
    if (fileSize == -1) {
        goto cleanup;
    }
    Int lenStandard = sizeof(standardText);
    // Allocate our buffer to that size, with space for the standard text in front of it.
    result = allocateOnArena(lenStandard + fileSize + 1 + sizeof(String), a);

    // Go back to the start of the file.
    if (fseek(file, 0L, SEEK_SET) != 0) {
        goto cleanup;
    }

    memcpy(result->content, standardText, lenStandard);
    // Read the entire file into memory.
    size_t newLen = fread(result->content + lenStandard, 1, fileSize, file);
    if (ferror(file) != 0 ) {
        fputs("Error reading file", stderr);
    } else {
        result->content[newLen] = '\0'; // Just to be safe.
    }
    result->length = newLen;
    cleanup:
    fclose(file);
    return result;
}

private untt nameOfToken(Token tk) {
    return ((untt)tk.lenBts << 24) + (untt)tk.pl2;
}


private untt nameOfStandard(Int strId) {
    Int len = standardStrings[strId + 1] - standardStrings[strId];
    Int nameId = standardStrings[strId] - strFirstNonReserved;
    return ((untt)len << 24) + (untt)(nameId);
}

//}}}
//{{{ Lexer proper
_Noreturn private void throwExcInternal0(Int errInd, Int lineNumber, Compiler* cm) {
    cm->wasError = true;
    printf("Internal error %d at %d\n", errInd, lineNumber);
    cm->errMsg = stringOfInt(errInd, cm->a);
    printString(cm->errMsg);
    longjmp(excBuf, 1);
}

#define throwExcInternal(errInd, cm) throwExcInternal0(errInd, __LINE__, cm)


_Noreturn private void throwExcLexer0(const char errMsg[], Int lineNumber, Compiler* lx) {
    /// Sets i to beyond input's length to communicate to callers that lexing is over
    lx->wasError = true;
#ifdef TEST
    printf("Error on code line %d, i = %d: %s\n", lineNumber, lx->i, errMsg);
#endif
    lx->errMsg = str(errMsg, lx->a);
    longjmp(excBuf, 1);
}
#define throwExcLexer(msg, lx) throwExcLexer0(msg, __LINE__, lx)

private void checkPrematureEnd(Int requiredSymbols, Compiler* lx) {
    /// Checks that there are at least 'requiredSymbols' symbols left in the input.
    VALIDATEL(lx->i + requiredSymbols <= lx->inpLength, errPrematureEndOfInput)
}

private void closeColons(Compiler* lx) {
    /// For all the dollars at the top of the backtrack, turns them into parentheses, sets their lengths
    /// and closes them
    while (hasValues(lx->lexBtrack) && peek(lx->lexBtrack).wasOrigDollar) {
        BtToken top = pop(lx->lexBtrack);
        Int j = top.tokenInd;
        lx->tokens.content[j].lenBts = lx->i - lx->tokens.content[j].startBt;
        lx->tokens.content[j].pl2 = lx->tokens.length - j - 1;
    }
}

private void setSpanLengthLexer(Int tokenInd, Compiler* lx) {
    /// Finds the top-level punctuation opener by its index, and sets its lengths.
    /// Called when the matching closer is lexed.
    lx->tokens.content[tokenInd].lenBts = lx->i - lx->tokens.content[tokenInd].startBt + 1;
    lx->tokens.content[tokenInd].pl2 = lx->tokens.length - tokenInd - 1;
}

private void setStmtSpanLength(Int topTokenInd, Compiler* lx) {
    /// Correctly calculates the lenBts for a single-line, statement-type span.
    Token lastToken = lx->tokens.content[lx->tokens.length - 1];
    Int byteAfterLastToken = lastToken.startBt + lastToken.lenBts;

    // This is for correctly calculating lengths of statements when they are ended by parens in case of a gap before ")"
    Int byteAfterLastPunct = lx->lastClosingPunctInd + 1;
    Int lenBts = (byteAfterLastPunct > byteAfterLastToken ? byteAfterLastPunct : byteAfterLastToken)
                    - lx->tokens.content[topTokenInd].startBt;

    lx->tokens.content[topTokenInd].lenBts = lenBts;
    lx->tokens.content[topTokenInd].pl2 = lx->tokens.length - topTokenInd - 1;
}


private Int probeReservedWords(Int indStart, Int indEnd, Int startBt, Int lenBts, Compiler* lx) {
    for (Int i = indStart; i < indEnd; i++) {
        Int len = standardStrings[i + 1] - standardStrings[i];
        if (len == lenBts &&
            memcmp(lx->sourceCode->content + standardStrings[i],
                   lx->sourceCode->content + startBt, len) == 0) {
            return standardToks[i];
        }
    }
    return -1;
}

private Int determineReservedA(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strAlias, strBreak, startBt, lenBts, lx);
}

private Int determineReservedB(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strBreak, strCatch, startBt, lenBts, lx);
}

private Int determineReservedC(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strCatch, strDefer, startBt, lenBts, lx);
}

private Int determineReservedD(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strDefer, strElse, startBt, lenBts, lx);
}

private Int determineReservedE(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strElse, strFalse, startBt, lenBts, lx);
}

private Int determineReservedF(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strFalse, strIf, startBt, lenBts, lx);
}

private Int determineReservedI(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strIf, strMatch, startBt, lenBts, lx);
}

private Int determineReservedM(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strMatch, strOr, startBt, lenBts, lx);
}

private Int determineReservedO(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strOr, strReturn, startBt, lenBts, lx);
}

private Int determineReservedR(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strReturn, strTrue, startBt, lenBts, lx);
}

private Int determineReservedT(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strTrue, strWhile, startBt, lenBts, lx);
}

private Int determineReservedW(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strWhile, strYield, startBt, lenBts, lx);
}

private Int determineReservedY(Int startBt, Int lenBts, Compiler* lx) {
    return probeReservedWords(strYield, strFirstNonReserved, startBt, lenBts, lx);
}

private Int determineUnreserved(Int startBt, Int lenBts, Compiler* lx) {
    return -1;
}


private void addStatementSpan(untt stmtType, Int startBt, Compiler* lx) {
    Int lenBts = 0;
    // Some types of statements may legitimately consist of 0 tokens; for them, we need to write their lenBts in the init token
    if (stmtType == tokBreak) {
        lenBts = 5;
    } else if (stmtType == tokContinue) {
        lenBts = 8;
    }
    push(((BtToken){ .tp = stmtType, .tokenInd = lx->tokens.length, .spanLevel = slStmt }), lx->lexBtrack);
    pushIntokens((Token){ .tp = stmtType, .startBt = startBt, .lenBts = lenBts}, lx);
}


private void wrapInAStatementStarting(Int startBt, Compiler* lx, Arr(byte) source) {
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel <= slParenMulti) {
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->tokens.length, .spanLevel = slStmt }), lx->lexBtrack);
            pushIntokens((Token){ .tp = tokStmt, .startBt = startBt},  lx);
        }
    } else {
        addStatementSpan(tokStmt, startBt, lx);
    }
}


private void wrapInAStatement(Compiler* lx, Arr(byte) source) {
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel <= slParenMulti) {
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->tokens.length, .spanLevel = slStmt }), lx->lexBtrack);
            pushIntokens((Token){ .tp = tokStmt, .startBt = lx->i},  lx);
        }
    } else {
        addStatementSpan(tokStmt, lx->i, lx);
    }
}

private void maybeBreakStatement(Compiler* lx) {
    /// If the lexer is in a statement, we need to close it
    if (hasValues(lx->lexBtrack)) {
        BtToken top = peek(lx->lexBtrack);
        if(top.spanLevel == slStmt) {
            setStmtSpanLength(top.tokenInd, lx);
            pop(lx->lexBtrack);
        }
    }
}

private void closeRegularPunctuation(Int closingType, Compiler* lx) {
    /// Processes a right paren which serves as the closer of all punctuation scopes.
    /// This doesn't actually add any tokens to the array, just performs validation and sets the token length
    /// for the opener token.
    StackBtToken* bt = lx->lexBtrack;
    closeColons(lx);
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);

    // since a closing paren might be closing something with statements inside it, like a lex scope
    // or a core syntax form, we need to close the last statement before closing its parent
    if (bt->length > 0 && top.spanLevel != slScope
          && (bt->content[bt->length - 1].spanLevel <= slParenMulti)) {
        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }
    setSpanLengthLexer(top.tokenInd, lx);
    lx->i++; // CONSUME the closing ")"
}


private int64_t calcIntegerWithinLimits(Compiler* lx) {
    int64_t powerOfTen = (int64_t)1;
    int64_t result = 0;
    Int j = lx->numeric.length - 1;

    Int loopLimit = -1;
    while (j > loopLimit) {
        result += powerOfTen*lx->numeric.content[j];
        powerOfTen *= 10;
        j--;
    }
    return result;
}

private bool integerWithinDigits(const byte* b, Int bLength, Compiler* lx){
    /// Checks if the current numeric <= b if they are regarded as arrays of decimal digits (0 to 9).
    if (lx->numeric.length != bLength) return (lx->numeric.length < bLength);
    for (Int j = 0; j < lx->numeric.length; j++) {
        if (lx->numeric.content[j] < b[j]) return true;
        if (lx->numeric.content[j] > b[j]) return false;
    }
    return true;
}


private Int calcInteger(int64_t* result, Compiler* lx) {
    if (lx->numeric.length > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lx)) return -1;
    *result = calcIntegerWithinLimits(lx);
    return 0;
}


private int64_t calcHexNumber(Compiler* lx) {
    int64_t result = 0;
    int64_t powerOfSixteen = 1;
    Int j = lx->numeric.length - 1;

    // If the literal is full 16 bits long, then its upper sign contains the sign bit
    Int loopLimit = -1; //if (byteBuf.ind == 16) { 0 } else { -1 }
    while (j > loopLimit) {
        result += powerOfSixteen*lx->numeric.content[j];
        powerOfSixteen = powerOfSixteen << 4;
        j--;
    }
    return result;
}

private void hexNumber(Compiler* lx, Arr(byte) source) {
    /// Lexes a hexadecimal numeric literal (integer or floating-point).
    /// Examples of accepted expressions: 0xCAFE_BABE, 0xdeadbeef, 0x123_45A
    /// Examples of NOT accepted expressions: 0xCAFE_babe, 0x_deadbeef, 0x123_
    /// Checks that the input fits into a signed 64-bit fixnum.
    /// TODO add floating-pointt literals like 0x12FA.
    checkPrematureEnd(2, lx);
    lx->numeric.length = 0;
    Int j = lx->i + 2;
    while (j < lx->inpLength) {
        byte cByte = source[j];
        if (isDigit(cByte)) {
            pushInnumeric(cByte - aDigit0, lx);
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            pushInnumeric(cByte - aALower + 10, lx);
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            pushInnumeric(cByte - aAUpper + 10, lx);
        } else if (cByte == aUnderscore && (j == lx->inpLength - 1 || isHexDigit(source[j + 1]))) {
            throwExcLexer(errNumericEndUnderscore, lx);
        } else {
            break;
        }
        VALIDATEL(lx->numeric.length <= 16, errNumericBinWidthExceeded)
        j++;
    }
    int64_t resultValue = calcHexNumber(lx);
    pushIntokens((Token){ .tp = tokInt, .pl1 = resultValue >> 32, .pl2 = resultValue & LOWER32BITS,
                .startBt = lx->i, .lenBts = j - lx->i }, lx);
    lx->numeric.content = 0;
    lx->i = j; // CONSUME the hex number
}

private Int calcFloating(double* result, Int powerOfTen, Compiler* lx, Arr(byte) source){
    /// Parses the floating-point numbers using just the "fast path" of David Gay's "strtod" function,
    /// extended to 16 digits.
    /// I.e. it handles only numbers with 15 digits or 16 digits with the first digit not 9,
    /// and decimal powers within [-22; 22]. Parsing the rest of numbers exactly is a huge and pretty
    /// useless effort. Nobody needs these floating literals in text form: they are better input in
    /// binary, or at least text-hex or text-binary.
    /// Input: array of bytes that are digits (without leading zeroes), and the negative power of ten.
    /// So for '0.5' the input would be (5 -1), and for '1.23000' (123000 -5).
    /// Example, for input text '1.23' this function would get the args: ([1 2 3] 1)
    /// Output: a 64-bit floating-pointt number, encoded as a long (same bits)
    Int indTrailingZeroes = lx->numeric.length - 1;
    Int ind = lx->numeric.length;
    while (indTrailingZeroes > -1 && lx->numeric.content[indTrailingZeroes] == 0) {
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

    // Transfer of decimal powers from significand to exponent to make them both fit within their respective limits
    // (10000 -6) -> (1 -2); (10000 -3) -> (10 0)
    Int transfer = (significandNeeds >= exponentNeeds) ? (
             (ind - significandNeeds == 16 && significandNeeds < significantCan && significandNeeds + 1 <= exponentCanAccept) ?
                (significandNeeds + 1) : significandNeeds
        ) : exponentNeeds;
    lx->numeric.length -= transfer;
    Int finalPowerTen = powerOfTen + transfer;

    if (!integerWithinDigits(maximumPreciselyRepresentedFloatingInt, sizeof(maximumPreciselyRepresentedFloatingInt), lx)) {
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

private void decNumber(bool isNegative, Compiler* lx, Arr(byte) source) {
    /// Lexes a decimal numeric literal (integer or floating-point). Adds a token.
    /// TODO: add support for the '1.23E4' format
    Int j = (isNegative) ? (lx->i + 1) : lx->i;
    Int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
    bool metDot = false;
    bool metNonzero = false;
    Int maximumInd = (lx->i + 40 > lx->inpLength) ? (lx->i + 40) : lx->inpLength;
    while (j < maximumInd) {
        byte cByte = source[j];

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
            VALIDATEL(j != (lx->inpLength - 1) && isDigit(source[j + 1]), errNumericEndUnderscore)
        } else if (cByte == aDot) {
            if (j + 1 < maximumInd && !isDigit(source[j + 1])) { // this dot is not decimal - it's a statement closer
                break;
            }

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
        Int errorCode = calcFloating(&resultValue, -digitsAfterDot, lx, source);
        VALIDATEL(errorCode == 0, errNumericFloatWidthExceeded)

        int64_t bitsOfFloat = longOfDoubleBits((isNegative) ? (-resultValue) : resultValue);
        pushIntokens((Token){ .tp = tokFloat, .pl1 = (bitsOfFloat >> 32), .pl2 = (bitsOfFloat & LOWER32BITS),
                    .startBt = lx->i, .lenBts = j - lx->i}, lx);
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


private void lexNumber(Compiler* lx, Arr(byte) source) {
    wrapInAStatement(lx, source);
    byte cByte = CURR_BT;
    if (lx->i == lx->inpLength - 1 && isDigit(cByte)) {
        pushIntokens((Token){ .tp = tokInt, .pl2 = cByte - aDigit0, .startBt = lx->i, .lenBts = 1 }, lx);
        lx->i++; // CONSUME the single-digit number
        return;
    }

    byte nByte = NEXT_BT;
    if (nByte == aXLower) {
        hexNumber(lx, source);
    } else {
        decNumber(false, lx, source);
    }
    lx->numeric.length = 0;
}

private void openPunctuation(untt tType, untt spanLevel, Int startBt, Compiler* lx) {
    /// Adds a token which serves punctuation purposes, i.e. either a ( or  a [
    /// These tokens are used to define the structure, that is, nesting within the AST.
    /// Upon addition, they are saved to the backtracking stack to be updated with their length
    /// once it is known.
    /// Consumes no bytes.
    push(((BtToken){ .tp = tType, .tokenInd = lx->tokens.length, .spanLevel = spanLevel}), lx->lexBtrack);
    pushIntokens((Token) {.tp = tType, .pl1 = (tType < firstParenSpanTokenType) ? 0 : spanLevel,
                          .startBt = startBt }, lx);
}

private void lexReservedWord(untt reservedWordType, Int startBt, Int lenBts,
                             Compiler* lx, Arr(byte) source) {
    /// Lexer action for a paren-type or statement-type reserved word. It turns parentheses into
    /// an slParenMulti core form.
    /// Precondition: we are looking at the character immediately after the keyword
    StackBtToken* bt = lx->lexBtrack;
    if (reservedWordType >= firstParenSpanTokenType) { // the "core(" case
        VALIDATEL(lx->i < lx->inpLength && CURR_BT == aParenLeft, errCoreMissingParen)
        if (hasValues(lx->lexBtrack)) {
            BtToken top = peek(lx->lexBtrack);
            VALIDATEL(top.spanLevel <= slStmt && top.tp != tokStmt, errPunctuationScope)
        }
        Int scopeLevel = reservedWordType == tokScope ? slScope : slParenMulti;
        push(((BtToken){ .tp = reservedWordType, .tokenInd = lx->tokens.length, .spanLevel = scopeLevel }),
             lx->lexBtrack);
        pushIntokens((Token){ .tp = reservedWordType, .pl1 = scopeLevel, .startBt = startBt, .lenBts = lenBts}, lx);
        ++lx->i; // CONSUME the opening "(" of the core form
    } else if (reservedWordType >= firstSpanTokenType) {
        VALIDATEL(!hasValues(bt) || peek(bt).spanLevel == slScope, errCoreNotInsideStmt)
        addStatementSpan(reservedWordType, startBt, lx);
    }
}

private bool wordChunk(Compiler* lx, Arr(byte) source) {
    /// Lexes a single chunk of a word, i.e. the characters between two dots
    /// (or the whole word if there are no dots).
    /// Returns True if the lexed chunk was capitalized
    bool result = false;
    if (CURR_BT == aUnderscore) {
        checkPrematureEnd(2, lx);
        lx->i++; // CONSUME the underscore
    } else {
        checkPrematureEnd(1, lx);
    }

    if (isCapitalLetter(CURR_BT)) {
        result = true;
    } else VALIDATEL(isLowercaseLetter(CURR_BT), errWordChunkStart)
    lx->i++; // CONSUME the first letter of the word

    while (lx->i < lx->inpLength && isAlphanumeric(CURR_BT)) {
        lx->i++; // CONSUME alphanumeric characters
    }
    VALIDATEL(lx->i >= lx->inpLength || CURR_BT != aUnderscore, errWordUnderscoresOnlyAtStart)
    return result;
}

private void closeStatement(Compiler* lx) {
    /// Closes the current statement. Consumes no tokens
    BtToken top = peek(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errPunctuationOnlyInMultiline)
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->lexBtrack);
    }
}

private void wordInternal(untt wordType, Compiler* lx, Arr(byte) source) {
    /// Lexes a word (both reserved and identifier) according to Tl's rules.
    /// Precondition: we are pointing at the first letter character of the word (i.e. past the possible
    /// "@" or ":")
    /// Examples of acceptable expressions: A-B-c-d, asdf123, ab-_cd45
    /// Examples of unacceptable expressions: 1asdf23, ab-cd_45
    Int startBt = lx->i;

    bool wasCapitalized = wordChunk(lx, source);

    while (lx->i < (lx->inpLength - 1)) {
        byte currBt = CURR_BT;
        if (currBt == aMinus) {
            byte nextBt = NEXT_BT;
            if (isLetter(nextBt) || nextBt == aUnderscore) {
                lx->i++; // CONSUME the letter or underscore
                bool isCurrCapitalized = wordChunk(lx, source);
                VALIDATEL(!wasCapitalized, errWordCapitalizationOrder)
                wasCapitalized = isCurrCapitalized;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    Int realStartByte = (wordType == tokWord) ? startBt : (startBt - 1);
    // accounting for the initial ".", ":" or other symbol
    byte firstByte = lx->sourceCode->content[startBt];
    Int lenBts = lx->i - realStartByte;
    Int lenString = lx->i - startBt;
    VALIDATEL(lenString <= maxWordLength, errWordLengthExceeded)

    Int mbReservedWord = -1;
    if (firstByte >= aALower && firstByte <= aYLower) {
        mbReservedWord = (*lx->langDef->possiblyReservedDispatch)[firstByte - aALower](
                startBt, lenString, lx
        );
    }
    if (mbReservedWord == -1) { // a normal, unreserved word
        Int uniqueStringInd = addStringDict(source, startBt, lenString, lx->stringTable, lx->stringDict);
        if (lx->i < lx->inpLength && CURR_BT == aParenLeft) {
            VALIDATEL(wordType == tokWord, errPunctuationWrongCall)
            wrapInAStatementStarting(startBt, lx, source);
            untt finalTkTp = wasCapitalized ? tokTypeCall : tokCall;
            push(((BtToken){ .tp = finalTkTp, .tokenInd = lx->tokens.length, .spanLevel = slSubexpr }),
                 lx->lexBtrack);
            pushIntokens((Token){ .tp = finalTkTp, .pl1 = uniqueStringInd,
                         .startBt = realStartByte, .lenBts = lenBts }, lx);
            ++lx->i; // CONSUME the opening "(" of the call
        } else if (lx->i < lx->inpLength && CURR_BT == aDivBy) {
            VALIDATEL(wasCapitalized, errTypeOnlyTypesArity)
            pushIntokens((Token){ .tp = tokTypeName, .pl1 = 1, .pl2 = uniqueStringInd,
                         .startBt = realStartByte, .lenBts = lenBts }, lx);
            ++lx->i; // CONSUME the "/" in the type func arity spec
        } else if (wordType == tokWord) {
            wrapInAStatementStarting(startBt, lx, source);
            pushIntokens((Token){ .tp = (wasCapitalized ? tokTypeName : tokWord), .pl2 = uniqueStringInd,
                         .startBt = realStartByte, .lenBts = lenBts }, lx);
        } else if (wordType == tokAccessor) {
            // What looks like an accessor ("a.x") may actually be a struct field ("(.id 5 .name `foo`")
            if (lx->tokens.length > 0) {
                Token prevToken = lx->tokens.content[lx->tokens.length - 1];
                if (prevToken.startBt + prevToken.lenBts < realStartByte) {
                    pushIntokens((Token){ .tp = tokStructField, .pl2 = uniqueStringInd,
                                 .startBt = realStartByte, .lenBts = lenBts }, lx);
                    return;
                }
            }
            pushIntokens((Token){ .tp = tokAccessor, .pl1 = tkAccDot, .pl2 = uniqueStringInd,
                         .startBt = realStartByte, .lenBts = lenBts }, lx);
        } else if (wordType == tokKwArg) {
            pushIntokens((Token){ .tp = tokKwArg, .pl2 = uniqueStringInd,
                         .startBt = realStartByte, .lenBts = lenBts }, lx);
        }
        return;
    }

    VALIDATEL(wordType != tokAccessor, errWordReservedWithDot)

    if (mbReservedWord == tokElse){
        closeStatement(lx);
        pushIntokens((Token){.tp = tokElse, .startBt = realStartByte, .lenBts = 4}, lx);
    } else if (mbReservedWord < firstSpanTokenType) {
        if (mbReservedWord == reservedAnd) {
            wrapInAStatementStarting(startBt, lx, source);
            pushIntokens((Token){.tp=tokOperator, .pl1 = opTAnd, .startBt=realStartByte, .lenBts=3}, lx);
        } else if (mbReservedWord == reservedOr) {
            wrapInAStatementStarting(startBt, lx, source);
            pushIntokens((Token){.tp=tokOperator, .pl1 = opTOr, .startBt=realStartByte, .lenBts=2}, lx);
        } else if (mbReservedWord == reservedTrue) {
            wrapInAStatementStarting(startBt, lx, source);
            pushIntokens((Token){.tp=tokBool, .pl2=1, .startBt=realStartByte, .lenBts=4}, lx);
        } else if (mbReservedWord == reservedFalse) {
            wrapInAStatementStarting(startBt, lx, source);
            pushIntokens((Token){.tp=tokBool, .pl2=0, .startBt=realStartByte, .lenBts=5}, lx);
        }
    } else {
        lexReservedWord(mbReservedWord, realStartByte, lenBts, lx, source);
    }
}


private void lexWord(Compiler* lx, Arr(byte) source) {
    wordInternal(tokWord, lx, source);
}

private void lexDot(Compiler* lx, Arr(byte) source) {
    /// The dot is a statement separator or the start of a field accessor
    if (lx->i < lx->inpLength - 1 && isLetter(NEXT_BT)) {
        ++lx->i; // CONSUME the dot
        wordInternal(tokAccessor, lx, source);
    } else if (hasValues(lx->lexBtrack) && peek(lx->lexBtrack).spanLevel != slScope) {
        closeStatement(lx);
        ++lx->i;  // CONSUME the dot
    }
}

private void processAssignment(Int mutType, untt opType, Compiler* lx) {
    /// Handles the "=", ":=" and "+=" tokens.
    /// Changes existing tokens and parent span to accout for the fact that we met an assignment operator
    /// mutType = 0 if it's immutable assignment, 1 if it's ":=", 2 if it's a regular operator mut "+="
    BtToken currSpan = peek(lx->lexBtrack);

    if (currSpan.tp == tokAssignment || currSpan.tp == tokReassign || currSpan.tp == tokMutation) {
        throwExcLexer(errOperatorMultipleAssignment, lx);
    } else if (currSpan.spanLevel != slStmt) {
        throwExcLexer(errOperatorAssignmentPunct, lx);
    }
    Int tokenInd = currSpan.tokenInd;
    Token* tok = (lx->tokens.content + tokenInd);
    if (mutType == 0) {
        tok->tp = tokAssignment;
    } else if (mutType == 1) {
        tok->tp = tokReassign;
    } else {
        tok->tp = tokMutation;
        tok->pl1 = (Int)((untt)((opType << 26) + lx->i));
    }
    lx->lexBtrack->content[lx->lexBtrack->length - 1] = (BtToken){ .tp = tok->tp, .spanLevel = slStmt, .tokenInd = tokenInd };
}

private void lexDollar(Compiler* lx, Arr(byte) source) {
    /// A single $ means "parentheses until the next closing paren or end of statement"
    push(((BtToken){ .tp = tokParens, .tokenInd = lx->tokens.length, .spanLevel = slSubexpr, .wasOrigDollar = true}),
         lx->lexBtrack);
    pushIntokens((Token) {.tp = tokParens, .startBt = lx->i }, lx);
    lx->i++; // CONSUME the "$"
}


private void lexColon(Compiler* lx, Arr(byte) source) {
    if (lx->i < lx->inpLength - 1) {
        byte nextBt = NEXT_BT;
        if (nextBt == aEqual) {
            processAssignment(1, 0, lx);
            lx->i += 2; // CONSUME the ":="
            return;
        } else if (isLowercaseLetter(nextBt) || nextBt == aUnderscore) {
            ++lx->i; // CONSUME the ":"
            wordInternal(tokKwArg, lx, source);
            return;
        }
    }
    VALIDATEL(lx->lexBtrack->length >= 2, errCoreMisplacedColon)
    maybeBreakStatement(lx);
    pushIntokens((Token){.tp = tokColon, .startBt = lx->i, .lenBts = 1 }, lx);
    ++lx->i; // CONSUME the ":"
}


private void lexOperator(Compiler* lx, Arr(byte) source) {
    wrapInAStatement(lx, source);

    byte firstSymbol = CURR_BT;
    byte secondSymbol = (lx->inpLength > lx->i + 1) ? source[lx->i + 1] : 0;
    byte thirdSymbol = (lx->inpLength > lx->i + 2) ? source[lx->i + 2] : 0;
    byte fourthSymbol = (lx->inpLength > lx->i + 3) ? source[lx->i + 3] : 0;
    Int k = 0;
    Int opType = -1; // corresponds to the opT... operator types
    OpDef (*operators)[countOperators] = lx->langDef->operators;
    while (k < countOperators && (*operators)[k].bytes[0] < firstSymbol) {
        k++;
    }
    while (k < countOperators && (*operators)[k].bytes[0] == firstSymbol) {
        OpDef opDef = (*operators)[k];
        byte secondTentative = opDef.bytes[1];
        if (secondTentative != 0 && secondTentative != secondSymbol) {
            k++;
            continue;
        }
        byte thirdTentative = opDef.bytes[2];
        if (thirdTentative != 0 && thirdTentative != thirdSymbol) {
            k++;
            continue;
        }
        byte fourthTentative = opDef.bytes[3];
        if (fourthTentative != 0 && fourthTentative != fourthSymbol) {
            k++;
            continue;
        }
        opType = k;
        break;
    }
    VALIDATEL(opType > -1, errOperatorUnknown)

    OpDef opDef = (*operators)[opType];
    bool isAssignment = false;

    Int lengthOfBaseOper = (opDef.bytes[1] == 0) ? 1 : (opDef.bytes[2] == 0 ? 2 : (opDef.bytes[3] == 0 ? 3 : 4));
    Int j = lx->i + lengthOfBaseOper;
    if (opDef.assignable && j < lx->inpLength && source[j] == aEqual) {
        isAssignment = true;
        j++;
    }
    if (isAssignment) { // mutation operators like "*=" or "*.="
        processAssignment(2, opType, lx);
    } else {
        if (j < lx->inpLength && source[j] == aParenLeft) {
            push(((BtToken){ .tp = tokOperCall, .tokenInd = lx->tokens.length, .spanLevel = slSubexpr }),
                 lx->lexBtrack);
            pushIntokens((Token){ .tp = tokOperCall, .pl1 = opType, .startBt = lx->i }, lx);
            ++j; // CONSUME the opening "(" of the operator call
        } else {
            pushIntokens((Token){ .tp = tokOperator, .pl1 = opType, .startBt = lx->i, .lenBts = j - lx->i}, lx);
        }
    }
    lx->i = j; // CONSUME the operator
}

private bool isOperatorTypeful(Int opType) {
    return false;
}

private void lexEqual(Compiler* lx, Arr(byte) source) {
    /// The humble "=" can be the definition statement, a marker that separates signature from definition,
    /// or an arrow "=>"
    checkPrematureEnd(2, lx);
    byte nextBt = NEXT_BT;
    if (nextBt == aEqual) {
        lexOperator(lx, source); // ==
    } else if (nextBt == aGT) {
        pushIntokens((Token){ .tp = tokArrow, .startBt = lx->i, .lenBts = 2 }, lx);
        lx->i += 2;  // CONSUME the arrow "=>"
    } else {
        processAssignment(0, 0, lx);
        lx->i++; // CONSUME the =
    }
}

private void lexNewline(Compiler* lx, Arr(byte) source) {
    /// Tl is not indentation-sensitive, but it is newline-sensitive. Thus, a newline charactor closes
    /// the current statement unless it's inside an inline span (i.e. parens or accessor parens)
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

private void lexComment(Compiler* lx, Arr(byte) source) {
    /// Either an ordinary until-line-end comment (which doesn't get included in the AST, just discarded
    /// by the lexer) or a doc-comment. Just like a newline, this needs to test if we're in a breakable
    /// statement because a comment goes until the line end.
    lx->i += 2; // CONSUME the "--"
    if (lx->i >= lx->inpLength) {
        return;
    }
    maybeBreakStatement(lx);

    Int j = lx->i;
    while (j < lx->inpLength) {
        byte cByte = source[j];
        if (cByte == aNewline) {
            lx->i = j + 1; // CONSUME the comment
            return;
        } else {
            j++;
        }
    }
    lx->i = j;  // CONSUME the comment
}

private void lexMinus(Compiler* lx, Arr(byte) source) {
    /// Handles the binary operator as well as the unary negation operator and the in-line comments
    if (lx->i == lx->inpLength - 1) {
        lexOperator(lx, source);
    } else {
        byte nextBt = NEXT_BT;
        if (isDigit(nextBt)) {
            wrapInAStatement(lx, source);
            decNumber(true, lx, source);
            lx->numeric.length = 0;
        } else if (isLowercaseLetter(nextBt) || nextBt == aUnderscore) {
            pushIntokens((Token){.tp = tokOperator, .pl1 = opTNegation, .startBt = lx->i, .lenBts = 1}, lx);
            lx->i++; // CONSUME the minus symbol
        } else if (nextBt == aMinus) {
            lexComment(lx, source);
        } else {
            lexOperator(lx, source);
        }
    }
}

private void openScope(Compiler* lx, Arr(byte) source) {
    /// An opener for a scope or a scopeful core form. Precondition: we are past the "(:" token.
    /// Consumes zero or 1 byte
    Int startBt = lx->i;
    lx->i += 2; // CONSUME the "(*"
    VALIDATEL(!hasValues(lx->lexBtrack) || peek(lx->lexBtrack).spanLevel == slScope, errPunctuationScope)
    VALIDATEL(lx->i < lx->inpLength, errPrematureEndOfInput)
    openPunctuation(tokScope, slScope, startBt, lx);
}


private void lexParenLeft(Compiler* lx, Arr(byte) source) {
    Int j = lx->i + 1;
    VALIDATEL(j < lx->inpLength, errPunctuationUnmatched)
    wrapInAStatement(lx, source);
    openPunctuation(tokParens, slSubexpr, lx->i, lx);
    lx->i++; // CONSUME the left parenthesis
}


private void lexParenRight(Compiler* lx, Arr(byte) source) {
    Int startInd = lx->i;
    closeRegularPunctuation(tokParens, lx);

    if (!hasValues(lx->lexBtrack)) return;

    lx->lastClosingPunctInd = startInd;
}


private void lexBracketLeft(Compiler* lx, Arr(byte) source) {
    Int j = lx->i + 1;
    VALIDATEL(j < lx->inpLength, errPunctuationUnmatched)
    wrapInAStatement(lx, source);
    openPunctuation(tokBrackets, slSubexpr, lx->i, lx);
    lx->i++; // CONSUME the left parenthesis
}


private void lexBracketRight(Compiler* lx, Arr(byte) source) {
    Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;
    closeColons(lx);
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);

    // since a closing paren might be closing something with statements inside it, like a lex scope
    // or a core syntax form, we need to close the last statement before closing its parent
    if (bt->length > 0 && top.spanLevel != slScope
          && (bt->content[bt->length - 1].spanLevel <= slParenMulti)) {
        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }
    setSpanLengthLexer(top.tokenInd, lx);
    lx->i++; // CONSUME the closing "]"

    if (!hasValues(lx->lexBtrack)) return;
    lx->lastClosingPunctInd = startInd;
}

private void lexAccessor(Compiler* lx, Arr(byte) source) {
    /// The "@" sign that is used for accessing arrays, lists, dictionaries
    VALIDATEL(lx->i < lx->inpLength, errPrematureEndOfInput)
    pushIntokens((Token){ .tp = tokAccessor, .pl1 = tkAccAt, .startBt = lx->i, .lenBts = 1 }, lx);
    ++lx->i; // CONSUME the "@" sign
}


private void lexSpace(Compiler* lx, Arr(byte) source) {
    lx->i++; // CONSUME the space
    while (lx->i < lx->inpLength && CURR_BT == aSpace) {
        lx->i++; // CONSUME a space
    }
}


private void lexStringLiteral(Compiler* lx, Arr(byte) source) {
    wrapInAStatement(lx, source);
    Int j = lx->i + 1;
    for (; j < lx->inpLength && source[j] != aBacktick; j++);
    VALIDATEL(j != lx->inpLength, errPrematureEndOfInput)
    pushIntokens((Token){.tp=tokString, .startBt=(lx->i), .lenBts=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


private void lexUnexpectedSymbol(Compiler* lx, Arr(byte) source) {
    printString(lx->sourceCode);
    throwExcLexer(errUnrecognizedByte, lx);
}

private void lexNonAsciiError(Compiler* lx, Arr(byte) source) {
    throwExcLexer(errNonAscii, lx);
}

/// Must agree in order with token types in tl.internal.h
const char* tokNames[] = {
    "Int", "Long", "Float", "Bool", "String", "_",
    "word", "Type", ":kwarg", ".strFld", "operator", "@cc", "=>",
    "stmt", "(", "call(", "Type(", "oper(", "[", "=", ":=", "*=", "alias",
    "assert", "assertDbg", "await", "break", "continue", "defer",
    "embed", "iface", "import", "return", "try", "yield", "colon",
    "else", "do(", "catch(", "fn(", "deF(", "for(",
    "meta(", "package(", "if(", "ifPr(", "match(", "impl(", "while("
};


testable void printLexer(Compiler* lx) {
    if (lx->wasError) {
        printf("Error: ");
        printString(lx->errMsg);
    }
    Int indent = 0;
    Arena* a = lx->a;
    Stackint32_t* sentinels = createStackint32_t(16, a);
    for (int i = 0; i < lx->tokens.length; i++) {
        Token tok = lx->tokens.content[i];
        for (int m = sentinels->length - 1; m > -1 && sentinels->content[m] == i; m--) {
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
    p[aUnderscore] = lexWord;
    p[aDot] = &lexDot;
    p[aDollar] = &lexDollar;
    p[aEqual] = &lexEqual;

    for (Int i = sizeof(operatorStartSymbols)/4 - 1; i > -1; i--) {
        p[operatorStartSymbols[i]] = &lexOperator;
    }
    p[aMinus] = &lexMinus;
    p[aParenLeft] = &lexParenLeft;
    p[aParenRight] = &lexParenRight;
    p[aBracketLeft] = &lexBracketLeft;
    p[aBracketRight] = &lexBracketRight;
    p[aAt] = &lexAccessor;
    p[aColon] = &lexColon;

    p[aSpace] = &lexSpace;
    p[aCarrReturn] = &lexSpace;
    p[aNewline] = &lexNewline;
    p[aBacktick] = &lexStringLiteral;
    return result;
}

private ReservedProbe (*tabulateReservedBytes(Arena* a))[countReservedLetters] {
    /// Table for dispatch on the first letter of a word in case it might be a reserved word.
    /// It's indexed on the diff between first byte and the letter "a" (the earliest letter a reserved word
    /// may start with)
    ReservedProbe (*result)[countReservedLetters] = allocateOnArena(countReservedLetters*sizeof(ReservedProbe), a);
    ReservedProbe* p = *result;
    for (Int i = 1; i < 25; i++) {
        p[i] = determineUnreserved;
    }
    p[0] = determineReservedA;
    p[1] = determineReservedB;
    p[2] = determineReservedC;
    p[3] = determineReservedD;
    p[4] = determineReservedE;
    p[5] = determineReservedF;
    p[8] = determineReservedI;
    p[12] = determineReservedM;
    p[14] = determineReservedO;
    p[17] = determineReservedR;
    p[19] = determineReservedT;
    p[22] = determineReservedW;
    p[24] = determineReservedY;
    return result;
}

private OpDef (*tabulateOperators(Arena* a))[countOperators] {
    /// The set of base operators in the language
    OpDef (*result)[countOperators] = allocateOnArena(countOperators*sizeof(OpDef), a);
    OpDef* p = *result;

    // This is an array of 4-byte arrays containing operator byte sequences.
    // Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
    // It's used to lex operator symbols using left-to-right search.
    p[ 0] = (OpDef){.name=s("!="),   .arity=2, .bytes={aExclamation, aEqual, 0, 0 } };
    p[ 1] = (OpDef){.name=s("!"),    .arity=1, .bytes={aExclamation, 0, 0, 0 } };
    p[ 2] = (OpDef){.name=s("#"),    .arity=1, .bytes={aSharp, 0, 0, 0 }, .overloadable=true};
    p[ 3] = (OpDef){.name=s("%"),    .arity=2, .bytes={aPercent, 0, 0, 0 } };
    p[ 4] = (OpDef){.name=s("&&"),   .arity=2, .bytes={aAmp, aAmp, 0, 0 }, .assignable=true};
    p[ 5] = (OpDef){.name=s("&"),    .arity=1, .bytes={aAmp, 0, 0, 0 } };
    p[ 6] = (OpDef){.name=s("'"),    .arity=1, .bytes={aApostrophe, 0, 0, 0 } };
    p[ 7] = (OpDef){.name=s("*."),   .arity=2, .bytes={aTimes, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[ 8] = (OpDef){.name=s("*"),    .arity=2, .bytes={aTimes, 0, 0, 0 }, .assignable = true, .overloadable = true};
    p[ 9] = (OpDef){.name=s("+."),   .arity=2, .bytes={aPlus, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[10] = (OpDef){.name=s("+"),    .arity=2, .bytes={aPlus, 0, 0, 0 }, .assignable = true, .overloadable = true};
    p[11] = (OpDef){.name=s(",,"),   .arity=1, .bytes={aComma, aComma, 0, 0}};
    p[12] = (OpDef){.name=s(","),    .arity=1, .bytes={aComma, 0, 0, 0}};
    p[13] = (OpDef){.name=s("-."),   .arity=2, .bytes={aMinus, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[14] = (OpDef){.name=s("-"),    .arity=2, .bytes={aMinus, 0, 0, 0}, .assignable = true, .overloadable = true };
    p[15] = (OpDef){.name=s("/."),   .arity=2, .bytes={aDivBy, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[16] = (OpDef){.name=s("/"),    .arity=2, .bytes={aDivBy, 0, 0, 0}, .assignable = true, .overloadable = true};
    p[17] = (OpDef){.name=s(";"),    .arity=1, .bytes={aSemicolon, 0, 0, 0 }, .overloadable=true};
    p[18] = (OpDef){.name=s("<<."),  .arity=2, .bytes={aLT, aLT, aDot, 0}, .assignable = true, .overloadable = true};
    p[19] = (OpDef){.name=s("<<"),   .arity=2, .bytes={aLT, aLT, 0, 0}, .assignable = true, .overloadable = true };
    p[20] = (OpDef){.name=s("<="),   .arity=2, .bytes={aLT, aEqual, 0, 0}};
    p[21] = (OpDef){.name=s("<>"),   .arity=2, .bytes={aLT, aGT, 0, 0}};
    p[22] = (OpDef){.name=s("<"),    .arity=2, .bytes={aLT, 0, 0, 0 } };
    p[23] = (OpDef){.name=s("=="),   .arity=2, .bytes={aEqual, aEqual, 0, 0 } };
    p[24] = (OpDef){.name=s(">=<="), .arity=3, .bytes={aGT, aEqual, aLT, aEqual } };
    p[25] = (OpDef){.name=s(">=<"),  .arity=3, .bytes={aGT, aEqual, aLT, 0 } };
    p[26] = (OpDef){.name=s("><="),  .arity=3, .bytes={aGT, aLT, aEqual, 0 } };
    p[27] = (OpDef){.name=s("><"),   .arity=3, .bytes={aGT, aLT, 0, 0 } };
    p[28] = (OpDef){.name=s(">="),   .arity=2, .bytes={aGT, aEqual, 0, 0 } };
    p[29] = (OpDef){.name=s(">>."),  .arity=2, .bytes={aGT, aGT, aDot, 0}, .assignable = true, .overloadable = true};
    p[30] = (OpDef){.name=s(">>"),   .arity=2, .bytes={aGT, aGT, 0, 0}, .assignable = true, .overloadable = true };
    p[31] = (OpDef){.name=s(">"),    .arity=2, .bytes={aGT, 0, 0, 0 }};
    p[32] = (OpDef){.name=s("?:"),   .arity=2, .bytes={aQuestion, aColon, 0, 0 } };
    p[33] = (OpDef){.name=s("?"),    .arity=1, .bytes={aQuestion, 0, 0, 0 } };
    p[34] = (OpDef){.name=s("^."),   .arity=2, .bytes={aCaret, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[35] = (OpDef){.name=s("^"),    .arity=2, .bytes={aCaret, 0, 0, 0}, .assignable = true, .overloadable = true};
    p[36] = (OpDef){.name=s("||"),   .arity=2, .bytes={aPipe, aPipe, 0, 0}, .assignable=true, };
    p[37] = (OpDef){.name=s("|"),    .arity=2, .bytes={aPipe, 0, 0, 0}};
    p[38] = (OpDef){.name=s("and"),  .arity=2, .bytes={0, 0, 0, 0 }, .assignable=true};
    p[39] = (OpDef){.name=s("or"),   .arity=2, .bytes={0, 0, 0, 0 }, .assignable=true};
    p[40] = (OpDef){.name=s("neg"),  .arity=1, .bytes={0, 0, 0, 0 }};
    for (Int k = 0; k < countOperators; k++) {
        Int m = 0;
        for (; m < 4 && p[k].bytes[m] > 0; m++) {}
        p[k].lenBts = m;
    }
    return result;
}
//}}}

//}}}
//{{{ Parser
//{{{ Parser utils

#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser0(errMsg, __LINE__, cm); }

private Int exprUpTo(Int sentinelToken, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm);
private void addBinding(int nameId, int bindingId, Compiler* cm);
private void maybeCloseSpans(Compiler* cm);
private void popScopeFrame(Compiler* cm);
private Int importAndActivateEntity(Entity ent, Compiler* cm);
private void createBuiltins(Compiler* cm);
testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a);
private Int exprHeadless(Int sentinel, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm);
private Int exprAfterHead(Token tk, Arr(Token) tokens, Compiler* cm);

#define BIG 70000000

_Noreturn private void throwExcParser0(const char errMsg[], Int lineNumber, Compiler* cm) {
    cm->wasError = true;
#ifdef TEST
    printf("Error on i = %d line %d\n", cm->i, lineNumber);
#endif
    cm->errMsg = str(errMsg, cm->a);
    longjmp(excBuf, 1);
}

#define throwExcParser(errMsg, cm) throwExcParser0(errMsg, __LINE__, cm)

private Int getActiveVar(Int nameId, Compiler* cm) {
    Int rawValue = cm->activeBindings[nameId];
    VALIDATEP(rawValue > -1 && rawValue < BIG, errUnknownFunction)
    return rawValue;
}

private Int getTypeOfVar(Int varId, Compiler* cm) {
    return cm->entities.content[varId].typeId;
}

private Int createEntity(untt name, Compiler* cm) {
    /// Validates a new binding (that it is unique), creates an entity for it,
    /// and adds it to the current scope
    Int nameId = name & LOWER24BITS;
    Int mbBinding = cm->activeBindings[nameId];
    VALIDATEP(mbBinding < 0, errAssignmentShadowing)
    // if it's a binding, it should be -1, and if overload, < -1

    Int newEntityId = cm->entities.length;
    pushInentities(((Entity){.emit = emitPrefix, .name = name, .class = classMutableGuaranteed}), cm);

    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (cm->scopeStack->length > 0) {
            addBinding(nameId, newEntityId, cm); // adds it to the ScopeStack
        }
        cm->activeBindings[nameId] = newEntityId; // makes it active
    }
    return newEntityId;
}

private Int calcSentinel(Token tok, Int tokInd) {
    /// Calculates the sentinel token for a token at a specific index
    return (tok.tp >= firstSpanTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}

//}}}

testable void pushLexScope(ScopeStack* scopeStack);
private Int parseLiteralOrIdentifier(Token tok, Compiler* cm);
testable Int typeExpr(Int nameId, Int nameLen, bool isFunction, Compiler* cm);

private void addParsedScope(Int sentinelToken, Int startBt, Int lenBts, Compiler* cm) {
    /// Performs coordinated insertions to start a scope within the parser
    push(((ParseFrame){.tp = nodScope, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodScope, .startBt = startBt, .lenBts = lenBts}, cm);
    pushLexScope(cm->scopeStack);
}

private void parseSkip(Token tok, Arr(Token) tokens, Compiler* cm) {
}


private void parseScope(Token tok, Arr(Token) tokens, Compiler* cm) {
    addParsedScope(cm->i + tok.pl2, tok.startBt, tok.lenBts, cm);
}

private void parseTry(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseYield(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}

private void ifLeftSide(Token tok, Arr(Token) tokens, Compiler* cm) {
    /// Precondition: we are 1 past the "stmt" token, which is the first parameter
    Int leftSentinel = calcSentinel(tok, cm->i - 1);
    VALIDATEP(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errIfLeft)

    VALIDATEP(leftSentinel + 1 < cm->inpLength, errPrematureEndOfTokens)
    VALIDATEP(tokens[leftSentinel].tp == tokColon, errIfMalformed)
    Int typeLeft = exprAfterHead(tok, tokens, cm);
    VALIDATEP(typeLeft == tokBool, errTypeMustBeBool)
}


private void parseIf(Token tok, Arr(Token) tokens, Compiler* cm) {
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = cm->nodes.length, .sentinelToken = cm->i + tok.pl2 };
    push(newParseFrame, cm->backtrack);
    pushInnodes((Node){.tp = nodIf, .pl1 = tok.pl1, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);

    Token stmtTok = tokens[cm->i];
    ++cm->i; // CONSUME the stmt token
    ifLeftSide(stmtTok, tokens, cm);
}

private void resumeIf(Token* tok, Arr(Token) tokens, Compiler* cm) {
    /// Returns to parsing within an if (either the beginning of a clause or an "else" block)
    if (tok->tp == tokColon || tok->tp == tokElse) {
        ++cm->i; // CONSUME the "=>" or "else"
        *tok = tokens[cm->i];
        Int sentinel = calcSentinel(*tok, cm->i);
        VALIDATEP(cm->i < cm->inpLength, errPrematureEndOfTokens)
        if (tok->tp == tokElse) {
            VALIDATEP(sentinel = peek(cm->backtrack).sentinelToken, errIfElseMustBeLast);
        }

        push(((ParseFrame){ .tp = nodIfClause, .startNodeInd = cm->nodes.length,
                            .sentinelToken = sentinel}), cm->backtrack);
        pushInnodes((Node){.tp = nodIfClause, .startBt = tok->startBt, .lenBts = tok->lenBts }, cm);
        ++cm->i; // CONSUME the first token of what follows
    } else {
        ++cm->i; // CONSUME the stmt token
        ifLeftSide(*tok, tokens, cm);
        *tok = tokens[cm->i];
    }
}

private void assignmentWordLeftSide(Token tk, Arr(Token) tokens, Compiler* cm) {
    /// Parses an assignment's left side that starts with a word, including accessor strips
    /// like "x.foo.bar" or "y@+(x 1)"

    if (tokens[cm->i + 1].tp != tokAccessor) {
        Int newEntityId = createEntity(nameOfToken(tk), cm);
        pushInnodes((Node){.tp = nodBinding, .pl1 = newEntityId, .startBt = tk.startBt,
                           .lenBts = tk.lenBts}, cm);
        cm->i++; // CONSUME the word token before the assignment sign
        return;
    }

}

private void assignmentWorker(Token tok, bool isToplevel, Arr(Token) tokens, Compiler* cm) {
    /// Parses an assignment like "x = 5 or Foo = (.id Int)". The right side must never
    /// be a scope or contain any loops, or recursion will ensue
    Int sentinelToken = cm->i + tok.pl2;
    VALIDATEP(sentinelToken >= cm->i + 2, errAssignment)
    Token bindingTk = tokens[cm->i];
    if (bindingTk.tp == tokWord) {
        push(((ParseFrame){.tp = nodAssignment, .startNodeInd = cm->nodes.length,
                           .sentinelToken = sentinelToken }), cm->backtrack);
        pushInnodes((Node){.tp = nodAssignment, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);

        assignmentWordLeftSide(bindingTk, tokens, cm);

        Token rTk = tokens[cm->i];
        Int rightType = -1;
        if (rTk.tp == tokFn) {
            // TODO newFnId = parseFnDef(&rightType, cm);
        } else {
            rightType = exprHeadless(sentinelToken, rTk.startBt,
                                       tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
            Int newBindingId = createEntity(nameOfToken(bindingTk), cm);
            VALIDATEP(rightType != -2, errAssignment)
            if (rightType > -1) {
                cm->entities.content[newBindingId].typeId = rightType;
            }
        }
    } else if (bindingTk.tp == tokTypeName) {
        typeExpr(bindingTk.pl2, bindingTk.lenBts, false, cm);
    } else {
        throwExcParser(errAssignment, cm);
    }
}

private void parseAssignment(Token tok, Arr(Token) tokens, Compiler* cm) {
    assignmentWorker(tok, false, tokens, cm);
}

private void parseWhile(Token loopTok, Arr(Token) tokens, Compiler* cm) {
    /// While loops. Look like "while(x = 0. <(x 100): x += 1)"
    ++cm->loopCounter;
    Int sentinel = cm->i + loopTok.pl2;

    Int condInd = cm->i;
    Token condTk = tokens[condInd];
    VALIDATEP(condTk.tp == tokStmt, errLoopSyntaxError)

    Int condSent = calcSentinel(condTk, condInd);
    Int startBtScope = tokens[condSent].startBt;

    push(((ParseFrame){ .tp = nodWhile, .startNodeInd = cm->nodes.length, .sentinelToken = sentinel,
                        .typeId = cm->loopCounter }), cm->backtrack);
    pushInnodes((Node){.tp = nodWhile,  .startBt = loopTok.startBt, .lenBts = loopTok.lenBts}, cm);

    addParsedScope(sentinel, startBtScope, loopTok.lenBts - startBtScope + loopTok.startBt, cm);

    // variable initializations, if any

    cm->i = condSent;
    while (cm->i < sentinel) {
        Token tok = tokens[cm->i];
        if (tok.tp != tokAssignment) {
            break;
        }
        ++cm->i; // CONSUME the assignment span marker
        parseAssignment(tok, tokens, cm);
        maybeCloseSpans(cm);
    }
    Int indBody = cm->i;
    VALIDATEP(indBody < sentinel, errLoopEmptyBody);

    // loop condition
    push(((ParseFrame){.tp = nodWhileCond, .startNodeInd = cm->nodes.length, .sentinelToken = condSent }),
         cm->backtrack);
    pushInnodes((Node){.tp = nodWhileCond, .pl1 = slStmt, .pl2 = condSent - condInd,
                       .startBt = condTk.startBt, .lenBts = condTk.lenBts}, cm);
    cm->i = condInd + 1; // + 1 because the expression parser needs to be 1 past the expr/single token

    Int condTypeId = exprAfterHead(tokens[condInd], tokens, cm);
    VALIDATEP(condTypeId == tokBool, errTypeMustBeBool)

    cm->i = indBody; // CONSUME the while token, its condition and variable initializations (if any)
}


private void resumeIfPr(Token* tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}

private void resumeImpl(Token* tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}

private void resumeMatch(Token* tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}

private void setSpanLengthParser(Int nodeInd, Compiler* cm) {
    /// Finds the top-level punctuation opener by its index, and sets its node length.
    /// Called when the parsing of a span is finished.
    cm->nodes.content[nodeInd].pl2 = cm->nodes.length - nodeInd - 1;
}

private void parseVerbatim(Token tok, Compiler* cm) {
    pushInnodes((Node){
        .tp = tok.tp, .startBt = tok.startBt, .lenBts = tok.lenBts, .pl1 = tok.pl1, .pl2 = tok.pl2}, cm);
}

private void parseErrorBareAtom(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private ParseFrame popFrame(Compiler* cm) {
    ParseFrame frame = pop(cm->backtrack);
    if (frame.tp == nodScope) {
        popScopeFrame(cm);
    }
    setSpanLengthParser(frame.startNodeInd, cm);
    return frame;
}

private Int exprSingleItem(Token tk, Compiler* cm) {
    /// A single-item expression, like "foo". Consumes no tokens.
    /// Pre-condition: we are 1 token past the token we're parsing.
    /// Returns the type of the single item.
    Int typeId = -1;
    if (tk.tp == tokWord) {
        Int varId = getActiveVar(tk.pl2, cm);
        typeId = getTypeOfVar(varId, cm);
        pushInnodes((Node){.tp = nodId, .pl1 = varId, .pl2 = tk.pl2,
                           .startBt = tk.startBt, .lenBts = tk.lenBts}, cm);
    } else if (tk.tp == tokOperator) {
        Int operBindingId = tk.pl1;
        OpDef operDefinition = (*cm->langDef->operators)[operBindingId];
        VALIDATEP(operDefinition.arity == 1, errOperatorWrongArity)
        pushInnodes((Node){ .tp = nodId, .pl1 = operBindingId, .startBt = tk.startBt, .lenBts = tk.lenBts}, cm);
        // TODO add the type when we support first-class functions
    } else if (tk.tp <= topVerbatimType) {
        pushInnodes((Node){.tp = tk.tp, .pl1 = tk.pl1, .pl2 = tk.pl2,
                           .startBt = tk.startBt, .lenBts = tk.lenBts }, cm);
        typeId = tk.tp;
    } else {
        throwExcParser(errUnexpectedToken, cm);
    }
    return typeId;
}

private void exprCountArity(Int* arity, Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
    /// Counts the arity of the call, including skipping unary operators. Consumes no tokens.
    /// Precondition:
    Token firstTok = tokens[cm->i];
    Int j = calcSentinel(firstTok, cm->i);
    while (j < sentinelToken) {
        Token tok = tokens[j];
        j += (tok.tp < firstSpanTokenType) ? 1 : (tok.pl2 + 1);
        if (tok.tp != tokOperator || (*cm->langDef->operators)[tok.pl1].arity > 1) {
            (*arity)++;
        }
    }
}

private void exprSubexpr(Int sentinelToken, Int* arity, Arr(Token) tokens, Compiler* cm) {
    /// Parses a subexpression within an expression.
    /// Precondition: the current token must be 1 past the opening paren / statement token
    /// Examples: "foo 5 a"
    ///           "+ 5 !a"
    ///
    /// Consumes 1 or 2 tokens
    /// TODO: allow for core forms (but not scopes!)
    Token firstTk = tokens[cm->i];

    exprCountArity(arity, sentinelToken, tokens, cm);
    if (firstTk.tp == tokWord || firstTk.tp == tokOperator) {
        Int mbBnd = -1;
        if (firstTk.tp == tokWord) {
            mbBnd = cm->activeBindings[firstTk.pl2];
        } else if (firstTk.tp == tokOperator) {
            VALIDATEP(*arity == (*cm->langDef->operators)[firstTk.pl1].arity, errOperatorWrongArity)
            mbBnd = -firstTk.pl1 - 2;
        }
        VALIDATEP(mbBnd != -1, errUnknownFunction)

        pushInnodes((Node){.tp = nodCall, .pl1 = mbBnd, .pl2 = *arity,
                           .startBt = firstTk.startBt, .lenBts = firstTk.lenBts}, cm);
        *arity = 0;
        cm->i++; // CONSUME the function or operator call token
    }
}

private void subexprClose(Arr(Token) tokens, Compiler* cm) {
    /// Flushes the finished subexpr frames from the top of the funcall stack.
    while (cm->scopeStack->length > 0 && cm->i == peek(cm->backtrack).sentinelToken) {
        popFrame(cm);
    }
}

private void exprOperator(Token tok, Arr(Token) tokens, Compiler* cm) {
    /// An operator token in non-initial position is either a funcall (if unary) or an operand. Consumes no tokens.
    Int bindingId = tok.pl1;
    OpDef operDefinition = (*cm->langDef->operators)[bindingId];

    if (operDefinition.arity == 1) {
        pushInnodes((Node){ .tp = nodCall, .pl1 = -bindingId - 2, .pl2 = 1,
                            .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    } else {
        pushInnodes((Node){ .tp = nodId, .pl1 = -bindingId - 2, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    }
}

testable Int typeCheckResolveExpr(Int indExpr, Int sentinel, Compiler* cm);

private Int exprUpTo(Int sentinelToken, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm) {
    /// General "big" expression parser. Parses an expression whether there is a token or not.
    /// Starts from cm->i and goes up to the sentinel token. Returns the expression's type
    /// Precondition: we are looking 1 past the tokExpr.
    Int arity = 0;
    Int startNodeInd = cm->nodes.length;
    push(((ParseFrame){.tp = nodExpr, .startNodeInd = startNodeInd, .sentinelToken = sentinelToken }),
         cm->backtrack);
    pushInnodes((Node){ .tp = nodExpr, .startBt = startBt, .lenBts = lenBts }, cm);

    exprSubexpr(sentinelToken, &arity, tokens, cm);
    while (cm->i < sentinelToken) {
        subexprClose(tokens, cm);
        Token cTk = tokens[cm->i];
        untt tokType = cTk.tp;

        if (tokType == tokParens) {
            cm->i++; // CONSUME the parens token
            exprSubexpr(cm->i + cTk.pl2, &arity, tokens, cm);
        } else VALIDATEP(tokType < firstSpanTokenType, errExpressionCannotContain)
        else if (tokType <= topVerbatimTokenVariant) {
            pushInnodes((Node){ .tp = cTk.tp, .pl1 = cTk.pl1, .pl2 = cTk.pl2,
                                .startBt = cTk.startBt, .lenBts = cTk.lenBts }, cm);
            cm->i++; // CONSUME the verbatim token
        } else {
            if (tokType == tokWord) {
                Int varId = getActiveVar(cTk.pl2, cm);
                pushInnodes((Node){ .tp = nodId, .pl1 = varId, .pl2 = cTk.pl2,
                                    .startBt = cTk.startBt, .lenBts = cTk.lenBts}, cm);
            } else if (tokType == tokOperator) {
                exprOperator(cTk, tokens, cm);
            }
            cm->i++; // CONSUME any leaf token
        }
    }

    subexprClose(tokens, cm);

    Int exprType = typeCheckResolveExpr(startNodeInd, cm->nodes.length, cm);
    return exprType;
}

private Int exprHeadless(Int sentinel, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm) {
    /// Precondition: we are looking at the first token of expr which does not have a tokStmt/tokParens header.
    /// Consumes 1 or more tokens.
    /// Returns the type of parsed expression.
    if (cm->i + 1 == sentinel) { // the [stmt 1, tokInt] case
        Token singleToken = tokens[cm->i];
        if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) {
            cm->i += 1; // CONSUME the single literal
            return exprSingleItem(singleToken, cm);
        }
    }
    return exprUpTo(sentinel, startBt, lenBts, tokens, cm);
}

private Int exprAfterHead(Token tk, Arr(Token) tokens, Compiler* cm) {
    /// Precondition: we are looking 1 past the first token of expr, which is the first parameter. Consumes 1
    /// or more tokens. Returns the type of parsed expression.
    if (tk.tp == tokStmt || tk.tp == tokParens) {
        if (tk.pl2 == 1) {
            Token singleToken = tokens[cm->i];
            if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) {
                // [stmt 1, tokInt]
                ++cm->i; // CONSUME the single literal token
                return exprSingleItem(singleToken, cm);
            }
        }
        return exprUpTo(cm->i + tk.pl2, tk.startBt, tk.lenBts, tokens, cm);
    } else {
        return exprSingleItem(tk, cm);
    }
}

private void parseExpr(Token tok, Arr(Token) tokens, Compiler* cm) {
    /// Parses an expression from an actual token. Precondition: we are 1 past that token. Handles statements
    /// only, not single items.
    exprAfterHead(tok, tokens, cm);
}

private void maybeCloseSpans(Compiler* cm) {
    /// When we are at the end of a function parsing a parse frame, we might be at the end of said frame
    /// (otherwise => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
    /// in which case this function handles all the corresponding stack poppin'.
    /// It also always handles updating all inner frames with consumed tokens.
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

private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
    /// Parses anything from current cm->i to "sentinelToken"
    while (cm->i < sentinelToken) {
        Token currTok = tokens[cm->i];
        untt contextType = peek(cm->backtrack).tp;
        // Pre-parse hooks that let contextful syntax forms (e.g. "if")
        // detect parsing errors and maintain their state
        if (contextType >= firstResumableForm) {
            ((*cm->langDef->resumableTable)[contextType - firstResumableForm])(&currTok, tokens, cm);
        } else {
            cm->i++; // CONSUME any span token
        }
        ((*cm->langDef->nonResumableTable)[currTok.tp])(currTok, tokens, cm);

        maybeCloseSpans(cm);
    }
}

private Int parseLiteralOrIdentifier(Token tok, Compiler* cm) {
    /// Consumes 0 or 1 tokens. Returns the type of what it parsed, including -1 if unknown.
    /// Returns -2 if didn't parse.
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

private void setClassToMutated(Int bindingId, Compiler* cm) {
    /// Changes a mutable variable to mutated. Throws an exception for an immutable one
    Int class = cm->entities.content[bindingId].class;
    VALIDATEP(class < classImmutable, errCannotMutateImmutable);
    if (class % 2 == 0) {
        ++cm->entities.content[bindingId].class;
    }
}


private void parseReassignment(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int rLen = tok.pl2 - 1;
    VALIDATEP(rLen >= 1, errAssignment)

    Int sentinelToken = cm->i + tok.pl2;

    Token bindingTk = tokens[cm->i];
    Int varId = getActiveVar(bindingTk.pl2, cm);
    Entity ent = cm->entities.content[varId];
    VALIDATEP(ent.class < classImmutable, errCannotMutateImmutable)

    push(((ParseFrame){ .tp = nodReassign, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodReassign, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    pushInnodes((Node){.tp = nodBinding, .pl1 = varId, .startBt = bindingTk.startBt, .lenBts = bindingTk.lenBts}, cm);

    cm->i++; // CONSUME the word token before the assignment sign

    Int typeId = cm->entities.content[varId].typeId;
    Token rTk = tokens[cm->i];
    Int rightTypeId = exprHeadless(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);

    VALIDATEP(rightTypeId == typeId, errTypeMismatch)
    setClassToMutated(varId, cm);
}

testable bool findOverload(Int typeId, Int start, Int* entityId, Compiler* cm);

private void mutationTwiddle(Int ind, Token mutTk, Compiler* cm) {
    /// Performs some AST twiddling to turn a mutation into a reassignment:
    /// 1. [.    .]
    /// 2. [.    .      Expr     ...]
    /// 3. [Expr OpCall LeftSide ...]
    ///
    /// The end result is an expression that is the right side but wrapped in another binary call,
    /// with .pl2 increased by + 2
    Node rNd = cm->nodes.content[ind + 2];
    if (rNd.tp == nodExpr) {
        cm->nodes.content[ind] = (Node){.tp = nodExpr, .pl2 = rNd.pl2 + 2, .startBt = mutTk.startBt, .lenBts = mutTk.lenBts};
    } else {
        // right side is single-node, so current AST is [ . . singleNode]
        pushInnodes((cm->nodes.content[cm->nodes.length - 1]), cm);
        // now it's [ . . singleNode singleNode]

        cm->nodes.content[ind] = (Node){.tp = nodExpr, .pl2 = 3, .startBt = mutTk.startBt, .lenBts = mutTk.lenBts};
    }
}


private void parseMutation(Token tok, Arr(Token) tokens, Compiler* cm) {
    untt encodedInfo = (untt)(tok.pl1);
    Int opType = (Int)(encodedInfo >> 26);
    Int operStartBt = (Int)(encodedInfo & LOWER26BITS);
    Int operLenBts = (*cm->langDef->operators)[opType].lenBts;
    Int sentinelToken = cm->i + tok.pl2;
    Token bindingTk = tokens[cm->i];

#ifdef SAFETY
    VALIDATEP(bindingTk.tp == tokWord, errMutation);
#endif
    Int leftEntityId = getActiveVar(bindingTk.pl2, cm);
    Int leftType = getTypeOfVar(leftEntityId, cm);
    Entity leftEntity = cm->entities.content[leftEntityId];
    VALIDATEP(leftEntityId > -1 && leftEntity.typeId > -1, errUnknownBinding);
    VALIDATEP(leftEntity.class < classImmutable, errCannotMutateImmutable);

    Int operatorOv;
    bool foundOv = findOverload(leftType, cm->activeBindings[opType], &operatorOv, cm);
    Int opTypeInd = cm->entities.content[operatorOv].typeId;
    VALIDATEP(foundOv && cm->types.content[opTypeInd] == 3, errTypeNoMatchingOverload); // operator must be binary

#ifdef SAFETY
    VALIDATEP(cm->types.content[opTypeInd] == 3 && cm->types.content[opTypeInd + 2] == leftType
                && cm->types.content[opTypeInd + 2] == leftType, errTypeNoMatchingOverload)
#endif

    push(((ParseFrame){.tp = nodReassign, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodReassign, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    pushInnodes((Node){.tp = nodBinding, .pl1 = leftEntityId, .startBt = bindingTk.startBt, .lenBts = bindingTk.lenBts}, cm);
    ++cm->i; // CONSUME the binding word

    Int ind = cm->nodes.length;
    // space for the operator and left side
    pushInnodes(((Node){}), cm);
    pushInnodes(((Node){}), cm);

    Token rTk = tokens[cm->i];
    Int rightType = exprHeadless(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    VALIDATEP(rightType == cm->types.content[opTypeInd + 3], errTypeNoMatchingOverload)

    mutationTwiddle(ind, tok, cm);
    cm->nodes.content[ind + 1] = (Node){.tp = nodCall, .pl1 = operatorOv, .pl2 = 2, .startBt = operStartBt, .lenBts = operLenBts};
    cm->nodes.content[ind + 2] = (Node){.tp = nodId, .pl1 = leftEntityId, .pl2 = bindingTk.pl2,
                                       .startBt = bindingTk.startBt, .lenBts = bindingTk.lenBts};
    setClassToMutated(leftEntityId, cm);
}

private void parseAlias(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}

private void parseAssert(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseAssertDbg(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseAwait(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private Int breakContinue(Token tok, Int* sentinel, Arr(Token) tokens, Compiler* cm) {
    VALIDATEP(tok.pl2 <= 1, errBreakContinueTooComplex);
    Int unwindLevel = 1;
    *sentinel = cm->i;
    if (tok.pl2 == 1) {
        Token nextTok = tokens[cm->i];
        VALIDATEP(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errBreakContinueInvalidDepth)

        unwindLevel = nextTok.pl2;
        ++(*sentinel); // CONSUME the Int after the "break"
    }
    if (unwindLevel == 1) {
        return -1;
    }

    Int j = cm->backtrack->length;
    while (j > -1) {
        Int pfType = cm->backtrack->content[j].tp;
        if (pfType == nodWhile) {
            --unwindLevel;
            if (unwindLevel == 0) {
                ParseFrame loopFrame = cm->backtrack->content[j];
                Int loopId = loopFrame.typeId;
                cm->nodes.content[loopFrame.startNodeInd].pl1 = loopId;
                return unwindLevel == 1 ? -1 : loopId;
            }
        }
        --j;
    }
    throwExcParser(errBreakContinueInvalidDepth, cm);

}


private void parseBreak(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int sentinel = cm->i;
    Int loopId = breakContinue(tok, &sentinel, tokens, cm);
    pushInnodes((Node){.tp = nodBreak, .pl1 = loopId, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    cm->i = sentinel; // CONSUME the whole break statement
}


private void parseContinue(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int sentinel = cm->i;
    Int loopId = breakContinue(tok, &sentinel, tokens, cm);
    pushInnodes((Node){.tp = nodContinue, .pl1 = loopId, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    cm->i = sentinel; // CONSUME the whole break statement
}


private void parseCatch(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}

private void parseDefer(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseDispose(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseExposePrivates(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseFnDef(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseInterface(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseImpl(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseLambda(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseLambda1(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseLambda2(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseLambda3(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parsePackage(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void typecheckFnReturn(Int typeId, Compiler* cm) {
    Int j = cm->backtrack->length - 1;
    while (j > -1 && cm->backtrack->content[j].tp != nodFnDef) {
        --j;
    }
    Int fnTypeInd = cm->backtrack->content[j].typeId;
    VALIDATEP(cm->types.content[fnTypeInd + 1] == typeId, errTypeWrongReturnType)
}


private void parseReturn(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int lenTokens = tok.pl2;
    Int sentinelToken = cm->i + lenTokens;

    push(((ParseFrame){ .tp = nodReturn, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodReturn, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);

    Token rTk = tokens[cm->i];
    Int typeId = exprHeadless(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    VALIDATEP(typeId > -1, errReturn)
    if (typeId > -1) {
        typecheckFnReturn(typeId, cm);
    }
}


testable void importEntities(Arr(EntityImport) impts, Int countEntities, Arr(Int) typeIds, Compiler* cm) {
    for (int j = 0; j < countEntities; j++) {
        EntityImport impt = impts[j];
        Int typeInd = typeIds[impt.typeInd];
        importAndActivateEntity((Entity){
            .name = impt.name, .class = classImmutable, .typeId = typeInd, .emit = emitPrefixExternal,
            .externalNameId = impt.externalNameId }, cm);
    }
    cm->countNonparsedEntities = cm->entities.length;
}

private ParserFunc (*tabulateNonresumableDispatch(Arena* a))[countSyntaxForms] {
    ParserFunc (*result)[countSyntaxForms] = allocateOnArena(countSyntaxForms*sizeof(ParserFunc), a);
    ParserFunc* p = *result;
    int i = 0;
    while (i <= firstSpanTokenType) {
        p[i] = &parseErrorBareAtom;
        i++;
    }
    p[tokScope]      = &parseScope;
    p[tokStmt]       = &parseExpr;
    p[tokParens]     = &parseErrorBareAtom;
    p[tokAssignment] = &parseAssignment;
    p[tokReassign]   = &parseReassignment;
    p[tokMutation]   = &parseMutation;
    p[tokColon]      = &parseSkip;

    p[tokAlias]      = &parseAlias;
    p[tokAssert]     = &parseAssert;
    p[tokAssertDbg]  = &parseAssertDbg;
    p[tokAwait]      = &parseAlias;
    p[tokBreak]      = &parseBreak;
    p[tokCatch]      = &parseAlias;
    p[tokContinue]   = &parseContinue;
    p[tokDefer]      = &parseAlias;
    p[tokEmbed]      = &parseAlias;
    p[tokFn]         = &parseAlias;
    p[tokIface]      = &parseAlias;
    p[tokImport]     = &parseAlias;
    p[tokPackage]    = &parseAlias;
    p[tokReturn]     = &parseReturn;
    p[tokTry]        = &parseAlias;
    p[tokYield]      = &parseAlias;

    p[tokIf]         = &parseIf;
    //p[tokElse]       = &parseSkip;
    p[tokWhile]      = &parseWhile;
    return result;
}


private ResumeFunc (*tabulateResumableDispatch(Arena* a))[countResumableForms] {
    ResumeFunc (*result)[countResumableForms] = allocateOnArena(countResumableForms*sizeof(ResumeFunc), a);
    ResumeFunc* p = *result;
    p[nodIf    - firstResumableForm] = &resumeIf;
    p[nodImpl  - firstResumableForm] = &resumeImpl;
    p[nodMatch - firstResumableForm] = &resumeMatch;
    return result;
}

testable LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    /// Definition of the operators, reserved words, lexer dispatch and parser dispatch tables for the
    /// compiler.
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    (*result) = (LanguageDefinition) {
        .possiblyReservedDispatch = tabulateReservedBytes(a), .dispatchTable = tabulateDispatch(a),
        .operators = tabulateOperators(a),
        .nonResumableTable = tabulateNonresumableDispatch(a),
        .resumableTable = tabulateResumableDispatch(a)
    };
    return result;
}


private Stackint32_t* copyStringTable(Stackint32_t* table, Arena* a) {
    Stackint32_t* result = createStackint32_t(table->capacity, a);
    result->length = table->length;
    result->capacity = table->capacity;
    memcpy(result->content, table->content, table->length*4);
    return result;
}


private StringDict* copyStringDict(StringDict* from, Arena* a) {
    StringDict* result = allocateOnArena(sizeof(StringDict), a);
    const Int dictSize = from->dictSize;
    Arr(Bucket*) dict = allocateOnArena(sizeof(Bucket*)*dictSize, a);

    result->a = a;
    for (int i = 0; i < dictSize; i++) {
        if (from->dict[i] == NULL) {
            dict[i] = NULL;
        } else {
            Bucket* old = from->dict[i];
            Int capacity = old->capAndLen >> 16;
            Int len = old->capAndLen & LOWER16BITS;
            Bucket* new = allocateOnArena(sizeof(Bucket) + capacity*sizeof(StringValue), a);
            new->capAndLen = old->capAndLen;
            memcpy(new->content, old->content, len*sizeof(StringValue));
            dict[i] = new;
        }
    }
    result->dictSize = from->dictSize;
    result->dict = dict;
    return result;
}

testable Compiler* createProtoCompiler(Arena* a) {
    /// Creates a proto-compiler, which is used not for compilation but as a seed value to be cloned
    /// for every source code module. The proto-compiler contains the following data:
    /// - langDef with operators
    /// - types that are sufficient for the built-in operators
    /// - entities with the built-in operator entities
    /// - overloadIds with counts
    Compiler* proto = allocateOnArena(sizeof(Compiler), a);
    (*proto) = (Compiler){
        .langDef = buildLanguageDefinitions(a), .entities = createInListEntity(32, a),
        .sourceCode = str(standardText, a),
        .stringTable = createStackint32_t(16, a), .stringDict = createStringDict(128, a),
        .types = createInListInt(64, a), .typesDict = createStringDict(128, a),
        .a = a
    };
    createBuiltins(proto);
    return proto;
}

private void finalizeLexer(Compiler* lx) {
    /// Finalizes the lexing of a single input: checks for unclosed scopes, and closes semicolons and
    /// an open statement, if any.
    if (!hasValues(lx->lexBtrack)) {
        return;
    }
    closeColons(lx);
    BtToken top = pop(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slScope && !hasValues(lx->lexBtrack), errPunctuationExtraOpening)

    setStmtSpanLength(top.tokenInd, lx);
}

testable Compiler* lexicallyAnalyze(String* sourceCode, Compiler* proto, Arena* a) {
    /// Main lexer function. Precondition: the input byte array has been prepended with StandardText
    Compiler* lx = createLexerFromProto(sourceCode, proto, a);
    Int inpLength = lx->inpLength;
    Arr(byte) inp = lx->sourceCode->content;
    VALIDATEL(inpLength > 0, "Empty input")

    // Check for UTF-8 BOM at start of file
    if ((lx->i + 3) < lx->inpLength
        && (unsigned char)inp[lx->i] == 0xEF
        && (unsigned char)inp[lx->i + 1] == 0xBB
        && (unsigned char)inp[lx->i + 2] == 0xBF) {
        lx->i += 3;
    }
    LexerFunc (*dispatch)[256] = proto->langDef->dispatchTable;
    // Main loop over the input
    if (setjmp(excBuf) == 0) {
        while (lx->i < inpLength) {
            ((*dispatch)[inp[lx->i]])(lx, inp);
        }
        finalizeLexer(lx);
    }
    return lx;
}

struct ScopeStackFrame {
    /// This frame corresponds either to a lexical scope or a subexpression.
    /// It contains string ids introduced in the current scope, used not for cleanup after the frame is popped.
    /// Otherwise, it contains the function call of the subexpression.
    int length;                // number of elements in scope stack

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

    firstChunk->length = FRESH_CHUNK_LEN;
    firstChunk->next = NULL;

    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->lastChunk = firstChunk;
    result->topScope = (ScopeStackFrame*)firstChunk->content;
    result->nextInd = ceiling4(sizeof(ScopeStackFrame))/4;
    Arr(int) firstFrame = (int*)firstChunk->content + result->nextInd;

    (*result->topScope) = (ScopeStackFrame){.length = 0, .previousChunk = NULL, .thisChunk = firstChunk,
        .thisInd = result->nextInd, .bindings = firstFrame };

    result->nextInd += 64;

    return result;
}

private void mbNewChunk(ScopeStack* scopeStack) {
    if (scopeStack->currChunk->next != NULL) {
        return;
    }
    ScopeChunk* newChunk = malloc(CHUNK_SIZE);
    newChunk->length = FRESH_CHUNK_LEN;
    newChunk->next = NULL;
    scopeStack->currChunk->next = newChunk;
    scopeStack->lastChunk = NULL;
}

testable void pushLexScope(ScopeStack* scopeStack) {
    /// Allocates a new scope, either within this chunk or within a pre-existing lastChunk or within a brand
    /// new chunk. Scopes have a simple size policy: 64 elements at first, then 256, then throw exception.
    /// This is because only 256 local variables are allowed in one function, and transitively in one scope.
    int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
    int necessarySpace = ceiling4(sizeof(ScopeStackFrame))/4 + 64;

    ScopeChunk* oldChunk = scopeStack->topScope->thisChunk;
    int oldInd = scopeStack->topScope->thisInd;
    ScopeStackFrame* newScope;
    int newInd;
    if (remainingSpace < necessarySpace) {
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (ScopeStackFrame*)scopeStack->currChunk->content;
        newInd = 0;
    } else {
        newScope = (ScopeStackFrame*)((int*)scopeStack->currChunk->content + scopeStack->nextInd);
        newInd = scopeStack->nextInd;
        scopeStack->nextInd += necessarySpace;
    }
    (*newScope) = (ScopeStackFrame){.previousChunk = oldChunk, .previousInd = oldInd, .length = 0,
        .thisChunk = scopeStack->currChunk, .thisInd = newInd,
        .bindings = (int*)newScope + ceiling4(sizeof(ScopeStackFrame))};

    scopeStack->topScope = newScope;
    scopeStack->length++;
}


private void resizeScopeArrayIfNecessary(Int initLength, ScopeStackFrame* topScope, ScopeStack* scopeStack) {
    int newLength = scopeStack->topScope->length + 1;
    if (newLength == initLength) {
        int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
        if (remainingSpace + initLength < 256) {
            mbNewChunk(scopeStack);
            scopeStack->currChunk = scopeStack->currChunk->next;
            Arr(int) newContent = scopeStack->currChunk->content;

            memcpy(topScope->bindings, newContent, initLength*4);
            topScope->bindings = newContent;
        }
    } else if (newLength == 256) {
        longjmp(excBuf, 1);
    }
}

private void addBinding(int nameId, int bindingId, Compiler* cm) {
    ScopeStackFrame* topScope = cm->scopeStack->topScope;
    resizeScopeArrayIfNecessary(64, topScope, cm->scopeStack);

    topScope->bindings[topScope->length] = nameId;
    topScope->length++;

    cm->activeBindings[nameId] = bindingId;
}

private void popScopeFrame(Compiler* cm) {
    ///  Pops a frame from the ScopeStack. For a scope type of frame, also deactivates its bindings.
    ///  Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any
    ScopeStackFrame* topScope = cm->scopeStack->topScope;
    ScopeStack* scopeStack = cm->scopeStack;
    if (topScope->bindings) {
        for (int i = 0; i < topScope->length; i++) {
            cm->activeBindings[*(topScope->bindings + i)] = -1;
        }
    }

    if (topScope->previousChunk) {
        scopeStack->currChunk = topScope->previousChunk;
        scopeStack->lastChunk = scopeStack->currChunk->next;
        scopeStack->topScope = (ScopeStackFrame*)(scopeStack->currChunk->content + topScope->previousInd);
    }

    // if the lastChunk is defined, it will serve as pre-allocated buffer for future frames, but everything after it
    // needs to go
    if (scopeStack->lastChunk) {
        ScopeChunk* ch = scopeStack->lastChunk->next;
        if (ch != NULL) {
            scopeStack->lastChunk->next = NULL;
            do {
                ScopeChunk* nextToDelete = ch->next;
                printf("ScopeStack is freeing a chunk of memory at %p next = %p\n", ch, nextToDelete);
                free(ch);

                if (nextToDelete == NULL) break;
                ch = nextToDelete;

            } while (ch != NULL);
        }
    }
    scopeStack->length--;
}

private Int isFunction(Int typeId, Compiler* cm);


private void addRawOverload(Int nameId, Int typeId, Int entityId, Compiler* cm) {
    /// Adds an overload to the [rawOverloads] and activates it, if needed
    Int mbListId = cm->activeBindings[nameId];
    if (mbListId == -1) {
        Int newListId = listAddMultiList(typeId, entityId, cm->rawOverloads);
        cm->activeBindings[nameId] = newListId;
    } else {
        Int updatedListId = addMultiList(typeId, entityId, mbListId, cm->rawOverloads);
        if (updatedListId != mbListId) {
            cm->activeBindings[nameId] = updatedListId;
        }
    }
    ++cm->countOverloads;
}

private Int importAndActivateEntity(Entity ent, Compiler* cm) {
    /// Adds an import to the entities table, activates it and, if function, adds an overload for it
    Int existingBinding = cm->activeBindings[ent.name & LOWER24BITS];
    Int isAFunc = isFunction(ent.typeId, cm);
    VALIDATEP(existingBinding == -1 || isAFunc > -1, errAssignmentShadowing)

    Int newEntityId = cm->entities.length;
    pushInentities(ent, cm);
    Int nameId = ent.name & LOWER24BITS;

    if (isAFunc > -1) {
        addRawOverload(nameId, ent.typeId, newEntityId, cm);
        pushInimports(nameId, cm);
    } else {
        cm->activeBindings[nameId] = newEntityId;
    }
    return newEntityId;
}


private Int mergeTypeWorker(Int startInd, Int lenInts, Compiler* cm) {
    byte* types = (byte*)cm->types.content;
    StringDict* hm = cm->typesDict;
    Int startBt = startInd*4;
    Int lenBts = lenInts*4;
    untt theHash = hashCode(types + startBt, lenBts);
    Int hashOffset = theHash % (hm->dictSize);
    if (*(hm->dict + hashOffset) == NULL) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;
        *firstElem = (StringValue){.hash = theHash, .indString = startBt };
        *(hm->dict + hashOffset) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hashOffset);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].hash == theHash
                  && memcmp(types + stringValues[i].indString, types + startBt, lenBts) == 0) {
                // key already present
                cm->types.length -= lenInts;
                return stringValues[i].indString/4;
            }
        }
        addValueToBucket((hm->dict + hashOffset), startBt, lenBts, hm->a);
    }
    ++hm->length;
    return startInd;
}

testable Int mergeType(Int startInd, Compiler* cm) {
    /// Unique'ing of types. Precondition: the type is parked at the end of cm->types, forming its tail.
    /// Returns the resulting index of this type and updates the length of cm->types if appropriate.
    Int lenInts = cm->types.content[startInd] + 1; // +1 for the type length
    return mergeTypeWorker(startInd, lenInts, cm);
}

testable Int addFunctionType(Int arity, Arr(Int) paramsAndReturn, Compiler* cm) {
    ///  Function types are stored as: (length, return type, paramType1, paramType2, ...)
    Int newInd = cm->types.length;
    pushIntypes(arity + 1, cm);
    pushIntypes(paramsAndReturn[0], cm);
    for (Int k = 0; k < arity; k++) {
        pushIntypes(paramsAndReturn[k + 1], cm);
    }
    return mergeType(newInd, cm);
}

//{{{ Browser codegen constants

const char codegenText[] = "returnifelsefunctionwhileconstletbreakcontinuetruefalseconsole.logfor"
                           "toStringMath.powMath.PIMath.E!==lengthMath.abs&&||===alertlo";
const Int codegenStrings[] = {
    0,     6,   8,  12,  20, //function
    25,   30,  33,  38,  46, //continue
    50,   55,  66,  69,  77, //toString
    85,   92,  98, 101, 107, //length
    115, 117, 119, 122, 127, //alert
    129
};

#define cgStrReturn      0
#define cgStrIf          1
#define cgStrElse        2
#define cgStrFunction    3
#define cgStrWhile       4

#define cgStrConst       5
#define cgStrLet         6
#define cgStrBreak       7
#define cgStrContinue    8
#define cgStrTrue        9

#define cgStrFalse      10
#define cgStrConsoleLog 11
#define cgStrFor        12
#define cgStrToStr      13
#define cgStrExpon      14

#define cgStrPi         15
#define cgStrE          16
#define cgStrNotEqual   17
#define cgStrLength     18
#define cgStrAbsolute   19

#define cgStrLogicalAnd 20
#define cgStrLogicalOr  21
#define cgStrEquality   22
#define cgStrAlert      23
#define cgStrLo         24

//}}}

testable void typeAddTypeParam(Int paramInd, Int arity, Compiler* cm);
testable void typeAddHeader(TypeHeader hdr, Compiler* cm);
private Int typeEncodeTag(untt sort, Int depth, Int arity, Compiler* cm);

private void buildStandardStrings(Compiler* lx) {
    /// Inserts the necessary strings from the standardText into the string table and the hash table
    /// The first "countOperators" nameIds are reserved for the operators (the lx->stringTable contains
    /// zeros in those places)
    for (Int j = 0; j < countOperators; j++) {
        push(0, lx->stringTable);
    }
    for (Int i = strFirstNonReserved; i <= strMathE; i++) {
        Int startBt = standardStrings[i];
        addStringDict(lx->sourceCode->content, startBt, standardStrings[i + 1] - startBt,
                      lx->stringTable, lx->stringDict);
    }
}

testable Int stToNameId(Int a) {
    /// Converts a standard string to its nameId. Doesn't work for reserved words, obviously
    return a - strFirstNonReserved + countOperators;
}

private void buildPreludeTypes(Compiler* cm) {
    /// Creates the built-in types in the proto compiler
    for (int i = strInt; i <= strVoid; i++) {
        pushIntypes(0, cm);
    }
    // List
    Int typeIndL = cm->types.length;
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
    Int typeIndA = cm->types.length;
    pushIntypes(4, cm);
    typeAddHeader((TypeHeader){.sort = sorStruct, .nameAndLen = (1 << 24) + stToNameId(strA),
                                 .depth = 1, .arity = 1}, cm);
    pushIntypes(0, cm); // the arity of the type param
    pushIntypes(stToNameId(strLen), cm);
    pushIntypes(stToNameId(strInt), cm);
    cm->activeBindings[stToNameId(strA)] = typeIndA;

    // Tuple
    Int typeIndTu = cm->types.length;
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

private void buildOperator(Int operId, Int typeId, uint8_t emitAs, uint16_t externalNameId, Compiler* cm) {
    /// Creates an entity, pushes it to [rawOverloads] and activates its name
    Int newEntityId = cm->entities.length;
    pushInentities((Entity){
        .typeId = typeId, .emit = emitAs, .class = classImmutable, .externalNameId = externalNameId}, cm);
    addRawOverload(operId, typeId, newEntityId, cm);
}

private void buildOperators(Compiler* cm) {
    /// Operators are the first-ever functions to be defined. This function builds their [types],
    /// [functions] and overload counts. The order must agree with the order of operator definitions,
    /// and every operator must have at least one function defined.
    Int boolOfIntInt = addFunctionType(2, (Int[]){tokBool, tokInt, tokInt}, cm);
    Int boolOfIntIntInt = addFunctionType(3, (Int[]){tokBool, tokInt, tokInt, tokInt}, cm);
    Int boolOfFlFl = addFunctionType(2, (Int[]){tokBool, tokFloat, tokFloat}, cm);
    Int boolOfFlFlFl = addFunctionType(3, (Int[]){tokBool, tokFloat, tokFloat, tokFloat}, cm);
    Int boolOfStrStr = addFunctionType(2, (Int[]){tokBool, tokString, tokString}, cm);
    Int boolOfBool = addFunctionType(1, (Int[]){tokBool, tokBool}, cm);
    Int boolOfBoolBool = addFunctionType(2, (Int[]){tokBool, tokBool, tokBool}, cm);
    Int intOfStr = addFunctionType(1, (Int[]){tokInt, tokString}, cm);
    Int intOfInt = addFunctionType(1, (Int[]){tokInt, tokInt}, cm);
    Int intOfFl = addFunctionType(1, (Int[]){tokInt, tokFloat}, cm);
    Int intOfIntInt = addFunctionType(2, (Int[]){tokInt, tokInt, tokInt}, cm);
    Int intOfFlFl = addFunctionType(2, (Int[]){tokInt, tokFloat, tokFloat}, cm);
    Int intOfStrStr = addFunctionType(2, (Int[]){tokInt, tokString, tokString}, cm);
    Int strOfInt = addFunctionType(1, (Int[]){tokString, tokInt}, cm);
    Int strOfFloat = addFunctionType(1, (Int[]){tokString, tokFloat}, cm);
    Int strOfBool = addFunctionType(1, (Int[]){tokString, tokBool}, cm);
    Int strOfStrStr = addFunctionType(2, (Int[]){tokString, tokString, tokString}, cm);
    Int flOfFlFl = addFunctionType(2, (Int[]){tokFloat, tokFloat, tokFloat}, cm);
    Int flOfInt = addFunctionType(1, (Int[]){tokFloat, tokInt}, cm);
    Int flOfFl = addFunctionType(1, (Int[]){tokFloat, tokFloat}, cm);

    buildOperator(opTNotEqual, boolOfIntInt, emitInfixExternal, cgStrNotEqual, cm);
    buildOperator(opTNotEqual, boolOfFlFl, emitInfixExternal, cgStrNotEqual, cm);
    buildOperator(opTNotEqual, boolOfStrStr, emitInfixExternal, cgStrNotEqual, cm);
    buildOperator(opTBoolNegation, boolOfBool, emitPrefix, 0, cm);
    buildOperator(opTSize,     intOfStr, emitField, cgStrLength, cm);
    buildOperator(opTSize,     intOfInt, emitPrefixExternal, cgStrAbsolute, cm);
    buildOperator(opTToString, strOfInt, emitInfixDot, cgStrToStr, cm);
    buildOperator(opTToString, strOfFloat, emitInfixDot, cgStrToStr, cm);
    buildOperator(opTToString, strOfBool, emitInfixDot, cgStrToStr, cm);
    buildOperator(opTRemainder, intOfIntInt, emitInfix, 0, cm);
    buildOperator(opTBinaryAnd, boolOfBoolBool, 0, 0, cm); // dummy
    buildOperator(opTTypeAnd,   boolOfBoolBool, 0, 0, cm); // dummy
    buildOperator(opTIsNull,    boolOfBoolBool, 0, 0, cm); // dummy
    buildOperator(opTTimesExt,  flOfFlFl, 0, 0, cm); // dummy
    buildOperator(opTTimes,     intOfIntInt, emitInfix, 0, cm);
    buildOperator(opTTimes,     flOfFlFl, emitInfix, 0, cm);
    buildOperator(opTPlusExt,   strOfStrStr, 0, 0, cm); // dummy
    buildOperator(opTPlus,      intOfIntInt, emitInfix, 0, cm);
    buildOperator(opTPlus,      flOfFlFl, emitInfix, 0, cm);
    buildOperator(opTPlus,      strOfStrStr, emitInfix, 0, cm);
    buildOperator(opTToInt,     intOfFl, emitNop, 0, cm);
    buildOperator(opTToFloat,   flOfInt, emitNop, 0, cm);
    buildOperator(opTMinusExt,  intOfIntInt, 0, 0, cm); // dummy
    buildOperator(opTMinus,     intOfIntInt, emitInfix, 0, cm);
    buildOperator(opTMinus,     flOfFlFl, emitInfix, 0, cm);
    buildOperator(opTDivByExt,  intOfIntInt, 0, 0, cm); // dummy
    buildOperator(opTDivBy,     intOfIntInt, emitInfix, 0, cm);
    buildOperator(opTDivBy,     flOfFlFl, emitInfix, 0, cm);
    buildOperator(opTBitShiftLeftExt,  intOfIntInt, 0, 0, cm);  // dummy
    buildOperator(opTBitShiftLeft,     intOfFlFl, 0, 0, cm);  // dummy
    buildOperator(opTLTEQ, boolOfIntInt, emitInfix, 0, cm);
    buildOperator(opTLTEQ, boolOfFlFl, emitInfix, 0, cm);
    buildOperator(opTLTEQ, boolOfStrStr, emitInfix, 0, cm);
    buildOperator(opTComparator, intOfIntInt, 0, 0, cm);  // dummy
    buildOperator(opTComparator, intOfFlFl, 0, 0, cm);  // dummy
    buildOperator(opTComparator, intOfStrStr, 0, 0, cm);  // dummy
    buildOperator(opTLessThan, boolOfIntInt, emitInfix, 0, cm);
    buildOperator(opTLessThan, boolOfFlFl, emitInfix, 0, cm);
    buildOperator(opTLessThan, boolOfStrStr, emitInfix, 0, cm);
    buildOperator(opTEquality,    boolOfIntInt, emitInfixExternal, cgStrEquality, cm);
    buildOperator(opTIntervalBoth, boolOfIntIntInt, 0, 0, cm); // dummy
    buildOperator(opTIntervalBoth, boolOfFlFlFl, 0, 0, cm); // dummy
    buildOperator(opTIntervalLeft, boolOfIntIntInt, 0, 0, cm); // dummy
    buildOperator(opTIntervalLeft, boolOfFlFlFl, 0, 0, cm); // dummy
    buildOperator(opTIntervalRight, boolOfIntIntInt, 0, 0, cm); // dummy
    buildOperator(opTIntervalRight, boolOfFlFlFl, 0, 0, cm); // dummy
    buildOperator(opTIntervalExcl, boolOfIntIntInt, 0, 0, cm); // dummy
    buildOperator(opTIntervalExcl, boolOfFlFlFl, 0, 0, cm); // dummy
    buildOperator(opTGTEQ, boolOfIntInt, emitInfix, 0, cm);
    buildOperator(opTGTEQ, boolOfFlFl, emitInfix, 0, cm);
    buildOperator(opTGTEQ, boolOfStrStr, emitInfix, 0, cm);
    buildOperator(opTBitShiftRightExt, intOfIntInt, 0, 0, cm);  // dummy
    buildOperator(opTBitShiftRight,    intOfFlFl, 0, 0, cm);  // dummy
    buildOperator(opTGreaterThan, boolOfIntInt, emitInfix, 0, cm);
    buildOperator(opTGreaterThan, boolOfFlFl, emitInfix, 0, cm);
    buildOperator(opTGreaterThan, boolOfStrStr, emitInfix, 0, cm);
    buildOperator(opTNullCoalesce, intOfIntInt, 0, 0, cm); // dummy
    buildOperator(opTQuestionMark, flOfFlFl, 0, 0, cm); // dummy
    buildOperator(opTExponentExt, intOfIntInt, 0, 0, cm); // dummy
    buildOperator(opTExponent, intOfIntInt, emitPrefixExternal, cgStrExpon, cm);
    buildOperator(opTExponent, flOfFlFl, emitPrefixExternal, cgStrExpon, cm);
    buildOperator(opTBoolOr, flOfFl, 0, 0, cm); // dummy
    buildOperator(opTXor,    flOfFlFl, 0, 0, cm); // dummy
    buildOperator(opTAnd,    boolOfBoolBool, emitInfixExternal, cgStrLogicalAnd, cm);
    buildOperator(opTOr,     boolOfBoolBool, emitInfixExternal, cgStrLogicalOr, cm);
    buildOperator(opTNegation, intOfInt, emitPrefix, 0, cm);
    buildOperator(opTNegation, flOfFl, emitPrefix, 0, cm);
}

private void createBuiltins(Compiler* cm) {
    /// Entities and functions for the built-in operators, types and functions.
    buildStandardStrings(cm);
    buildOperators(cm);
}

private void importPrelude(Compiler* cm) {
    /// Imports the standard, Prelude kind of stuff into the compiler immediately after the lexing phase
    buildPreludeTypes(cm);
    Int voidOfStr = addFunctionType(1, (Int[]){tokUnderscore, tokString}, cm);
    Int voidOfInt = addFunctionType(1, (Int[]){tokUnderscore, tokInt}, cm);
    Int voidOfFloat = addFunctionType(1, (Int[]){tokUnderscore, tokFloat}, cm);

    EntityImport imports[6] =  {
        (EntityImport) { .name = nameOfStandard(strMathPi), .externalNameId = cgStrPi, .typeInd = 0},
        (EntityImport) { .name = nameOfStandard(strMathE), .externalNameId = cgStrE, .typeInd = 0},
        (EntityImport) { .name = nameOfStandard(strPrint), .externalNameId = cgStrConsoleLog, .typeInd = 1},
        (EntityImport) { .name = nameOfStandard(strPrint), .externalNameId = cgStrConsoleLog, .typeInd = 2},
        (EntityImport) { .name = nameOfStandard(strPrint), .externalNameId = cgStrConsoleLog, .typeInd = 3},
        (EntityImport) { .name = nameOfStandard(strAlert), .externalNameId = cgStrAlert, .typeInd = 1}
    };
    // These base types occupy the first places in the stringTable and in the types table.
    // So for them nameId == typeId, unlike type funcs like L(ist) and A(rray)
    for (Int j = strInt; j <= strVoid; j++) {
        cm->activeBindings[j - strInt] = j - strInt;
    }
    importEntities(imports, sizeof(imports)/sizeof(EntityImport),
                   ((Int[]){strDouble - strFirstNonReserved, voidOfStr, voidOfInt, voidOfFloat}), cm);
}

testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a) {
    /// A proto compiler contains just the built-in definitions and tables. This fn copies it and performs init
    Compiler* lx = allocateOnArena(sizeof(Compiler), a);
    Arena* aTmp = mkArena();
    (*lx) = (Compiler){
        // this assumes that the source code is prefixed with the "standardText"
        .i = sizeof(standardText) - 1, .langDef = proto->langDef, .sourceCode = sourceCode,
        .inpLength = sourceCode->length,
        .tokens = createInListToken(LEXER_INIT_SIZE, a),
        .newlines = createInListInt(500, a),
        .numeric = createInListInt(50, aTmp),
        .lexBtrack = createStackBtToken(16, aTmp),
        .stringTable = copyStringTable(proto->stringTable, a),
        .stringDict = copyStringDict(proto->stringDict, a),
        .sourceCode = sourceCode,
        .wasError = false, .errMsg = &empty,
        .a = a, .aTmp = aTmp
    };
    return lx;
}

testable void initializeParser(Compiler* lx, Compiler* proto, Arena* a) {
    /// Turns a lexer into a parser. Initializes all the parser & typer stuff after lexing is done
    if (lx->wasError) {
        return;
    }

    Compiler* cm = lx;
    cm->activeBindings = allocateOnArena(4*lx->stringTable->length, lx->aTmp);

    Int initNodeCap = lx->tokens.length > 64 ? lx->tokens.length : 64;
    cm->scopeStack = createScopeStack();
    cm->backtrack = createStackParseFrame(16, lx->aTmp);
    cm->i = 0;
    cm->loopCounter = 0;
    cm->nodes = createInListNode(initNodeCap, a);
    cm->monoCode = createInListNode(initNodeCap, a);
    cm->monoIds = createMultiList(a);
    cm->rawOverloads = createMultiList(cm->aTmp);

    if (lx->stringTable->length > 0) {
        memset(cm->activeBindings, 0xFF, lx->stringTable->length*4); // activeBindings is filled with -1
    }

    cm->entities = createInListEntity(proto->entities.capacity, a);
    memcpy(cm->entities.content, proto->entities.content, proto->entities.length*sizeof(Entity));
    cm->entities.length = proto->entities.length;
    cm->entities.capacity = proto->entities.capacity;

    cm->overloads = (InListInt){.length = 0, .content = NULL};

    cm->types.content = allocateOnArena(proto->types.capacity*8, a);
    memcpy(cm->types.content, proto->types.content, proto->types.length*4);
    cm->types.length = proto->types.length;
    cm->types.capacity = proto->types.capacity*2;

    cm->typesDict = copyStringDict(proto->typesDict, a);

    cm->imports = createInListInt(8, lx->aTmp);

    cm->expStack = createStackint32_t(16, cm->aTmp);
    cm->typeStack = createStackint32_t(16, cm->aTmp);
    cm->tempStack = createStackint32_t(16, cm->aTmp);
    cm->scopeStack = createScopeStack();

    importPrelude(cm);
}

private Int getFirstParamType(Int funcTypeId, Compiler* cm);
private bool isFunctionWithParams(Int typeId, Compiler* cm);
private Int typeGetOuter(Int typeId, Compiler* cm);
private Int typeGetArity(Int typeId, Compiler* cm);

private void reserveSpaceInList(Int neededSpace, InListInt st, Compiler* cm) {
    if (st.length + neededSpace >= st.capacity) {
        Arr(Int) newContent = allocateOnArena(8*(st.capacity), cm->a);
        memcpy(newContent, st.content, st.length*4);
        st.capacity *= 2;
        st.content = newContent;
    }
}

private bool validateNameOverloads(Int listId, Int countOverloads, StackInt* scratch, Compiler* cm) {
    /// Validates the overloads for a name for non-intersecting via their outer types and full types
    /// 1. For parameter outer types, their arities must be unique and not match any other outer types
    /// 2. A zero-arity function, if any, must be unique
    /// 3. For any run of types with same outerType, all refs must be either to concrete entities or
    ///    to [monoIds] (no mixing within same outerType)
    /// 4. For refs to concrete entities, full types (which are concrete) must be different
    /// 5. outerTypes with refs to [monoIds] must not intersect amongst themselves
    Arr(Int) ov = cm->overloads.content;
    Int start = listId + 1;
    Int outerSentinel = start + countOverloads;
    Int prevParamOuter = -1;
    Int o = start;
    for (; o < outerSentinel && ov[o] < -1; o++) {
        // These are the param outer types, like the outer of "[U/1]U(Int)". The values are (-arity - 1)
        VALIDATEP(ov[o] != prevParamOuter, errTypeOverloadsIntersect)
        prevParamOuter = ov[o];
    }
    if (o > 0) {
        // Since we have some param outer types, we need to check every other outer type
        // The simple (though rough) criterion is they must have different arities
        Int countParamOuter = o;
        for (Int k = countParamOuter; k < outerSentinel; k++) {
            if (ov[k] == -1) {
                continue;
            }
            Int outerArityNegated = -typeGetArity(ov[k], cm) - 1;
            VALIDATEP(binarySearch(outerArityNegated, 0, countParamOuter, ov) == -1,
                      errTypeOverloadsIntersect)
        }
    }
    if (o + 1 < outerSentinel && ov[o] == -1) {
        VALIDATEP(ov[o + 1] > -1, errTypeOverloadsOnlyOneZero)
    }
    // Validate every pocket of nonnegative outerType for: 1) homogeneity (either all concrete or all generic
    // types) 2) non-intersection (for concrete types it's uniqueness, for generics non-intersection)
}


testable Int createNameOverloads(Int nameId, StackInt* scratch, Compiler* cm) {
    /// Creates a final overloads table for a name and returns its index
    /// Precondition: [rawOverloads] contain pairs of (typeId ref)
    scratch->length = 0;
    Arr(Int) raw = cm->rawOverloads->content;
    Int listId = cm->activeBindings[nameId];
    Int listSentinel = listId + 2 + raw[listId];
    for (Int j = listId + 2; j < listSentinel; j += 2) {
        Int outerType = typeGetOuter(raw[j], cm);
        push(outerType, scratch);
        push((j - listId - 2)/2, scratch);  // we need the index to get the original items after sorting
    }
    Int countOverloads = scratch->length/2;
    sortPairs(0, scratch->length, scratch->content); // sort by outerType ASC
    Int newInd = cm->overloads.length;

    reserveSpaceInList(3*countOverloads + 1, cm->overloads, cm); // 3 because it's: outerType, typeId, ref
    Arr(Int) ov = cm->overloads.content;
    ov[newInd] = 3*countOverloads;
    for (Int j = 0; j < countOverloads; j++) {
        ov[newInd + j + 1] = scratch->content[2*j]; // outerTypeId
        ov[newInd + j + 1 + countOverloads] = raw[scratch->content[2*j + 1]]; // typeId
        ov[newInd + j + 1 + 2*countOverloads] = raw[scratch->content[2*j + 1] + 1]; // ref
    }
    VALIDATEP(validateNameOverloads(newInd, countOverloads, scratch, cm), errTypeOverloadsIntersect)
    return newInd;
}

testable void createOverloads(Compiler* cm) {
    /// Fills [overloads] from [rawOverloads]. Replaces all indices in
    /// [activeBindings] to point to the new overloads table (they pointed to [rawOverloads] previously)
    StackInt* scratch = createStackint32_t(16, cm->aTmp);
    for (Int j = 0; j < countOperators; j++) {
        Int newIndex = createNameOverloads(j, scratch, cm);
        cm->activeBindings[j] = newIndex;
    }
    for (Int j = 0; j < cm->imports.length; j++) {
        Int nameId = cm->imports.content[j];
        Int newIndex = createNameOverloads(nameId, scratch, cm);
        cm->activeBindings[nameId] = newIndex;
    }
    for (Int j = 0; j < cm->toplevels.length; j++) {
        Int nameId = cm->toplevels.content[j].name & LOWER24BITS;
        Int newIndex = createNameOverloads(nameId, scratch, cm);
        cm->activeBindings[nameId] = newIndex;
    }
}

testable Int typeDef(Token assignTk, bool isFunction, Arr(Token) toks, Compiler* cm);

private void parseToplevelTypes(Compiler* cm) {
    /// Parses top-level types but not functions. Writes them to the types table and adds
    /// their bindings to the scope
    cm->i = 0;
    Arr(Token) toks = cm->tokens.content;
    const Int len = cm->tokens.length;
    while (cm->i < len) {
        Token tok = toks[cm->i];
        if (tok.tp == tokAssignment && toks[cm->i + 1].tp == tokTypeName) {
            ++cm->i; // CONSUME the assignment token
            typeDef(tok, false, toks, cm);
        } else {
            cm->i += (tok.pl2 + 1);
        }
    }
}

private void parseToplevelConstants(Compiler* cm) {
    /// Parses top-level constants but not functions, and adds their bindings to the scope
    cm->i = 0;
    Arr(Token) toks = cm->tokens.content;
    const Int len = cm->tokens.length;
    while (cm->i < len) {
        Token tok = toks[cm->i];
        if (tok.tp == tokAssignment && (cm->i + 2) < len
           && toks[cm->i + 1].tp == tokWord && toks[cm->i + 2].tp != tokFn) {
            parseUpTo(cm->i + tok.pl2, toks, cm);
        } else {
            cm->i += (tok.pl2 + 1);
        }
    }
}

#ifdef SAFETY
private void validateOverloadsFull(Compiler* cm) {
    Int lenTypes = cm->types.length;
    Int lenEntities = cm->entities.length;
    for (Int i = 1; i < cm->overloadIds.length; i++) {
        Int currInd = cm->overloadIds.content[i - 1];
        Int nextInd = cm->overloadIds.content[i];

        VALIDATEI((nextInd > currInd + 2) && (nextInd - currInd) % 2 == 1, iErrorOverloadsIncoherent)

        Int countOverloads = (nextInd - currInd - 1)/2;
        Int countConcreteOverloads = cm->overloads.content[currInd];
        VALIDATEI(countConcreteOverloads <= countOverloads, iErrorOverloadsIncoherent)
        for (Int j = currInd + 1; j < currInd + countOverloads; j++) {
            if (cm->overloads.content[j] < 0) {
                throwExcInternal(iErrorOverloadsNotFull, cm);
            }
            if (cm->overloads.content[j] >= lenTypes) {
                throwExcInternal(iErrorOverloadsIncoherent, cm);
            }
        }
        for (Int j = currInd + countOverloads + 1; j < nextInd; j++) {
            if (cm->overloads.content[j] < 0) {
                print("ERR overload missing entity currInd %d nextInd %d j %d cm->overloads.content[j] %d", currInd, nextInd,
                    j, cm->overloads.content[j])
                throwExcInternal(iErrorOverloadsNotFull, cm);
            }
            if (cm->overloads.content[j] >= lenEntities) {
                throwExcInternal(iErrorOverloadsIncoherent, cm);
            }
        }
    }
}
#endif

testable void parseFnSignature(Token fnDef, bool isToplevel, untt name, Compiler* cm) {
    /// Parses a function signature. Emits no nodes, adds data to [toplevels], [functions], [overloads].
    /// Pre-condition: we are 1 token past the tokAssignment.
    Int fnStartTokenId = cm->i - 1;
    Int fnSentinelToken = fnStartTokenId + fnDef.pl2 + 1;

    Int fnNameId = name & LOWER24BITS;
    cm->i++; // CONSUME the function name token

    Int tentativeTypeInd = cm->types.length;
    pushIntypes(0, cm); // will overwrite it with the type's length once we know it
    // the function's return type, interpreted to be Void if absent
    if (cm->tokens.content[cm->i].tp == tokTypeName) {
        Token fnReturnType = cm->tokens.content[cm->i];
        Int returnTypeId = cm->activeBindings[fnReturnType.pl2];
        VALIDATEP(returnTypeId > -1, errUnknownType)
        pushIntypes(returnTypeId, cm);

        cm->i++; // CONSUME the function return type token
    } else {
        pushIntypes(tokUnderscore, cm); // underscore stands for the Void type
    }
    VALIDATEP(cm->tokens.content[cm->i].tp == tokParens, errFnNameAndParams)

    Int paramsTokenInd = cm->i;
    Token parens = cm->tokens.content[paramsTokenInd];
    Int paramsSentinel = cm->i + parens.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list

    Int fnEntityId = cm->entities.length;

    Int arity = 0;
    while (cm->i < paramsSentinel) {
        Token paramName = cm->tokens.content[cm->i];
        VALIDATEP(paramName.tp == tokWord, errFnNameAndParams)
        ++cm->i; // CONSUME a param name
        ++arity;

        VALIDATEP(cm->i < paramsSentinel, errFnNameAndParams)
        Token paramType = cm->tokens.content[cm->i];
        VALIDATEP(paramType.tp == tokTypeName, errFnNameAndParams)

        Int paramTypeId = cm->activeBindings[paramType.pl2]; // the binding of this parameter's type
        VALIDATEP(paramTypeId > -1, errUnknownType)
        pushIntypes(paramTypeId, cm);

        ++cm->i; // CONSUME the param's type name
    }

    VALIDATEP(cm->i < (fnSentinelToken - 1) && cm->tokens.content[cm->i].tp == tokColon, errFnMissingBody)
    cm->types.content[tentativeTypeInd] = arity + 1;

    Int uniqueTypeId = mergeType(tentativeTypeInd, cm);

    pushInentities(((Entity){.emit = emitOverloaded, .class = classImmutable,
                            .typeId = uniqueTypeId}), cm);
    addRawOverload(fnNameId, uniqueTypeId, fnEntityId, cm);

    pushIntoplevels((Toplevel){.indToken = fnStartTokenId, .sentinelToken = fnSentinelToken,
            .name = name, .entityId = fnEntityId }, cm);
}

private void parseToplevelBody(Toplevel toplevelSignature, Arr(Token) toks, Compiler* cm) {
    /// Parses a top-level function.
    /// The result is the AST ([] FnDef EntityName Scope EntityParam1 EntityParam2 ... )
    Int fnStartInd = toplevelSignature.indToken;
    Int fnSentinel = toplevelSignature.sentinelToken;

    cm->i = fnStartInd + 2; // tokFn
    Int startBt = toks[fnStartInd + 1].startBt;
    Token fnTk = toks[cm->i];

    // the fnDef scope & node
    Int entityId = toplevelSignature.entityId;
    push(((ParseFrame){ .tp = nodFnDef, .startNodeInd = cm->nodes.length, .sentinelToken = fnSentinel,
                        .typeId = cm->entities.content[entityId].typeId}), cm->backtrack);
    pushInnodes(((Node){ .tp = nodFnDef, .startBt = fnTk.startBt, .lenBts = fnTk.lenBts }), cm);
    pushInnodes((Node){ .tp = nodBinding, .pl1 = entityId, .startBt = startBt, 
                        .lenBts = (Int)(startBt >> 24) }, cm);

    // the scope for the function's body
    addParsedScope(fnSentinel, fnTk.startBt, fnTk.lenBts, cm);

    Int paramsSentinel = cm->i + fnTk.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list
    while (cm->i < paramsSentinel) {
        Token paramName = toks[cm->i];
        Int newEntityId = createEntity(nameOfToken(paramName), cm);
        ++cm->i; // CONSUME the param name
        Int typeId = cm->activeBindings[paramName.pl2];
        cm->entities.content[newEntityId].typeId = typeId;

        Node paramNode = (Node){.tp = nodBinding, .pl1 = newEntityId, .startBt = paramName.startBt, .lenBts = paramName.lenBts };

        ++cm->i; // CONSUME the param's type name
        pushInnodes(paramNode, cm);
    }

    ++cm->i; // CONSUME the "=" sign
    parseUpTo(fnSentinel, toks, cm);
}

private void parseFunctionBodies(Arr(Token) toks, Compiler* cm) {
    /// Parses top-level function params and bodies
    for (int j = 0; j < cm->toplevels.length; j++) {
        cm->loopCounter = 0;
        parseToplevelBody(cm->toplevels.content[j], toks, cm);
    }
}

private void parseToplevelSignatures(Compiler* cm) {
    /// Walks the top-level functions' signatures (but not bodies). Increments counts of overloads
    /// Result: the overload counts and the list of toplevel functions to parse
    cm->i = 0;
    Arr(Token) toks = cm->tokens.content;
    Int len = cm->inpLength;
    while (cm->i < len) {
        Token tok = toks[cm->i];
        if (tok.tp == tokAssignment && (cm->i + 2) < len
           && toks[cm->i + 1].tp == tokWord && toks[cm->i + 2].tp == tokFn) {
            Int sentinel = calcSentinel(tok, cm->i); 
            parseUpTo(cm->i + tok.pl2, toks, cm);
            Token nameTk = toks[cm->i + 1];

            untt name =  ((untt)nameTk.lenBts << 24) + (untt)nameTk.pl2;
            parseFnSignature(tok, true, name, cm);
            cm->i = sentinel; 
        } else {
            cm->i += (tok.pl2 + 1);  // CONSUME the whole non-function span
        }
    }
}

/// Must agree in order with node types in tl.internal.h
const char* nodeNames[] = {
    "Int", "Long", "Float", "Bool", "String", "_", "DocComment",
    "id", "call", "binding",
    "(:", "expr", "assign", ":=",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue",
    "defer", "embed", "export", "import", "fnDef", "interface",
    "lambda", "meta", "package", "return", "try", "yield",
    "ifClause", "while", "whileCond", "if", "impl", "match"
};


private void printType(Int typeInd, Compiler* cm) {
    if (typeInd < 5) {
        printf("%s\n", nodeNames[typeInd]);
        return;
    }
    Int arity = cm->types.content[typeInd] - 1;
    Int retType = cm->types.content[typeInd + 1];
    if (retType < 5) {
        printf("%s(", nodeNames[retType]);
    } else {
        printf("Void(");
    }
    for (Int j = typeInd + 2; j < typeInd + arity + 2; j++) {
        Int tp = cm->types.content[j];
        if (tp < 5) {
            printf("%s ", nodeNames[tp]);
        }
    }
    print(")");
}


testable void printParser(Compiler* cm, Arena* a) {
    if (cm->wasError) {
        printf("Error: ");
        printString(cm->errMsg);
    }
    Int indent = 0;
    Stackint32_t* sentinels = createStackint32_t(16, a);
    for (int i = 0; i < cm->nodes.length; i++) {
        Node nod = cm->nodes.content[i];
        for (int m = sentinels->length - 1; m > -1 && sentinels->content[m] == i; m--) {
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
            printType(cm->entities.content[nod.pl1].typeId, cm);
        } else if (nod.pl1 != 0 || nod.pl2 != 0) {
            printf("%s %d %d [%d; %d]\n", nodeNames[nod.tp], nod.pl1, nod.pl2, nod.startBt, nod.lenBts);
        } else {
            printf("%s [%d; %d]\n", nodeNames[nod.tp], nod.startBt, nod.lenBts);
        }
        if (nod.tp >= nodScope && nod.pl2 > 0) {
            pushint32_t(i + nod.pl2 + 1, sentinels);
            indent++;
        }
    }
}

testable Compiler* parseMain(Compiler* cm, Arena* a) {
    if (setjmp(excBuf) == 0) {
        parseToplevelTypes(cm);

        // This gives us the semi-complete overloads & overloadIds tables (with only the built-ins and imports)
        parseToplevelConstants(cm);

        // This gives us the complete overloads & overloadIds tables, and the list of toplevel functions
        parseToplevelSignatures(cm);
        createOverloads(cm);

#ifdef SAFETY
        validateOverloadsFull(cm);
#endif
        // The main parse (all top-level function bodies)
        parseFunctionBodies(cm->tokens.content, cm);
    }
    return cm;
}

testable Compiler* parse(Compiler* cm, Compiler* proto, Arena* a) {
    /// Parses a single file in 4 passes, see docs/parser.txt
    initializeParser(cm, proto, a);
    return parseMain(cm, a);
}

//}}}
//{{{ Types
//{{{ Type utils

private Int typeEncodeTag(untt sort, Int depth, Int arity, Compiler* cm) {
    return (Int)((untt)(sort << 16) + (depth << 8) + arity);
}

testable void typeAddHeader(TypeHeader hdr, Compiler* cm) {
    /// Writes the bytes for the type header to the tail of the cm->types table.
    /// Adds 2 elements
    pushIntypes((Int)((untt)(hdr.sort << 16) + (hdr.depth << 8) + hdr.arity), cm);
    pushIntypes((Int)hdr.nameAndLen, cm);
}

testable TypeHeader typeReadHeader(Int typeId, Compiler* cm) {
    /// Reads a type header from the type array
    Int tag = cm->types.content[typeId + 1];
    untt nameAndLen = (untt)cm->types.content[typeId + 2];
    TypeHeader result = (TypeHeader){.sort = (tag >> 16) & LOWER24BITS,
            .nameAndLen = nameAndLen,
            .depth = (tag >> 8) & 0xFF, .arity = tag & 0xFF };
    return result;
}


private Int typeGetArity(Int typeId, Compiler* cm) {
    if (cm->types.content[typeId] == 0) {
        return 0;
    }
    Int tag = cm->types.content[typeId + 1];
    return tag & 0xFF;
}


private Int typeGetOuter(Int typeId, Compiler* cm) {
    TypeHeader hdr = typeReadHeader(typeId, cm);
    if (hdr.sort <= sorFunction) {
        return typeId;
    } else if (hdr.sort == sorPartial) {
        Int genElt = cm->types.content[typeId + hdr.arity + 3];
        if ((genElt >> 24) & 0xFF == 0xFF) {
            return -(genElt & 0xFF) - 1; // a param type in outer position, so we return its (-arity -1)
        } else {
            return genElt & LOWER24BITS;
        } 
    } else {
        return cm->types.content[typeId + hdr.arity + 3] & LOWER24BITS;
    }
}

private Int isFunction(Int typeId, Compiler* cm) {
    /// Returns the function's depth (arity) if the type is a function type, -1 otherwise. 
    TypeHeader hdr = typeReadHeader(typeId, cm);
    return (hdr.sort == sorFunction) ? hdr.depth : -1;
}

//}}}
//{{{ Parsing type names

#ifdef TEST
void printIntArray(Int count, Arr(Int) arr);
#endif


testable void typeAddTypeParam(Int paramInd, Int arity, Compiler* cm) {
    /// Adds a type param to a Concretization-sort type. Arity > 0 means it's a type call.
    pushIntypes((0xFF << 24) + (paramInd << 8) + arity, cm);
}

testable void typeAddTypeCall(Int typeInd, Int arity, Compiler* cm) {
    /// Known type fn call
    pushIntypes((arity << 24) + typeInd, cm);
}

testable Int typeSingleItem(Token tk, Compiler* cm) {
    /// A single-item type, like "Int". Consumes no tokens.
    /// Pre-condition: we are 1 token past the token we're parsing.
    /// Returns the type of the single item, or in case it's a type param, (-nameId - 1)
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
        throwExcParser(errUnexpectedToken, cm);
    }
}


private Int typeCountArgs(Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
    /// Counts the arity of a type call. Consumes no tokens.
    /// Precondition: we are pointing 1 past the type call
    Int arity = 0;
    Int j = cm->i;
    bool metArrow = false;
    while (j < sentinelToken) {
        Token tok = tokens[j];

        if (tok.tp == tokArrow) {
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
    /// Performs a binary search among the binary params in cm->typeStack. Returns -1 if nothing is found
    if (cm->typeStack->length == 0) {
        return -1;
    }
    Arr(Int) st = cm->typeStack->content;
    Int i = 0;
    Int j = cm->typeStack->length - 2;
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

/// Type expression data format: First element is the tag (one of the following
/// constants), second is payload. Type calls need to have an extra payload, so their tag
/// is (8 bits of "tye", 24 bits of typeId)
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

private Int typeCreateStruct(StackInt* exp, Int startInd, Int length, StackInt* params,
                             untt nameAndLen, Compiler* cm) {
    /// Creates/merges a new struct type from a sequence of pairs in "exp" and a list of type params
    /// in "params". The sequence must be flat, i.e. not include any nested structs.
    /// Returns the typeId of the new type
    Int tentativeTypeId = cm->types.length;
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
        .sort = sorStruct, .arity = params->length/2, .depth = countFields,
        .nameAndLen = nameAndLen}, cm);
    for (Int j = 1; j < params->length; j += 2) {
        pushIntypes(params->content[j], cm);
    }

    for (Int j = startInd + 1; j < sentinel; j += 4) {
        // names of fields
#ifdef SAFETY
        VALIDATEP(exp->content[j - 1] == tydField, "not a field")
#endif
        pushIntypes(exp->content[j], cm);
    }

    for (Int j = startInd + 3; j < sentinel; j += 4) {
        // types of fields
#ifdef SAFETY
        VALIDATEP(exp->content[j - 1] == tyeType, "not a type")
#endif
        pushIntypes(exp->content[j], cm);
    }
    cm->types.content[tentativeTypeId] = cm->types.length - tentativeTypeId - 1;
    return mergeType(tentativeTypeId, cm);
}

private Int typeCreateTypeCall(StackInt* exp, Int startInd, Int length, StackInt* params,
                               bool isFunction, Compiler* cm) {
    /// Creates/merges a new type call from a sequence of pairs in "exp" and a list of type params
    /// in "params". Handles both ordinary type calls and function types.
    /// Returns the typeId of the new type
    Int tentativeTypeId = cm->types.length;
    pushIntypes(0, cm);
    Int sentinel = startInd + length;
    Int countFnParams = (sentinel - startInd)/2;
    bool metRetType = exp->length >= 4 && exp->content[exp->length - 4] == tyeRetType;
    if (metRetType) {
        --countFnParams;
    }
    typeAddHeader((TypeHeader){
        .sort = (isFunction ? sorFunction : sorPartial), .arity = params->length/2, .depth = countFnParams,
        .nameAndLen = 0}, cm);
    for (Int j = 1; j < params->length; j += 2) {
        pushIntypes(params->content[j], cm);
    }

    for (Int j = startInd + 1; j < sentinel; j += 2) {
        Int tye = exp->content[j - 1] & LOWER24BITS;
        if (tye == tyeRetType) {
        } else if (tye == tyeType) {
            pushIntypes(exp->content[j], cm);
        } else {
            throwExcParser("not a type", cm);
        }
    }
    if (!metRetType) {
        pushIntypes(stToNameId(strVoid), cm);
    }

    cm->types.content[tentativeTypeId] = cm->types.length - tentativeTypeId - 1;
    print("created type call of length %d", cm->types.content[tentativeTypeId]);
    return mergeType(tentativeTypeId, cm);
}

private Int typeCreateFnSignature(StackInt* exp, Int startInd, Int length, StackInt* params,
                             untt nameAndLen, Compiler* cm) {
    /// Creates/merges a new struct type from a sequence of pairs in "exp" and a list of type params
    /// in "params". The sequence must be flat, i.e. not include any nested structs.
    /// Returns the typeId of the new type
    Int tentativeTypeId = cm->types.length;
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
        .sort = sorStruct, .arity = params->length/2, .depth = countFields,
        .nameAndLen = nameAndLen}, cm);
    for (Int j = 1; j < params->length; j += 2) {
        pushIntypes(params->content[j], cm);
    }

    for (Int j = startInd + 1; j < sentinel; j += 4) {
        // names of fields
#ifdef SAFETY
        VALIDATEP(exp->content[j - 1] == tydField, "not a field")
#endif
        pushIntypes(exp->content[j], cm);
    }

    for (Int j = startInd + 3; j < sentinel; j += 4) {
        // types of fields
#ifdef SAFETY
        VALIDATEP(exp->content[j - 1] == tyeType, "not a type")
#endif
        pushIntypes(exp->content[j], cm);
    }
    cm->types.content[tentativeTypeId] = cm->types.length - tentativeTypeId - 1;
    return mergeType(tentativeTypeId, cm);
}

private Int typeEvalExpr(StackInt* exp, StackInt* params, untt nameAndLen, Compiler* cm) {
    /// Processes the "type expression" produced by "typeDef".
    /// Returns the typeId of the new typeId
    Int j = exp->length - 2;
    printIntArray(exp->length, exp->content);
    while (j > 0) {
        Int tyeContent = exp->content[j];
        Int tye = (exp->content[j] >> 24) & 0xFF;
        if (tye == 0) {
            tye = tyeContent;
        }
        if (tye == tyeStruct) {
            Int lengthStruct = exp->content[j + 1]*4; // *4 because for every field there are 4 ints
            Int typeNestedStruct = typeCreateStruct(exp, j + 2, lengthStruct, params, 0, cm);
            exp->content[j] = tyeType;
            exp->content[j + 1] = typeNestedStruct;
            shiftTypeStackLeft(j + 2 + lengthStruct, lengthStruct, cm);

            print("after struct shift:");
            printIntArray(exp->length, exp->content);
        } else if (tye == tyeTypeCall || tye == tyeFnType) {
            Int lengthFn = exp->content[j + 1]*2;
            Int typeFn = typeCreateTypeCall(exp, j + 2, lengthFn, params, tye == tyeFnType, cm);
            exp->content[j] = tyeType;
            exp->content[j + 1] = typeFn;
            shiftTypeStackLeft(j + 2 + lengthFn, lengthFn, cm);

            print("after typeCall/fnType shift:");
            printIntArray(exp->length, exp->content);
        } else if (tye == tyeParamCall) {

        }
        j -= 2;
    }
    print("after processing")
    printIntArray(exp->length, exp->content);
    if (nameAndLen > 0) {
        // at this point, exp must contain just a single struct with its fields followed by just typeIds
        Int lengthNewStruct = exp->content[1]*4; // *4 because for every field there are 4 ints
        return typeCreateStruct(exp, 2, lengthNewStruct, params, nameAndLen, cm);
    } else {
        Int lengthNewSignature = exp->content[1]*4; // *4 because for every field there are 4 ints
        return typeCreateFnSignature(exp, 2, lengthNewSignature, params, 0, cm);
    }
}

#define maxTypeParams 254

private void typeDefReadParams(Token bracketTk, StackInt* params, Compiler* cm) {
    /// Precondition: we are pointing 1 past the bracket token
    Int brackSentinel = cm->i + bracketTk.pl2;
    while (cm->i < brackSentinel) {
        Token tk = cm->tokens.content[cm->i];
        VALIDATEP(tk.tp == tokTypeName, errTypeDefParamsError)
        Int nameId = tk.pl2;
        VALIDATEP(cm->activeBindings[nameId] == -1, errAssignmentShadowing)
        cm->activeBindings[nameId] = -params->length - 2;
        push(tk.pl2, params);
        if (tk.pl1 > 0) {
            VALIDATEP(cm->i + 1 < brackSentinel, errTypeDefParamsError)
            Token arityTk = cm->tokens.content[cm->i];
            VALIDATEP(arityTk.tp == tokInt && arityTk.pl1 == 0
                      && arityTk.pl2 > 0 && arityTk.pl2 < 255, errTypeDefParamsError)
            push(arityTk.pl2, params);
        } else {
            push(0, params);
        }
        VALIDATEP(params->length <= maxTypeParams, errWordLengthExceeded)
        ++cm->i;
    }
}

private void typeDefClearParams(StackInt* params, Compiler* cm) {
    for (Int j = 0; j < params->length; j += 2) {
        cm->activeBindings[params->content[j]] = -1;
    }
    params->length = 0;
}

private Int typeCountFieldsInStruct(Int length, Compiler* cm) {
    /// Returns the number of fields in struct definition. Precondition: we are 1 past the parens token
    Int count = 0;
    Int j = cm->i;
    Int sentinel = j + length;
    while (j < sentinel) {
        VALIDATEP(cm->tokens.content[j].tp == tokStructField, errTypeDefCannotContain)
        ++j;
        Token nextTk = cm->tokens.content[j];
        if (nextTk.tp == tokTypeCall || nextTk.tp == tokParens) {
            j += nextTk.pl2 + 1;
        } else if (nextTk.tp == tokTypeName) {
            ++j;
        } else {
            throwExcParser(errTypeDefCannotContain, cm);
        }
        ++count;
    }
    return count;
}

private bool typeExprIsInside(Int tp, Compiler* cm) {
    /// Is the current type expression inside e.g. a struct or a function
    return (cm->tempStack->length >= 2
            && penultimate(cm->tempStack) == tp);
}


private void typeBuildExpr(StackInt* exp, Int sentinel, Compiler* cm) {
    StackInt* params = cm->typeStack;
    while (cm->i < sentinel) {
        Token cTk = cm->tokens.content[cm->i];
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
            Int countArgs = typeCountArgs(calcSentinel(cTk, cm->i - 1), cm->tokens.content, cm);
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
                    print("typeId %d countArgs %d nameId %d", typeId, countArgs, nameId)
                    Int typeAr = typeGetArity(typeId, cm);
                    print("typeAr %d", typeAr)
                    VALIDATEP(typeGetArity(typeId, cm) == countArgs, errTypeConstructorWrongArity)
                    push(((untt)tyeTypeCall << 24) + nameId, exp);
                    push(countArgs, exp);
                }
            } else {
                VALIDATEP(params->content[mbParamId + 1] == countArgs, errTypeConstructorWrongArity)
                push(((untt)tyeParamCall << 24) + nameId, exp);
                push(countArgs, exp);
            }
        } else if (cTk.tp == tokStructField) {
            VALIDATEP(typeExprIsInside(tyeStruct, cm), errTypeDefError)

            push(tyeName, exp);
            push(cTk.pl2, exp);  // nameId
            Token nextTk = cm->tokens.content[cm->i];
            VALIDATEP(nextTk.tp == tokTypeName || nextTk.tp == tokTypeCall || nextTk.tp == tokParens,
                      errTypeDefError)
        } else if (cTk.tp == tokWord) {
            VALIDATEP(typeExprIsInside(tyeFunction, cm), errTypeDefError)

            push(tyeName, exp);
            push(cTk.pl2, exp);  // nameId
            Token nextTk = cm->tokens.content[cm->i];
            VALIDATEP(nextTk.tp == tokTypeName || nextTk.tp == tokTypeCall || nextTk.tp == tokParens,
                      errTypeDefError)
        } else if (cTk.tp == tokArrow) {
            VALIDATEP(cm->tempStack->length >= 2, errTypeDefError)
            Int penult = penultimate(cm->tempStack);
            printIntArray(cm->tempStack->length, cm->tempStack->content);
            VALIDATEP(penult == tyeFunction || penult == tyeFnType, errTypeDefError)

            push(tyeRetType, exp);
            push(0, exp);
        } else if (cTk.tp == tokMeta) {
            VALIDATEP(cm->tempStack->length >= 2, errTypeDefError)
            VALIDATEP(penultimate(cm->tempStack) == tyeStruct, errTypeDefError)

            push(tyeMeta, exp);
            push(cm->i - 1, exp);
        } else {
            print("erroneous type %d", cTk.tp)
            throwExcParser(errTypeDefError, cm);
        }
        while (cm->tempStack->length > 0) {
            if (peek(cm->tempStack) != cm->i) {
                break;
            }
            pop(cm->tempStack);
            pop(cm->tempStack);
        }
    }
}

testable Int typeDef(Token assignTk, bool isFunction, Arr(Token) toks, Compiler* cm) {
    /// Builds a type expression. Accepts a name or -1 for nameless type exprs (like function signatures).
    /// Uses cm->expStack to build a "type expression" and cm->params for the type parameters
    /// Produces no AST nodes, but potentially lots of new types
    /// Consumes the whole type assignment, or the whole function signature
    /// Data format: see "Type expression data format"
    /// Precondition: we are 1 past the assignment token
    StackInt* exp = cm->expStack;
    StackInt* params = cm->typeStack;
    exp->length = 0;
    params->length = 0;

    Int sentinel = cm->i + assignTk.pl2;
    Token nameTk = toks[cm->i];
    ++cm->i; // CONSUME the type name

    VALIDATEP(cm->i < sentinel && toks[cm->i].tp == tokParens, errTypeDefError)
    ++cm->i; // CONSUME the parens token
    Token firstTok = toks[cm->i];
    if (firstTok.tp == tokBrackets) {
        ++cm->i; // CONSUME the brackets token
        typeDefReadParams(firstTok, params, cm);
    }
    if (isFunction) {
        push(tyeFunction, cm->tempStack);
        push(sentinel, cm->tempStack);
    }
    typeBuildExpr(exp, sentinel, cm);
    printIntArray(exp->length, exp->content);

    Int newTypeId = typeEvalExpr(exp, params, ((untt)(nameTk.lenBts) << 24) + nameTk.pl2, cm);
    typeDefClearParams(params, cm);
    return newTypeId;
}

///}}}
//{{{ Overloads, type check & resolve

private Int getFirstParamType(Int funcTypeId, Compiler* cm) {
    /// Gets the type of the last param of a function.
    return cm->types.content[funcTypeId + 2];
}

private bool isFunctionWithParams(Int typeId, Compiler* cm) {
    return cm->types.content[typeId] > 1;
}

private Int findOnPlateauOverload(Int outerTypeVal, Int ind, Int typeId, Int ovStart, Int countOvs,
                                  Arr(Int) overs) {
    /// Find the inclusive left and right indices of a plateau (same outer type) in [overloads]. 
    /// Returns index of the reference in [overloads], or -1 if not match was found
    Int j = ind - 1; 
    while (j > ovStart && overs[j] == val) {
        --j;
    }
    Int plateauLeft = j + 1 + countOvs; 
    j = ind + 1; 
    Int sentinel = ovStart + countOvs + 1; 
    while (j < sentinel && overs[j] == val) {
        ++j;
    }
    Int plateauRight = j - 1 + countOvs;
    // We've found the plateau in the outer types, now to search the full types.
    // They may contain either only negative values (for generic function's first parameter) or only
    // non-negative values (for concrete fn param)
    bool genericMode = overs[plateauLeft] < 0;
    if (genericMode) {
        for (Int k = plateauLeft; k <= plateauRight; k++) {
            if (typeSatisfiesGeneric(typeId, overs[k], cm)) {
                return k + countOvs;
            }
        }
        return -1; 
    } else {
        Int indFullType = binarySearch(typeId, plateauLeft, plateauRight, overs); 
        return indFullType > -1 ? (indFullType + countOvs) : -1; 
    }
}
    
testable bool findOverload(Int typeId, Int ovInd, Int* entityId, Compiler* cm) {
    /// Params: typeId = type of the first function parameter, or -1 if it's 0-arity
    ///         ovInd = ind in [overloads]
    ///         entityId = address where to store the result, if successful
    /// We have 4 scenarios here, sorted from left to right in the outerType part of [overloads]:
    /// 1. outerType < -1: non-function generic types with outer generic, e.g. "U(Int)" => -2
    /// 2. outerType = -1: 0-arity function
    /// 3. outerType >=< 0 BIG: non-function types with outer concrete, e.g. "L(U)" => ind of L
    /// 4. outerType >= BIG: function types (generic or concrete), e.g. "F(Int => String)" => BIG + 1
    Int start = ovInd + 1; 
    Arr(Int) overs = cm->overloads.content;
    Int countOvs = overs[ovInd];
    Int sentinel = ovInd + countOvs + 1;
    if (typeId == -1) { // looking for scenario 2
        Int j = 0;
        while (j < sentinel && overs[j] < 0) {
            if (overs[j] == -1) {
                (*entityId) = overs[j + 2*countOvs]; 
                return true; 
            }
            ++j;
        }
        throwExcParser(errTypeNoMatchingOverload, cm); 
    } else {
        Int outerType = typeGetOuter(typeId, cm);
        Int mbFuncArity = isFunction(typeId, cm);
        if (mbFuncArity > -1) { // scenario 4
            mbFuncArity += BIG; 
            Int j = sentinel - 1;
            for (Int j = sentinel - 1; j > start && overs[j] > BIG; j--) {
                if (overs[j] != mbFuncArity) {
                    continue;
                }
                Int res = overloadOnPlateau(mbFuncArity, j, ovInd, &plateauLeft, &plateauRight, overs);
                (*entityId) = overs[j + 2*countOvs]; 
                return true;
            }
        } else { // scenarios 1 or 3
            Int j = start;
            while (j < sentinel + overs[j] < -1) { // scenario 1
            }
            if (overs[j] == -1) {
                ++j;
            }
            Int k = sentinel - 1;
            while (k > j && overs[k] >= BIG) {
                --k;
            }
            // scenario 3 
            Int ind = binarySearch(outerType, j, k, overs);
            if (ind == -1) {
                return false;
            }

            Int res = overloadOnPlateau(outerType, ind, ovInd, overs);
            (*entityId) = overs[j + 2*countOvs]; 
            return true;
        }
    }
}

private void shiftTypeStackLeft(Int startInd, Int byHowMany, Compiler* cm) {
    /// Shifts ints from start and until the end to the left.
    /// E.g. the call with args (4, 2) takes the stack from [x x x x 1 2 3] to [x x 1 2 3]
    /// startInd and byHowMany are measured in units of Int.
    Int from = startInd;
    Int to = startInd - byHowMany;
    Int sentinel = cm->expStack->length;
    while (from < sentinel) {
        Int pieceSize = MIN(byHowMany, sentinel - from);
        memcpy(cm->expStack->content + to, cm->expStack->content + from, pieceSize*4);
        from += pieceSize;
        to += pieceSize;
    }
    cm->expStack->length -= byHowMany;
}

private void populateExpStack(Int indExpr, Int sentinelNode, Int* currAhead, Compiler* cm) {
    /// Populates the expression's type stack with the operands and functions of an expression
    cm->expStack->length = 0;
    for (Int j = indExpr + 1; j < sentinelNode; ++j) {
        Node nd = cm->nodes.content[j];
        if (nd.tp <= tokString) {
            push((Int)nd.tp, cm->expStack);
        } else if (nd.tp == nodCall) {
            push(BIG + nd.pl2, cm->expStack); // signifies that it's a call, and its arity
            push((nd.pl2 > 0 ? -nd.pl1 - 2 : nd.pl1), cm->expStack);
            // index into overloadIds, or entityId for 0-arity fns

            ++(*currAhead);
        } else if (nd.pl1 > -1) { // entityId
            push(cm->entities.content[nd.pl1].typeId, cm->expStack);
        } else { // overloadId
            push(nd.pl1, cm->expStack); // overloadId
        }
    }
#if defined(TRACE) && defined(TEST)
    printExpSt(st);
#endif
}

/// Typechecks and resolves overloads in a single expression
testable Int typeCheckResolveExpr(Int indExpr, Int sentinelNode, Compiler* cm) {
    Int currAhead = 0; // how many elements ahead we are compared to the token array (because of extra call indicators)
    StackInt* st = cm->expStack;

    populateExpStack(indExpr, sentinelNode, &currAhead, cm);
    // now go from back to front: resolving the calls, typechecking & collapsing args, and replacing calls
    // with their return types
    Int j = cm->expStack->length - 1;
    Arr(Int) cont = st->content;
    while (j > -1) {
        if (cont[j] < BIG) { // it's not a function call, because function call indicators have BIG in them
            --j;
            continue;
        }

        // It's a function call. cont[j] contains the arity, cont[j + 1] the index into overloads table
        Int arity = cont[j] - BIG;
        Int o = cont[j + 1];
        if (arity == 0) {
            VALIDATEP(o > -1, errTypeOverloadsOnlyOneZero)

            Int functionTypeInd = cm->entities.content[o].typeId;
#ifdef SAFETY
            VALIDATEI(cm->types.content[functionTypeInd] == 1, iErrorOverloadsIncoherent)
#endif

            cont[j] = cm->types.content[functionTypeInd + 1]; // write the return type
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
                print("cm->overloadIds.content[o] %d cm->overloadIds.content[o + 1] %d", cm->overloadIds.content[o], cm->overloadIds.content[o + 1])
                printExpSt(st);
            }
#endif
            VALIDATEP(ovFound, errTypeNoMatchingOverload)

            Int typeOfFunc = cm->entities.content[entityId].typeId;
            VALIDATEP(cm->types.content[typeOfFunc] - 1 == arity, errTypeNoMatchingOverload) // first parm matches, but not arity

            for (int k = j + 3; k < j + arity + 2; k++) {
                // We know the type of the function, now to validate arg types against param types
                // Loop init is not "k = j + 2" because we've already checked the first param
                if (cont[k] > -1) {
                    VALIDATEP(cont[k] == cm->types.content[typeOfFunc + k - j], errTypeWrongArgumentType)
                } else {
                    Int argBindingId = cm->nodes.content[indExpr + k - currAhead].pl1;
                    cm->entities.content[argBindingId].typeId = cm->types.content[typeOfFunc + k - j];
                }
            }
            --currAhead;
            cm->nodes.content[j + indExpr + 1 - currAhead].pl1 = entityId;
            // the type-resolved function of the call
            cont[j] = cm->types.content[typeOfFunc + 1];    // the function return type
            shiftTypeStackLeft(j + arity + 2, arity + 1, cm);
#if defined(TRACE) && defined(TEST)
            printExpSt(st);
#endif
        }
        --j;
    }

    if (st->length == 1) {
        return st->content[0]; // the last remaining element is the type of the whole expression
    } else {
        return -1;
    }
}

//}}}
//{{{ Generic types

testable void typeSkipNode(Int* ind, Compiler* cm) {
    /// Completely skips the current node in a type, whether the node is concrete or param, a call or not
    Int toSkip = 1;
    while (toSkip > 0) {
        Int cType = cm->types.content[*ind];
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
    /// Get type-arg-count from a type element (which may be a type call, concrete type, type param etc)
    untt genericTag = typeGenTag(typeElt);
    if (genericTag == 255) {
        return typeElt & 0xFF;
    } else {
        return genericTag;
    }
}


testable bool typeGenericsIntersect(Int type1, Int type2, Compiler* cm) {
    /// Returns true iff two generic types intersect (i.e. a concrete type may satisfy both of them)
    /// Warning: this function assumes that both types have sort = Partial
    TypeHeader hdr1 = typeReadHeader(type1, cm);
    TypeHeader hdr2 = typeReadHeader(type2, cm);
    Int i1 = type1 + 3 + hdr1.arity; // +3 to skip the type length, type tag and name
    Int i2 = type2 + 3 + hdr2.arity;

    Int sentinel1 = type1 + cm->types.content[type1] + 1;
    Int sentinel2 = type2 + cm->types.content[type2] + 1;
    untt top1 = cm->types.content[i1];
    untt top2 = cm->types.content[i2];
    if (top1 != top2) {
        return false;
    } else if (typeGenTag(top1) == 255 || typeGenTag(top2) == 255) {
        // If any type has a param in top position, they already intersect
        return true;
    }
    ++i1;
    ++i2;
    while (i1 < sentinel1 && i2 < sentinel2) {
        untt nod1 = cm->types.content[i1];
        untt nod2 = cm->types.content[i2];
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
    /// Takes a single call from within a concrete type, and merges it into the type dictionary
    /// as a whole, separate type. Returns the unique typeId
    return -1;
}

testable StackInt* typeSatisfiesGeneric(Int typeId, Int genericId, Compiler* cm) {
    /// Checks whether a concrete type satisfies a generic type. Returns a pointer to
    /// cm->typeStack with the values of parameters if satisfies, NULL otherwise.
    /// Example: for typeId = L(L(Int)) and genericId = [T/1]L(T(Int)) returns Generic[L]
    /// Warning: assumes that typeId points to a concrete type, and genericId to a partial one
    StackInt* tStack = cm->typeStack;
    tStack->length = 0;
    TypeHeader genericHeader = typeReadHeader(genericId, cm);
    // yet-unknown values of the type parameters
    for (Int j = 0; j < genericHeader.arity; j++) {
        push(-1, cm->typeStack);
    }

    Int i1 = typeId + 3; // +3 to skip the type length, type tag and name
    Int i2 = genericId + 3 + genericHeader.arity;
    Int sentinel1 = typeId + cm->types.content[typeId] + 1;
    Int sentinel2 = genericId + cm->types.content[genericId] + 1;
    while (i1 < sentinel1 && i2 < sentinel2) {
        untt nod1 = cm->types.content[i1];
        untt nod2 = cm->types.content[i2];
        if (typeGenTag(nod2) < 255) {
            if (nod1 != nod2) {
                return NULL;
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
                // E.g. for type = L(Int), gen = [U/1 V]U(V), param U = L, not L(Int)
                if (paramArgcount != concreteArgcount) {
                    return NULL;
                }
                if (tStack->content[paramId] == -1) {
                    tStack->content[paramId] = concreteElt;
                } else if (tStack->content[paramId] != concreteElt) {
                    return NULL;
                }
                ++i1;
                ++i2;
            } else {
                // A param not being called may correspond to a simple type, a type call or
                // a callable type.
                // E.g. for a generic type G = [U/1] U(Int), concrete type G(L), generic type [T/1] G(T):
                // we will have T = L, even though the type param T is not being called in generic type
                Int declaredArity = cm->types.content[genericId + 3 + paramId];
                Int typeFromConcrete = -1;
                if (concreteArgcount > 0) {
                    typeFromConcrete = typeMergeTypeCall(i1, concreteArgcount, cm);
                    typeSkipNode(&i1, cm);
                } else {
                    if (typeEltArgcount(nod1) != declaredArity) {
                        return NULL;
                    }
                    typeFromConcrete = concreteElt;
                    ++i1;
                }
                if (tStack->content[paramId] == -1) {
                    tStack->content[paramId] = typeFromConcrete;
                } else {
                    if (tStack->content[paramId] != typeFromConcrete) {
                        return NULL;
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
//{{{ Codegen

typedef struct Codegen Codegen;
typedef void CgFunc(Node, bool, Arr(Node), Codegen*);
typedef struct {
    Int startInd; // or externalNameId
    Int length; // only for native names
    uint8_t emit;
    uint8_t arity;
    uint8_t countArgs;
    bool needClosingParen;
} CgCall;

DEFINE_STACK_HEADER(CgCall)
DEFINE_STACK(CgCall)

struct Codegen {
    Int i; // current node index
    Int indentation;
    Int length;
    Int capacity;
    Arr(byte) buffer;
    StackCgCall* calls; // temporary stack for generating expressions

    StackNode backtrack; // these "nodes" are modified from what is in the AST. .startBt = sentinelNode
    CgFunc* cgTable[countSpanForms];
    String* sourceCode;
    Compiler* cm;
    Arena* a;
};


private void ensureBufferLength(Int additionalLength, Codegen* cg) {
    /// Ensures that the buffer has space for at least that many bytes plus 10
    if (cg->length + additionalLength + 10 < cg->capacity) {
        return;
    }
    Int neededLength = cg->length + additionalLength + 10;
    Int newCap = 2*cg->capacity;
    while (newCap <= neededLength) {
        newCap *= 2;
    }
    Arr(byte) new = allocateOnArena(newCap, cg->a);
    memcpy(new, cg->buffer, cg->length);
    cg->buffer = new;
    cg->capacity = newCap;
}


private void writeBytes(byte* ptr, Int countBytes, Codegen* cg) {
    ensureBufferLength(countBytes + 10, cg);
    memcpy(cg->buffer + cg->length, ptr, countBytes);
    cg->length += countBytes;
}

private void writeConstant(Int indConst, Codegen* cg) {
    /// Write a constant to codegen
    Int len = codegenStrings[indConst + 1] - codegenStrings[indConst];
    ensureBufferLength(len, cg);
    memcpy(cg->buffer + cg->length, standardText + codegenStrings[indConst], len);
    cg->length += len;
}

private void writeConstantWithSpace(Int indConst, Codegen* cg) {
    /// Write a constant to codegen and add a space after it
    Int len = codegenStrings[indConst + 1] - codegenStrings[indConst];
    ensureBufferLength(len, cg); // no need for a "+ 1", for the function automatically ensures 10 extra bytes
    memcpy(cg->buffer + cg->length, codegenText + codegenStrings[indConst], len);
    cg->length += (len + 1);
    cg->buffer[cg->length - 1] = 32;
}


private void writeChar(byte chr, Codegen* cg) {
    ensureBufferLength(1, cg);
    cg->buffer[cg->length] = chr;
    ++cg->length;
}


private void writeChars0(Codegen* cg, Int count, Arr(byte) chars){
    ensureBufferLength(count, cg);
    for (Int j = 0; j < count; j++) {
        cg->buffer[cg->length + j] = chars[j];
    }
    cg->length += count;
}

#define writeChars(cg, chars) writeChars0(cg, sizeof(chars), chars)

private void writeStr(String* str, Codegen* cg) {
    ensureBufferLength(str->length + 2, cg);
    cg->buffer[cg->length] = aBacktick;
    memcpy(cg->buffer + cg->length + 1, str->content, str->length);
    cg->length += (str->length + 2);
    cg->buffer[cg->length - 1] = aBacktick;
}

private void writeBytesFromSource(Node nd, Codegen* cg) {
    writeBytes(cg->sourceCode->content + nd.startBt, nd.lenBts, cg);
}


private void writeExprProcessFirstArg(CgCall* top, Codegen* cg) {
    if (top->countArgs != 1) {
        return;
    }
    switch (top->emit) {
    case emitField:
        writeChar(aDot, cg);
        writeConstant(top->startInd, cg); return;
    case emitInfix:
        writeChar(aSpace, cg);
        writeBytes(cg->sourceCode->content + top->startInd, top->length, cg);
        writeChar(aSpace, cg); return;
    case emitInfixExternal:
        writeChar(aSpace, cg);
        writeConstant(top->startInd, cg);
        writeChar(aSpace, cg); return;
    case emitInfixDot:
        writeChar(aParenRight, cg);
        writeChar(aDot, cg);
        writeConstant(top->startInd, cg);
        writeChar(aParenLeft, cg); return;
    }
}


private void writeIndex(Int ind, Codegen* cg) {
    ensureBufferLength(40, cg);
    Int lenWritten = sprintf(cg->buffer + cg->length, "%d", ind);
    cg->length += lenWritten;
}


private void writeId(Node nd, Codegen* cg) {
    Entity ent = cg->cm->entities.content[nd.pl1];
    if (ent.emit == emitPrefix) {
        writeBytes(cg->sourceCode->content + nd.startBt, nd.lenBts, cg);
    } else if (ent.emit == emitPrefixExternal) {
        writeConstant(ent.externalNameId, cg);
    } else if (ent.emit == emitPrefixShielded) {
        writeBytes(cg->sourceCode->content + nd.startBt, nd.lenBts, cg);
        writeChar(aUnderscore, cg);
    }
}


private void writeExprOperand(Node n, Int countPrevArgs, Codegen* cg) {
    if (n.tp == nodId) {
        writeId(n, cg);
    } else if (n.tp == tokInt) {
        uint64_t upper = n.pl1;
        uint64_t lower = n.pl2;
        uint64_t total = (upper << 32) + lower;
        int64_t signedTotal = (int64_t)total;
        ensureBufferLength(45, cg);
        Int lenWritten = sprintf(cg->buffer + cg->length, "%d", signedTotal);
        cg->length += lenWritten;
    } else if (n.tp == tokString) {
        writeBytesFromSource(n, cg);
    } else if (n.tp == tokFloat) {
        uint64_t upper = n.pl1;
        uint64_t lower = n.pl2;
        int64_t total = (int64_t)((upper << 32) + lower);
        double floating = doubleOfLongBits(total);
        ensureBufferLength(45, cg);
        Int lenWritten = sprintf(cg->buffer + cg->length, "%f", floating);
        cg->length += lenWritten;
    } else if (n.tp == tokBool) {
        writeBytesFromSource(n, cg);
    }
}

/// Precondition: we are 1 past the expr node
private void writeExprWorker(Int sentinel, Arr(Node) nodes, Codegen* cg) {
    while (cg->i < sentinel) {
        Node n = nodes[cg->i];
        if (n.tp == nodCall) {
            Entity ent = cg->cm->entities.content[n.pl1];
            if (ent.emit == emitNop) {
                ++cg->i;
                continue;
            }
            CgCall new = (ent.emit == emitPrefix || ent.emit == emitInfix)
                        ? (CgCall){.emit = ent.emit, .arity = n.pl2, .countArgs = 0,
                                   .startInd = n.startBt, .length = n.lenBts }
                        : (CgCall){.emit = ent.emit, .arity = n.pl2, .countArgs = 0,
                                   .startInd = ent.externalNameId };

            if (n.pl2 == 0) { // 0 arity
                switch (ent.emit) {
                case emitPrefix:
                    writeBytesFromSource(n, cg); break;
                case emitPrefixExternal:
                    writeConstant(ent.externalNameId, cg); break;
                case emitPrefixShielded:
                    writeBytesFromSource(n, cg);
                    writeChar(aUnderscore, cg); break;
#ifdef SAFETY
                default:
                    throwExcInternal(iErrorZeroArityFuncWrongEmit, cg->cm);
#endif
                }
                writeChars(cg, ((byte[]){aParenLeft, aParenRight}));
                ++cg->i;
                continue;
            }
            switch (ent.emit) {
            case emitPrefix:
                writeBytesFromSource(n, cg);
                new.needClosingParen = true; break;
            case emitOverloaded:
                writeBytesFromSource(n, cg);
                writeChar(aUnderscore, cg);
                writeIndex(n.pl1, cg);
                new.needClosingParen = true; break;
            case emitPrefixExternal:
                writeConstant(ent.externalNameId, cg);
                new.needClosingParen = true; break;
            case emitPrefixShielded:
                writeBytesFromSource(n, cg);
                writeChar(aUnderscore, cg);
                new.needClosingParen = true; break;
            case emitField:
                new.needClosingParen = false; break;
            default:
                new.needClosingParen = cg->calls->length > 0;
            }
            if (new.needClosingParen) {
                writeChar(aParenLeft, cg);
            }
            pushCgCall(new, cg->calls);
        } else {
            CgCall* top = cg->calls->content + (cg->calls->length - 1);
            if (top->emit != emitInfix && top->emit != emitInfixExternal && top->countArgs > 0) {
                writeChars(cg, ((byte[]){aComma, aSpace}));
            }
            writeExprOperand(n, top->countArgs, cg);
            ++top->countArgs;
            writeExprProcessFirstArg(top, cg);
            while (top->countArgs == top->arity) {
                popCgCall(cg->calls);
                if (top->needClosingParen) {
                    writeChar(aParenRight, cg);
                }
                if (!hasValuesCgCall(cg->calls)) {
                    break;
                }
                CgCall* second = cg->calls->content + (cg->calls->length - 1);
                ++second->countArgs;
                writeExprProcessFirstArg(second, cg);
                top = second;
            }
        }
        ++cg->i;
    }
    cg->i = sentinel;
}

private void writeExprInternal(Node nd, Arr(Node) nodes, Codegen* cg) {
    /// Precondition: we are looking 1 past the nodExpr/singular node. Consumes all nodes of the expr
    if (nd.tp <= topVerbatimType) {
        writeBytes(cg->sourceCode->content + nd.startBt, nd.lenBts, cg);
    } else if (nd.tp == nodId) {
        writeId(nd, cg);
    } else {
        writeExprWorker(cg->i + nd.pl2, nodes, cg);
    }
#if SAFETY
    if(nd.tp != nodExpr) {
        throwExcInternal(iErrorExpressionIsNotAnExpr, cg->cm);
    }
#endif
}


private void writeIndentation(Codegen* cg) {
    ensureBufferLength(cg->indentation, cg);
    memset(cg->buffer + cg->length, aSpace, cg->indentation);
    cg->length += cg->indentation;
}


private void writeExpr(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    writeIndentation(cg);
    writeExprInternal(fr, nodes, cg);
    writeChars(cg, ((byte[]){ aSemicolon, aNewline}));
}


private void pushCgFrame(Node nd, Codegen* cg) {
    push(((Node){.tp = nd.tp, .pl2 = nd.pl2, .startBt = cg->i + nd.pl2}), &cg->backtrack);
}

private void pushCgFrameWithSentinel(Node nd, Int sentinel, Codegen* cg) {
    push(((Node){.tp = nd.tp, .pl2 = nd.pl2, .startBt = sentinel}), &cg->backtrack);
}


private void writeFn(Node nd, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    /// Writes a function definition
    if (isEntry) {
        pushCgFrame(nd, cg);
        writeIndentation(cg);
        writeConstantWithSpace(cgStrFunction, cg);

        Node fnBinding = nodes[cg->i];
        Entity fnEnt = cg->cm->entities.content[fnBinding.pl1];
        writeBytesFromSource(fnBinding, cg);
        if (fnEnt.emit == emitOverloaded) { // TODO replace with getting arity from type
            writeChar(aUnderscore, cg);
            writeIndex(fnBinding.pl1, cg);
        } else if (fnEnt.emit == emitPrefixShielded) {
            writeChar(aUnderscore, cg);
        }
        writeChar(aParenLeft, cg);
        Int sentinel = cg->i + nd.pl2;
        Int j = cg->i + 2; // +2 to skip the function binding node and nodScope
        if (nodes[j].tp == nodBinding) {
            Node binding = nodes[j];
            writeBytes(cg->sourceCode->content + binding.startBt, binding.lenBts, cg);
            ++j;
        }

        // function params
        while (j < sentinel && nodes[j].tp == nodBinding) {
            writeChar(aComma, cg);
            writeChar(aSpace, cg);
            Node binding = nodes[j];
            writeBytes(cg->sourceCode->content + binding.startBt, binding.lenBts, cg);
            ++j;
        }
        cg->i = j;
        writeChars(cg, ((byte[]){aParenRight, aSpace, aCurlyLeft, aNewline}));
        cg->indentation += 4;
    } else {
        cg->indentation -= 4;
        writeChars(cg, ((byte[]){aCurlyRight, aNewline, aNewline}));
    }

}

private void writeDummy(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {

}

private void writeAssignmentWorker(Arr(Node) nodes, Codegen* cg) {
    /// Pre-condition: we are looking at the binding node, 1 past the assignment node
    Node binding = nodes[cg->i];

    writeBytes(cg->sourceCode->content + binding.startBt, binding.lenBts, cg);
    writeChars(cg, ((byte[]){aSpace, aEqual, aSpace}));

    ++cg->i; // CONSUME the binding node
    Node rightSide = nodes[cg->i];
    if (rightSide.tp == nodId) {
        writeId(rightSide, cg);
        ++cg->i; // CONSUME the id node on the right side of the assignment
    } else {
        ++cg->i; // CONSUME the expr/verbatim node
        writeExprInternal(rightSide, nodes, cg);
    }
    writeChar(aSemicolon, cg);
    writeChar(aNewline, cg);
}


private void writeAssignment(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    Int sentinel = cg->i + fr.pl2;
    writeIndentation(cg);
    Node binding = nodes[cg->i];
    Int class = cg->cm->entities.content[binding.pl1].class;
    if (class == classMutatedGuaranteed || class == classMutatedNullable) {
        writeConstantWithSpace(cgStrLet, cg);
    } else {
        writeConstantWithSpace(cgStrConst, cg);
    }
    writeAssignmentWorker(nodes, cg);

    cg->i = sentinel; // CONSUME the whole assignment
}


private void writeReassignment(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    Int sentinel = cg->i + fr.pl2;
    writeIndentation(cg);

    writeAssignmentWorker(nodes, cg);

    cg->i = sentinel; // CONSUME the whole reassignment
}


private void writeReturn(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    Int sentinel = cg->i + fr.pl2;
    writeIndentation(cg);
    writeConstantWithSpace(cgStrReturn, cg);

    Node rightSide = nodes[cg->i];
    ++cg->i; // CONSUME the expr node
    writeExprInternal(rightSide, nodes, cg);

    writeChar(aSemicolon, cg);
    writeChar(aNewline, cg);
    cg->i = sentinel; // CONSUME the whole "return" statement
}


private void writeScope(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (isEntry) {
        writeIndentation(cg);
        writeChar(aCurlyLeft, cg);
        cg->indentation += 4;
        pushCgFrame(fr, cg);
    } else {
        cg->indentation -= 4;
        writeChar(aCurlyRight, cg);
    }
}


private void writeIf(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (!isEntry) {
        return;
    }
    pushCgFrame(fr, cg);
    writeIndentation(cg);
    writeConstantWithSpace(cgStrIf, cg);
    writeChar(aParenLeft, cg);

    Node expression = nodes[cg->i];
    ++cg->i; // CONSUME the expression node for the first condition
    writeExprInternal(expression, nodes, cg);
    writeChars(cg, ((byte[]){aParenRight, aSpace, aCurlyLeft}));
}


private void writeIfClause(Node nd, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (isEntry) {
        pushCgFrame(nd, cg);
        writeChar(aNewline, cg);
        cg->indentation += 4;
    } else {
        Int sentinel = nd.startBt;
        cg->indentation -= 4;
        writeIndentation(cg);
        writeChar(aCurlyRight, cg);
        Node top = peek(&cg->backtrack);
        Int ifSentinel = top.startBt;
        if (ifSentinel == sentinel) {
            writeChar(aNewline, cg);
            return;
        } else if (nodes[sentinel].tp == nodIfClause) {
            // there is only the "else" clause after this clause
            writeChar(aSpace, cg);
            writeConstantWithSpace(cgStrElse, cg);
            writeChar(aCurlyLeft, cg);
        } else {
            // there is another condition after this clause, so we write it out as an "else if"
            writeChar(aSpace, cg);
            writeConstantWithSpace(cgStrElse, cg);
            writeConstantWithSpace(cgStrIf, cg);
            writeChar(aParenLeft, cg);
            cg->i = sentinel; // CONSUME ???
            Node expression = nodes[cg->i];
            ++cg->i; // CONSUME the expr node for the "else if" clause
            writeExprInternal(expression, nodes, cg);
            writeChars(cg, ((byte[]){aParenRight, aSpace, aCurlyLeft}));
        }
    }
}


private void writeLoopLabel(Int labelId, Codegen* cg) {
    writeConstant(cgStrLo, cg);
    ensureBufferLength(14, cg);
    Int lenWritten = sprintf(cg->buffer + cg->length, "%d", labelId);
    cg->length += lenWritten;
}


private void writeWhile(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (isEntry) {
        if (fr.pl1 > 0) { // this loop requires a label
            writeIndentation(cg);
            writeLoopLabel(fr.pl1, cg);
            writeChars(cg, ((byte[]){ aColon, aNewline }));
        }
        Int sentinel = cg->i + fr.pl2;
        ++cg->i; // CONSUME the scope node immediately inside loop
        if (nodes[cg->i].tp == nodAssignment) { // there is at least one assignment, so an extra nested scope
            // noting in .pl1 that we have a two scopes
            push(((Node){.tp = fr.tp, .pl1 = 1, .pl2 = fr.pl2, .startBt = sentinel}), &cg->backtrack);
            writeIndentation(cg);
            writeChar(aCurlyLeft, cg);
            writeChar(aNewline, cg);

            while (cg->i < sentinel && nodes[cg->i].tp == nodAssignment) {
                ++cg->i; // CONSUME the assignment node
                writeIndentation(cg);
                writeConstantWithSpace(cgStrLet, cg);
                writeAssignmentWorker(nodes, cg);
            }
        } else {
            pushCgFrameWithSentinel(fr, sentinel, cg);
        }

        writeIndentation(cg);
        writeConstantWithSpace(cgStrWhile, cg);
        writeChar(aParenLeft, cg);
        Node cond = nodes[cg->i + 1];
        cg->i += 2; // CONSUME the loopCond node and expr/verbatim node
        writeExprInternal(cond, nodes, cg);

        writeChars(cg, ((byte[]){aParenRight, aSpace, aCurlyLeft, aNewline}));
        cg->indentation += 4;
    } else {
        cg->indentation -= 4;
        writeIndentation(cg);
        if (fr.pl1 > 0) {
            writeChars(cg, ((byte[]){aCurlyRight, aCurlyRight, aNewline}));
        } else {
            writeChars(cg, ((byte[]){aCurlyRight, aNewline}));
        }
    }
}


private void writeBreakContinue(Node fr, Int indKeyword, Codegen* cg) {
    writeIndentation(cg);
    if (fr.pl1 == -1) {
        writeConstant(indKeyword, cg);
    } else {
        writeConstantWithSpace(indKeyword, cg);
        writeLoopLabel(fr.pl1, cg);
    }
    writeChar(aSemicolon, cg);
    writeChar(aNewline, cg);
}


private void writeBreak(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    writeBreakContinue(fr, strBreak, cg);
}


private void writeContinue(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    writeBreakContinue(fr, strContinue, cg);
}


private void maybeCloseCgFrames(Codegen* cg) {
    for (Int j = cg->backtrack.length - 1; j > -1; j--) {
        if (cg->backtrack.content[j].startBt != cg->i) {
            return;
        }
        Node fr = pop(&cg->backtrack);
        (*cg->cgTable[fr.tp - nodScope])(fr, false, cg->cm->nodes.content, cg);
    }
}


private void tabulateCgDispatch(Codegen* cg) {
    for (Int j = 0; j < countSpanForms; j++) {
        cg->cgTable[j] = &writeDummy;
    }
    cg->cgTable[0] = writeScope;
    cg->cgTable[nodAssignment - nodScope] = &writeAssignment;
    cg->cgTable[nodReassign   - nodScope] = &writeReassignment;
    cg->cgTable[nodExpr       - nodScope] = &writeExpr;
    cg->cgTable[nodScope      - nodScope] = &writeScope;
    cg->cgTable[nodWhile      - nodScope] = &writeWhile;
    cg->cgTable[nodIf         - nodScope] = &writeIf;
    cg->cgTable[nodIfClause   - nodScope] = &writeIfClause;
    cg->cgTable[nodFnDef      - nodScope] = &writeFn;
    cg->cgTable[nodReturn     - nodScope] = &writeReturn;
    cg->cgTable[nodBreak      - nodScope] = &writeBreak;
    cg->cgTable[nodContinue   - nodScope] = &writeContinue;
}


private Codegen* createCodegen(Compiler* cm, Arena* a) {
    Codegen* cg = allocateOnArena(sizeof(Codegen), a);
    (*cg) = (Codegen) {
        .i = 0, .backtrack = *createStackNode(16, a), .calls = createStackCgCall(16, a),
        .length = 0, .capacity = 64, .buffer = allocateOnArena(64, a),
        .sourceCode = cm->sourceCode, .cm = cm, .a = a
    };
    tabulateCgDispatch(cg);
    return cg;
}


private Codegen* generateCode(Compiler* cm, Arena* a) {
#if defined(TRACE) && defined(TEST)
    printParser(cm, a);
#endif
    if (cm->wasError) {
        return NULL;
    }
    Codegen* cg = createCodegen(cm, a);
    const Int len = cm->nodes.length;
    while (cg->i < len) {
        Node nd = cm->nodes.content[cg->i];
        ++cg->i; // CONSUME the span node

        (cg->cgTable[nd.tp - nodScope])(nd, true, cg->cm->nodes.content, cg);
        maybeCloseCgFrames(cg);
    }
    return cg;
}

//}}}
//{{{ Utils for tests

#ifdef TEST

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

StandardText getStandardTextLength(void) {
    return (StandardText){
        .len = sizeof(standardText) - 1,
        .numNames = (sizeof(standardStrings)/4 - strFirstNonReserved - 1),
        .firstNonreserved = strFirstNonReserved};
}

String* prepareInput(const char* content, Arena* a) {
    /// Allocates a test input into an arena after prepending it with the standardText.
    if (content == NULL) return NULL;
    const char* ind = content;
    Int lenStandard = sizeof(standardText) - 1; // -1 for the invisible \0 char at end
    Int len = 0;
    for (; *ind != '\0'; ind++) {
        len++;
    }

    String* result = allocateOnArena(lenStandard + len + 1 + sizeof(String), a); // + 1 for the \0
    result->length = lenStandard + len;
    memcpy(result->content, standardText, lenStandard);
    memcpy(result->content + lenStandard, content, len + 1); // + 1 to copy the \0
    return result;
}

int equalityLexer(Compiler a, Compiler b) {
    /// Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first
    /// differing token otherwise
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.tokens.length < b.tokens.length ? a.tokens.length : b.tokens.length;
    int i = 0;
    for (; i < commonLength; i++) {
        Token tokA = a.tokens.content[i];
        Token tokB = b.tokens.content[i];
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
                printf("Diff in startBt, %d but was expected %d\n", tokA.startBt, tokB.startBt);
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
    return (a.tokens.length == b.tokens.length) ? -2 : i;
}

private void printExpSt(StackInt* st) {
    printIntArray(st->length, st->content);
}


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
    printf("Type [ind = %d, len = %d]\n", typeId, cm->types.content[typeId]);
    Int sentinel = typeId + cm->types.content[typeId] + 1;
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

    untt nameParamId = cm->types.content[typeId + 2];
    Int mainNameLen = (nameParamId >> 24) & 0xFF;
    Int mainName = nameParamId & LOWER24BITS;

    if (mainNameLen == 0) {
        printf("[$anonymous]");
    } else {
        printf("{");
        Int startBt = cm->stringTable->content[mainName];
        fwrite(cm->sourceCode->content + startBt, 1, mainNameLen, stdout);
        printf("}\n");
    }

    if (hdr.sort == sorStruct) {
        if (hdr.arity > 0) {
            printf("[Params: ");
            for (Int j = 0; j < hdr.arity; j++) {
                printf("%d ", cm->types.content[j + typeId + 3]);
            }
            printf("]");
        }
        printf("(");
        Int fieldsStart = typeId + hdr.arity + 3;
        Int fieldsSentinel = fieldsStart + hdr.depth;
        for (Int j = fieldsStart; j < fieldsSentinel; j++) {
            printf("name %d ", cm->types.content[j]);
        }
        fieldsStart += hdr.depth;
        fieldsSentinel += hdr.depth;
        for (Int j = fieldsStart; j < fieldsSentinel; j++) {
            typePrintGenElt(cm->types.content[j]);
        }

        printf("\n)");
    } else if (hdr.sort == sorFunction) {
        if (hdr.arity > 0) {
            printf("[Params: ");
            for (Int j = 0; j < hdr.arity; j++) {
                printf("%d ", cm->types.content[j + typeId + 3]);
            }
            printf("]");
        }
        printf("(");
        Int fnParamsStart = typeId + hdr.arity + 3;
        Int fnParamsSentinel = fnParamsStart + hdr.depth - 1;
        for (Int j = fnParamsStart; j < fnParamsSentinel; j++) {
            typePrintGenElt(cm->types.content[j]);
        }
        // now for the return type
        printf("=> ");
        typePrintGenElt(cm->types.content[fnParamsSentinel]);

        printf("\n)");

    } else if (hdr.sort == sorPartial) {
        printf("(\n    ");
        for (Int j = typeId + hdr.arity + 3; j < sentinel; j++) {
            typePrintGenElt(cm->types.content[j]);
        }
        printf("\n)");
    }
    print(")");
}

void overloadsPrint(Int nameId, Compiler* cm) {
    Int listId = -cm->activeBindings[nameId] - 1;
}

#endif

//}}}
//{{{ Main

Codegen* compile(String* source) {
    Arena* a = mkArena();
    Compiler* proto = createProtoCompiler(a);

    Compiler* lx = lexicallyAnalyze(source, proto, a);
    if (lx->wasError) {
        print("lexer error");
    }
#if defined(TRACE) && defined(TEST)
    printLexer(lx);
#endif

    Compiler* cm = parse(lx, proto, a);
    if (cm->wasError) {
        print("parser error");
    }
    Codegen* cg = generateCode(cm, a);
    return cg;
}

#ifndef TEST
Int main(int argc, char* argv) {
    Arena* a = mkArena();
    String* sourceCode = readSourceFile("_bin/code.tl", a);
    if (sourceCode == NULL) {
        goto cleanup;
    }
    printString(sourceCode);

    Codegen* cg = compile(sourceCode);
    if (cg != NULL) {
        fwrite(cg->buffer, 1, cg->length, stdout);
    }
    cleanup:
    deleteArena(a);
    return 0;
}
#endif

//}}}
