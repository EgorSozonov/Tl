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

//{{{ Utils

#ifdef DEBUG
#define DBG(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)
#else
#define DBG(fmt, ...) // empty
#endif      

jmp_buf excBuf;

//{{{ Arena

#define CHUNK_QUANT 32768

private size_t minChunkSize() {
    return (size_t)(CHUNK_QUANT - 32);
}

testable Arena* mkArena() {
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

/**
 * Calculates memory for a new chunk. Memory is quantized and is always 32 bytes less
 */
private size_t calculateChunkSize(size_t allocSize) {
    // 32 for any possible padding malloc might use internally,
    // so that the total allocation size is a good even number of OS memory pages.
    // TODO review
    size_t fullMemory = sizeof(ArenaChunk) + allocSize + 32; // struct header + main memory chunk + space for malloc bookkeep
    int mallocMemory = fullMemory < CHUNK_QUANT
                        ? CHUNK_QUANT
                        : (fullMemory % CHUNK_QUANT > 0
                           ? (fullMemory/CHUNK_QUANT + 1)*CHUNK_QUANT
                           : fullMemory);

    return mallocMemory - 32;
}

/**
 * Allocate memory in the arena, malloc'ing a new chunk if needed
 */
testable void* allocateOnArena(size_t allocSize, Arena* ar) {
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

      
DEFINE_STACK(int32_t)
DEFINE_STACK(BtToken)
DEFINE_STACK(ParseFrame)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Node)
DEFINE_INTERNAL_LIST(nodes, Node, a)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Entity)
DEFINE_INTERNAL_LIST(entities, Entity, a)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Int)
DEFINE_INTERNAL_LIST(overloads, Int, a)
DEFINE_INTERNAL_LIST(types, Int, a)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(uint32_t)
DEFINE_INTERNAL_LIST(overloadIds, uint32_t, aTmp)

DEFINE_INTERNAL_LIST_CONSTRUCTOR(EntityImport)
DEFINE_INTERNAL_LIST(imports, EntityImport, aTmp)

testable void printIntArray(Int count, Arr(Int) arr) {
    printf("[");
    for (Int k = 0; k < count; k++) {
        printf("%d ", arr[k]);
    }
    printf("]\n");
}

testable void printIntArrayOff(Int startInd, Int count, Arr(Int) arr) {
    printf("[...");
    for (Int k = 0; k < count; k++) {
        printf("%d ", arr[startInd + k]);
    }
    printf("...]\n");
}

//}}}
//{{{ Good strings

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

/**
 * Allocates a C string literal into an arena. The length of the literal is determined in O(N).
 */
testable String* str(const char* content, Arena* a) {
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

/** Does string "a" end with string "b"? */
testable bool endsWith(String* a, String* b) {
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

/** Tests if the following several bytes in the input match an array */
private bool testByteSequence(String* inp, int startBt, const byte letters[], int lengthLetters) {
    if (startBt + lengthLetters > inp->length) return false;

    for (int j = (lengthLetters - 1); j > -1; j--) {
        if (inp->content[startBt + j] != letters[j]) return false;
    }
    return true;
}

/** Tests if the following several bytes in the input match a word. Tests also that it is the whole word */
private bool testForWord(String* inp, int startBt, const byte letters[], int lengthLetters) {
    Int j = startBt + lengthLetters;
    if (j > inp->length) return false;
    
    if (j < inp->length && isAlphanumeric(inp->content[j])) {
        return false;        
    }

    for (j = (lengthLetters - 1); j > -1; j--) {
        if (inp->content[startBt + j] != letters[j]) return false;
    }
    return true;
}

String empty = { .length = 0 };
//}}}
//{{{ Stack

private void setSpanLengthParser(Int, Compiler* cm);

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

/** Throws an exception when key is absent */
private int getUnsafeIntMap(int key, IntMap* hm) {
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

private StringStore* createStringStore(int initSize, Arena* a) {
    StringStore* result = allocateOnArena(sizeof(StringStore), a);
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
        result = ((result << 5) + result) + p[i]; /* hash*33 + c */
    }

    return result;
}


private void addValueToBucket(Bucket** ptrToBucket, Int newIndString, Int lenBts, Arena* a) {
    Bucket* p = *ptrToBucket;                                        
    Int capacity = (p->capAndLen) >> 16;
    Int lenBucket = (p->capAndLen & 0xFFFF);
    if (lenBucket + 1 < capacity) {
        (*ptrToBucket)->content[lenBucket] = (StringValue){.length = lenBts, .indString = newIndString};
        p->capAndLen = (capacity << 16) + lenBucket + 1;
    } else {
        // TODO handle the case where we've overflowing the 16 bits of capacity
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + 2*capacity*sizeof(StringValue), a);
        memcpy(newBucket->content, p->content, capacity*sizeof(StringValue));
        
        Arr(StringValue) newValues = (StringValue*)newBucket->content;                 
        newValues[capacity] = (StringValue){.indString = newIndString, .length = lenBts};
        *ptrToBucket = newBucket;
        newBucket->capAndLen = ((2*capacity) << 16) + capacity + 1;      
    }
}

/** Unique'ing of symbols within source code */
private Int addStringStore(byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable, StringStore* hm) {
    Int hash = hashCode(text + startBt, lenBts) % (hm->dictSize);
    Int newIndString;
    if (*(hm->dict + hash) == NULL) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;
        
        newIndString = stringTable->length;
        push(startBt, stringTable);

        *firstElem = (StringValue){.length = lenBts, .indString = newIndString };
        *(hm->dict + hash) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hash);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == lenBts
                && memcmp(text + stringTable->content[stringValues[i].indString], text + startBt, lenBts) == 0) {
                // key already present                
                return stringValues[i].indString;
            }
        }
        
        newIndString = stringTable->length;
        push(startBt, stringTable);        
        addValueToBucket((hm->dict + hash), newIndString, lenBts, hm->a);
    }
    return newIndString;
}

/** Returns the index of a string within the string table, or -1 if it's not present */
testable Int getStringStore(byte* text, String* strToSearch, Stackint32_t* stringTable, StringStore* hm) {
    Int lenBts = strToSearch->length;
    Int hash = hashCode(strToSearch->content, lenBts) % (hm->dictSize);
    if (*(hm->dict + hash) == NULL) {
        return -1;
    } else {
        Bucket* p = *(hm->dict + hash);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == lenBts
                && memcmp(strToSearch->content, text + stringTable->content[stringValues[i].indString], lenBts) == 0) {
                return stringValues[i].indString;
            }
        }
        
        return -1;
    }
}

//{{{ Source code files

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

    /* Allocate our buffer to that size. */
    result = allocateOnArena(fileSize + 1 + sizeof(String), a);
    
    /* Go back to the start of the file. */
    if (fseek(file, 0L, SEEK_SET) != 0) {
        goto cleanup;
    }

    /* Read the entire file into memory. */
    size_t newLen = fread(result->content, 1, fileSize, file);
    if (ferror(file) != 0 ) {
        fputs("Error reading file", stderr);
    } else {
        result->content[newLen] = '\0'; /* Just to be safe. */
    }
    result->length = newLen;
    cleanup:
    fclose(file);
    return result;
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

//}}}
//{{{ Syntax errors
const char errNonAscii[]                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!";
const char errPrematureEndOfInput[]        = "Premature end of input";
const char errUnrecognizedByte[]           = "Unrecognized byte in source code!";
const char errWordChunkStart[]             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!";
const char errWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!";
const char errWordUnderscoresOnlyAtStart[] = "Underscores are only allowed at start of word (snake case is forbidden)!";
const char errWordWrongAccessor[]          = "Only regular identifier words may be used for data access with []!";
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
const char errPunctuationOnlyInMultiline[] = "The dot separator is only allowed in multi-line syntax forms like (: )";
const char errPunctuationUnmatched[]       = "Unmatched closing punctuation";
const char errPunctuationWrongOpen[]       = "Wrong opening punctuation";
const char errPunctuationScope[]           = "Scopes may only be opened in multi-line syntax forms";
const char errOperatorUnknown[]            = "Unknown operator";
const char errOperatorAssignmentPunct[]    = "Incorrect assignment operator: must be directly inside an ordinary statement, after the binding name!";
const char errOperatorTypeDeclPunct[]      = "Incorrect type declaration operator placement: must be the first in a statement!";
const char errOperatorMultipleAssignment[] = "Multiple assignment / type declaration operators within one statement are not allowed!";
const char errOperatorMutableDef[]         = "Definition of a mutable var should look like this: `mut x = 10`";
const char errCoreNotInsideStmt[]          = "Core form must be directly inside statement";
const char errCoreMisplacedArrow[]         = "The arrow separator (=>) must be inside an if, ifEq, ifPr or match form";
const char errCoreMisplacedElse[]          = "The else statement must be inside an if, ifEq, ifPr or match form";
const char errCoreMissingParen[]           = "Core form requires opening parenthesis/curly brace immediately after keyword!"; 
const char errDocComment[]                 = "Doc comments must have the syntax: (*comment)";
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
const char errTypeDeclCannotContain[]       = "Type declarations may only contain types (like Int), type params (like A), type constructors (like List) and parentheses!";
const char errTypeDeclError[]               = "Cannot parse type declaration!";
const char errUnknownType[]                 = "Unknown type";
const char errUnknownTypeFunction[]         = "Unknown type constructor";
const char errOperatorWrongArity[]          = "Wrong number of arguments for operator!";
const char errUnknownBinding[]              = "Unknown binding!";
const char errUnknownFunction[]             = "Unknown function!";
const char errOperatorUsedInappropriately[] = "Operator used in an inappropriate location!";
const char errAssignment[]                  = "Cannot parse assignment, it must look like `freshIdentifier` = `expression`";
const char errMutation[]                    = "Cannot parse mutation, it must look like `freshIdentifier` += `expression`";
const char errAssignmentShadowing[]         = "Assignment error: existing identifier is being shadowed";
const char errReturn[]                      = "Cannot parse return statement, it must look like `return ` {expression}";
const char errScope[]                       = "A scope may consist only of expressions, assignments, function definitions and other scopes!";
const char errLoopBreakOutside[]            = "The break keyword can only be used inside a loop scope!";
const char errTemp[]                        = "Temporary, delete it when finished";

//}}}

//{{{ Type errors

const char errTypeUnknownFirstArg[]         = "The type of first argument to a call must be known, otherwise I can't resolve the function overload!";
const char errTypeZeroArityOverload[]       = "A function with no parameters cannot be overloaded.";
const char errTypeNoMatchingOverload[]      = "No matching function overload was found";
const char errTypeWrongArgumentType[]       = "Wrong argument type";
const char errTypeWrongReturnType[]         = "Wrong return type";
const char errTypeMismatch[]                = "Declared type doesn't match actual type";
const char errTypeMustBeBool[]              = "Expression must have the Bool type";

//}}}
//}}}
//{{{ Lexer

//{{{ LexerConstants



/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
const byte maxInt[19] = (byte []){
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
};

/** 2**53 */
const byte maximumPreciselyRepresentedFloatingInt[16] = (byte []){ 9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2 };


/** The indices of reserved words that are stored in token pl2. Must be positive, unique,
 * and below "firstPunctuationTokenType"
 */
#define reservedFalse   2
#define reservedTrue    1
#define reservedAnd     3
#define reservedOr      4

/** Reserved words of Tl in ASCII byte form */

#define countReservedWords           30 // count of different reserved words below

static const byte reservedBytesAlias[]       = {  97, 108, 105, 97, 115 };
static const byte reservedBytesAnd[]         = {  97, 110, 100 };
static const byte reservedBytesAssert[]      = {  97, 115, 115, 101, 114, 116 };
static const byte reservedBytesAssertDbg[]   = {  97, 115, 115, 101, 114, 116, 68, 98, 103 };
static const byte reservedBytesAwait[]       = {  97, 119,  97, 105, 116 };
static const byte reservedBytesBreak[]       = {  98, 114, 101,  97, 107 };
static const byte reservedBytesCatch[]       = {  99,  97, 116,  99, 104 };
static const byte reservedBytesContinue[]    = {  99, 111, 110, 116, 105, 110, 117, 101 };
static const byte reservedBytesDefer[]       = { 100, 101, 102, 101, 114 };
static const byte reservedBytesDef[]         = { 100, 101, 102, };
static const byte reservedBytesPublicDef[]   = { 100, 101,  70, };
static const byte reservedBytesDo[]          = { 100, 111 };
static const byte reservedBytesElse[]        = { 101, 108, 115, 101 };
static const byte reservedBytesEmbed[]       = { 101, 109,  98, 101, 100 };
static const byte reservedBytesFalse[]       = { 102,  97, 108, 115, 101 };
static const byte reservedBytesFor[]         = { 102, 111, 114 };
static const byte reservedBytesIf[]          = { 105, 102 };
static const byte reservedBytesIfPr[]        = { 105, 102,  80, 114 };
static const byte reservedBytesImpl[]        = { 105, 109, 112, 108 };
static const byte reservedBytesImport[]      = { 105, 109, 112, 111, 114, 116 };
static const byte reservedBytesInterface[]   = { 105, 110, 116, 101, 114, 102, 97, 99, 101 };
static const byte reservedBytesLambda[]      = { 108,  97, 109 };
static const byte reservedBytesMatch[]       = { 109,  97, 116,  99, 104 };
static const byte reservedBytesOr[]          = { 111, 114 };
static const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
static const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
static const byte reservedBytesTry[]         = { 116, 114, 121 };
static const byte reservedBytesYield[]       = { 121, 105, 101, 108, 100 };
static const byte reservedBytesWhile[]       = { 119, 104, 105, 108, 101 };

/** All the symbols an operator may start with. "-" is absent because it's handled by lexMinus, "=" is handled by lexEqual
 */
const int operatorStartSymbols[15] = {
    aExclamation, aSharp, aPercent, aAmp, aApostrophe, aTimes, aPlus, aComma, aDivBy, 
    aSemicolon, aLT, aGT, aQuestion, aCaret, aPipe
};

//}}}


#define CURR_BT source[lx->i]
#define NEXT_BT source[lx->i + 1]
#define VALIDATEI(cond, errInd) if (!(cond)) { throwExcInternal0(errInd, __LINE__, cm); }
#define VALIDATEL(cond, errMsg) if (!(cond)) { throwExcLexer(errMsg, lx); }


typedef union {
    uint64_t i;
    double   d;
} FloatingBits;

_Noreturn private void throwExcInternal0(Int errInd, Int lineNumber, Compiler* cm) {   
    cm->wasError = true;    
    printf("Internal error %d at %d\n", errInd, lineNumber);  
    cm->errMsg = stringOfInt(errInd, cm->a);
    printString(cm->errMsg);
    longjmp(excBuf, 1);
}

#define throwExcInternal(errInd, cm) throwExcInternal0(errInd, __LINE__, cm)


/** Sets i to beyond input's length to communicate to callers that lexing is over */
_Noreturn private void throwExcLexer(const char errMsg[], Compiler* lx) {   
    lx->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d: %s\n", lx->i, errMsg);
#endif    
    lx->errMsg = str(errMsg, lx->a);
    longjmp(excBuf, 1);
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private void checkPrematureEnd(Int requiredSymbols, Compiler* lx) {
    VALIDATEL(lx->i + requiredSymbols <= lx->inpLength, errPrematureEndOfInput)
}


testable void add(Token t, Compiler* lx) {
    lx->tokens[lx->nextInd] = t;
    ++lx->nextInd;
    if (lx->nextInd == lx->capacity) {
        Token* newStorage = allocateOnArena(lx->capacity*2*sizeof(Token), lx->a);
        memcpy(newStorage, lx->tokens, (lx->capacity)*(sizeof(Token)));
        lx->tokens = newStorage;
        lx->capacity *= 2;
    }
}

/** For all the dollars at the top of the backtrack, turns them into parentheses, sets their lengths and closes them */
private void closeColons(Compiler* lx) {
    while (hasValues(lx->lexBtrack) && peek(lx->lexBtrack).wasOrigDollar) {
        BtToken top = pop(lx->lexBtrack);
        Int j = top.tokenInd;        
        lx->tokens[j].lenBts = lx->i - lx->tokens[j].startBt;
        lx->tokens[j].pl2 = lx->nextInd - j - 1;
    }
}

/**
 * Finds the top-level punctuation opener by its index, and sets its lengths.
 * Called when the matching closer is lexed.
 */
private void setSpanLengthLexer(Int tokenInd, Compiler* lx) {
    lx->tokens[tokenInd].lenBts = lx->i - lx->tokens[tokenInd].startBt + 1;
    lx->tokens[tokenInd].pl2 = lx->nextInd - tokenInd - 1;
}

/**
 * Correctly calculates the lenBts for a single-line, statement-type span.
 */
private void setStmtSpanLength(Int topTokenInd, Compiler* lx) {
    Token lastToken = lx->tokens[lx->nextInd - 1];
    Int byteAfterLastToken = lastToken.startBt + lastToken.lenBts;

    // This is for correctly calculating lengths of statements when they are ended by parens, even if there is a gap before ")"
    Int byteAfterLastPunct = lx->lastClosingPunctInd + 1;
    Int lenBts = (byteAfterLastPunct > byteAfterLastToken ? byteAfterLastPunct : byteAfterLastToken) 
                    - lx->tokens[topTokenInd].startBt;

    lx->tokens[topTokenInd].lenBts = lenBts;
    lx->tokens[topTokenInd].pl2 = lx->nextInd - topTokenInd - 1;
}


#define PROBERESERVED(reservedBytesName, returnVarName)    \
    lenReser = sizeof(reservedBytesName); \
    if (lenBts == lenReser && testForWord(lx->sourceCode, startBt, reservedBytesName, lenReser)) \
        return returnVarName;


private Int determineReservedA(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesAlias, tokAlias)
    PROBERESERVED(reservedBytesAnd, reservedAnd)
    PROBERESERVED(reservedBytesAwait, tokAwait)
    PROBERESERVED(reservedBytesAssert, tokAssert)
    PROBERESERVED(reservedBytesAssertDbg, tokAssertDbg)
    return 0;
}

private Int determineReservedB(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesBreak, tokBreak)
    return 0;
}

private Int determineReservedC(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesCatch, tokCatch)
    PROBERESERVED(reservedBytesContinue, tokContinue)
    return 0;
}

private Int determineReservedD(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesDefer, tokDefer)
    PROBERESERVED(reservedBytesDef, tokDef)
    PROBERESERVED(reservedBytesPublicDef, tokPublicDef)
    PROBERESERVED(reservedBytesDo, tokScope)
    return 0;
}


private Int determineReservedE(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesElse, tokElse)
    PROBERESERVED(reservedBytesEmbed, tokEmbed)
    return 0;
}


private Int determineReservedF(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesFalse, reservedFalse)
    PROBERESERVED(reservedBytesFor, tokFor)
    return 0;
}


private Int determineReservedI(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesIf, tokIf)
    PROBERESERVED(reservedBytesIfPr, tokIfPr)
    PROBERESERVED(reservedBytesImpl, tokImpl)
    PROBERESERVED(reservedBytesImport, tokImport)
    PROBERESERVED(reservedBytesInterface, tokIface)
    return 0;
}


private Int determineReservedL(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesLambda, tokLambda)
    PROBERESERVED(reservedBytesWhile, tokWhile)
    return 0;
}


private Int determineReservedM(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesMatch, tokMatch)
    return 0;
}


private Int determineReservedO(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesOr, reservedOr)
    return 0;
}


private Int determineReservedR(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesReturn, tokReturn)
    return 0;
}


private Int determineReservedS(Int startBt, Int lenBts, Compiler* lx) {
    return 0;
}


private Int determineReservedT(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesTrue, reservedTrue)
    PROBERESERVED(reservedBytesTry, tokTry)
    return 0;
}


private Int determineReservedY(Int startBt, Int lenBts, Compiler* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesYield, tokYield)
    return 0;
}

private Int determineUnreserved(Int startBt, Int lenBts, Compiler* lx) {
    return -1;
}


private void addNewLine(Int j, Compiler* lx) {
    lx->newlines[lx->newlinesNextInd] = j;
    lx->newlinesNextInd++;
    if (lx->newlinesNextInd == lx->newlinesCapacity) {
        Arr(Int) newNewlines = allocateOnArena(lx->newlinesCapacity*2*4, lx->a);
        memcpy(newNewlines, lx->newlines, lx->newlinesCapacity*4);
        lx->newlines = newNewlines;
        lx->newlinesCapacity *= 2;
    }
}

private void addStatement(untt stmtType, Int startBt, Compiler* lx) {
    Int lenBts = 0;
    // Some types of statements may legitimately consist of 0 tokens; for them, we need to write their lenBts in the init token
    if (stmtType == tokBreak) {
        lenBts = 5;
    } else if (stmtType == tokContinue) {
        lenBts = 8;
    }
    push(((BtToken){ .tp = stmtType, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->lexBtrack);
    add((Token){ .tp = stmtType, .startBt = startBt, .lenBts = lenBts}, lx);
}


private void wrapInAStatementStarting(Int startBt, Compiler* lx, Arr(byte) source) {    
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel <= slParenMulti) {            
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->lexBtrack);
            add((Token){ .tp = tokStmt, .startBt = startBt},  lx);
        }
    } else {
        addStatement(tokStmt, startBt, lx);
    }
}


private void wrapInAStatement(Compiler* lx, Arr(byte) source) {
    if (hasValues(lx->lexBtrack)) {
        if (peek(lx->lexBtrack).spanLevel <= slParenMulti) {
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->lexBtrack);
            add((Token){ .tp = tokStmt, .startBt = lx->i},  lx);
        }
    } else {
        addStatement(tokStmt, lx->i, lx);
    }
}


private void maybeBreakStatement(Compiler* lx) {
    if (hasValues(lx->lexBtrack)) {
        BtToken top = peek(lx->lexBtrack);
        if(top.spanLevel == slStmt) {
            setStmtSpanLength(top.tokenInd, lx);
            pop(lx->lexBtrack);
        }
    }
}

/**
 * Processes a right paren which serves as the closer of all punctuation scopes.
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private void closeRegularPunctuation(Int closingType, Compiler* lx) {
    StackBtToken* bt = lx->lexBtrack;
    closeColons(lx);
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);
    // since a closing bracket might be closing something with statements inside it, like a lex scope
    // or a core syntax form, we need to close the last statement before closing its parent
    if (bt->length > 0 && top.spanLevel != slScope 
          && (bt->content[bt->length - 1].spanLevel <= slParenMulti)) {
        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }
    setSpanLengthLexer(top.tokenInd, lx);
    lx->i++; // CONSUME the closing ")"
}


private void addNumeric(byte b, Compiler* lx) {
    if (lx->numericNextInd < lx->numericCapacity) {
        lx->numeric[lx->numericNextInd] = b;
    } else {
        Arr(Int) new = allocateOnArena(lx->numericCapacity*8, lx->a);
        memcpy(new, lx->numeric, lx->numericCapacity*4);
        new[lx->numericCapacity] = b;
        
        lx->numeric = new;
        lx->numericCapacity *= 2;       
    }
    lx->numericNextInd++;
}


private int64_t calcIntegerWithinLimits(Compiler* lx) {
    int64_t powerOfTen = (int64_t)1;
    int64_t result = 0;
    Int j = lx->numericNextInd - 1;

    Int loopLimit = -1;
    while (j > loopLimit) {
        result += powerOfTen*lx->numeric[j];
        powerOfTen *= 10;
        j--;
    }
    return result;
}

/**
 * Checks if the current numeric <= b if they are regarded as arrays of decimal digits (0 to 9).
 */
private bool integerWithinDigits(const byte* b, Int bLength, Compiler* lx){
    if (lx->numericNextInd != bLength) return (lx->numericNextInd < bLength);
    for (Int j = 0; j < lx->numericNextInd; j++) {
        if (lx->numeric[j] < b[j]) return true;
        if (lx->numeric[j] > b[j]) return false;
    }
    return true;
}


private Int calcInteger(int64_t* result, Compiler* lx) {
    if (lx->numericNextInd > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lx)) return -1;
    *result = calcIntegerWithinLimits(lx);
    return 0;
}


private int64_t calcHexNumber(Compiler* lx) {
    int64_t result = 0;
    int64_t powerOfSixteen = 1;
    Int j = lx->numericNextInd - 1;

    // If the literal is full 16 bits long, then its upper sign contains the sign bit
    Int loopLimit = -1; //if (byteBuf.ind == 16) { 0 } else { -1 }
    while (j > loopLimit) {
        result += powerOfSixteen*lx->numeric[j];
        powerOfSixteen = powerOfSixteen << 4;
        j--;
    }
    return result;
}

/**
 * Lexes a hexadecimal numeric literal (integer or floating-point).
 * Examples of accepted expressions: 0xCAFE_BABE, 0xdeadbeef, 0x123_45A
 * Examples of NOT accepted expressions: 0xCAFE_babe, 0x_deadbeef, 0x123_
 * Checks that the input fits into a signed 64-bit fixnum.
 * TODO add floating-pointt literals like 0x12FA.
 */
private void hexNumber(Compiler* lx, Arr(byte) source) {
    checkPrematureEnd(2, lx);
    lx->numericNextInd = 0;
    Int j = lx->i + 2;
    while (j < lx->inpLength) {
        byte cByte = source[j];
        if (isDigit(cByte)) {
            addNumeric(cByte - aDigit0, lx);
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            addNumeric(cByte - aALower + 10, lx);
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            addNumeric(cByte - aAUpper + 10, lx);
        } else if (cByte == aUnderscore && (j == lx->inpLength - 1 || isHexDigit(source[j + 1]))) {            
            throwExcLexer(errNumericEndUnderscore, lx);            
        } else {
            break;
        }
        VALIDATEL(lx->numericNextInd <= 16, errNumericBinWidthExceeded)
        j++;
    }
    int64_t resultValue = calcHexNumber(lx);
    add((Token){ .tp = tokInt, .pl1 = resultValue >> 32, .pl2 = resultValue & LOWER32BITS, 
                .startBt = lx->i, .lenBts = j - lx->i }, lx);
    lx->numericNextInd = 0;
    lx->i = j; // CONSUME the hex number
}

/**
 * Parses the floating-pointt numbers using just the "fast path" of David Gay's "strtod" function,
 * extended to 16 digits.
 * I.e. it handles only numbers with 15 digits or 16 digits with the first digit not 9,
 * and decimal powers within [-22; 22].
 * Parsing the rest of numbers exactly is a huge and pretty useless effort. Nobody needs these
 * floating literals in text form: they are better input in binary, or at least text-hex or text-binary.
 * Input: array of bytes that are digits (without leading zeroes), and the negative power of ten.
 * So for '0.5' the input would be (5 -1), and for '1.23000' (123000 -5).
 * Example, for input text '1.23' this function would get the args: ([1 2 3] 1)
 * Output: a 64-bit floating-pointt number, encoded as a long (same bits)
 */
private Int calcFloating(double* result, Int powerOfTen, Compiler* lx, Arr(byte) source){
    Int indTrailingZeroes = lx->numericNextInd - 1;
    Int ind = lx->numericNextInd;
    while (indTrailingZeroes > -1 && lx->numeric[indTrailingZeroes] == 0) { 
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

    if (significantCan < significandNeeds || significantCan < exponentNeeds || significandNeeds > exponentCanAccept) {
        return -1;
    }

    // Transfer of decimal powers from significand to exponent to make them both fit within their respective limits
    // (10000 -6) -> (1 -2); (10000 -3) -> (10 0)
    Int transfer = (significandNeeds >= exponentNeeds) ? (
             (ind - significandNeeds == 16 && significandNeeds < significantCan && significandNeeds + 1 <= exponentCanAccept) ? 
                (significandNeeds + 1) : significandNeeds
        ) : exponentNeeds;
    lx->numericNextInd -= transfer;
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

/**
 * Lexes a decimal numeric literal (integer or floating-point). Adds a token.
 * TODO: add support for the '1.23E4' format
 */
private void decNumber(bool isNegative, Compiler* lx, Arr(byte) source) {
    Int j = (isNegative) ? (lx->i + 1) : lx->i;
    Int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
    bool metDot = false;
    bool metNonzero = false;
    Int maximumInd = (lx->i + 40 > lx->inpLength) ? (lx->i + 40) : lx->inpLength;
    while (j < maximumInd) {
        byte cByte = source[j];

        if (isDigit(cByte)) {
            if (metNonzero) {
                addNumeric(cByte - aDigit0, lx);
            } else if (cByte != aDigit0) {
                metNonzero = true;
                addNumeric(cByte - aDigit0, lx);
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
        add((Token){ .tp = tokFloat, .pl1 = (bitsOfFloat >> 32), .pl2 = (bitsOfFloat & LOWER32BITS),
                    .startBt = lx->i, .lenBts = j - lx->i}, lx);
    } else {
        int64_t resultValue = 0;
        Int errorCode = calcInteger(&resultValue, lx);
        VALIDATEL(errorCode == 0, errNumericIntWidthExceeded)

        if (isNegative) resultValue = -resultValue;
        add((Token){ .tp = tokInt, .pl1 = resultValue >> 32, .pl2 = resultValue & LOWER32BITS, 
                .startBt = lx->i, .lenBts = j - lx->i }, lx);
    }
    lx->i = j; // CONSUME the decimal number
}


private void lexNumber(Compiler* lx, Arr(byte) source) {
    wrapInAStatement(lx, source);
    byte cByte = CURR_BT;
    if (lx->i == lx->inpLength - 1 && isDigit(cByte)) {
        add((Token){ .tp = tokInt, .pl2 = cByte - aDigit0, .startBt = lx->i, .lenBts = 1 }, lx);
        lx->i++; // CONSUME the single-digit number
        return;
    }
    
    byte nByte = NEXT_BT;
    if (nByte == aXLower) {
        hexNumber(lx, source);
    } else {
        decNumber(false, lx, source);
    }
    lx->numericNextInd = 0;
}

/**
 * Adds a token which serves punctuation purposes, i.e. either a ( or  a [
 * These tokens are used to define the structure, that is, nesting within the AST.
 * Upon addition, they are saved to the backtracking stack to be updated with their length
 * once it is known.
 * Consumes no bytes.
 */
private void openPunctuation(untt tType, untt spanLevel, Int startBt, Compiler* lx) {
    push( ((BtToken){ .tp = tType, .tokenInd = lx->nextInd, .spanLevel = spanLevel}), lx->lexBtrack);
    add((Token) {.tp = tType, .pl1 = (tType < firstParenSpanTokenType) ? 0 : spanLevel, .startBt = startBt }, lx);
}

/**
 * Lexer action for a paren-type reserved word. It turns parentheses into an slParenMulti core form.
 * If necessary (parens inside statement) it also deletes last token and removes the top frame
 * Precondition: we are looking at the character immediately after the keyword
 */
private void lexReservedWord(untt reservedWordType, Int startBt, Compiler* lx, Arr(byte) source) {    
    StackBtToken* bt = lx->lexBtrack;
    if (reservedWordType >= firstParenSpanTokenType) { // the "core(" case
        VALIDATEL(lx->i < lx->inpLength && CURR_BT == aParenLeft, errCoreMissingParen)
        
        BtToken top = peek(bt);
        // update the token type and the corresponding frame type
        lx->tokens[top.tokenInd].tp = reservedWordType;
        lx->tokens[top.tokenInd].pl1 = slParenMulti;
        bt->content[bt->length - 1].tp = reservedWordType;
        bt->content[bt->length - 1].spanLevel = top.tp == tokScope ? slScope : slParenMulti;
    } else if (reservedWordType >= firstSpanTokenType) {
        VALIDATEL(!hasValues(bt) || peek(bt).spanLevel == slScope, errCoreNotInsideStmt)
        addStatement(reservedWordType, startBt, lx);
    }
}

/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was capitalized
 */
private bool wordChunk(Compiler* lx, Arr(byte) source) {
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

/** Closes the current statement. Consumes no tokens */
private void closeStatement(Compiler* lx) {
    BtToken top = peek(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errPunctuationOnlyInMultiline)
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->lexBtrack);
    }    
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Precondition: we are pointing at the first letter character of the word (i.e. past the possible "@" or ":")
 * Examples of acceptable expressions: A-B-c-d, asdf123, ab-_cd45
 * Examples of unacceptable expressions: 1asdf23, ab-cd_45
 */
private void wordInternal(untt wordType, Compiler* lx, Arr(byte) source) {
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

    Int realStartByte = (wordType == tokWord) ? startBt : (startBt - 1); // accounting for the initial . or :
    byte firstByte = lx->sourceCode->content[startBt];
    Int lenBts = lx->i - realStartByte;
    Int lenString = lx->i - startBt;
        
    Int mbReservedWord = -1; 
    if (firstByte >= aALower && firstByte <= aYLower) {
        mbReservedWord = (*lx->langDef->possiblyReservedDispatch)[firstByte - aALower](startBt, lenString, lx);
    }
    if (mbReservedWord <= 0) { // a normal, unreserved word
        Int uniqueStringInd = addStringStore(source, startBt, lenString, lx->stringTable, lx->stringStore);
        if (wordType == tokAccessor) {
            add((Token){ .tp=tokAccessor, .pl1 = tkAccDot, .pl2 = uniqueStringInd, 
                         .startBt = realStartByte, .lenBts = lenBts }, lx);
        } else {
            wrapInAStatementStarting(startBt, lx, source);
            untt finalTokType = wasCapitalized ? tokTypeName : wordType;
            if (finalTokType == tokWord && lx->i < lx->inpLength && CURR_BT == aParenLeft) {
                add((Token){ .tp=tokCall, .pl1 = (wasCapitalized ? 1 : 0), .pl2 = uniqueStringInd, 
                             .startBt = realStartByte, .lenBts = lenBts }, lx);
            } else { 
                add((Token){ .tp=finalTokType, .pl2 = uniqueStringInd, 
                             .startBt = realStartByte, .lenBts = lenBts }, lx);
            }
        }
        return;
    }

    VALIDATEL(wordType != tokAccessor, errWordReservedWithDot)

    if (mbReservedWord == tokElse){
        closeStatement(lx);
        add((Token){.tp = tokElse, .startBt = realStartByte, .lenBts = 4}, lx);
    } else if (mbReservedWord < firstSpanTokenType) {
        if (mbReservedWord == reservedAnd) {
            wrapInAStatementStarting(startBt, lx, source);
            add((Token){.tp=tokOperator, .pl1 = opTAnd, .startBt=realStartByte, .lenBts=3}, lx);
        } else if (mbReservedWord == reservedOr) {
            wrapInAStatementStarting(startBt, lx, source);
            add((Token){.tp=tokOperator, .pl1 = opTOr, .startBt=realStartByte, .lenBts=2}, lx);
        } else if (mbReservedWord == reservedTrue) {
            wrapInAStatementStarting(startBt, lx, source);
            add((Token){.tp=tokBool, .pl2=1, .startBt=realStartByte, .lenBts=4}, lx);
        } else if (mbReservedWord == reservedFalse) {
            wrapInAStatementStarting(startBt, lx, source);
            add((Token){.tp=tokBool, .pl2=0, .startBt=realStartByte, .lenBts=5}, lx);
        }
    } else {
        lexReservedWord(mbReservedWord, realStartByte, lx, source);
    }
}


private void lexWord(Compiler* lx, Arr(byte) source) {
    wordInternal(tokWord, lx, source);
}

/** 
 * The dot is a statement separator or the start of a field accessor
 */
private void lexDot(Compiler* lx, Arr(byte) source) {
    if (lx->i < lx->inpLength - 1 && isLetter(NEXT_BT)) {        
        ++lx->i; // CONSUME the dot
        wordInternal(tokAccessor, lx, source);
    } else if (hasValues(lx->lexBtrack) && peek(lx->lexBtrack).spanLevel != slScope) {
        closeStatement(lx);
        ++lx->i;  // CONSUME the dot
    }
}

/**
 * Handles the "=", ":=" and "+=" tokens. 
 * Changes existing tokens and parent span to accout for the fact that we met an assignment operator 
 * mutType = 0 if it's immutable assignment, 1 if it's ":=", 2 if it's a regular operator mut "+="
 */
private void processAssignment(Int mutType, untt opType, Compiler* lx) {
    BtToken currSpan = peek(lx->lexBtrack);

    if (currSpan.tp == tokAssignment || currSpan.tp == tokReassign || currSpan.tp == tokMutation) {
        throwExcLexer(errOperatorMultipleAssignment, lx);
    } else if (currSpan.spanLevel != slStmt) {
        throwExcLexer(errOperatorAssignmentPunct, lx);
    }
    Int tokenInd = currSpan.tokenInd;
    Token* tok = (lx->tokens + tokenInd);   
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

/**
 * A single $ means "parentheses until the next closing paren or end of statement"
 */
private void lexDollar(Compiler* lx, Arr(byte) source) {           
    push(((BtToken){ .tp = tokParens, .tokenInd = lx->nextInd, .spanLevel = slSubexpr, .wasOrigDollar = true}),
         lx->lexBtrack);
    add((Token) {.tp = tokParens, .startBt = lx->i }, lx);
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
    add((Token) {.tp = tokColon, .startBt = lx->i, .lenBts = 1 }, lx);
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
        add((Token){ .tp = tokOperator, .pl1 = opType, .startBt = lx->i, .lenBts = j - lx->i}, lx);
    }
    lx->i = j; // CONSUME the operator
}

/** The humble "=" can be the definition statement, a marker that separates signature from definition, or an arrow "=>" */
private void lexEqual(Compiler* lx, Arr(byte) source) {
    checkPrematureEnd(2, lx);
    byte nextBt = NEXT_BT;
    if (nextBt == aEqual) {
        lexOperator(lx, source); // ==        
    } else if (nextBt == aGT) { // => is a statement terminator inside if-like scopes        
        // arrows are only allowed inside "if"s and the like
        VALIDATEL(lx->lexBtrack->length >= 2, errCoreMisplacedArrow)
        
        BtToken grandparent = lx->lexBtrack->content[lx->lexBtrack->length - 2];
        VALIDATEL(grandparent.tp == tokIf, errCoreMisplacedArrow)
        closeStatement(lx);
        add((Token){ .tp = tokColon, .startBt = lx->i, .lenBts = 2 }, lx);
        lx->i += 2;  // CONSUME the arrow "=>"
    } else {
        processAssignment(0, 0, lx);
        lx->i++; // CONSUME the =
    }
}

/** 
 * Tl is not indentation-sensitive, but it is newline-sensitive. Thus, a newline charactor closes the current
 * statement unless it's inside an inline span (i.e. parens or accessor parens)
 */
private void lexNewline(Compiler* lx, Arr(byte) source) {
    addNewLine(lx->i, lx);    
    maybeBreakStatement(lx);
    
    lx->i++;     // CONSUME the LF
    while (lx->i < lx->inpLength) {
        if (CURR_BT != aSpace && CURR_BT != aTab && CURR_BT != aCarrReturn) {
            break;
        }
        lx->i++; // CONSUME a space or tab
    }
}

/** Doc comments, syntax is "---Doc comment". Pre-condition: we are pointing at the third minus */
private void lexDocComment(Compiler* lx, Arr(byte) source) {
    Int startBt = lx->i - 2; // -2 for the two minuses that have already been consumed
    
    for (; lx->i < lx->inpLength; ++lx->i) {
        if (CURR_BT == aNewline) {
            lexNewline(lx, source);
            break;
        }
    }
    Int lenTokens = lx->nextInd;
    if (lenTokens > 0 && lx->tokens[lenTokens - 1].tp == tokDocComment) {
        lx->tokens[lenTokens - 1].lenBts = lx->i - lx->tokens[lenTokens - 1].startBt;
    } else {
        add((Token){.tp = tokDocComment, .startBt = startBt, .lenBts = lx->i - startBt}, lx);
    }
}

/** Either an ordinary until-line-end comment (which doesn't get included in the AST, just discarded 
 * by the lexer) or a doc-comment.
 * Just like a newline, this needs to test if we're in a breakable statement because a comment goes until the line end.
 */
private void lexComment(Compiler* lx, Arr(byte) source) {    
    lx->i += 2; // CONSUME the "--"
    if (lx->i >= lx->inpLength) {
        return;
    }
    if (CURR_BT == aMinus) {
        lexDocComment(lx, source);
    } else {
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
}

/** Handles the binary operator as well as the unary negation operator and the in-line comments */
private void lexMinus(Compiler* lx, Arr(byte) source) {
    if (lx->i == lx->inpLength - 1) {        
        lexOperator(lx, source);
    } else {
        byte nextBt = NEXT_BT;
        if (isDigit(nextBt)) {
            wrapInAStatement(lx, source);
            decNumber(true, lx, source);
            lx->numericNextInd = 0;
        } else if (isLowercaseLetter(nextBt) || nextBt == aUnderscore) {
            add((Token){.tp = tokOperator, .pl1 = opTNegation, .startBt = lx->i, .lenBts = 1}, lx);
            lx->i++; // CONSUME the minus symbol
        } else if (nextBt == aMinus) {
            lexComment(lx, source);
        } else {
            lexOperator(lx, source);
        }    
    }
}

/** An opener for a scope or a scopeful core form. Precondition: we are past the "(:" token.
 * Consumes zero or 1 byte
 */
private void openScope(Compiler* lx, Arr(byte) source) {
    Int startBt = lx->i;
    lx->i += 2; // CONSUME the "(*"
    VALIDATEL(!hasValues(lx->lexBtrack) || peek(lx->lexBtrack).spanLevel == slScope, errPunctuationScope)
    VALIDATEL(lx->i < lx->inpLength, errPrematureEndOfInput)
    byte currBt = CURR_BT;
    if (currBt == aWLower && testForWord(lx->sourceCode, lx->i, reservedBytesWhile, 5)) {
        openPunctuation(tokWhile, slScope, startBt, lx);
        lx->i += 5; // CONSUME the "while"
        return; 
    } else if (lx->i < lx->inpLength - 2 && isSpace(source[lx->i + 1])) {        
        if (currBt == aFLower) {
            openPunctuation(tokDef, slScope, startBt, lx);
            lx->i += 2; // CONSUME the "f "
            return;
        } else if (currBt == aILower) {
            openPunctuation(tokIf, slScope, startBt, lx);
            lx->i += 2; // CONSUME the "i "
            return;
        }  
    }        
    openPunctuation(tokScope, slScope, startBt, lx);    
}

/** Handles the "(*" case (scope) as well as the common (subexpression) case */
private void lexParenLeft(Compiler* lx, Arr(byte) source) {
    Int j = lx->i + 1;
    VALIDATEL(j < lx->inpLength, errPunctuationUnmatched)
    if (source[j] == aColon) {
        openScope(lx, source);
    } else {
        wrapInAStatement(lx, source);
        openPunctuation(tokParens, slSubexpr, lx->i, lx);
        lx->i++; // CONSUME the left parenthesis
    }
}


private void lexParenRight(Compiler* lx, Arr(byte) source) {
    // TODO handle syntax like "(foo 5).field" and "(: id 5 name "asdf").id"
    Int startInd = lx->i;
    closeRegularPunctuation(tokParens, lx);
    
    if (!hasValues(lx->lexBtrack)) return;    
    
    lx->lastClosingPunctInd = startInd;    
}

/** The "@" sign that is used for accessing arrays, lists, dictionaries */
private void lexAccessor(Compiler* lx, Arr(byte) source) {
    VALIDATEL(lx->i < lx->inpLength, errPrematureEndOfInput)
    add((Token){ .tp = tokAccessor, .pl1 = tkAccAt, .startBt = lx->i, .lenBts = 1 }, lx);
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
    add((Token){.tp=tokString, .startBt=(lx->i), .lenBts=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


private void lexUnexpectedSymbol(Compiler* lx, Arr(byte) source) {
    throwExcLexer(errUnrecognizedByte, lx);
}

private void lexNonAsciiError(Compiler* lx, Arr(byte) source) {
    throwExcLexer(errNonAscii, lx);
}

/** Must agree in order with token types in tl.internal.h */
const char* tokNames[] = {
    "Int", "Long", "Float", "Bool", "String", "_", "DocComment", 
    "word", "Type", ":kwarg", ".strFlkd", "operator", "@cc", 
    "stmt", "(", "call(", "=", ":=", "*=", "data", "alias", "assert", "assertDbg",
    "await", "break", "continue", "defer", "embed", "iface", "import", "return", 
    "try", "yield", "colon", "else", "do(", "catch(", "def(", "Def(",
    "for(", "lambda(", "meta(", "package(", "if(", "ifPr(", "match(",
    "impl(", "while("
};


testable void printLexer(Compiler* a) {
    if (a->wasError) {
        printf("Error: ");
        printString(a->errMsg);
    }
    for (int i = 0; i < a->totalTokens; i++) {
        Token tok = a->tokens[i];
        if (tok.pl1 != 0 || tok.pl2 != 0) {
            printf("%d: %s %d %d [%d; %d]\n", i, tokNames[tok.tp], tok.pl1, tok.pl2, tok.startBt, tok.lenBts);
        } else {
            printf("%d: %s [%d; %d]\n", i, tokNames[tok.tp], tok.startBt, tok.lenBts);
        }        
    }
}

/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
testable int equalityLexer(Compiler a, Compiler b) {    
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.totalTokens < b.totalTokens ? a.totalTokens : b.totalTokens;
    int i = 0;
    for (; i < commonLength; i++) {
        Token tokA = a.tokens[i];
        Token tokB = b.tokens[i];
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
    return (a.totalTokens == b.totalTokens) ? -2 : i;        
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
    p[aAt] = &lexAccessor;
    p[aColon] = &lexColon;
    
    p[aSpace] = &lexSpace;
    p[aCarrReturn] = &lexSpace;
    p[aNewline] = &lexNewline;
    p[aBacktick] = &lexStringLiteral;
    return result;
}

/**
 * Table for dispatch on the first letter of a word in case it might be a reserved word.
 * It's indexed on the diff between first byte and the letter "a" (the earliest letter a reserved word may start with)
 */
private ReservedProbe (*tabulateReservedBytes(Arena* a))[countReservedLetters] {
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
    p[11] = determineReservedL;
    p[12] = determineReservedM;
    p[14] = determineReservedO;
    p[17] = determineReservedR;
    p[19] = determineReservedT;
    p[24] = determineReservedY;
    return result;
}

/** The set of base operators in the language */
private OpDef (*tabulateOperators(Arena* a))[countOperators] {
    OpDef (*result)[countOperators] = allocateOnArena(countOperators*sizeof(OpDef), a);

    OpDef* p = *result;
    /**
    * This is an array of 4-byte arrays containing operator byte sequences.
    * Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
    * It's used to lex operator symbols using left-to-right search.
    */
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
        p[k].builtinOverloads = 0;
        Int m = 0;
        for (; m < 4 && p[k].bytes[m] > 0; m++) {}
        p[k].lenBts = m;
    }
    return result;
}

#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser0(errMsg, __LINE__, cm); }
private Int exprUpTo(Int sentinelToken, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm);
private void addBinding(int nameId, int bindingId, Compiler* cm);
private void maybeCloseSpans(Compiler* cm);
private void popScopeFrame(Compiler* cm);
private Int addAndActivateEntity(Int nameId, Entity ent, Compiler* cm);
private void createBuiltins(Compiler* cm);
testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a);
private Int exprHeadless(Int sentinel, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm);
private Int exprAfterHead(Token tk, Arr(Token) tokens, Compiler* cm);

#define BIG 70000000

_Noreturn private void throwExcParser0(const char errMsg[], Int lineNumber, Compiler* cm) {   
    cm->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d line %d\n", cm->i, lineNumber);
#endif    
    cm->errMsg = str(errMsg, cm->a);
    longjmp(excBuf, 1);
}

#define throwExcParser(errMsg, cm) throwExcParser0(errMsg, __LINE__, cm)

/** Validates a new binding (that it is unique), creates an entity for it, and adds it to the current scope */
private Int createEntity(Int nameId, Compiler* cm) {
    Int mbBinding = cm->activeBindings[nameId];
    VALIDATEP(mbBinding < 0, errAssignmentShadowing) // if it's a binding, it should be -1, and if overload, <-1

    Int newEntityId = cm->entities.length;
    pushInentities(((Entity){.emit = emitPrefix, .class = classMutableGuaranteed}), cm);

    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (cm->scopeStack->length > 0) {
            addBinding(nameId, newEntityId, cm); // adds it to the ScopeStack
        }
        cm->activeBindings[nameId] = newEntityId; // makes it active
    }
    return newEntityId;
}

/** Calculates the sentinel token for a token at a specific index */
private Int calcSentinel(Token tok, Int tokInd) {
    return (tok.tp >= firstSpanTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}

testable void pushLexScope(ScopeStack* scopeStack);
private Int parseLiteralOrIdentifier(Token tok, Compiler* cm);

/** Performs coordinated insertions to start a scope within the parser */
private void addParsedScope(Int sentinelToken, Int startBt, Int lenBts, Compiler* cm) {
    push(((ParseFrame){.tp = nodScope, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodScope, .startBt = startBt, .lenBts = lenBts}, cm);
    pushLexScope(cm->scopeStack);
}

private void parseSkip(Token tok, Arr(Token) tokens, Compiler* cm) {
}


private void parseScope(Token tok, Arr(Token) tokens, Compiler* cm) {
    addParsedScope(cm->i + tok.pl2, tok.startBt, tok.lenBts, cm);
}

private void parseStruct(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseTry(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}


private void parseYield(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errTemp, cm);
}

/** Precondition: we are 1 past the "stmt" token, which is the first parameter */
private void ifLeftSide(Token tok, Arr(Token) tokens, Compiler* cm) {
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

/** Returns to parsing within an if (either the beginning of a clause or an "else" block) */
private void resumeIf(Token* tok, Arr(Token) tokens, Compiler* cm) {
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

/** Parses an assignment like "x = 5". The right side must never be a scope or contain any loops, or recursion will ensue */
private void parseAssignment(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int rLen = tok.pl2 - 1;
    VALIDATEP(rLen >= 1, errAssignment)
    
    Int sentinelToken = cm->i + tok.pl2;

    Token bindingTk = tokens[cm->i];
    VALIDATEP(bindingTk.tp == tokWord, errAssignment)
    Int newBindingId = createEntity(bindingTk.pl2, cm);

    push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodAssignment, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    
    pushInnodes((Node){.tp = nodBinding, .pl1 = newBindingId, .startBt = bindingTk.startBt, .lenBts = bindingTk.lenBts}, cm);
    cm->i++; // CONSUME the word token before the assignment sign

    Int declaredTypeId = -1;
    if (tokens[cm->i].tp == tokTypeName) {
        declaredTypeId = cm->activeBindings[tokens[cm->i].pl1];
        VALIDATEP(declaredTypeId > -1, errUnknownType)
        ++cm->i; // CONSUME the type decl of the binding
        --rLen; // for the type decl token
    }

    Token rTk = tokens[cm->i];
    Int rightTypeId = exprHeadless(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    VALIDATEP(rightTypeId != -2, errAssignment)
    VALIDATEP(declaredTypeId == -1 || rightTypeId == declaredTypeId, errTypeMismatch)
    
    if (rightTypeId > -1) {
        cm->entities.content[newBindingId].typeId = rightTypeId;
    }
}

/** While loops. Look like "(:while < x 100. x = 0. ++x)" This function handles assignments being lexically after the condition */
private void parseWhile(Token loopTok, Arr(Token) tokens, Compiler* cm) {
    ++cm->loopCounter;
    Int sentinel = cm->i + loopTok.pl2;

    Int condInd = cm->i;
    Token condTk = tokens[condInd]; 
    VALIDATEP(condTk.tp == tokStmt, errLoopSyntaxError)
    
    Int condSent = calcSentinel(condTk, condInd);
    Int startBtScope = tokens[condSent].startBt;
        
    push(((ParseFrame){ .tp = nodWhile, .startNodeInd = cm->nodes.length, .sentinelToken = sentinel, .typeId = cm->loopCounter }),
         cm->backtrack);
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
    push(((ParseFrame){.tp = nodWhileCond, .startNodeInd = cm->nodes.length, .sentinelToken = condSent }), cm->backtrack);
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

/**
 * Finds the top-level punctuation opener by its index, and sets its node length.
 * Called when the parsing of a span is finished.
 */
private void setSpanLengthParser(Int nodeInd, Compiler* cm) {
    cm->nodes.content[nodeInd].pl2 = cm->nodes.length - nodeInd - 1;
}

private void parseVerbatim(Token tok, Compiler* cm) {
    pushInnodes((Node){.tp = tok.tp, .startBt = tok.startBt, .lenBts = tok.lenBts, .pl1 = tok.pl1, .pl2 = tok.pl2}, cm);
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

/**
 * A single-item expression, like "foo". Consumes no tokens.
 * Pre-condition: we are 1 token past the token we're parsing.
 * Returns the type of the single item.
 */
private Int exprSingleItem(Token tk, Compiler* cm) {
    Int typeId = -1;
    if (tk.tp == tokWord) {
        Int entityId = cm->activeBindings[tk.pl2];
        VALIDATEP(entityId > -1, errUnknownBinding) 
        typeId = cm->entities.content[entityId].typeId;
        pushInnodes((Node){.tp = nodId, .pl1 = entityId, .pl2 = tk.pl2, .startBt = tk.startBt, .lenBts = tk.lenBts}, cm);
    } else if (tk.tp == tokOperator) {
        Int operBindingId = tk.pl1;
        OpDef operDefinition = (*cm->langDef->operators)[operBindingId];
        VALIDATEP(operDefinition.arity == 1, errOperatorWrongArity)
        pushInnodes((Node){ .tp = nodId, .pl1 = operBindingId, .startBt = tk.startBt, .lenBts = tk.lenBts}, cm);
        // TODO add the type when we support first-class functions
    } else if (tk.tp <= topVerbatimType) {
        pushInnodes((Node){.tp = tk.tp, .pl1 = tk.pl1, .pl2 = tk.pl2, .startBt = tk.startBt, .lenBts = tk.lenBts }, cm);
        typeId = tk.tp;
    } else {
        throwExcParser(errUnexpectedToken, cm);
    }
    return typeId;
}

/** Counts the arity of the call, including skipping unary operators. Consumes no tokens.
 *  Precondition: 
 */
private void exprCountArity(Int* arity, Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
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

/**
 * Parses a subexpression within an expression.
 * Precondition: the current token must be 1 past the opening paren / statement token
 * Examples: "foo 5 a"
 *           "+ 5 !a"
 * 
 * Consumes 1 or 2 tokens
 * TODO: allow for core forms (but not scopes!)
 */
private void exprSubexpr(Int sentinelToken, Int* arity, Arr(Token) tokens, Compiler* cm) {
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

        pushInnodes((Node){.tp = nodCall, .pl1 = mbBnd, .pl2 = *arity, .startBt = firstTk.startBt, .lenBts = firstTk.lenBts},
                    cm);
        *arity = 0;
        cm->i++; // CONSUME the function or operator call token
    }
}

/**
 * Flushes the finished subexpr frames from the top of the funcall stack.
 */
private void subexprClose(Arr(Token) tokens, Compiler* cm) {
    while (cm->scopeStack->length > 0 && cm->i == peek(cm->backtrack).sentinelToken) {
        popFrame(cm);
    }
}

/** An operator token in non-initial position is either a funcall (if unary) or an operand. Consumes no tokens. */
private void exprOperator(Token tok, ScopeStackFrame* topSubexpr, Arr(Token) tokens, Compiler* cm) {
    Int bindingId = tok.pl1;
    OpDef operDefinition = (*cm->langDef->operators)[bindingId];

    if (operDefinition.arity == 1) {
        pushInnodes((Node){ .tp = nodCall, .pl1 = -bindingId - 2, .pl2 = 1, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    } else {
        pushInnodes((Node){ .tp = nodId, .pl1 = -bindingId - 2, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    }
}

testable Int typeCheckResolveExpr(Int indExpr, Int sentinel, Compiler* cm);

/** General "big" expression parser. Parses an expression whether there is a token or not.
 * Starts from cm->i and goes up to the sentinel token. Returns the expression's type
 * Precondition: we are looking 1 past the tokExpr.
 */
private Int exprUpTo(Int sentinelToken, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm) {
    Int arity = 0;
    Int startNodeInd = cm->nodes.length;
    push(((ParseFrame){.tp = nodExpr, .startNodeInd = startNodeInd, .sentinelToken = sentinelToken }), cm->backtrack);        
    pushInnodes((Node){ .tp = nodExpr, .startBt = startBt, .lenBts = lenBts }, cm);

    exprSubexpr(sentinelToken, &arity, tokens, cm);
    while (cm->i < sentinelToken) {
        subexprClose(tokens, cm);
        Token cTk = tokens[cm->i];
        untt tokType = cTk.tp;
        
        ScopeStackFrame* topSubexpr = cm->scopeStack->topScope;
        if (tokType == tokParens) {
            cm->i++; // CONSUME the parens token
            exprSubexpr(cm->i + cTk.pl2, &arity, tokens, cm);
        } else VALIDATEP(tokType < firstSpanTokenType, errExpressionCannotContain)
        else if (tokType <= topVerbatimTokenVariant) {
            pushInnodes((Node){ .tp = cTk.tp, .pl1 = cTk.pl1, .pl2 = cTk.pl2, .startBt = cTk.startBt, .lenBts = cTk.lenBts }, cm);
            cm->i++; // CONSUME the verbatim token
        } else {
            if (tokType == tokWord) {
                Int mbBnd = cm->activeBindings[cTk.pl2];
                VALIDATEP(mbBnd != -1, errUnknownBinding)
                pushInnodes((Node){ .tp = nodId, .pl1 = mbBnd, .pl2 = cTk.pl2, .startBt = cTk.startBt, .lenBts = cTk.lenBts}, cm);                
            } else if (tokType == tokOperator) {
                exprOperator(cTk, topSubexpr, tokens, cm);
            }
            cm->i++; // CONSUME any leaf token
        }
    }

    subexprClose(tokens, cm);
    
    Int exprType = typeCheckResolveExpr(startNodeInd, cm->nodes.length, cm);
    return exprType;
}

/**
 * Precondition: we are looking at the first token of expr which does not have a tokStmt/tokParens header.
 * Consumes 1 or more tokens.
 * Returns the type of parsed expression.
 */
private Int exprHeadless(Int sentinel, Int startBt, Int lenBts, Arr(Token) tokens, Compiler* cm) {
    if (cm->i + 1 == sentinel) { // the [stmt 1, tokInt] case
        Token singleToken = tokens[cm->i];
        if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) { 
            cm->i += 1; // CONSUME the single literal
            return exprSingleItem(singleToken, cm);
        }
    }
    return exprUpTo(sentinel, startBt, lenBts, tokens, cm);
}

/**
 * Precondition: we are looking 1 past the first token of expr, which is the first parameter. Consumes 1 or more tokens.
 * Returns the type of parsed expression.
 */
private Int exprAfterHead(Token tk, Arr(Token) tokens, Compiler* cm) {
    if (tk.tp == tokStmt || tk.tp == tokParens) {
        if (tk.pl2 == 1) {
            Token singleToken = tokens[cm->i];
            if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) { // the [stmt 1, tokInt] case
                ++cm->i; // CONSUME the single literal token
                return exprSingleItem(singleToken, cm);
            }
        }
        return exprUpTo(cm->i + tk.pl2, tk.startBt, tk.lenBts, tokens, cm);
    } else {
        return exprSingleItem(tk, cm);
    }
}

/**
 * Parses an expression from an actual token. Precondition: we are 1 past that token. Handles statements only, not single items.
 */
private void parseExpr(Token tok, Arr(Token) tokens, Compiler* cm) {
    exprAfterHead(tok, tokens, cm);
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
private void maybeCloseSpans(Compiler* cm) {
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

/** Parses anything from current cm->i to "sentinelToken" */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
    while (cm->i < sentinelToken) {
        Token currTok = tokens[cm->i];
        untt contextType = peek(cm->backtrack).tp;
        // pre-parse hooks that let contextful syntax forms (e.g. "if") detect parsing errors and maintain their state
        if (contextType >= firstResumableForm) {
            ((*cm->langDef->resumableTable)[contextType - firstResumableForm])(&currTok, tokens, cm);
        } else {
            cm->i++; // CONSUME any span token
        }
        ((*cm->langDef->nonResumableTable)[currTok.tp])(currTok, tokens, cm);
        
        maybeCloseSpans(cm);
    }    
}

/** Consumes 0 or 1 tokens. Returns the type of what it parsed, including -1 if unknown. Returns -2 if didn't parse. */
private Int parseLiteralOrIdentifier(Token tok, Compiler* cm) {
    Int typeId = -1;
    if (tok.tp <= topVerbatimTokenVariant) {
        parseVerbatim(tok, cm);
        typeId = tok.tp;
    } else if (tok.tp == tokWord) {        
        Int nameId = tok.pl2;
        Int mbBinding = cm->activeBindings[nameId];
        VALIDATEP(mbBinding != -1, errUnknownBinding)    
        pushInnodes((Node){.tp = nodId, .pl1 = mbBinding, .pl2 = nameId, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
        if (mbBinding > -1) {
            typeId = cm->entities.content[mbBinding].typeId;
        }
    } else {
        return -2;
    }
    cm->i++; // CONSUME the literal or ident token
    return typeId;
}

/** Changes a mutable variable to mutated. Throws an exception for an immutable one */
private void setClassToMutated(Int bindingId, Compiler* cm) {
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
    Int entityId = cm->activeBindings[bindingTk.pl2];
    Entity ent = cm->entities.content[entityId];
    VALIDATEP(entityId > -1, errUnknownBinding)
    VALIDATEP(ent.class < classImmutable, errCannotMutateImmutable)

    push(((ParseFrame){ .tp = nodReassign, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodReassign, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    pushInnodes((Node){.tp = nodBinding, .pl1 = entityId, .startBt = bindingTk.startBt, .lenBts = bindingTk.lenBts}, cm);
    
    cm->i++; // CONSUME the word token before the assignment sign

    Int typeId = cm->entities.content[entityId].typeId;
    Token rTk = tokens[cm->i];
    Int rightTypeId = exprHeadless(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    
    VALIDATEP(rightTypeId == typeId, errTypeMismatch)
    setClassToMutated(entityId, cm);
}

testable bool findOverload(Int typeId, Int start, Int sentinel, Int* entityId, Compiler* cm);

/** Performs some AST twiddling to turn a mutation into a reassignment:
 *  1. [.    .]
 *  2. [.    .      Expr     ...]
 *  3. [Expr OpCall LeftSide ...]
 *
 *  The end result is an expression that is the right side but wrapped in another binary call, with .pl2 increased by + 2
 */
private void mutationTwiddle(Int ind, Token mutTk, Compiler* cm) {
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
    Int leftEntityId = cm->activeBindings[bindingTk.pl2];
    Entity leftEntity = cm->entities.content[leftEntityId];
    VALIDATEP(leftEntityId > -1 && leftEntity.typeId > -1, errUnknownBinding);
    VALIDATEP(leftEntity.class < classImmutable, errCannotMutateImmutable);
    Int leftType = cm->entities.content[leftEntityId].typeId;

    Int operatorOv;
    bool foundOv = findOverload(leftType, cm->overloadIds.content[opType], cm->overloadIds.content[opType + 1], &operatorOv, cm);
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
        Int mbNameId = getStringStore(cm->sourceCode->content, impts[j].name, cm->stringTable, cm->stringStore);
        if (mbNameId > -1) {
            EntityImport impt = impts[j];
            Int typeInd = typeIds[impt.typeInd];
            impt.nameId = mbNameId;
            addAndActivateEntity(mbNameId, (Entity){
                .class = classImmutable, .typeId = typeInd, .emit = emitPrefixExternal, .externalNameId = impt.externalNameId },
                cm);
            pushInimports(impt, cm);
        }
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
    p[tokDef]      = &parseAlias;
    p[tokIface]      = &parseAlias;
    p[tokImport]     = &parseAlias;
    p[tokLambda]     = &parseAlias;
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

/*
* Definition of the operators, reserved words, lexer dispatch and parser dispatch tables for the compiler.
*/
testable LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    (*result) = (LanguageDefinition) {
        .possiblyReservedDispatch = tabulateReservedBytes(a), .dispatchTable = tabulateDispatch(a),
        .operators = tabulateOperators(a),
        .nonResumableTable = tabulateNonresumableDispatch(a),
        .resumableTable = tabulateResumableDispatch(a)
    };
    result->baseTypes[tokInt] = str("Int", a);
    result->baseTypes[tokLong] = str("Long", a);
    result->baseTypes[tokFloat] = str("Float", a);
    result->baseTypes[tokString] = str("String", a);
    result->baseTypes[tokBool] = str("Bool", a);
    result->baseTypes[tokUnderscore] = str("Void", a);
    return result;
}

/**
 * Creates a proto-compiler, which is used not for compilation but as a seed value to be cloned for every source code module.
 * The proto-compiler contains the following data:
 * - langDef with operators whose "builtinOverloads" is filled in
 * - types that are sufficient for the built-in operators
 * - entities with the built-in operator entities
 * - overloadIds with counts
 * - entImportedZero
 * - countOperatorEntities
 */
testable Compiler* createCompilerProto(Arena* a) {
    Compiler* proto = allocateOnArena(sizeof(Compiler), a);
    (*proto) = (Compiler){
        .langDef = buildLanguageDefinitions(a), .entities = createInListEntity(32, a),
        .overloadIds = createInListuint32_t(countOperators, a),
        .types = createInListInt(64, a), .typesDict = createStringStore(100, a),
        .a = a
    };
    createBuiltins(proto);
    return proto;
}

/**
 * Finalizes the lexing of a single input: checks for unclosed scopes, and closes semicolons and an open statement, if any.
 */
private void finalizeLexer(Compiler* lx) {
    if (!hasValues(lx->lexBtrack)) {
        return;
    }
    closeColons(lx);
    BtToken top = pop(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slScope && !hasValues(lx->lexBtrack), errPunctuationExtraOpening)

    setStmtSpanLength(top.tokenInd, lx);
}


testable Compiler* lexicallyAnalyze(String* input, Compiler* proto, Arena* a) {
    Compiler* lx = createLexerFromProto(input, proto, a);
    Int inpLength = input->length;
    Arr(byte) inp = input->content;

    VALIDATEL(inpLength > 0, "Empty input")

    // Check for UTF-8 BOM at start of file
    if (inpLength >= 3
        && (unsigned char)inp[0] == 0xEF
        && (unsigned char)inp[1] == 0xBB
        && (unsigned char)inp[2] == 0xBF) {
        lx->i = 3;
    }
    LexerFunc (*dispatch)[256] = proto->langDef->dispatchTable;

    // Main loop over the input
    if (setjmp(excBuf) == 0) {
        while (lx->i < inpLength) {
            ((*dispatch)[inp[lx->i]])(lx, inp);
        }
        finalizeLexer(lx);
    }
    lx->totalTokens = lx->nextInd;
    return lx;
}
//}}}
//{{{ Parser



/**
 * This frame corresponds either to a lexical scope or a subexpression.
 * It contains string ids introduced in the current scope, used not for cleanup after the frame is popped. 
 * Otherwise, it contains the function call of the subexpression.
 */
struct ScopeStackFrame {
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


testable ScopeStack* createScopeStack() {
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

/** Allocates a new scope, either within this chunk or within a pre-existing lastChunk or within a brand new chunk 
 * Scopes have a simple size policy: 64 elements at first, then 256, then throw exception. This is because
 * only 256 local variables are allowed in one function, and transitively in one scope.
 */
testable void pushLexScope(ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
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

/**
 * Pops a scope frame. For a scope type of frame, also deactivates its bindings.
 * Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any
 */
private void popScopeFrame(Compiler* cm) {
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
    
    // if the lastChunk is defined, it will serve as pre-allocated buffer for future frames, but everything after it needs to go
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

/** Processes the name of a defined function. Creates an overload counter, or increments it if it exists. Consumes no tokens. */
private void fnDefIncrementOverlCount(Int nameId, Compiler* cm) {
    Int activeValue = (nameId > -1) ? cm->activeBindings[nameId] : -1;
    VALIDATEP(activeValue < 0, errAssignmentShadowing);
    if (activeValue == -1) { // this is the first-registered overload of this function
        cm->activeBindings[nameId] = -cm->overloadIds.length - 2;
        pushInoverloadIds(SIXTEENPLUSONE, cm);
    } else { // other overloads have already been registered, so just increment the counts
        cm->overloadIds.content[-activeValue - 2] += SIXTEENPLUSONE;
    }
}


private Int addAndActivateEntity(Int nameId, Entity ent, Compiler* cm) {
    Int existingBinding = cm->activeBindings[nameId];
    Int typeLength = cm->types.content[ent.typeId];
    VALIDATEP(existingBinding == -1 || typeLength > 0, errAssignmentShadowing) // either the name unique, or it's a function

    Int newEntityId = cm->entities.length;
    pushInentities(ent, cm);
    if (typeLength <= 1) { // not a function, or is a 0-arity function => not overloaded
        cm->activeBindings[nameId] = newEntityId;
    } else {
        fnDefIncrementOverlCount(nameId, cm);
    }
    return newEntityId;
}


/** Unique'ing of types. Precondition: the type is parked at the end of cm->types, forming its tail.
 *  Returns the resulting index of this type and updates the length of cm->types accordingly.
 */
testable Int mergeType(Int startInd, Int len, Compiler* cm) {
    byte* types = (byte*)cm->types.content;
    StringStore* hm = cm->typesDict;
    Int startBt = startInd*4;
    Int lenBts = len*4;
    untt theHash = hashCode(types + startBt, lenBts);
    Int hashInd = theHash % (hm->dictSize);
    if (*(hm->dict + hashInd) == NULL) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;        
        *firstElem = (StringValue){.length = lenBts, .indString = startBt };
        *(hm->dict + hashInd) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hashInd);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == lenBts
                  && memcmp(types + stringValues[i].indString, types + startBt, lenBts) == 0) {                    
                // key already present
                cm->types.length -= (len + 1);
                return stringValues[i].indString/4;
            }
        }
        addValueToBucket((hm->dict + hashInd), startBt, lenBts, hm->a);
    }
    ++hm->length;
    return startInd;
}

/** Function types are stored as: (length, return type, paramType1, paramType2, ...) */
testable Int addFunctionType(Int arity, Arr(Int) paramsAndReturn, Compiler* cm) {
    Int newInd = cm->types.length;
    pushIntypes(arity + 1, cm);
    pushIntypes(paramsAndReturn[0], cm);
    for (Int k = 0; k < arity; k++) {
        pushIntypes(paramsAndReturn[k + 1], cm);
    }
    return mergeType(newInd, arity + 2, cm);
}

//{{{ Built-ins

const char constantStrings[] = "returnifelsefunctionwhileconstletbreakcontinuetruefalseconsole.logfor"
                               "toStringMath.powMath.PIMath.E!==lengthMath.abs&&||===alertprintlo";
const Int constantOffsets[] = {
    0,   6,  8, 12,
    20, 25, 30, 33,
    38, 46, 50, 55,
    66, 69, 77, 85,
    92, 98, 101, 107,
    115, 117, 119, 122,
    127, 132, 134
    
};

#define strReturn   0
#define strIf       1
#define strElse     2
#define strFunction 3
#define strWhile    4

#define strConst    5
#define strLet      6
#define strBreak    7
#define strContinue 8
#define strTrue     9

#define strFalse   10
#define strPrint   11
#define strFor     12
#define strToStr   13
#define strExpon   14

#define strPi       15
#define strE        16
#define strNotEqual 17
#define strLength   18
#define strAbsolute 19

#define strLogicalAnd 20
#define strLogicalOr  21
#define strEquality   22
#define strAlert      23
#define strPrint2     24

#define strLo         25

private void buildOperator(Int operId, Int typeId, uint8_t emitAs, uint16_t externalNameId, Compiler* cm) {
    pushInentities((Entity){
        .typeId = typeId, .emit = emitAs, .class = classImmutable, .externalNameId = externalNameId}, cm);
    cm->overloadIds.content[operId] += SIXTEENPLUSONE;
    ++((*cm->langDef->operators)[operId].builtinOverloads);
}

/** Operators are the first-ever functions to be defined. This function builds their types, entities and overload counts. 
 * The order must agree with the order of operator definitions, and every operator must have at least one entity.
*/
private void buildOperators(Compiler* cm) {
    for (Int j = 0; j < countOperators; j++) {
        pushInoverloadIds(0, cm);
    }
    
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

    buildOperator(opTNotEqual, boolOfIntInt, emitInfixExternal, strNotEqual, cm);
    buildOperator(opTNotEqual, boolOfFlFl, emitInfixExternal, strNotEqual, cm);
    buildOperator(opTNotEqual, boolOfStrStr, emitInfixExternal, strNotEqual, cm);
    buildOperator(opTBoolNegation, boolOfBool, emitPrefix, 0, cm);
    buildOperator(opTSize,     intOfStr, emitField, strLength, cm);
    buildOperator(opTSize,     intOfInt, emitPrefixExternal, strAbsolute, cm);
    buildOperator(opTToString, strOfInt, emitInfixDot, strToStr, cm);
    buildOperator(opTToString, strOfFloat, emitInfixDot, strToStr, cm);
    buildOperator(opTToString, strOfBool, emitInfixDot, strToStr, cm);
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
    buildOperator(opTEquality,    boolOfIntInt, emitInfixExternal, strEquality, cm);
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
    buildOperator(opTExponent, intOfIntInt, emitPrefixExternal, strExpon, cm);
    buildOperator(opTExponent, flOfFlFl, emitPrefixExternal, strExpon, cm);
    buildOperator(opTBoolOr, flOfFl, 0, 0, cm); // dummy
    buildOperator(opTXor,    flOfFlFl, 0, 0, cm); // dummy
    buildOperator(opTAnd,    boolOfBoolBool, emitInfixExternal, strLogicalAnd, cm);
    buildOperator(opTOr,     boolOfBoolBool, emitInfixExternal, strLogicalOr, cm);
    buildOperator(opTNegation, intOfInt, emitPrefix, 0, cm);
    buildOperator(opTNegation, flOfFl, emitPrefix, 0, cm);
    cm->countOperatorEntities = cm->entities.length;
}

/* Entities and overloads for the built-in operators, types and functions. */
private void createBuiltins(Compiler* cm) {
    const Int countBaseTypes = sizeof(cm->langDef->baseTypes)/sizeof(String*);
    for (int i = 0; i < countBaseTypes; i++) {
        pushIntypes(0, cm);
    }
    buildOperators(cm);
    cm->entImportedZero = cm->overloadIds.length;
}
//}}}

private StringStore* copyStringStore(StringStore* source, Arena* a) {
    StringStore* result = allocateOnArena(sizeof(StringStore), a);
    const Int dictSize = source->dictSize;
    Arr(Bucket*) dict = allocateOnArena(sizeof(Bucket*)*dictSize, a);
    
    result->a = a;
    for (int i = 0; i < dictSize; i++) {
        if (source->dict[i] == NULL) {
            dict[i] = NULL;
        } else {
            Bucket* old = source->dict[i];
            Int capacity = old->capAndLen >> 16;
            Int len = old->capAndLen && LOWER16BITS;
            Bucket* new = allocateOnArena(sizeof(Bucket) + capacity*sizeof(StringValue), a);
            memcpy(new->content, old->content, len*sizeof(StringValue));
            dict[i] = new;
        }
    }
    result->dictSize = source->dictSize;
    result->dict = dict;

    return result;
}

/** Imports the standard, Prelude kind of stuff into the compiler immediately after the lexing phase */
private void importPrelude(Compiler* cm) {
    Int voidOfStr = addFunctionType(1, (Int[]){tokUnderscore, tokString}, cm);
    
    EntityImport imports[4] =  {
        (EntityImport) { .name = str("math-pi", cm->a), .externalNameId = strPi, .typeInd = 0, .nameId = -1 },
        (EntityImport) { .name = str("math-e", cm->a), .externalNameId = strE, .typeInd = 0, .nameId = -1 },
        (EntityImport) { .name = str("print", cm->a), .externalNameId = strPrint2, .typeInd = 1, .nameId = -1 },
        (EntityImport) { .name = str("alert", cm->a), .externalNameId = strAlert, .typeInd = 1, .nameId = -1 }
    };
    Int countBaseTypes = sizeof(cm->langDef->baseTypes)/sizeof(String*);
    for (Int j = 0; j < countBaseTypes; j++) {
        Int mbNameId = getStringStore(cm->sourceCode->content, cm->langDef->baseTypes[j], cm->stringTable, cm->stringStore);
        if (mbNameId > -1) {
            cm->activeBindings[mbNameId] = j;
        }
    }
    importEntities(imports, sizeof(imports)/sizeof(EntityImport), ((Int[]){tokFloat, voidOfStr}), cm);
}

/** A proto compiler contains just the built-in definitions and tables. This function copies it  */
testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a) {
    Compiler* lx = allocateOnArena(sizeof(Compiler), a);
    Arena* aTmp = mkArena();

    (*lx) = (Compiler){
        .i = 0, .langDef = proto->langDef, .sourceCode = sourceCode, .nextInd = 0, .inpLength = sourceCode->length,
        .tokens = allocateOnArena(LEXER_INIT_SIZE*sizeof(Token), a), .capacity = LEXER_INIT_SIZE,
        .newlines = allocateOnArena(500*sizeof(int), a), .newlinesCapacity = 500,
        .numeric = allocateOnArena(50*sizeof(int), aTmp), .numericCapacity = 50,
        .lexBtrack = createStackBtToken(16, aTmp),
        .stringTable = createStackint32_t(16, a), .stringStore = createStringStore(100, a),
        .sourceCode = sourceCode,
        .countOperatorEntities = proto->countOperatorEntities, .entImportedZero = proto->entities.length,
        .wasError = false, .errMsg = &empty,
        .a = a, .aTmp = aTmp
    };
    return lx;
}

/** Initialize all the parser & typer stuff after lexing is done */
testable void initializeParser(Compiler* lx, Compiler* proto, Arena* a) {
    if (lx->wasError) {
        return;
    }

    Compiler* cm = lx;
    cm->activeBindings = allocateOnArena(4*lx->stringTable->length, lx->aTmp);
    const Int countBaseTypes = sizeof(*cm->langDef->baseTypes)/sizeof(String*);
    for (int j = 0; j < countBaseTypes; j++) {
        Int typeNameId = getStringStore(cm->sourceCode->content, cm->langDef->baseTypes[j], cm->stringTable, cm->stringStore);
        if (typeNameId > -1) {
            cm->activeBindings[typeNameId] = j;
        }
    }
    
    Int initNodeCap = lx->totalTokens > 64 ? lx->totalTokens : 64;
    cm->scopeStack = createScopeStack();
    cm->backtrack = createStackParseFrame(16, lx->aTmp);
    cm->i = 0;
    cm->loopCounter = 0;
    cm->nodes = createInListNode(initNodeCap, a);

    if (lx->stringTable->length > 0) {
        memset(cm->activeBindings, 0xFF, lx->stringTable->length*4); // activeBindings is filled with -1
    }

    cm->entities = createInListEntity(proto->entities.capacity, a);
    memcpy(cm->entities.content, proto->entities.content, proto->entities.length*sizeof(Entity));
    cm->entities.length = proto->entities.length;
    cm->entities.capacity = proto->entities.capacity;

    cm->overloadIds = createInListuint32_t(proto->overloadIds.capacity, cm->aTmp);
    memcpy(cm->overloadIds.content, proto->overloadIds.content, proto->overloadIds.length*4);
    cm->overloadIds.length = proto->overloadIds.length;
    cm->overloadIds.capacity = proto->overloadIds.capacity;
    cm->overloads = (InListInt){.length = 0, .content = NULL};    
    
    cm->types.content = allocateOnArena(proto->types.capacity*8, a);
    memcpy(cm->types.content, proto->types.content, proto->types.length*4);
    cm->types.length = proto->types.length;
    cm->types.capacity = proto->types.capacity*2;
    
    cm->typesDict = copyStringStore(proto->typesDict, a);

    cm->imports = createInListEntityImport(8, lx->aTmp);

    cm->expStack = createStackint32_t(16, cm->aTmp);
    cm->scopeStack = createScopeStack();
    
    importPrelude(cm);
}


private Int getFirstParamType(Int funcTypeId, Compiler* cm);
private bool isFunctionWithParams(Int typeId, Compiler* cm);

/** Now that the overloads are freshly allocated, we need to write all the existing entities (operators and imported functions)
 * to the overloads table, which consists of variable-length entries of the form: [count typeId1 typeId2 entId1 entId2]
 */
private void populateOverloadsForOperatorsAndImports(Compiler* cm) {
    Int o;
    Int countOverls;
    Int currOperId = -1;
    Int entitySentinel = 0;

    // overloads for built-in operators
    for (Int j = 0; j < cm->countOperatorEntities; j++) {
        Entity ent = cm->entities.content[j];

        if (j == entitySentinel) {
            ++currOperId;
            
            o = cm->overloadIds.content[currOperId] + 1;
            countOverls = (cm->overloadIds.content[currOperId + 1] - cm->overloadIds.content[currOperId] - 1)/2;
            entitySentinel += (*cm->langDef->operators)[currOperId].builtinOverloads;
        }
        cm->overloads.content[o] = getFirstParamType(ent.typeId, cm);
#ifdef SAFETY
        VALIDATEI(cm->overloads.content[o + countOverls] == -1, iErrorOverloadsOverflow)
#endif

        cm->overloads.content[o + countOverls] = j;
        
        o++;
    }
    
    // overloads for imported functions
    Int currFuncId = -1;
    for (Int j = cm->entImportedZero; j < cm->entities.length; j++) {
        Entity ent = cm->entities.content[j];
        if (!isFunctionWithParams(ent.typeId, cm)) {
            continue;
        }
        EntityImport imp = cm->imports.content[j - cm->entImportedZero];
        currFuncId = cm->activeBindings[imp.nameId];
#ifdef SAFETY
        VALIDATEI(currFuncId < -1, iErrorImportedFunctionNotInScope)
#endif            
        o = cm->overloadIds.content[-currFuncId - 2] + 1;
        countOverls = cm->overloads.content[o - 1];

#ifdef SAFETY
        VALIDATEI(cm->overloads.content[o + countOverls] == -1, iErrorOverloadsOverflow)
#endif
        cm->overloads.content[o] = getFirstParamType(ent.typeId, cm);
        cm->overloads.content[o + countOverls] = j;
        o++;
    }
}

/**
 * Allocates the table with overloads and semi-fills it (only the imports and built-ins, no toplevel).
 * Also finalizes the overloadIds table to contain actual indices (not counts that were there initially)
 */
testable void createOverloads(Compiler* cm) {
    Int neededCount = 0;
    for (Int i = 0; i < cm->overloadIds.length; i++) {
        // a typeId and an entityId for each overload, plus a length field for the list
        neededCount += (2*(cm->overloadIds.content[i] >> 16) + 1);
    }
    cm->overloads = createInListInt(neededCount, cm->a);
    cm->overloads.length = neededCount;
    memset(cm->overloads.content, 0xFF, neededCount*4);

    Int j = 0;
    for (Int i = 0; i < cm->overloadIds.length; i++) {
        // length of the overload list (to be filled during type check/resolution)
        Int maxCountOverloads = (Int)(cm->overloadIds.content[i] >> 16);
        cm->overloads.content[j] = maxCountOverloads;
        cm->overloadIds.content[i] = j;
        j += (2*maxCountOverloads + 1);
    }
    pushInoverloadIds(neededCount, cm); // the extra sentinel, since lengths of overloads are deduced from overloadIds
}

/** Parses top-level types but not functions and adds their bindings to the scope */
private void parseToplevelTypes(Compiler* cm) {
}

/** Parses top-level constants but not functions, and adds their bindings to the scope */
private void parseToplevelConstants(Compiler* cm) {
    cm->i = 0;
    const Int len = cm->totalTokens;
    while (cm->i < len) {
        Token tok = cm->tokens[cm->i];
        if (tok.tp == tokAssignment) {
            parseUpTo(cm->i + tok.pl2, cm->tokens, cm);
        } else {
            cm->i += (tok.pl2 + 1);
        }
    }    
}

/** Parses top-level function names, and adds their bindings to the scope */
private void surveyToplevelFunctionNames(Compiler* cm) {
    cm->i = 0;
    const Int len = cm->totalTokens;
    Token* tokens = cm->tokens;
    while (cm->i < len) {
        Token tok = tokens[cm->i];
        if (tok.tp == tokDef) {
            Int lenTokens = tok.pl2;
            VALIDATEP(lenTokens >= 3, errFnNameAndParams)
            
            Token fnName = tokens[(cm->i) + 2]; // + 2 because we skip over the "fn" and "stmt" span tokens
            VALIDATEP(fnName.tp == tokWord, errFnNameAndParams)
            Int j = cm->i + 3;
            if (tokens[j].tp == tokTypeName) {
                ++j;
            }
            VALIDATEP(tokens[j].tp == tokParens, errFnNameAndParams)
            Int nameId = fnName.pl2;
            if (tokens[j].pl2 > 0) {
                fnDefIncrementOverlCount(nameId, cm);
            }
        }
        cm->i += (tok.pl2 + 1);        
    } 
}

/** A new parsed overload (i.e. a top-level function). Called when parsing a top-level function signature.  */
private void addParsedOverload(Int overloadId, Int firstParamTypeId, Int entityId, Compiler* cm) {
    Int overloadInd = cm->overloadIds.content[overloadId];
    Int nextOverloadInd = cm->overloadIds.content[overloadId + 1];
    Int maxOverloadCount = (nextOverloadInd - overloadInd - 1)/2;
    Int sentinel = overloadInd + maxOverloadCount + 1;
    Int j = overloadInd + 1;
    for (; j < sentinel && cm->overloads.content[j] > -1; j++) {}
    
#ifdef SAFETY
    VALIDATEI(j < sentinel, iErrorOverloadsOverflow)
#endif

    cm->overloads.content[j] = firstParamTypeId;
    cm->overloads.content[j + maxOverloadCount] = entityId;
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
                print("type over board j %d %d", j, cm->overloads.content[j]);
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

/** 
 * Parses a top-level function signature. Emits no nodes, only an entity and an overload. Saves the info to "toplevelSignatures"
 * Pre-condition: we are 2 tokens past the fnDef.
 */
private void parseToplevelSignature(Token fnDef, StackNode* toplevelSignatures, Compiler* cm) {
    Int fnStartTokenId = cm->i - 2;    
    Int fnSentinelToken = fnStartTokenId + fnDef.pl2 + 1;
    
    Token fnName = cm->tokens[cm->i];
    Int fnNameId = fnName.pl2;
    Int activeBinding = cm->activeBindings[fnNameId];
    Int overloadId = activeBinding < -1 ? (-activeBinding - 2) : -1;
    
    cm->i++; // CONSUME the function name token
    
    Int tentativeTypeInd = cm->types.length;
    pushIntypes(0, cm); // will overwrite it with the type's length once we know it
    // the function's return type, interpreted to be Void if absent
    if (cm->tokens[cm->i].tp == tokTypeName) {
        Token fnReturnType = cm->tokens[cm->i];
        Int returnTypeId = cm->activeBindings[fnReturnType.pl2];
        VALIDATEP(returnTypeId > -1, errUnknownType)
        pushIntypes(returnTypeId, cm);
        
        cm->i++; // CONSUME the function return type token
    } else {
        pushIntypes(tokUnderscore, cm); // underscore stands for the Void type
    }
    VALIDATEP(cm->tokens[cm->i].tp == tokParens, errFnNameAndParams)

    Int paramsTokenInd = cm->i;
    Token parens = cm->tokens[paramsTokenInd];   
    Int paramsSentinel = cm->i + parens.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list            
    Int arity = 0;
    while (cm->i < paramsSentinel) {
        Token paramName = cm->tokens[cm->i];
        VALIDATEP(paramName.tp == tokWord, errFnNameAndParams)
        ++cm->i; // CONSUME a param name
        ++arity;
        
        VALIDATEP(cm->i < paramsSentinel, errFnNameAndParams)
        Token paramType = cm->tokens[cm->i];
        VALIDATEP(paramType.tp == tokTypeName, errFnNameAndParams)
        
        Int paramTypeId = cm->activeBindings[paramType.pl2]; // the binding of this parameter's type
        VALIDATEP(paramTypeId > -1, errUnknownType)
        pushIntypes(paramTypeId, cm);
        
        ++cm->i; // CONSUME the param's type name
    }

    VALIDATEP(cm->i < (fnSentinelToken - 1) && cm->tokens[cm->i].tp == tokColon, errFnMissingBody)
    cm->types.content[tentativeTypeInd] = arity + 1;
    
    Int uniqueTypeId = mergeType(tentativeTypeInd, arity + 2, cm);

    bool isOverload = arity > 0;
    Int fnEntityId = cm->entities.length;    
    pushInentities(((Entity){.emit = isOverload ? emitOverloaded : emitPrefix, .class = classImmutable,
                            .typeId = uniqueTypeId}), cm);
    if (isOverload) {
        addParsedOverload(overloadId, cm->types.content[uniqueTypeId + 2], fnEntityId, cm);
    } else {
        cm->activeBindings[fnNameId] = fnEntityId;
    }

    // not a real node, just a record to later find this signature
    pushNode((Node){ .tp = nodFnDef, .pl1 = fnEntityId, .pl2 = paramsTokenInd,
                     .startBt = fnStartTokenId, .lenBts = fnSentinelToken }, toplevelSignatures);
}

/** 
 * Parses a top-level function.
 * The result is the AST ([] FnDef EntityName Scope EntityParam1 EntityParam2 ... )
 */
private void parseToplevelBody(Node toplevelSignature, Arr(Token) tokens, Compiler* cm) {
    Int fnStartInd = toplevelSignature.startBt;
    Int fnSentinel = toplevelSignature.lenBts;

    cm->i = toplevelSignature.pl2; // paramsTokenInd from fn "parseToplevelSignature"
    Token fnDefTk = tokens[fnStartInd];
    Token fnNameTk = tokens[fnStartInd + 2];

    // the fnDef scope & node
    Int entityId = toplevelSignature.pl1;
    push(((ParseFrame){ .tp = nodFnDef, .startNodeInd = cm->nodes.length, .sentinelToken = fnSentinel,
                        .typeId = cm->entities.content[entityId].typeId}), cm->backtrack);
    pushInnodes(((Node){ .tp = nodFnDef, .startBt = fnDefTk.startBt, .lenBts = fnDefTk.lenBts }), cm);
    pushInnodes((Node){ .tp = nodBinding, .pl1 = entityId, .startBt = fnNameTk.startBt, .lenBts = fnNameTk.lenBts }, cm);
    
    // the scope for the function's body
    addParsedScope(fnSentinel, tokens[cm->i].startBt, fnDefTk.lenBts - tokens[cm->i].startBt + fnDefTk.startBt, cm);

    Token parens = tokens[cm->i];
        
    Int paramsSentinel = cm->i + parens.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list            
    while (cm->i < paramsSentinel) {
        Token paramName = tokens[cm->i];
        Int newEntityId = createEntity(paramName.pl2, cm);
        ++cm->i; // CONSUME the param name
        Int typeId = cm->activeBindings[tokens[cm->i].pl2];
        cm->entities.content[newEntityId].typeId = typeId;

        Node paramNode = (Node){.tp = nodBinding, .pl1 = newEntityId, .startBt = paramName.startBt, .lenBts = paramName.lenBts };

        ++cm->i; // CONSUME the param's type name
        pushInnodes(paramNode, cm);
    }
    
    ++cm->i; // CONSUME the "=" sign
    parseUpTo(fnSentinel, tokens, cm);
}

/** Parses top-level function params and bodies */
private void parseFunctionBodies(StackNode* toplevelSignatures, Arr(Token) tokens, Compiler* cm) {
    for (int j = 0; j < toplevelSignatures->length; j++) {
        cm->loopCounter = 0;
        parseToplevelBody(toplevelSignatures->content[j], tokens, cm);
    }
}

/** Walks the top-level functions' signatures (but not bodies).
 * Result: the complete overloads & overloadIds tables, and the list of toplevel functions to parse bodies for.
 */
private StackNode* parseToplevelSignatures(Compiler* cm) {
    StackNode* topLevelSignatures = createStackNode(16, cm->aTmp);
    cm->i = 0;
    const Int len = cm->totalTokens;
    while (cm->i < len) {
        Token tok = cm->tokens[cm->i];
        if (tok.tp == tokDef) {
            Int lenTokens = tok.pl2;
            Int sentinelToken = cm->i + lenTokens + 1;
            VALIDATEP(lenTokens >= 2, errFnNameAndParams)
            
            cm->i += 2; // CONSUME the function def token and the stmt token
            parseToplevelSignature(tok, topLevelSignatures, cm);
            cm->i = sentinelToken;
        } else {            
            cm->i += (tok.pl2 + 1);  // CONSUME the whole non-function span
        }
    }
    return topLevelSignatures;
}

/** Must agree in order with node types in tl.internal.h */
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
        
        // This gives us overload counts
        surveyToplevelFunctionNames(cm);

        // This gives us the semi-complete overloads & overloadIds tables (with only the built-ins and imports)
        createOverloads(cm);
        populateOverloadsForOperatorsAndImports(cm);
        parseToplevelConstants(cm);
        
        // This gives us the complete overloads & overloadIds tables, and the list of toplevel functions
        StackNode* topLevelSignatures = parseToplevelSignatures(cm);

#ifdef SAFETY
        validateOverloadsFull(cm);
#endif
        // The main parse (all top-level function bodies)
        parseFunctionBodies(topLevelSignatures, cm->tokens, cm);
    }
    return cm;
}

/** Parses a single file in 4 passes, see docs/parser.txt */
testable Compiler* parse(Compiler* cm, Compiler* proto, Arena* a) {
    initializeParser(cm, proto, a);
    return parseMain(cm, a);
}

//}}}
//{{{ Typer

/** Gets the type of the last param of a function. */
private Int getFirstParamType(Int funcTypeId, Compiler* cm) {
    return cm->types.content[funcTypeId + 2];
}

private bool isFunctionWithParams(Int typeId, Compiler* cm) {
    return cm->types.content[typeId] > 1;
}

/**
 * Performs a "twin" sort: for every swap of param types, the same swap on entityIds is also performed.
 * Example of such a swap: [countConcrete type1 type2 type3 entity1 entity2 entity3] ->
 *                         [countConcrete type3 type1 type2 entity3 entity1 entity2]
 * Sorts only the concrete group of overloads, doesn't touch the generic part. Sorts ASCENDING.
 * Params: startInd = the first index of the overload (the one with the count of concrete overloads)
 *         endInd = the last index belonging to the overload (the one with the last entityId)
 */
testable void sortOverloads(Int startInd, Int endInd, Arr(Int) overloads) {
    Int countAllOverloads = (endInd - startInd)/2;
    if (countAllOverloads == 1) return;
    
    Int concreteEndInd = startInd + overloads[startInd]; // inclusive
    Int i = startInd + 1; // skipping the cell with the count of concrete overloads
    for (i = startInd + 1; i < concreteEndInd; i++) {
        Int minValue = overloads[i];
        Int minInd = i;
        for (Int j = i + 1; j <= concreteEndInd; j++) {
            if (overloads[j] < minValue) {
                minValue = overloads[j];
                minInd = j;
            }
        }
        if (minInd == i) {
            continue;
        }
        
        // swap the typeIds
        Int tmp = overloads[i];
        overloads[i] = overloads[minInd];
        overloads[minInd] = tmp;
        // swap the corresponding entities
        tmp = overloads[i + countAllOverloads];
        overloads[i + countAllOverloads] = overloads[minInd + countAllOverloads];
        overloads[minInd + countAllOverloads] = tmp;
    }
}

/**
 * After the overloads have been sorted, we need to make sure they're all unique.
 * Params: startInd = the first index of the overload (the one with the count of concrete overloads)
 *         endInd = the last index belonging to the overload (the one with the last entityId)
 * Returns: true if they're all unique
 */
testable bool makeSureOverloadsUnique(Int startInd, Int endInd, Arr(Int) overloads) {
    Int concreteEndInd = startInd + overloads[startInd]; // inclusive
    Int currTypeId = overloads[startInd + 1];
    Int i = startInd + 2;
    while (i <= concreteEndInd) {
        if (overloads[i] == currTypeId) {
            return false;
        }
        currTypeId = overloads[i];
        ++i;
    }
    return true;
}

/** Performs a binary search among the concrete overloads. Returns false if nothing is found */
testable bool overloadBinarySearch(Int typeIdToFind, Int startInd, Int sentinelInd, Int* entityId, Arr(Int) overloads) {
    Int countAllOverloads = (sentinelInd - startInd - 1)/2;
    Int i = startInd + 1;
    Int j = startInd + overloads[startInd];
    
    if (overloads[i] == typeIdToFind) {
        *entityId = overloads[i + countAllOverloads];
        return true;
    } else if (overloads[j] == typeIdToFind) {
        *entityId = overloads[j + countAllOverloads];
        return true;
    }
    
    while (i < j) {
        if (j - i == 1) {
            return false;            
        }
        Int midInd = (i + j)/2;
        Int mid = overloads[midInd];
        if (mid > typeIdToFind) {
            j = midInd;
        } else if (mid < typeIdToFind) {
            i = midInd;
        } else {
            *entityId = overloads[midInd + countAllOverloads];
            return true;
        }                        
    }    
    return false;
}

/**
 * Params: start = ind of the first element of the overload
 *         end = ind of the last entityId of the overload (inclusive)
 *         entityId = address where to store the result, if successful
 */
testable bool findOverload(Int typeId, Int start, Int sentinel, Int* entityId, Compiler* cm) {
    return overloadBinarySearch(typeId, start, sentinel, entityId, cm->overloads.content);
}

/** Shifts elements from start and until the end to the left.
 * E.g. the call with args (4, 2) takes the stack from [x x x x 1 2 3] to [x x 1 2 3]
 */
private void shiftTypeStackLeft(Int startInd, Int byHowMany, Compiler* cm) {
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


#ifdef TRACE
private void printExpSt(StackInt* st) {
    printIntArray(st->length, st->content);
}
#endif

/** Populates the expression's type stack with the operands and functions of an expression */
private void populateExpStack(Int indExpr, Int sentinelNode, Int* currAhead, Compiler* cm) {
    cm->expStack->length = 0;
    for (Int j = indExpr + 1; j < sentinelNode; ++j) {
        Node nd = cm->nodes.content[j];
        if (nd.tp <= tokString) {
            push((Int)nd.tp, cm->expStack);
        } else if (nd.tp == nodCall) {
            push(BIG + nd.pl2, cm->expStack); // signifies that it's a call, and its arity
            push((nd.pl2 > 0 ? -nd.pl1 - 2 : nd.pl1), cm->expStack); // index into overloadIds, or entityId for 0-arity fns
            ++(*currAhead);
        } else if (nd.pl1 > -1) { // entityId 
            push(cm->entities.content[nd.pl1].typeId, cm->expStack);
        } else { // overloadId
            push(nd.pl1, cm->expStack); // overloadId            
        }
    }
}

/** Typechecks and resolves overloads in a single expression */
testable Int typeCheckResolveExpr(Int indExpr, Int sentinelNode, Compiler* cm) {
    Int currAhead = 0; // how many elements ahead we are compared to the token array (because of extra call indicators)
    StackInt* st = cm->expStack;
    
    populateExpStack(indExpr, sentinelNode, &currAhead, cm);
#ifdef TRACE
    printExpSt(st);
#endif
    // now go from back to front, resolving the calls, typechecking & collapsing args, and replacing calls with their return types
    Int j = cm->expStack->length - 1;
    Arr(Int) cont = st->content;
    while (j > -1) {
        if (cont[j] < BIG) { // it's not a function call, because function call indicators have BIG in them
            --j;
            continue;
        }
        
        // It's a function call. cont[j] contains the arity, cont[j + 1] the index into overloads table
        Int arity = cont[j] - BIG;
        Int o = cont[j + 1]; // index into the table of overloadIds, or, for 0-arity funcs, directly their entityId
        if (arity == 0) {
            VALIDATEP(o > -1, errTypeZeroArityOverload)

            Int functionTypeInd = cm->entities.content[o].typeId;
#ifdef SAFETY
            VALIDATEI(cm->types.content[functionTypeInd] == 1, iErrorOverloadsIncoherent)
#endif
            
            cont[j] = cm->types.content[functionTypeInd + 1]; // write the return type
            shiftTypeStackLeft(j + 2, 1, cm); // the function returns nothing, so there's no return type to write
            --currAhead;
#ifdef TRACE
            printExpSt(st);
#endif
        } else {
            Int tpFstArg = cont[j + 2]; // + 1 for the element with the overloadId of the func, +1 for return type
            VALIDATEP(tpFstArg > -1, errTypeUnknownFirstArg)
            Int entityId;

            bool ovFound = findOverload(tpFstArg, cm->overloadIds.content[o], cm->overloadIds.content[o + 1], &entityId, cm);

#ifdef TRACE
            if (!ovFound) {
                print("type first %d", tpFstArg)
                print("cm->overloadIds.content[o] %d cm->overloadIds.content[o + 1] %d", cm->overloadIds.content[o], cm->overloadIds.content[o + 1])
                printIntArrayOff(190, 3, cm->overloads.content);
                printExpSt(st);
            }
#endif
            VALIDATEP(ovFound, errTypeNoMatchingOverload)
            
            Int typeOfFunc = cm->entities.content[entityId].typeId;
            VALIDATEP(cm->types.content[typeOfFunc] - 1 == arity, errTypeNoMatchingOverload) // first parm matches, but not arity
            
            // We know the type of the function, now to validate arg types against param types
            for (int k = j + 3; k < j + arity + 2; k++) { // not "k = j + 2", because we've already checked the first param
                if (cont[k] > -1) {                  
                    VALIDATEP(cont[k] == cm->types.content[typeOfFunc + k - j], errTypeWrongArgumentType)
                } else {
                    Int argBindingId = cm->nodes.content[indExpr + k - currAhead].pl1;
                    cm->entities.content[argBindingId].typeId = cm->types.content[typeOfFunc + k - j];
                }
            }
            --currAhead;
            cm->nodes.content[j + indExpr + 1 - currAhead].pl1 = entityId; // the type-resolved function of the call
            cont[j] = cm->types.content[typeOfFunc + 1];         // the function return type
            shiftTypeStackLeft(j + arity + 2, arity + 1, cm);
#ifdef TRACE
            printExpSt(st);
#endif
        }
        --j;
    }
    
    if (st->length == 1) {
        return st->content[0]; // the last remaining element is the type of the expression
    } else {
        return -1;
    }
}

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


/** Ensures that the buffer has space for at least that many bytes plus 10 by increasing its capacity if necessary */
private void ensureBufferLength(Int additionalLength, Codegen* cg) {
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

/** Write a constant to codegen */
private void writeConstant(Int indConst, Codegen* cg) {
    Int len = constantOffsets[indConst + 1] - constantOffsets[indConst];
    ensureBufferLength(len, cg);
    memcpy(cg->buffer + cg->length, constantStrings + constantOffsets[indConst], len);
    cg->length += len;
}

/** Write a constant to codegen and add a space after it */
private void writeConstantWithSpace(Int indConst, Codegen* cg) {
    Int len = constantOffsets[indConst + 1] - constantOffsets[indConst];
    ensureBufferLength(len, cg); // no need for a "+ 1", for the function automatically ensures 10 extra bytes
    memcpy(cg->buffer + cg->length, constantStrings + constantOffsets[indConst], len);
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

/** Precondition: we are 1 past the expr node */
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
                        ? (CgCall){.emit = ent.emit, .arity = n.pl2, .countArgs = 0, .startInd = n.startBt, .length = n.lenBts }
                        : (CgCall){.emit = ent.emit, .arity = n.pl2, .countArgs = 0, .startInd = ent.externalNameId };

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

/** Precondition: we are looking 1 past the nodExpr/singular node. Consumes all nodes of the expr */
private void writeExprInternal(Node nd, Arr(Node) nodes, Codegen* cg) {
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


/** Writes a function definition */
private void writeFn(Node nd, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (isEntry) {
        pushCgFrame(nd, cg);
        writeIndentation(cg);
        writeConstantWithSpace(strFunction, cg);

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

/** Pre-condition: we are looking at the binding node, 1 past the assignment node */
private void writeAssignmentWorker(Arr(Node) nodes, Codegen* cg) {
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

/**
 * 
 */
private void writeAssignment(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    Int sentinel = cg->i + fr.pl2;
    writeIndentation(cg);
    Node binding = nodes[cg->i];
    Int class = cg->cm->entities.content[binding.pl1].class;
    if (class == classMutatedGuaranteed || class == classMutatedNullable) {
        writeConstantWithSpace(strLet, cg);
    } else {
        writeConstantWithSpace(strConst, cg);
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
    writeConstantWithSpace(strReturn, cg);

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
    writeConstantWithSpace(strIf, cg);
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
            writeConstantWithSpace(strElse, cg);
            writeChar(aCurlyLeft, cg);
        } else {
            // there is another condition after this clause, so we write it out as an "else if"
            writeChar(aSpace, cg);
            writeConstantWithSpace(strElse, cg);
            writeConstantWithSpace(strIf, cg);
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
    writeConstant(strLo, cg);
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
                writeConstantWithSpace(strLet, cg);
                writeAssignmentWorker(nodes, cg);
            }
        } else {
            pushCgFrameWithSentinel(fr, sentinel, cg);
        }

        writeIndentation(cg);
        writeConstantWithSpace(strWhile, cg);
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
#ifdef TRACE
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
//{{{ Main
Codegen* compile(String* source) {
    Arena* a = mkArena();
    Compiler* proto = createCompilerProto(a);
    
    Compiler* lx = lexicallyAnalyze(source, proto, a);
    if (lx->wasError) {
        print("lexer error");
    }
#ifdef TRACE
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
