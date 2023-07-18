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

#ifdef TEST
    #define testable
#else
    #define testable static
#endif

#ifdef DEBUG
#define DBG(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)
#else
#define DBG(fmt, ...) // empty
#endif      

jmp_buf excBuf;

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



#define DEFINE_INTERNAL_LIST_CONSTRUCTOR(T)                  \
testable InList##T createInList##T(Int initCap, Arena* a) { \
    return (InList##T){                                      \
        .content = allocateOnArena(initCap*sizeof(T), a),     \
        .length = 0, .capacity = initCap };                   \
}

#define DEFINE_INTERNAL_LIST(fieldName, T, aName)              \
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

//{{{ Good strings

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
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
#define aQuote        34
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
        print("j %d", j)
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
#define setSpanLength(A, X) _Generic((X), Lexer*: setSpanLengthLexer, Compiler*: setSpanLengthParser)(A, X)

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
        newBucket->capAndLen = (8 << 16) + 1; // left u16 = capacity, right u16 = length
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
    Int newIndString;    
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
const char errCoreMissingParen[]           = "Core form requires opening parenthesis/curly brace before keyword!"; 
const char errCoreNotAtSpanStart[]         = "Reserved word must be at the start of a parenthesized span";
const char errDocComment[]                 = "Doc comments must have the syntax: (*comment)";
const char errBareAtom[]                   = "Malformed token stream (atoms and parentheses must not be bare)";
const char errImportsNonUnique[]           = "Import names must be unique!";
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

#define LEXER_INIT_SIZE 2000

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

static const byte reservedBytesAlias[]       = { 97, 108, 105, 97, 115 };
static const byte reservedBytesAnd[]         = { 97, 110, 100 };
static const byte reservedBytesAssert[]      = { 97, 115, 115, 101, 114, 116 };
static const byte reservedBytesAssertDbg[]   = { 97, 115, 115, 101, 114, 116, 68, 98, 103 };
static const byte reservedBytesAwait[]       = { 97, 119, 97, 105, 116 };
static const byte reservedBytesBreak[]       = { 98, 114, 101, 97, 107 };
static const byte reservedBytesCase[]        = { 99, 97, 115, 101 };   // probably won't need, just use "=>"
static const byte reservedBytesCatch[]       = { 99, 97, 116, 99, 104 };
static const byte reservedBytesContinue[]    = { 99, 111, 110, 116, 105, 110, 117, 101 };
static const byte reservedBytesDispose[]     = { 100, 105, 115, 112, 111, 115, 101 }; // change to "defer"
static const byte reservedBytesElse[]        = { 101, 108, 115, 101 };
static const byte reservedBytesEmbed[]       = { 101, 109, 98, 101, 100 };
static const byte reservedBytesExport[]      = { 101, 120, 112, 111, 114, 116 };
static const byte reservedBytesFalse[]       = { 102, 97, 108, 115, 101 };
static const byte reservedBytesFn[]          = { 102, 110 };  // maybe also "Fn" for function types
static const byte reservedBytesIf[]          = { 105, 102 };
static const byte reservedBytesIfEq[]        = { 105, 102, 69, 113 };
static const byte reservedBytesIfPr[]        = { 105, 102, 80, 114 };
static const byte reservedBytesImpl[]        = { 105, 109, 112, 108 };
static const byte reservedBytesInterface[]   = { 105, 110, 116, 101, 114, 102, 97, 99, 101 };
static const byte reservedBytesLambda[]      = { 108, 97, 109 };
static const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
static const byte reservedBytesMut[]         = { 109, 117, 116 }; // replace with "immut" or "val" for struct fields
static const byte reservedBytesOr[]          = { 111, 114 };
static const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
static const byte reservedBytesStruct[]      = { 115, 116, 114, 117, 99, 116 };
static const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
static const byte reservedBytesTry[]         = { 116, 114, 121 };
static const byte reservedBytesYield[]       = { 121, 105, 101, 108, 100 };
static const byte reservedBytesWhile[]       = { 119, 104, 105, 108, 101 };

/** All the symbols an operator may start with. "-" is absent because it's handled by lexMinus, "=" is handled by lexEqual
 */
const int operatorStartSymbols[16] = {
    aExclamation, aSharp, aDollar, aPercent, aAmp, aApostrophe, aTimes, aPlus, aComma, aDivBy, 
    aLT, aGT, aQuestion, aAt, aCaret, aPipe
};

//}}}


#define CURR_BT inp[lx->i]
#define NEXT_BT inp[lx->i + 1]
#define VALIDATEI(cond, errInd) if (!(cond)) { throwExcInternal(errInd, cm); }
#define VALIDATEL(cond, errMsg) if (!(cond)) { throwExcLexer(errMsg, lx); }


typedef union {
    uint64_t i;
    double   d;
} FloatingBits;

_Noreturn private void throwExcInternal(Int errInd, Compiler* cm) {   
    cm->wasError = true;    
    printf("Internal error %d\n", errInd);  
    cm->errMsg = stringOfInt(errInd, cm->a);
    printString(cm->errMsg);
    longjmp(excBuf, 1);
}


/** Sets i to beyond input's length to communicate to callers that lexing is over */
_Noreturn private void throwExcLexer(const char errMsg[], Lexer* lx) {   
    lx->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d: %s\n", lx->i, errMsg);
#endif    
    lx->errMsg = str(errMsg, lx->arena);
    longjmp(excBuf, 1);
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private void checkPrematureEnd(Int requiredSymbols, Lexer* lx) {
    VALIDATEL(lx->i + requiredSymbols <= lx->inpLength, errPrematureEndOfInput)
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunkLexer(Lexer* lexer) {
    Token* newStorage = allocateOnArena(lexer->capacity*2*sizeof(Token), lexer->arena);
    memcpy(newStorage, lexer->tokens, (lexer->capacity)*(sizeof(Token)));
    lexer->tokens = newStorage;

    lexer->capacity *= 2;
}


testable void add(Token t, Lexer* lexer) {
    lexer->tokens[lexer->nextInd] = t;
    lexer->nextInd++;
    if (lexer->nextInd == lexer->capacity) {
        handleFullChunkLexer(lexer);
    }
}

/** For all the dollars at the top of the backtrack, turns them into parentheses, sets their lengths and closes them */
private void closeColons(Lexer* lx) {
    while (hasValues(lx->backtrack) && peek(lx->backtrack).wasOrigColon) {
        BtToken top = pop(lx->backtrack);
        Int j = top.tokenInd;        
        lx->tokens[j].lenBts = lx->i - lx->tokens[j].startBt;
        lx->tokens[j].pl2 = lx->nextInd - j - 1;
    }
}

/**
 * Finds the top-level punctuation opener by its index, and sets its lengths.
 * Called when the matching closer is lexed.
 */
private void setSpanLengthLexer(Int tokenInd, Lexer* lx) {
    lx->tokens[tokenInd].lenBts = lx->i - lx->tokens[tokenInd].startBt + 1;
    lx->tokens[tokenInd].pl2 = lx->nextInd - tokenInd - 1;
}

/**
 * Correctly calculates the lenBts for a single-line, statement-type span.
 */
private void setStmtSpanLength(Int topTokenInd, Lexer* lx) {
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
    if (lenBts == lenReser && testForWord(lx->inp, startBt, reservedBytesName, lenReser)) \
        return returnVarName;


private Int determineReservedA(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesAlias, tokAlias)
    PROBERESERVED(reservedBytesAnd, reservedAnd)
    PROBERESERVED(reservedBytesAwait, tokAwait)
    PROBERESERVED(reservedBytesAssert, tokAssert)
    PROBERESERVED(reservedBytesAssertDbg, tokAssertDbg)
    return 0;
}

private Int determineReservedB(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesBreak, tokBreak)
    return 0;
}

private Int determineReservedC(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesCatch, tokCatch)
    PROBERESERVED(reservedBytesContinue, tokContinue)
    return 0;
}


private Int determineReservedE(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesElse, tokElse)
    PROBERESERVED(reservedBytesEmbed, tokEmbed)
    return 0;
}


private Int determineReservedF(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesFalse, reservedFalse)
    PROBERESERVED(reservedBytesFn, tokFnDef)
    return 0;
}


private Int determineReservedI(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesIf, tokIf)
    PROBERESERVED(reservedBytesIfPr, tokIfPr)
    PROBERESERVED(reservedBytesImpl, tokImpl)
    PROBERESERVED(reservedBytesInterface, tokIface)
    return 0;
}


private Int determineReservedL(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesLambda, tokLambda)
    PROBERESERVED(reservedBytesWhile, tokWhile)
    return 0;
}


private Int determineReservedM(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesMatch, tokMatch)
    return 0;
}


private Int determineReservedO(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesOr, reservedOr)
    return 0;
}


private Int determineReservedR(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesReturn, tokReturn)
    return 0;
}


private Int determineReservedS(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesStruct, tokStruct)
    return 0;
}


private Int determineReservedT(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesTrue, reservedTrue)
    PROBERESERVED(reservedBytesTry, tokTry)
    return 0;
}


private Int determineReservedY(Int startBt, Int lenBts, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesYield, tokYield)
    return 0;
}

private Int determineUnreserved(Int startBt, Int lenBts, Lexer* lx) {
    return -1;
}


private void addNewLine(Int j, Lexer* lx) {
    lx->newlines[lx->newlinesNextInd] = j;
    lx->newlinesNextInd++;
    if (lx->newlinesNextInd == lx->newlinesCapacity) {
        Arr(Int) newNewlines = allocateOnArena(lx->newlinesCapacity*2*4, lx->arena);
        memcpy(newNewlines, lx->newlines, lx->newlinesCapacity*4);
        lx->newlines = newNewlines;
        lx->newlinesCapacity *= 2;
    }
}

private void addStatement(untt stmtType, Int startBt, Lexer* lx) {
    Int lenBts = 0;
    // Some types of statements may legitimately consist of 0 tokens; for them, we need to write their lenBts in the init token
    if (stmtType == tokBreak) {
        lenBts = 5;
    } else if (stmtType == tokContinue) {
        lenBts = 8;
    }
    push(((BtToken){ .tp = stmtType, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->backtrack);
    add((Token){ .tp = stmtType, .startBt = startBt, .lenBts = lenBts}, lx);
}


private void wrapInAStatementStarting(Int startBt, Lexer* lx, Arr(byte) inp) {    
    if (hasValues(lx->backtrack)) {
        if (peek(lx->backtrack).spanLevel <= slParenMulti) {            
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->backtrack);
            add((Token){ .tp = tokStmt, .startBt = startBt},  lx);
        }
    } else {
        addStatement(tokStmt, startBt, lx);
    }
}


private void wrapInAStatement(Lexer* lx, Arr(byte) inp) {
    if (hasValues(lx->backtrack)) {
        if (peek(lx->backtrack).spanLevel <= slParenMulti) {
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->backtrack);
            add((Token){ .tp = tokStmt, .startBt = lx->i},  lx);
        }
    } else {
        addStatement(tokStmt, lx->i, lx);
    }
}

private void maybeBreakStatement(Lexer* lx) {
    if (hasValues(lx->backtrack)) {
        BtToken top = peek(lx->backtrack);
        if(top.spanLevel == slStmt) {
            Int len = lx->backtrack->length;
            setStmtSpanLength(top.tokenInd, lx);
            pop(lx->backtrack);
        }
    }
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ) or a ] .
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private void closeRegularPunctuation(Int closingType, Lexer* lx) {
    StackBtToken* bt = lx->backtrack;
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
    setSpanLength(top.tokenInd, lx);
    lx->i++; // CONSUME the closing ")" or "]"
}


private void addNumeric(byte b, Lexer* lx) {
    if (lx->numericNextInd < lx->numericCapacity) {
        lx->numeric[lx->numericNextInd] = b;
    } else {
        Arr(byte) newNumeric = allocateOnArena(lx->numericCapacity*2, lx->arena);
        memcpy(newNumeric, lx->numeric, lx->numericCapacity*4);
        newNumeric[lx->numericCapacity] = b;
        
        lx->numeric = newNumeric;
        lx->numericCapacity *= 2;       
    }
    lx->numericNextInd++;
}


private int64_t calcIntegerWithinLimits(Lexer* lx) {
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
private bool integerWithinDigits(const byte* b, Int bLength, Lexer* lx){
    if (lx->numericNextInd != bLength) return (lx->numericNextInd < bLength);
    for (Int j = 0; j < lx->numericNextInd; j++) {
        if (lx->numeric[j] < b[j]) return true;
        if (lx->numeric[j] > b[j]) return false;
    }
    return true;
}


private Int calcInteger(int64_t* result, Lexer* lx) {
    if (lx->numericNextInd > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lx)) return -1;
    *result = calcIntegerWithinLimits(lx);
    return 0;
}


private int64_t calcHexNumber(Lexer* lx) {
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
private void hexNumber(Lexer* lx, Arr(byte) inp) {
    checkPrematureEnd(2, lx);
    lx->numericNextInd = 0;
    Int j = lx->i + 2;
    while (j < lx->inpLength) {
        byte cByte = inp[j];
        if (isDigit(cByte)) {
            addNumeric(cByte - aDigit0, lx);
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            addNumeric(cByte - aALower + 10, lx);
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            addNumeric(cByte - aAUpper + 10, lx);
        } else if (cByte == aUnderscore && (j == lx->inpLength - 1 || isHexDigit(inp[j + 1]))) {            
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
private Int calcFloating(double* result, Int powerOfTen, Lexer* lx, Arr(byte) inp){
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

/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO: add support for the '1.23E4' format
 */
private void decNumber(bool isNegative, Lexer* lx, Arr(byte) inp) {
    Int j = (isNegative) ? (lx->i + 1) : lx->i;
    Int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
    bool metDot = false;
    bool metNonzero = false;
    Int maximumInd = (lx->i + 40 > lx->inpLength) ? (lx->i + 40) : lx->inpLength;
    while (j < maximumInd) {
        byte cByte = inp[j];

        if (isDigit(cByte)) {
            if (metNonzero) {
                addNumeric(cByte - aDigit0, lx);
            } else if (cByte != aDigit0) {
                metNonzero = true;
                addNumeric(cByte - aDigit0, lx);
            }
            if (metDot) digitsAfterDot++;
        } else if (cByte == aUnderscore) {
            VALIDATEL(j != (lx->inpLength - 1) && isDigit(inp[j + 1]), errNumericEndUnderscore)
        } else if (cByte == aDot) {
            if (j + 1 < maximumInd && !isDigit(inp[j + 1])) { // this dot is not decimal - it's a statement closer
                break;
            }
            
            VALIDATEL(!metDot, errNumericMultipleDots)
            metDot = true;
        } else {
            break;
        }
        j++;
    }

    VALIDATEL(j >= lx->inpLength || !isDigit(inp[j]), errNumericWidthExceeded)

    if (metDot) {
        double resultValue = 0;
        Int errorCode = calcFloating(&resultValue, -digitsAfterDot, lx, inp);
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

private void lexNumber(Lexer* lx, Arr(byte) inp) {
    wrapInAStatement(lx, inp);
    byte cByte = CURR_BT;
    if (lx->i == lx->inpLength - 1 && isDigit(cByte)) {
        add((Token){ .tp = tokInt, .pl2 = cByte - aDigit0, .startBt = lx->i, .lenBts = 1 }, lx);
        lx->i++; // CONSUME the single-digit number
        return;
    }
    
    byte nByte = NEXT_BT;
    if (nByte == aXLower) {
        hexNumber(lx, inp);
    } else {
        decNumber(false, lx, inp);
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
private void openPunctuation(untt tType, untt spanLevel, Int startBt, Lexer* lx) {
    push( ((BtToken){ .tp = tType, .tokenInd = lx->nextInd, .spanLevel = spanLevel}), lx->backtrack);
    add((Token) {.tp = tType, .pl1 = (tType < firstCoreFormTokenType) ? 0 : spanLevel, .startBt = startBt }, lx);
}

/**
 * Lexer action for a paren-type reserved word. It turns parentheses into an slParenMulti core form.
 * If necessary (parens inside statement) it also deletes last token and removes the top frame
 */
private void lexReservedWord(untt reservedWordType, Int startBt, Lexer* lx, Arr(byte) inp) {    
    StackBtToken* bt = lx->backtrack;
    
    Int expectations = (*lx->langDef->reservedParensOrNot)[reservedWordType - firstCoreFormTokenType];
    if (expectations == 0 || expectations == 2) { // the reserved words that live at the start of a statement
        VALIDATEL(!hasValues(bt) || peek(bt).spanLevel == slScope, errCoreNotInsideStmt)
        addStatement(reservedWordType, startBt, lx);
    } else if (expectations == 1) { // the "(core" case
        VALIDATEL(hasValues(bt), errCoreMissingParen)
        
        BtToken top = peek(bt);
        VALIDATEL(top.tokenInd == lx->nextInd - 1, errCoreNotAtSpanStart) // if this isn't the first token inside the parens
        
        if (bt->length > 1 && bt->content[bt->length - 2].tp == tokStmt) {
            // Parens are wrapped in statements because we didn't know that the following token is a reserved word.
            // So when meeting a reserved word at start of parens, we need to delete the paren token & lex frame.
            // The new reserved token type will be written over the tokStmt that precedes the tokParen.
            pop(bt);
            lx->nextInd--;
            top = peek(bt);
        }
        
        // update the token type and the corresponding frame type
        lx->tokens[top.tokenInd].tp = reservedWordType;
        lx->tokens[top.tokenInd].pl1 = slParenMulti;
        bt->content[bt->length - 1].tp = reservedWordType;
        bt->content[bt->length - 1].spanLevel = top.tp == tokScope ? slScope : slParenMulti;
    }
}

/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was capitalized
 */
private bool wordChunk(Lexer* lx, Arr(byte) inp) {
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
private void closeStatement(Lexer* lx) {
    BtToken top = peek(lx->backtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errPunctuationOnlyInMultiline)
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->backtrack);
    }    
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Precondition: we are pointing at the first letter character of the word (i.e. past the possible "@" or ":")
 * Examples of acceptable expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of unacceptable expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private void wordInternal(untt wordType, Lexer* lx, Arr(byte) inp) {
    Int startBt = lx->i;

    bool wasCapitalized = wordChunk(lx, inp);

    while (lx->i < (lx->inpLength - 1)) {
        byte currBt = CURR_BT;
        if (currBt == aMinus) {
            byte nextBt = NEXT_BT;
            if (isLetter(nextBt) || nextBt == aUnderscore) {
                lx->i++; // CONSUME the letter or underscore
                bool isCurrCapitalized = wordChunk(lx, inp);
                VALIDATEL(!wasCapitalized, errWordCapitalizationOrder)
                wasCapitalized = isCurrCapitalized;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    Int realStartByte = (wordType == tokWord) ? startBt : (startBt - 1); // accounting for the . or @ at the start
    untt finalTokType = wasCapitalized ? tokTypeName : wordType;

    byte firstByte = lx->inp->content[startBt];
    Int lenBts = lx->i - realStartByte;
    Int lenString = lx->i - startBt;
        
    if (firstByte < aALower || firstByte > aYLower) {
        wrapInAStatementStarting(startBt, lx, inp);
        Int uniqueStringInd = addStringStore(inp, startBt, lenBts, lx->stringTable, lx->stringStore);
        add((Token){ .tp = finalTokType, .pl2 = uniqueStringInd, .startBt = realStartByte, .lenBts = lenBts }, lx);
        return;
    }
    Int mbReservedWord = (*lx->possiblyReservedDispatch)[firstByte - aALower](startBt, lenString, lx);
    if (mbReservedWord <= 0) {
        wrapInAStatementStarting(startBt, lx, inp);
        Int uniqueStringInd = addStringStore(inp, startBt, lenString, lx->stringTable, lx->stringStore);
        add((Token){ .tp=finalTokType, .pl2 = uniqueStringInd, .startBt = realStartByte, .lenBts = lenBts }, lx);
        return;
    }

    VALIDATEL(wordType != tokDotWord, errWordReservedWithDot)

    if (mbReservedWord == tokElse){
        closeStatement(lx);
        add((Token){.tp = tokElse, .startBt = realStartByte, .lenBts = 4}, lx);
    } else if (mbReservedWord < firstCoreFormTokenType) {
        if (mbReservedWord == reservedAnd) {
            wrapInAStatementStarting(startBt, lx, inp);
            add((Token){.tp=tokOperator, .pl1 = opTAnd, .startBt=realStartByte, .lenBts=3}, lx);
        } else if (mbReservedWord == reservedOr) {
            wrapInAStatementStarting(startBt, lx, inp);
            add((Token){.tp=tokOperator, .pl1 = opTOr, .startBt=realStartByte, .lenBts=2}, lx);
        } else if (mbReservedWord == reservedTrue) {
            wrapInAStatementStarting(startBt, lx, inp);
            add((Token){.tp=tokBool, .pl2=1, .startBt=realStartByte, .lenBts=4}, lx);
        } else if (mbReservedWord == reservedFalse) {
            wrapInAStatementStarting(startBt, lx, inp);
            add((Token){.tp=tokBool, .pl2=0, .startBt=realStartByte, .lenBts=5}, lx);
        }
    } else {
        lexReservedWord(mbReservedWord, realStartByte, lx, inp);
    }
}


private void lexWord(Lexer* lx, Arr(byte) inp) {
    wordInternal(tokWord, lx, inp);
}

/** 
 * The dot is a statement separator
 */
private void lexDot(Lexer* lx, Arr(byte) inp) {
    if (lx->i < lx->inpLength - 1 && isLetter(NEXT_BT)) {        
        lx->i++; // CONSUME the dot
        wordInternal(tokDotWord, lx, inp);
        return;        
    }
    if (!hasValues(lx->backtrack) || peek(lx->backtrack).spanLevel == slScope) {
        // if we're at top level or directly inside a scope, do nothing since there're no stmts to close
    } else {
        closeStatement(lx);
        lx->i++;  // CONSUME the dot
    }
}

/**
 * Handles the "=", ":=" and "+=" tokens. 
 * Changes existing tokens and parent span to accout for the fact that we met an assignment operator 
 * mutType = 0 if it's immutable assignment, 1 if it's ":=", 2 if it's a regular operator mut "+="
 */
private void processAssignment(Int mutType, untt opType, Lexer* lx) {
    BtToken currSpan = peek(lx->backtrack);

    if (currSpan.tp == tokAssignment || currSpan.tp == tokReassign || currSpan.tp == tokMutation) {
        throwExcLexer(errOperatorMultipleAssignment, lx);
    } else if (currSpan.spanLevel != slStmt) {
        throwExcLexer(errOperatorAssignmentPunct, lx);
    }
    Int tokenInd = currSpan.tokenInd;
    Token* tok = (lx->tokens + tokenInd);
    untt tp;
    
    if (mutType == 0) {
        tok->tp = tokAssignment;
    } else if (mutType == 1) {
        tok->tp = tokReassign;
    } else {
        tok->tp = tokMutation;
        tok->pl1 = opType;
    } 
    lx->backtrack->content[lx->backtrack->length - 1] = (BtToken){ .tp = tok->tp, .spanLevel = slStmt, .tokenInd = tokenInd }; 
}

/**
 * A single colon means "parentheses until the next closing paren or end of statement"
 */
private void lexColon(Lexer* lx, Arr(byte) inp) {           
    if (lx->i < lx->inpLength && NEXT_BT == aEqual) { // mutation assignment, :=
        lx->i += 2; // CONSUME the ":="
        processAssignment(1, 0, lx);
    } else {
        push(((BtToken){ .tp = tokParens, .tokenInd = lx->nextInd, .spanLevel = slSubexpr, .wasOrigColon = true}),
             lx->backtrack);
        add((Token) {.tp = tokParens, .startBt = lx->i }, lx);
        lx->i++; // CONSUME the ":"
    }    
}


private void lexOperator(Lexer* lx, Arr(byte) inp) {
    wrapInAStatement(lx, inp);    
    
    byte firstSymbol = CURR_BT;
    byte secondSymbol = (lx->inpLength > lx->i + 1) ? inp[lx->i + 1] : 0;
    byte thirdSymbol = (lx->inpLength > lx->i + 2) ? inp[lx->i + 2] : 0;
    byte fourthSymbol = (lx->inpLength > lx->i + 3) ? inp[lx->i + 3] : 0;
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
    if (opDef.assignable && j < lx->inpLength && inp[j] == aEqual) {
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
private void lexEqual(Lexer* lx, Arr(byte) inp) {
    checkPrematureEnd(2, lx);
    byte nextBt = NEXT_BT;
    if (nextBt == aEqual) {
        lexOperator(lx, inp); // ==        
    } else if (nextBt == aGT) { // => is a statement terminator inside if-like scopes        
        // arrows are only allowed inside "if"s and the like
        VALIDATEL(lx->backtrack->length >= 2, errCoreMisplacedArrow)
        
        BtToken grandparent = lx->backtrack->content[lx->backtrack->length - 2];
        VALIDATEL(grandparent.tp == tokIf, errCoreMisplacedArrow)
        closeStatement(lx);
        add((Token){ .tp = tokArrow, .startBt = lx->i, .lenBts = 2 }, lx);
        lx->i += 2;  // CONSUME the arrow "=>"
    } else {
        if (lx->backtrack->length > 1) {

            BtToken grandparent = lx->backtrack->content[lx->backtrack->length - 2];
            BtToken parent = lx->backtrack->content[lx->backtrack->length - 1];
            if (grandparent.tp == tokFnDef && parent.tokenInd == (grandparent.tokenInd + 1)) { // we are in the 1st stmt of fn def
                closeStatement(lx);
                add((Token){ .tp = tokEqualsSign, .startBt = lx->i, .lenBts = 1 }, lx);
                lx->i++; // CONSUME the =
                return;
            }
        }
        processAssignment(0, 0, lx);
        lx->i++; // CONSUME the =
    }
}

/** Doc comments, syntax is "{ Doc comment }" */
private void lexDocComment(Lexer* lx, Arr(byte) inp) {
    Int startBt = lx->i;
    
    Int curlyLevel = 0;
    Int j = lx->i;
    for (; j < lx->inpLength; j++) {
        byte cByte = inp[j];
        // Doc comments may contain arbitrary UTF-8 symbols, but we care only about newlines and parentheses
        if (cByte == aNewline) {
            addNewLine(j, lx);
        } else if (cByte == aCurlyLeft) {
            curlyLevel++;
        } else if (cByte == aCurlyRight) {
            curlyLevel--;
            if (curlyLevel == 0) {
                j++; // CONSUME the right curly brace
                break;
            }
        }
    }
    VALIDATEL(curlyLevel == 0, errPunctuationExtraOpening)
    if (j > lx->i) {
        add((Token){.tp = tokDocComment, .startBt = startBt, .lenBts = j - startBt}, lx);
    }
    lx->i = j; // CONSUME the doc comment
}

/** Handles the binary operator as well as the unary negation operator */
private void lexMinus(Lexer* lx, Arr(byte) inp) {
    if (lx->i == lx->inpLength - 1) {        
        lexOperator(lx, inp);
    } else {
        byte nextBt = NEXT_BT;
        if (isDigit(nextBt)) {
            wrapInAStatement(lx, inp);
            decNumber(true, lx, inp);
            lx->numericNextInd = 0;
        } else if (isLowercaseLetter(nextBt) || nextBt == aUnderscore) {
            add((Token){.tp = tokOperator, .pl1 = opTNegation, .startBt = lx->i, .lenBts = 1}, lx);
            lx->i++; // CONSUME the minus symbol
        } else {
            lexOperator(lx, inp);
        }    
    }
}

/** An ordinary until-line-end comment. It doesn't get included in the AST, just discarded by the lexer.
 * Just like a newline, this needs to test if we're in a breakable statement because a comment goes until the line end.
 */
private void lexComment(Lexer* lx, Arr(byte) inp) {    
    if (lx->i >= lx->inpLength) return;
    
    maybeBreakStatement(lx);
        
    Int j = lx->i;
    while (j < lx->inpLength) {
        byte cByte = inp[j];
        if (cByte == aNewline) {
            lx->i = j + 1; // CONSUME the comment
            return;
        } else {
            j++;
        }
    }
    lx->i = j;  // CONSUME the comment
}

/** If we are inside a compound (=2) core form, we need to increment the clause count */ 
private void mbIncrementClauseCount(Lexer* lx) {
    if (hasValues(lx->backtrack)) {
        BtToken top = peek(lx->backtrack);
        if (top.tp >= firstCoreFormTokenType && (*lx->langDef->reservedParensOrNot)[top.tp - firstCoreFormTokenType] == 2) {
            lx->backtrack->content[lx->backtrack->length - 1].countClauses++;
        }
    }
}

/** If we're inside a compound (=2) core form, we need to check if its clause count is saturated */ 
private void mbCloseCompoundCoreForm(Lexer* lx) {
    BtToken top = peek(lx->backtrack);
    if (top.tp >= firstCoreFormTokenType && (*lx->langDef->reservedParensOrNot)[top.tp - firstCoreFormTokenType] == 2) {
        if (top.countClauses == 2) {
            setSpanLength(top.tokenInd, lx);
            pop(lx->backtrack);
        }
    }
}

/** An opener for a scope or a scopeful core form. Precondition: we are past the "(:" token.
 * Consumes zero or 1 byte
 */
private void openScope(Lexer* lx, Arr(byte) inp) {
    Int startBt = lx->i;
    lx->i += 2; // CONSUME the "(*"
    VALIDATEL(!hasValues(lx->backtrack) || peek(lx->backtrack).spanLevel == slScope, errPunctuationScope)
    VALIDATEL(lx->i < lx->inpLength, errPrematureEndOfInput)
    byte currBt = CURR_BT;
    if (currBt == aWLower && testForWord(lx->inp, lx->i, reservedBytesWhile, 5)) {
        openPunctuation(tokWhile, slScope, startBt, lx);
        lx->i += 5; // CONSUME the "while"
        return; 
    } else if (lx->i < lx->inpLength - 2 && isSpace(inp[lx->i + 1])) {        
        if (currBt == aFLower) {
            openPunctuation(tokFnDef, slScope, startBt, lx);
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
private void lexParenLeft(Lexer* lx, Arr(byte) inp) {
    mbIncrementClauseCount(lx);
    Int j = lx->i + 1;
    VALIDATEL(j < lx->inpLength, errPunctuationUnmatched)
    if (inp[j] == aColon) {
        openScope(lx, inp);
    } else {
        wrapInAStatement(lx, inp);
        openPunctuation(tokParens, slSubexpr, lx->i, lx);
        lx->i++; // CONSUME the left parenthesis
    }
}


private void lexParenRight(Lexer* lx, Arr(byte) inp) {
    // TODO handle syntax like "(foo 5).field" and "(: id 5 name "asdf").id"
    Int startInd = lx->i;
    closeRegularPunctuation(tokParens, lx);
    
    if (!hasValues(lx->backtrack)) return;    
    mbCloseCompoundCoreForm(lx);
    
    lx->lastClosingPunctInd = startInd;    
}


private void lexSpace(Lexer* lx, Arr(byte) inp) {
    lx->i++; // CONSUME the space
    while (lx->i < lx->inpLength && CURR_BT == aSpace) {
        lx->i++; // CONSUME a space
    }
}

/** 
 * Tl is not indentation-sensitive, but it is newline-sensitive. Thus, a newline charactor closes the current
 * statement unless it's inside an inline span (i.e. parens or accessor parens)
 */
private void lexNewline(Lexer* lx, Arr(byte) inp) {
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


private void lexStringLiteral(Lexer* lx, Arr(byte) inp) {
    wrapInAStatement(lx, inp);
    Int j = lx->i + 1;
    for (; j < lx->inpLength && inp[j] != aQuote; j++);
    VALIDATEL(j != lx->inpLength, errPrematureEndOfInput)
    add((Token){.tp=tokString, .startBt=(lx->i), .lenBts=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


private void lexUnexpectedSymbol(Lexer* lx, Arr(byte) inp) {
    throwExcLexer(errUnrecognizedByte, lx);
}

private void lexNonAsciiError(Lexer* lx, Arr(byte) inp) {
    throwExcLexer(errNonAscii, lx);
}

/** Must agree in order with token types in LexerConstants.h */
const char* tokNames[] = {
    "Int", "Long", "Float", "Bool", "String", "_", "DocComment", 
    "word", "Type", ".word", "operator", "equals sign", 
    ":", "(.", "stmt", "(", "type(", "(:", "=", ":=", "mutation", "=>", "else",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue", 
    "defer", "each", "embed", "export", "exposePriv", "fn", "interface", 
    "lambda", "meta", "package", "return", "struct", "try", "typeDef", "yield",
    "if", "ifPr", "match", "impl", "while"
};


testable void printLexer(Lexer* a) {
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
testable int equalityLexer(Lexer a, Lexer b) {    
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
    p[aColon] = &lexColon;
    p[aEqual] = &lexEqual;

    for (Int i = sizeof(operatorStartSymbols)/4 - 1; i > -1; i--) {
        p[operatorStartSymbols[i]] = &lexOperator;
    }
    p[aMinus] = &lexMinus;
    p[aParenLeft] = &lexParenLeft;
    p[aParenRight] = &lexParenRight;
    
    p[aSpace] = &lexSpace;
    p[aCarrReturn] = &lexSpace;
    p[aNewline] = &lexNewline;
    p[aQuote] = &lexStringLiteral;
    p[aSemicolon] = &lexComment;
    p[aCurlyLeft] = &lexDocComment;
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
    p[4] = determineReservedE;
    p[5] = determineReservedF;
    p[8] = determineReservedI;
    p[11] = determineReservedL;
    p[12] = determineReservedM;
    p[14] = determineReservedO;
    p[17] = determineReservedR;
    p[18] = determineReservedS;
    p[19] = determineReservedT;
    p[24] = determineReservedY;
    return result;
}

/**
 * Table for properties of core syntax forms, organized by reserved word.
 * It's indexed on the diff between token id and firstCoreFormTokenType.
 * It contains 1 for all parenthesized core forms and 2 for more complex
 * forms like "fn"
 */
private Int (*tabulateReserved(Arena* a))[countCoreForms] {
    Int (*result)[countCoreForms] = allocateOnArena(countCoreForms*sizeof(int), a);
    int* p = *result;        
    p[tokIf - firstCoreFormTokenType] = 1;
    p[tokFnDef - firstCoreFormTokenType] = 1;
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
    p[ 3] = (OpDef){.name=s("$"),    .arity=1, .bytes={aDollar, 0, 0, 0 }, .overloadable=true};
    p[ 4] = (OpDef){.name=s("%"),    .arity=2, .bytes={aPercent, 0, 0, 0 } };
    p[ 5] = (OpDef){.name=s("&&"),   .arity=2, .bytes={aAmp, aAmp, 0, 0 }, .assignable=true};
    p[ 6] = (OpDef){.name=s("&"),    .arity=1, .bytes={aAmp, 0, 0, 0 } };
    p[ 7] = (OpDef){.name=s("'"),    .arity=1, .bytes={aApostrophe, 0, 0, 0 } };
    p[ 8] = (OpDef){.name=s("*."),   .arity=2, .bytes={aTimes, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[ 9] = (OpDef){.name=s("*"),    .arity=2, .bytes={aTimes, 0, 0, 0 }, .assignable = true, .overloadable = true};
    p[10] = (OpDef){.name=s("++"),   .arity=1, .bytes={aPlus, aPlus, 0, 0}, .overloadable=true };
    p[11] = (OpDef){.name=s("+."),   .arity=2, .bytes={aPlus, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[12] = (OpDef){.name=s("+"),    .arity=2, .bytes={aPlus, 0, 0, 0 }, .assignable = true, .overloadable = true};
    p[13] = (OpDef){.name=s(","),    .arity=1, .bytes={aComma, 0, 0, 0}};    
    p[14] = (OpDef){.name=s("--"),   .arity=1, .bytes={aMinus, aMinus, 0, 0}, .overloadable=true};
    p[15] = (OpDef){.name=s("-."),   .arity=2, .bytes={aMinus, aDot, 0, 0}, .assignable = true, .overloadable = true};    
    p[16] = (OpDef){.name=s("-"),    .arity=2, .bytes={aMinus, 0, 0, 0}, .assignable = true, .overloadable = true };
    p[17] = (OpDef){.name=s("/."),   .arity=2, .bytes={aDivBy, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[18] = (OpDef){.name=s("/"),    .arity=2, .bytes={aDivBy, 0, 0, 0}, .assignable = true, .overloadable = true};
    p[19] = (OpDef){.name=s("<<."),  .arity=2, .bytes={aLT, aLT, aDot, 0}, .assignable = true, .overloadable = true};
    p[20] = (OpDef){.name=s("<<"),   .arity=2, .bytes={aLT, aLT, 0, 0}, .assignable = true, .overloadable = true };    
    p[21] = (OpDef){.name=s("<="),   .arity=2, .bytes={aLT, aEqual, 0, 0}};    
    p[22] = (OpDef){.name=s("<>"),   .arity=2, .bytes={aLT, aGT, 0, 0}};    
    p[23] = (OpDef){.name=s("<"),    .arity=2, .bytes={aLT, 0, 0, 0 } };
    p[24] = (OpDef){.name=s("=="),   .arity=2, .bytes={aEqual, aEqual, 0, 0 } };
    p[25] = (OpDef){.name=s(">=<="), .arity=3, .bytes={aGT, aEqual, aLT, aEqual } };
    p[26] = (OpDef){.name=s(">=<"),  .arity=3, .bytes={aGT, aEqual, aLT, 0 } };
    p[27] = (OpDef){.name=s("><="),  .arity=3, .bytes={aGT, aLT, aEqual, 0 } };
    p[28] = (OpDef){.name=s("><"),   .arity=3, .bytes={aGT, aLT, 0, 0 } };
    p[29] = (OpDef){.name=s(">="),   .arity=2, .bytes={aGT, aEqual, 0, 0 } };
    p[30] = (OpDef){.name=s(">>."),  .arity=2, .bytes={aGT, aGT, aDot, 0}, .assignable = true, .overloadable = true};
    p[31] = (OpDef){.name=s(">>"),   .arity=2, .bytes={aGT, aGT, 0, 0}, .assignable = true, .overloadable = true };
    p[32] = (OpDef){.name=s(">"),    .arity=2, .bytes={aGT, 0, 0, 0 }};
    p[33] = (OpDef){.name=s("?:"),   .arity=2, .bytes={aQuestion, aColon, 0, 0 } };
    p[34] = (OpDef){.name=s("?"),    .arity=1, .bytes={aQuestion, 0, 0, 0 } };
    p[35] = (OpDef){.name=s("@"),    .arity=1, .bytes={aAt, 0, 0, 0 } };
    p[36] = (OpDef){.name=s("^."),   .arity=2, .bytes={aCaret, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[37] = (OpDef){.name=s("^"),    .arity=2, .bytes={aCaret, 0, 0, 0}, .assignable = true, .overloadable = true};
    p[38] = (OpDef){.name=s("||"),   .arity=2, .bytes={aPipe, aPipe, 0, 0}, .assignable=true, };
    p[39] = (OpDef){.name=s("|"),    .arity=2, .bytes={aPipe, 0, 0, 0}};
    p[40] = (OpDef){.name=s("and"),  .arity=2, .bytes={0, 0, 0, 0 }, .assignable=true};
    p[41] = (OpDef){.name=s("or"),   .arity=2, .bytes={0, 0, 0, 0 }, .assignable=true};
    p[42] = (OpDef){.name=s("neg"),  .arity=2, .bytes={0, 0, 0, 0 }};
    return result;
}


testable Lexer* createLexer(String* inp, LanguageDefinition* langDef, Arena* a) {
    Lexer* lx = allocateOnArena(sizeof(Lexer), a);
    Arena* aTmp = mkArena();
    (*lx) = (Lexer){
        .i = 0, .langDef = langDef, .inp = inp, .nextInd = 0, .inpLength = inp->length,
        .tokens = allocateOnArena(LEXER_INIT_SIZE*sizeof(Token), a), .capacity = LEXER_INIT_SIZE,
        .newlines = allocateOnArena(500*sizeof(int), a), .newlinesCapacity = 500,
        .numeric = allocateOnArena(50*sizeof(int), aTmp), .numericCapacity = 50,
        .backtrack = createStackBtToken(16, aTmp),
        .stringTable = createStackint32_t(16, a), .stringStore = createStringStore(100, a),
        .wasError = false, .errMsg = &empty,
        .arena = a, .aTmp = aTmp
    };
    return lx;
}

/**
 * Finalizes the lexing of a single input: checks for unclosed scopes, and closes semicolons and an open statement, if any.
 */
private void finalizeLexer(Lexer* lx) {
    if (!hasValues(lx->backtrack)) return;
    closeColons(lx);
    BtToken top = pop(lx->backtrack);
    VALIDATEL(top.spanLevel != slScope && !hasValues(lx->backtrack), errPunctuationExtraOpening)

    setStmtSpanLength(top.tokenInd, lx);    
    deleteArena(lx->aTmp);
}


testable Lexer* lexicallyAnalyze(String* input, LanguageDefinition* langDef, Arena* a) {
    Lexer* lx = createLexer(input, langDef, a);

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
    LexerFunc (*dispatch)[256] = langDef->dispatchTable;
    lx->possiblyReservedDispatch = langDef->possiblyReservedDispatch;

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

#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser(errMsg, cm); }

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

testable void addBinding(int nameId, int bindingId, Compiler* cm) {
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
testable void popScopeFrame(Compiler* cm) {
    ScopeStackFrame* topScope = cm->scopeStack->topScope;
    ScopeStack* scopeStack = cm->scopeStack;
    if (topScope->bindings) {
        for (int i = 0; i < topScope->length; i++) {
            Int bindingOrOverload = cm->activeBindings[*(topScope->bindings + i)];
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

#define BIG 70000000

_Noreturn private void throwExcParser(const char errMsg[], Compiler* cm) {   
    cm->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d\n", cm->i);
#endif    
    cm->errMsg = str(errMsg, cm->a);
    longjmp(excBuf, 1);
}


private Int parseLiteralOrIdentifier(Token tok, Compiler* cm);

/** Validates a new binding (that it is unique), creates an entity for it, and adds it to the current scope */
private Int createEntity(Int nameId, Compiler* cm) {
    Int mbBinding = cm->activeBindings[nameId];
    VALIDATEP(mbBinding < 0, errAssignmentShadowing) // if it's a binding, it should be -1, and if overload, <-1

    Int newEntityId = cm->entities.length;    
    pushInentities(((Entity){.nameId = nameId}), cm);
    
    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (cm->scopeStack->length > 0) {
            addBinding(nameId, newEntityId, cm); // adds it to the ScopeStack
        }
        cm->activeBindings[nameId] = newEntityId; // makes it active
    }
    
    return newEntityId;
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


private Int importEntity(Int nameId, Entity ent, Compiler* cm) {
    Int existingBinding = cm->activeBindings[nameId];
    Int typeLength = cm->types.content[ent.typeId];
    VALIDATEP(existingBinding == -1 || typeLength > 0, errAssignmentShadowing) // either the name unique, or it's a function

    Int newEntityId = cm->entities.length;
    pushInentities(ent, cm);

    if (typeLength <= 1) { // not a function, or a 0-arity function => not overloaded
        cm->activeBindings[nameId] = newEntityId;
    } else {
        fnDefIncrementOverlCount(nameId, cm);
    }
    return newEntityId;
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


/** Performs coordinated insertions to start a scope within the parser */
private void addParsedScope(Int sentinelToken, Int startBt, Int lenBts, Compiler* cm) {
    push(((ParseFrame){.tp = nodScope, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodScope, .startBt = startBt, .lenBts = lenBts}, cm);
    pushLexScope(cm->scopeStack);
}

private ParseFrame popFrame(Compiler* cm) {    
    ParseFrame frame = pop(cm->backtrack);
    if (frame.tp == nodScope) {
        popScopeFrame(cm);
    }
    setSpanLength(frame.startNodeInd, cm);
    return frame;
}

/** Calculates the sentinel token for a token at a specific index */
private Int calcSentinel(Token tok, Int tokInd) {
    return (tok.tp >= firstPunctuationTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}

/**
 * A single-item expression, like "foo". Consumes 1 token. Returns the type of the subexpression.
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
    } else VALIDATEP(tk.tp < firstCoreFormTokenType, errCoreFormInappropriate)
    else {
        throwExcParser(errUnexpectedToken, cm);
    }
    ++cm->i; // CONSUME the single item
    return typeId;
}

/** Counts the arity of the call, including skipping unary operators. Consumes no tokens. */
private void exprCountArity(Int* arity, Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
    Int j = cm->i;
    Token firstTok = tokens[j];

    j = calcSentinel(firstTok, j);
    while (j < sentinelToken) {
        Token tok = tokens[j];
        j += (tok.tp < firstPunctuationTokenType) ? 1 : (tok.pl2 + 1);
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

Int typeCheckResolveExpr(Int indExpr, Int sentinel, Compiler* cm);

/** General "big" expression parser. Parses an expression whether there is a token or not.
 *  Starts from cm->i and goes up to the sentinel token. Returns the expression's type
 * Precondition: we are looking 1 past the tokExpr, unlike "exprOrSingleItem".
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
        } else VALIDATEP(tokType < firstPunctuationTokenType, errExpressionCannotContain)
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
    //ParseFrame exprFrame = pop(cm->backtrack); // the same frame we've pushed earlier in this function
#if SAFETY
    //print("at i %d frame tp %d", cm->i, exprFrame.tp)
    //VALIDATEI(exprFrame.tp == nodExpr, iErrorInconsistentSpans)
#endif
    //setSpanLengthParser(exprFrame.startNodeInd, cm);
    
    Int exprType = typeCheckResolveExpr(startNodeInd, cm->nodes.length, cm);
    return exprType;
}

/**
 * Precondition: we are looking at the first token of expr, not 1 past it (unlike "exprUpTo"). Consumes 1 or more tokens.
 */
private Int exprOrSingleItem(Arr(Token) tokens, Compiler* cm) {
    Token tk = tokens[cm->i];
    if (tk.tp == tokStmt || tk.tp == tokParens) {
        ++cm->i; // CONSUME the "("
        exprUpTo(cm->i + tk.pl2, tk.startBt, tk.lenBts, tokens, cm);
    } else {
        return exprSingleItem(tokens[cm->i], cm);
    }
}

/**
 * Parses an expression from an actual token. Precondition: we are 1 past that token. Handles statements only, not single items.
 */
private void parseExpr(Token tok, Arr(Token) tokens, Compiler* cm) {
    if (tok.pl2 > 1) {
        exprUpTo(cm->i + tok.pl2, tok.startBt, tok.lenBts, tokens, cm);
    } else {
        exprSingleItem(tokens[cm->i], cm);
    }
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

    Int rightTypeId = -1;
    Int declaredTypeId = -1;
    if (tokens[cm->i].tp == tokTypeName) {
        declaredTypeId = cm->activeBindings[tokens[cm->i].pl1];
        VALIDATEP(declaredTypeId > -1, errUnknownType)
        ++cm->i; // CONSUME the type decl of the binding
        --rLen; // for the type decl token
    }
    Token rTk = tokens[cm->i];
    if (rLen == 1) {
        rightTypeId = parseLiteralOrIdentifier(rTk, cm);
        VALIDATEP(rightTypeId != -2, errAssignment)
    } else if (rTk.tp == tokIf) { // TODO
    } else {
        rightTypeId = exprUpTo(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    }
    VALIDATEP(declaredTypeId == -1 || rightTypeId == declaredTypeId, errTypeMismatch)
    if (rightTypeId > -1) {
        cm->entities.content[newBindingId].typeId = rightTypeId;
    }
}

private void parseReassignment(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int rLen = tok.pl2 - 1;
    VALIDATEP(rLen >= 1, errAssignment)
    
    Int sentinelToken = cm->i + tok.pl2;

    Token bindingTk = tokens[cm->i];
    Int bindingId = cm->activeBindings[bindingTk.pl2];
    VALIDATEP(bindingId > -1, errUnknownBinding)

    push(((ParseFrame){ .tp = nodReassign, .startNodeInd = cm->nodes.length, .sentinelToken = sentinelToken }), cm->backtrack);
    pushInnodes((Node){.tp = nodReassign, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    pushInnodes((Node){.tp = nodBinding, .pl1 = bindingId, .startBt = bindingTk.startBt, .lenBts = bindingTk.lenBts}, cm);
    
    cm->i++; // CONSUME the word token before the assignment sign

    Int typeId = cm->entities.content[bindingId].typeId;
    Int rightSideTypeId = -1;
    Token rTk = tokens[cm->i];
    if (rTk.tp == tokIf) { // TODO
    } else if (sentinelToken > cm->i + 1) {
        rightSideTypeId = exprUpTo(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    } else {
        rightSideTypeId = exprSingleItem(rTk, cm);
    }
    VALIDATEP(rightSideTypeId == typeId, errTypeMismatch)
    cm->entities.content[bindingId].isMutable = true;
}

bool findOverload(Int typeId, Int start, Int sentinel, Int* entityId, Compiler* cm);

/** Reassignments like "x += 5". Performs some AST twiddling:
 *  1. [0    0]
 *  2. [0    0      Expr     ...]
 *  3. [Expr OpCall LeftSide ...]
 *
 *  The end result is an expression that is the right side but wrapped in another binary call, with .pl2 increased by + 2
 */
private void parseMutation(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int opType = tok.pl1;
    Int sentinelToken = cm->i + tok.pl2;
    Token bindingTk = tokens[cm->i];
    
#ifdef SAFETY
    VALIDATEP(bindingTk.tp == tokWord, errMutation);
#endif
    Int leftEntityId = cm->activeBindings[bindingTk.pl2];
    VALIDATEP(leftEntityId > -1 && cm->entities.content[leftEntityId].typeId > -1, errUnknownBinding);
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

    Int operStartBt = bindingTk.startBt + bindingTk.lenBts;
    ++cm->i; // CONSUME the binding word
    Int operLenBts = tokens[cm->i].startBt - operStartBt;


    Int resInd = cm->nodes.length;
    // space for the operator and left side
    pushInnodes(((Node){}), cm);
    pushInnodes(((Node){}), cm);

    Token rTk = tokens[cm->i];
    Int rightType = exprUpTo(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    VALIDATEP(rightType == cm->types.content[opTypeInd + 3], errTypeNoMatchingOverload)

    Node rNd = cm->nodes.content[resInd + 2];
    if (rNd.tp == nodExpr) {
        Node exprNd = cm->nodes.content[resInd + 2];
        cm->nodes.content[resInd] = (Node){.tp = nodExpr, .pl2 = exprNd.pl2 + 2, .startBt = tok.startBt, .lenBts = tok.lenBts};
    } else {
        // right side is single-node, so there's no nodExpr, hence we need to push another node 
        pushInnodes((cm->nodes.content[cm->nodes.length - 1]), cm);
        cm->nodes.content[resInd] = (Node){.tp = nodExpr, .pl2 = 3, .startBt = tok.startBt, .lenBts = tok.lenBts};
    }
    cm->nodes.content[resInd + 1] = (Node){.tp = nodCall, .pl1 = operatorOv, .pl2 = 2,
                                           .startBt = operStartBt, .lenBts = operLenBts};
    cm->nodes.content[resInd + 2] = (Node){.tp = nodId, .pl1 = leftEntityId, .pl2 = bindingTk.pl2,
                                       .startBt = bindingTk.startBt, .lenBts = bindingTk.lenBts};
    cm->entities.content[leftEntityId].isMutable = true;
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


private Int breakContinueUnwind(Int unwindLevel, Compiler* cm) {
    Int level = unwindLevel;
    Int j = cm->backtrack->length;
    while (j > -1) {
        Int pfType = cm->backtrack->content[j].tp;
        if (pfType == nodWhile || pfType == nodEach) {
            --level;
            if (level == 0) {
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
        if (pfType == nodWhile || pfType == nodEach) {
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
    Int typeId;
    if (sentinelToken > cm->i + 1) {
        typeId = exprUpTo(sentinelToken, rTk.startBt, tok.lenBts - rTk.startBt + tok.startBt, tokens, cm);
    } else {
        typeId = exprSingleItem(rTk, cm);
    }
    
    VALIDATEP(typeId > -1, errReturn)
    if (typeId > -1) {
        typecheckFnReturn(typeId, cm);
    }
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

/** Precondition: we are 1 past the "stmt token*/
private void ifLeftSide(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int leftSentinel = calcSentinel(tok, cm->i - 1);
    VALIDATEP(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errIfLeft)

    VALIDATEP(leftSentinel + 1 < cm->inpLength, errPrematureEndOfTokens)
    VALIDATEP(tokens[leftSentinel].tp == tokArrow, errIfMalformed)

    Int typeLeft = exprUpTo(leftSentinel, tok.startBt, tok.lenBts, tokens, cm);
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
    if (tok->tp == tokArrow) {
        VALIDATEP(cm->i < cm->inpLength, errPrematureEndOfTokens)
        ++cm->i; // CONSUME the "else"
        *tok = tokens[cm->i];

        push(((ParseFrame){ .tp = nodIfClause, .startNodeInd = cm->nodes.length,
                            .sentinelToken = calcSentinel(*tok, cm->i)}), cm->backtrack);
        pushInnodes((Node){.tp = nodIfClause, .startBt = tok->startBt, .lenBts = tok->lenBts }, cm);
        cm->i++; // CONSUME the token after the "else"
    } else if (tok->tp == tokElse) {
        VALIDATEP(cm->i < cm->inpLength, errPrematureEndOfTokens)
        ++cm->i; // CONSUME the "=>"
        *tok = tokens[cm->i];

        push(((ParseFrame){ .tp = nodIfClause, .startNodeInd = cm->nodes.length,
                            .sentinelToken = calcSentinel(*tok, cm->i)}), cm->backtrack);
        pushInnodes((Node){.tp = nodIfClause, .startBt = tok->startBt, .lenBts = tok->lenBts }, cm);
        ++cm->i; // CONSUME the token after the "else"
    } else {
        ++cm->i; // CONSUME the stmt token
        ifLeftSide(*tok, tokens, cm);
        *tok = tokens[cm->i];
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
    cm->i = condInd + 1;
    
    Int condTypeId = exprUpTo(condSent, condTk.startBt, condTk.lenBts, tokens, cm);
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


private ParserFunc (*tabulateNonresumableDispatch(Arena* a))[countSyntaxForms] {
    ParserFunc (*result)[countSyntaxForms] = allocateOnArena(countSyntaxForms*sizeof(ParserFunc), a);
    ParserFunc* p = *result;
    int i = 0;
    while (i <= firstPunctuationTokenType) {
        p[i] = &parseErrorBareAtom;
        i++;
    }
    p[tokScope]      = &parseScope;
    p[tokStmt]       = &parseExpr;
    p[tokParens]     = &parseErrorBareAtom;
    p[tokAssignment] = &parseAssignment;
    p[tokReassign]   = &parseReassignment;
    p[tokMutation]   = &parseMutation;
    p[tokArrow]      = &parseSkip;
    
    p[tokAlias]      = &parseAlias;
    p[tokAssert]     = &parseAssert;
    p[tokAssertDbg]  = &parseAssertDbg;
    p[tokAwait]      = &parseAlias;
    p[tokBreak]      = &parseBreak;
    p[tokCatch]      = &parseAlias;
    p[tokContinue]   = &parseContinue;
    p[tokDefer]      = &parseAlias;
    p[tokEmbed]      = &parseAlias;
    p[tokExport]     = &parseAlias;
    p[tokExposePriv] = &parseAlias;
    p[tokFnDef]      = &parseAlias;
    p[tokIface]      = &parseAlias;
    p[tokLambda]     = &parseAlias;
    p[tokPackage]    = &parseAlias;
    p[tokReturn]     = &parseReturn;
    p[tokStruct]     = &parseAlias;
    p[tokTry]        = &parseAlias;
    p[tokYield]      = &parseAlias;

    p[tokIf]         = &parseIf;
    p[tokElse]       = &parseSkip;
    p[tokWhile]      = &parseWhile;
    return result;
}
 

private ResumeFunc (*tabulateResumableDispatch(Arena* a))[countResumableForms] {
    ResumeFunc (*result)[countResumableForms] = allocateOnArena(countResumableForms*sizeof(ResumeFunc), a);
    ResumeFunc* p = *result;
    int i = 0;
    p[nodIf    - firstResumableForm] = &resumeIf;
    p[nodIfPr  - firstResumableForm] = &resumeIfPr;
    p[nodImpl  - firstResumableForm] = &resumeImpl;
    p[nodMatch - firstResumableForm] = &resumeMatch;    
    return result;
}

/*
* Definition of the operators, reserved words, lexer dispatch and parser dispatch tables for the compiler.
*/
testable LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    result->possiblyReservedDispatch = tabulateReservedBytes(a);
    result->dispatchTable = tabulateDispatch(a);
    result->operators = tabulateOperators(a);
    result->reservedParensOrNot = tabulateReserved(a);
    result->resumableTable = tabulateResumableDispatch(a);    
    result->nonResumableTable = tabulateNonresumableDispatch(a);
    return result;
}


testable void importEntities(Arr(EntityImport) impts, Int countEntities, Arr(Int) typeIds, Compiler* cm) {
    for (int i = 0; i < countEntities; i++) {
        Int mbNameId = getStringStore(cm->text->content, impts[i].name, cm->stringTable, cm->stringStore);
        if (mbNameId > -1) {
            Int typeInd = typeIds[impts[i].typeInd];
            Int newEntityId = importEntity(mbNameId, (Entity){.nameId = mbNameId, .typeId = typeInd }, cm);
        }
    }
    cm->countNonparsedEntities = cm->entities.length;
}

/** Unique'ing of types. Precondition: the type is parked at the end of cm->types, forming its tail.
 *  Returns the resulting index of this type and updates the length of cm->types accordingly.
 */
testable Int mergeType(Int startInd, Int len, Compiler* cm) {
    byte* types = (byte*)cm->types.content;
    StringStore* hm = cm->typesDict;
    Int startBt = startInd*4;
    Int lenBts = len*4;
    Int hash = hashCode(types + startBt, lenBts) % (hm->dictSize);
    if (*(hm->dict + hash) == NULL) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (8 << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;        

        *firstElem = (StringValue){.length = lenBts, .indString = startBt };
        *(hm->dict + hash) = newBucket;
        return startInd;
    } else {
        Bucket* p = *(hm->dict + hash);
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
        addValueToBucket((hm->dict + hash), startBt, lenBts, hm->a);
        return startInd;
    }
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

private void buildOperator(Int operId, Int typeId, Compiler* cm) {
    pushInentities((Entity){.typeId = typeId, .nameId = operId}, cm);
    cm->overloadIds.content[operId] += SIXTEENPLUSONE;
}

/** Operators are the first-ever functions to be defined. This function builds their types, entities and overload counts. */
private void buildInOperators(Compiler* cm) {
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
    Int voidOfStr = addFunctionType(1, (Int[]){tokUnderscore, tokString}, cm);
    Int voidOfInt = addFunctionType(1, (Int[]){tokUnderscore, tokInt}, cm);
    
    buildOperator(opTNotEqual, boolOfIntInt, cm);
    buildOperator(opTNotEqual, boolOfFlFl, cm);
    buildOperator(opTNotEqual, boolOfStrStr, cm);
    buildOperator(opTBoolNegation, boolOfBool, cm);
    buildOperator(opTSize, intOfStr, cm);
    buildOperator(opTSize, intOfInt, cm);
    buildOperator(opTToString, strOfInt, cm);
    buildOperator(opTToString, strOfFloat, cm);
    buildOperator(opTToString, strOfBool, cm);
    buildOperator(opTRemainder, intOfIntInt, cm);
    buildOperator(opTBinaryAnd, boolOfBoolBool, cm); // dummy
    buildOperator(opTTypeAnd, boolOfBoolBool, cm); // dummy
    buildOperator(opTIsNull, boolOfBoolBool, cm); // dummy
    buildOperator(opTTimesExt, flOfFlFl, cm);
    buildOperator(opTTimes, intOfIntInt, cm);
    buildOperator(opTTimes, flOfFlFl, cm);
    buildOperator(opTIncrement, voidOfInt, cm);
    buildOperator(opTPlusExt, strOfStrStr, cm); // dummy
    buildOperator(opTPlus, intOfIntInt, cm);    
    buildOperator(opTPlus, flOfFlFl, cm);
    buildOperator(opTPlus, strOfStrStr, cm);
    buildOperator(opTToFloat, flOfInt, cm);
    buildOperator(opTDecrement, voidOfInt, cm);
    buildOperator(opTMinusExt, intOfIntInt, cm); // dummy
    buildOperator(opTMinus, intOfIntInt, cm);
    buildOperator(opTMinus, flOfFlFl, cm);
    buildOperator(opTDivByExt, intOfIntInt, cm); // dummy
    buildOperator(opTDivBy, intOfIntInt, cm);
    buildOperator(opTDivBy, flOfFlFl, cm);
    buildOperator(opTBitShiftLeftExt, intOfIntInt, cm);  // dummy
    buildOperator(opTBitShiftLeft, intOfFlFl, cm);  // dummy
    buildOperator(opTBitShiftRightExt, intOfIntInt, cm);  // dummy
    buildOperator(opTBitShiftRight, intOfFlFl, cm);  // dummy
       
    buildOperator(opTComparator, intOfIntInt, cm);
    buildOperator(opTComparator, intOfFlFl, cm);
    buildOperator(opTComparator, intOfStrStr, cm);
    buildOperator(opTLTEQ, boolOfIntInt, cm);
    buildOperator(opTLTEQ, boolOfFlFl, cm);
    buildOperator(opTLTEQ, boolOfStrStr, cm);
    buildOperator(opTGTEQ, boolOfIntInt, cm);
    buildOperator(opTGTEQ, boolOfFlFl, cm);
    buildOperator(opTGTEQ, boolOfStrStr, cm);
    buildOperator(opTLessThan, boolOfIntInt, cm);
    buildOperator(opTLessThan, boolOfFlFl, cm);
    buildOperator(opTLessThan, boolOfStrStr, cm);
    buildOperator(opTGreaterThan, boolOfIntInt, cm);
    buildOperator(opTGreaterThan, boolOfFlFl, cm);
    buildOperator(opTGreaterThan, boolOfStrStr, cm);
    buildOperator(opTEquality, boolOfIntInt, cm);

    buildOperator(opTIntervalBoth, intOfIntInt, cm); // dummy
    buildOperator(opTIntervalLeft, flOfFlFl, cm); // dummy
    buildOperator(opTIntervalRight, intOfInt, cm); // dummy
    buildOperator(opTIntervalExcl, flOfFl, cm); // dummy 
    buildOperator(opTNullCoalesce, intOfIntInt, cm); // dummy
    buildOperator(opTQuestionMark, flOfFlFl, cm); // dummy
    buildOperator(opTAccessor, intOfInt, cm); // dummy
    buildOperator(opTBoolOr, flOfFl, cm); // dummy
    buildOperator(opTXor, flOfFlFl, cm); // dummy
    buildOperator(opTAnd, boolOfBoolBool, cm);
    buildOperator(opTOr, boolOfBoolBool, cm);
    
    buildOperator(opTExponentExt, intOfIntInt, cm); // dummy
    buildOperator(opTExponent, intOfIntInt, cm);
    buildOperator(opTExponent, flOfFlFl, cm);
    buildOperator(opTNegation, intOfInt, cm);
    buildOperator(opTNegation, flOfFl, cm);

    cm->countOperatorEntities = cm->entities.length;
}

/* Entities and overloads for the built-in operators, types and functions. */
private void importBuiltins(LanguageDefinition* langDef, Compiler* cm) {
    Arr(String*) baseTypes = allocateOnArena(6*sizeof(String*), cm->aTmp);
    baseTypes[tokInt] = str("Int", cm->aTmp);
    baseTypes[tokLong] = str("Long", cm->aTmp);
    baseTypes[tokFloat] = str("Float", cm->aTmp);
    baseTypes[tokString] = str("String", cm->aTmp);
    baseTypes[tokBool] = str("Bool", cm->aTmp);
    baseTypes[tokUnderscore] = str("Void", cm->aTmp);
    for (int i = 0; i < 5; i++) {
        pushIntypes(0, cm);
        Int typeNameId = getStringStore(cm->text->content, baseTypes[i], cm->stringTable, cm->stringStore);
        if (typeNameId > -1) {
            cm->activeBindings[typeNameId] = i;
        }
    }
    buildInOperators(cm);

    cm->entImportedZero = cm->overloadIds.length;

    Int voidOfStr = addFunctionType(1, (Int[]){tokUnderscore, tokString}, cm);
    
    EntityImport builtins[3] =  {
        (EntityImport) { .name = str("math-pi", cm->a), .typeInd = 0 },
        (EntityImport) { .name = str("math-e", cm->a),  .typeInd = 0 },
        (EntityImport) { .name = str("print", cm->a),  .typeInd = 1 }
    };    

    importEntities(builtins, sizeof(builtins)/sizeof(EntityImport), ((Int[]){tokFloat, voidOfStr}), cm);
}
//}}}

testable Compiler* createCompiler(Lexer* lx, Arena* a) {
    if (lx->wasError) {
        Compiler* result = allocateOnArena(sizeof(Compiler), a);
        result->wasError = true;
        return result;
    }
    
    Compiler* result = allocateOnArena(sizeof(Compiler), a);
    Arena* aTmp = mkArena();
    Int stringTableLength = lx->stringTable->length;
    Int initNodeCap = lx->totalTokens > 64 ? lx->totalTokens : 64;
    (*result) = (Compiler) {
        .text = lx->inp, .inp = lx, .inpLength = lx->totalTokens, .langDef = lx->langDef,
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aTmp), .i = 0, .loopCounter = 0,
        
        .nodes = createInListNode(initNodeCap, a),
        
        .entities = createInListEntity(64, a),
        .overloadIds = createInListuint32_t(64, aTmp),
        .activeBindings = allocateOnArena(4*stringTableLength, a),

        .overloads = (InListInt){.length = 0, .content = NULL},
        .types = createInListInt(64, a), .typesDict = createStringStore(100, aTmp),
        .expStack = createStackint32_t(16, aTmp),
        
        .stringStore = lx->stringStore, .stringTable = lx->stringTable, .strLength = stringTableLength,
        .wasError = false, .errMsg = &empty, .a = a, .aTmp = aTmp
    };
    if (stringTableLength > 0) {
        memset(result->activeBindings, 0xFF, stringTableLength*sizeof(Int)); // activeBindings is filled with -1
    }
    importBuiltins(lx->langDef, result);
    return result;    
}


private Int getFirstParamType(Int funcTypeId, Compiler* cm);
private bool isFunctionWithParams(Int typeId, Compiler* cm);

/** Now that the overloads are freshly allocated, we need to write all the existing entities (operators and imported functions)
 * to the table. The table consists of variable-length entries of the form: [count typeId1 typeId2 entId1 entId2]
 */
private void populateOverloadsForOperatorsAndImports(Compiler* cm) {
    Int currOperId = -1;
    Int o = -1;
    Int countOverls = -1;
    Int sentinel = -1;

    // operators
    for (Int i = 0; i < cm->countOperatorEntities; i++) {
        Entity ent = cm->entities.content[i];
        
        if (ent.nameId != currOperId) {
            currOperId = ent.nameId;
            o = cm->overloadIds.content[currOperId] + 1;
            countOverls = (cm->overloadIds.content[currOperId + 1] - cm->overloadIds.content[currOperId] - 1)/2;
            sentinel = cm->overloadIds.content[currOperId + 1];
        }
        
        if (countOverls > 0) {
            cm->overloads.content[o] = getFirstParamType(ent.typeId, cm);
            cm->overloads.content[o + countOverls] = i;
        }
        
        o++;
    }
    // imported functions
    Int currFuncNameId = -1;
    Int currFuncId = -1;
    for (Int i = cm->countOperatorEntities; i < cm->countNonparsedEntities; i++) {
        Entity ent = cm->entities.content[i];
        if (!isFunctionWithParams(ent.typeId, cm)) {
            continue;
        }

        if (ent.nameId != currFuncNameId) {
            currFuncNameId = ent.nameId;
            currFuncId = cm->activeBindings[currFuncNameId];
#ifdef SAFETY
            VALIDATEI(currFuncId < -1, iErrorImportedFunctionNotInScope)
#endif            
            o = cm->overloadIds.content[-currFuncId - 2] + 1;
            countOverls = cm->overloads.content[o - 1];
            sentinel = o + countOverls;
        }

#ifdef SAFETY
        VALIDATEI(o < sentinel, iErrorOverloadsOverflow)
#endif
        cm->overloads.content[o] = getFirstParamType(ent.typeId, cm);
        cm->overloads.content[o + countOverls] = i;
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
    populateOverloadsForOperatorsAndImports(cm);
}

/** Parses top-level types but not functions and adds their bindings to the scope */
private void parseToplevelTypes(Lexer* lr, Compiler* cm) {
}

/** Parses top-level constants but not functions, and adds their bindings to the scope */
private void parseToplevelConstants(Lexer* lx, Compiler* cm) {
    cm->i = 0;
    const Int len = lx->totalTokens;
    while (cm->i < len) {
        Token tok = lx->tokens[cm->i];
        if (tok.tp == tokAssignment) {
            parseUpTo(cm->i + tok.pl2, lx->tokens, cm);
        } else {
            cm->i += (tok.pl2 + 1);
        }
    }    
}

/** Parses top-level function names, and adds their bindings to the scope */
private void surveyToplevelFunctionNames(Lexer* lx, Compiler* cm) {
    cm->i = 0;
    const Int len = lx->totalTokens;
    Token* tokens = lx->tokens;
    while (cm->i < len) {
        Token tok = tokens[cm->i];
        if (tok.tp == tokFnDef) {
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
    Int currConcreteOverloadCount = cm->overloads.content[overloadInd];
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
                print("p1")
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
                print("p2")
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
private void parseToplevelSignature(Token fnDef, StackNode* toplevelSignatures, Lexer* lx, Compiler* cm) {
    Int fnStartTokenId = cm->i - 2;    
    Int fnSentinelToken = fnStartTokenId + fnDef.pl2 + 1;
    Int byteSentinel = fnDef.startBt + fnDef.lenBts;
    
    Token fnName = lx->tokens[cm->i];
    Int fnNameId = fnName.pl2;
    Int activeBinding = cm->activeBindings[fnNameId];
    Int overloadId = activeBinding < -1 ? (-activeBinding - 2) : -1;
    Int fnEntityId = cm->entities.length;    
    pushInentities(((Entity){.nameId = fnNameId}), cm);
    
    cm->i++; // CONSUME the function name token
    
    Int tentativeTypeInd = cm->types.length;
    pushIntypes(0, cm); // will overwrite it with the type's length once we know it
    // the function's return type, interpreted to be Void if absent
    if (lx->tokens[cm->i].tp == tokTypeName) {
        Token fnReturnType = lx->tokens[cm->i];
        Int returnTypeId = cm->activeBindings[fnReturnType.pl2];
        VALIDATEP(returnTypeId > -1, errUnknownType)
        pushIntypes(returnTypeId, cm);
        
        cm->i++; // CONSUME the function return type token
    } else {
        pushIntypes(tokUnderscore, cm); // underscore stands for the Void type
    }
    VALIDATEP(lx->tokens[cm->i].tp == tokParens, errFnNameAndParams)

    Int paramsTokenInd = cm->i;
    Token parens = lx->tokens[paramsTokenInd];   
    Int paramsSentinel = cm->i + parens.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list            
    Int arity = 0;
    while (cm->i < paramsSentinel) {
        Token paramName = lx->tokens[cm->i];
        VALIDATEP(paramName.tp == tokWord, errFnNameAndParams)
        ++cm->i; // CONSUME a param name
        ++arity;
        
        VALIDATEP(cm->i < paramsSentinel, errFnNameAndParams)
        Token paramType = lx->tokens[cm->i];
        VALIDATEP(paramType.tp == tokTypeName, errFnNameAndParams)
        
        Int paramTypeId = cm->activeBindings[paramType.pl2]; // the binding of this parameter's type
        VALIDATEP(paramTypeId > -1, errUnknownType)
        pushIntypes(paramTypeId, cm);
        
        ++cm->i; // CONSUME the param's type name
    }

    VALIDATEP(cm->i < (fnSentinelToken - 1) && lx->tokens[cm->i].tp == tokEqualsSign, errFnMissingBody)
    cm->types.content[tentativeTypeInd] = arity + 1;
    
    Int uniqueTypeId = mergeType(tentativeTypeInd, arity + 2, cm);
    cm->entities.content[fnEntityId].typeId = uniqueTypeId;

    if (arity > 0) {
        addParsedOverload(overloadId, cm->types.content[uniqueTypeId + 2], fnEntityId, cm);
    }
    pushNode((Node){ .tp = nodFnDef, .pl1 = fnEntityId, .pl2 = paramsTokenInd,
         .startBt = fnStartTokenId, .lenBts = fnSentinelToken }, toplevelSignatures);
}

/** 
 * Parses a top-level function.
 * The result is the AST (: FnDef EntityName Scope EntityParam1 EntityParam2 ... )
 */
private void parseToplevelBody(Node toplevelSignature, Arr(Token) tokens, Compiler* cm) {
    Int fnSentinelToken = toplevelSignature.lenBts;
    cm->i = toplevelSignature.pl2; // paramsTokenInd from fn "parseToplevelSignature"
    Token fnDefToken = tokens[toplevelSignature.startBt];

    // the fnDef scope & node
    Int entityId = toplevelSignature.pl1;
    push(((ParseFrame){ .tp = nodFnDef, .startNodeInd = cm->nodes.length, .sentinelToken = fnSentinelToken,
                        .typeId = cm->entities.content[entityId].typeId}), cm->backtrack);
    pushInnodes(((Node){ .tp = nodFnDef, .pl1 = entityId, .startBt = fnDefToken.startBt, .lenBts = fnDefToken.lenBts }), cm);
    
    // the scope for the function's body
    addParsedScope(fnSentinelToken, tokens[cm->i].startBt, fnDefToken.lenBts - tokens[cm->i].startBt + fnDefToken.startBt, cm);

    Token parens = tokens[cm->i];
        
    Int paramsSentinel = cm->i + parens.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list            

    Int arity = 0;
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
    parseUpTo(fnSentinelToken, tokens, cm);
}

/** Parses top-level function params and bodies */
private void parseFunctionBodies(StackNode* toplevelSignatures, Lexer* lx, Compiler* cm) {
    for (int j = 0; j < toplevelSignatures->length; j++) {
        cm->loopCounter = 0;
        parseToplevelBody(toplevelSignatures->content[j], lx->tokens, cm);
    }
}

/** Walks the top-level functions' signatures (but not bodies).
 * Result: the complete overloads & overloadIds tables, and the list of toplevel functions to parse bodies for.
 */
private StackNode* parseToplevelSignatures(Lexer* lx, Compiler* cm) {
    StackNode* topLevelSignatures = createStackNode(16, cm->aTmp);
    cm->i = 0;
    const Int len = lx->totalTokens;
    while (cm->i < len) {
        Token tok = lx->tokens[cm->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.pl2;
            Int sentinelToken = cm->i + lenTokens + 1;
            VALIDATEP(lenTokens >= 2, errFnNameAndParams)
            
            cm->i += 2; // CONSUME the function def token and the stmt token
            parseToplevelSignature(tok, topLevelSignatures, lx, cm);
            cm->i = sentinelToken;
        } else {            
            cm->i += (tok.pl2 + 1);  // CONSUME the whole non-function span
        }
    }
    return topLevelSignatures;
}

testable Compiler* parseMain(Lexer* lx, Compiler* cm, Arena* a) {
    LanguageDefinition* pDef = cm->langDef;
    int inpLength = lx->totalTokens;
    int i = 0;
    if (setjmp(excBuf) == 0) {
        parseToplevelTypes(lx, cm);
        
        // This gives us overload counts
        surveyToplevelFunctionNames(lx, cm);

        // This gives us the semi-complete overloads & overloadIds tables (with only the built-ins and imports)
        createOverloads(cm);
        
        parseToplevelConstants(lx, cm);

        // This gives us the complete overloads & overloadIds tables, and the list of toplevel functions
        StackNode* topLevelSignatures = parseToplevelSignatures(lx, cm);

#ifdef SAFETY
        validateOverloadsFull(cm);
#endif

        // The main parse (all top-level function bodies)
        parseFunctionBodies(topLevelSignatures, lx, cm);
    }
    return cm;
}

/** Parses a single file in 4 passes, see docs/parser.txt */
private Compiler* parse(Lexer* lx, Arena* a) {
    Compiler* cm = createCompiler(lx, a);
    return parseMain(lx, cm, a);
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


#ifdef TEST
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
    Node expr = cm->nodes.content[indExpr];
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
            Int typeLength = cm->types.content[functionTypeInd];
#ifdef SAFETY
            VALIDATEI(typeLength == 1, iErrorOverloadsIncoherent)
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
struct Codegen {
    Int i; // current node index
    Int indentation;
    Int length;
    Int capacity;
    Arr(byte) buffer;

    StackNode backtrack; // these "nodes" are modified from what is in the AST. .startBt = sentinelNode
    CgFunc* cgTable[countSpanForms];
    String* sourceCode;
    Compiler* cm;
    Arena* a;
};


const char constantStrings[] = "returnifelsefunctionwhileconstletbreakcontinuetruefalseconsole.logfor";
const Int constantOffsets[] = {
    0,   6,  8, 12,
    20, 25, 30, 33,
    38, 46, 50, 55,
    66, 69
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

/** Ensures that the buffer has space for at least that many bytes plus 10 by increasing its capacity if necessary */
private void ensureBufferLength(Int additionalLength, Codegen* cg) {
    if (cg->length + additionalLength + 10 < cg->capacity) {
        return;
    }
    print("enlarging")
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
    ensureBufferLength(countBytes, cg);
    memcpy(cg->buffer + cg->length, ptr, 10);
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
    cg->buffer[cg->length] = aQuote;
    memcpy(cg->buffer + cg->length + 1, str->content, str->length);
    cg->length += (str->length + 2);
    cg->buffer[cg->length - 1] = aQuote;
}

/** Precondition: we are looking directly at the first node of the expression. Consumes all nodes of the expr */
private void writeExpr(Arr(Node) nodes, Codegen* cg) {
    Node nd = nodes[cg->i];
    if (nd.tp <= topVerbatimType) {
        writeBytes(cg->sourceCode->content + nd.startBt, nd.lenBts, cg);
        ++cg->i; // CONSUME the single node of the expression
    } else {
        writeChar(aParenLeft, cg);
        writeChar(aParenRight, cg);
    }
#if SAFETY
    VALIDATEI(nd.tp == nodExpr, iErrorExpressionIsNotAnExpr)
#endif
    
}

private void pushCodegenFrame(Node nd, Codegen* cg) {
    push(((Node){.tp = nd.tp, .pl2 = nd.pl2, .startBt = cg->i + nd.pl2}), &cg->backtrack);
}


private void writeIndentation(Codegen* cg) {
    ensureBufferLength(cg->indentation, cg);
    memset(cg->buffer + cg->length, aSpace, cg->indentation);
    cg->length += cg->indentation;
}

private void writeFn(Node nd, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (isEntry) {
        pushCodegenFrame(nd, cg);
        writeIndentation(cg);
        writeConstantWithSpace(strFunction, cg);
        Entity fnEnt = cg->cm->entities.content[nd.pl1];
        writeChar(aParenLeft, cg);
        Int sentinel = cg->i + nd.pl2;
        Int j = cg->i + 1; // +1 to skip the nodScope
        if (nodes[j].tp == nodBinding) {
            Node binding = nodes[j];
            writeBytes(cg->sourceCode->content + binding.startBt, binding.lenBts, cg);
            ++j;
        }
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
        writeIndentation(cg);
        writeChars(cg, ((byte[]){aCurlyRight, aNewline, aNewline}));
    }

}

private void writeDummy(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    
}
/**
 * 
 */
private void writeAssignment(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    writeIndentation(cg);
    
    Node binding = nodes[cg->i];
    writeBytes(cg->sourceCode->content + binding.startBt, binding.lenBts, cg);
    writeChars(cg, ((byte[]){aSpace, aEqual, aSpace}));
    
    Node rightSide = nodes[cg->i + 1];
    if (rightSide.tp == nodId) {
        writeBytes(cg->sourceCode->content + rightSide.startBt, rightSide.lenBts, cg);
    }

    writeChar(aSemicolon, cg);
    writeChar(aNewline, cg);
    cg->i += fr.pl2;
}

private void writeReturn(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    writeIndentation(cg);
    writeConstantWithSpace(strReturn, cg);
    writeChar(aSemicolon, cg);
    writeChar(aNewline, cg);
    cg->i += fr.pl2; // CONSUME the "return" node
}


private void writeScope(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (isEntry) {
        writeIndentation(cg);
        writeChar(aCurlyLeft, cg);
        cg->indentation += 4;
        pushCodegenFrame(fr, cg);
    } else {
        cg->indentation -= 4;
        writeChar(aCurlyRight, cg);
    }
}


private void writeIf(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (!isEntry) {
        return;
    }
    writeConstantWithSpace(strIf, cg);
    writeChar(aParenLeft, cg);
    writeExpr(nodes, cg);
    writeChar(aParenRight, cg);
    writeChar(32, cg);
    
    cg->indentation += 4;
    pushCodegenFrame(fr, cg);
}

private void writeIfClause(Node nd, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    Int sentinel = cg->i + nd.pl2;
    if (isEntry) {
        pushCodegenFrame(nd, cg);
        writeIndentation(cg);
        writeChar(aCurlyLeft, cg);
        cg->indentation += 4;
    } else {
        cg->indentation -= 4;
        writeIndentation(cg);
        writeChar(aCurlyRight, cg);
        Node top = peek(&cg->backtrack);
        Int ifSentinel = top.startBt + top.pl2;
        if (ifSentinel == sentinel) {
            return;
        } else if (nodes[sentinel].tp == nodIfClause) {
            // there is only the "else" clause after this clause
            writeChar(32, cg);
            writeConstantWithSpace(strElse, cg);
        } else {
            // there is another condition after this clause, so we write it out as an "else if"
            writeChar(32, cg);
            writeChar(aParenLeft, cg);
            cg->i = sentinel;
            writeExpr(nodes, cg);
            writeChar(aParenRight, cg);
            writeChar(32, cg);
        }

    }
}

private void writeWhile(Node fr, bool isEntry, Arr(Node) nodes, Codegen* cg) {
    if (isEntry) {
        writeIndentation(cg);
        writeConstantWithSpace(strFor, cg);
        writeChar(aParenLeft, cg);
        writeChar(aSemicolon, cg);
        writeChar(aSemicolon, cg);
        writeChar(aParenRight, cg);
        writeChar(aSpace, cg);
        writeChar(aCurlyLeft, cg);
        cg->indentation += 4;
    } else {
        cg->indentation -= 4;
        writeIndentation(cg);
        writeChar(aCurlyRight, cg);
    }
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
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
    cg->cgTable[nodScope      - nodScope] = &writeScope;
    cg->cgTable[nodWhile      - nodScope] = &writeWhile;
    cg->cgTable[nodIf         - nodScope] = &writeIf;
    cg->cgTable[nodFnDef      - nodScope] = &writeFn;
    cg->cgTable[nodReturn     - nodScope] = &writeReturn;
}


private Codegen* createCodegen(Compiler* cm, Arena* a) {
    Codegen* cg = allocateOnArena(sizeof(Codegen), a);
    (*cg) = (Codegen) {
        .i = 0, .backtrack = *createStackNode(16, a),
        .length = 0, .capacity = 64, .buffer = allocateOnArena(64, a),
        .sourceCode = cm->text, .cm = cm, .a = a
    };
    tabulateCgDispatch(cg);
    return cg;
}

private Codegen* generateCode(Compiler* cm, Arena* a) {
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

Codegen* compile() {
    Arena* a = mkArena();
    LanguageDefinition* langDef = buildLanguageDefinitions(a);
    String* inp = str(
              "(:f foo Int(x Int y Float) =\n"
              "    a = x\n"
              "    return bar y a)\n"
              "(:f bar Int(y Float c Int) =\n"
              "    return foo c y)", a);
    Lexer* lx = lexicallyAnalyze(inp, langDef, a);
    if (lx->wasError) {
        print("lexer error");
    }
    Compiler* cm = parse(lx, a);
    if (cm->wasError) {
        print("parser error");
    }
    Codegen* cg = generateCode(cm, a);
    return cg;
}

Int main(int argc, char* argv) {
    Codegen* cg = compile();
    if (cg != NULL) {
        print(";---------- len %d\n", cg->length)
        fwrite(cg->buffer, 1, cg->length, stdout);
    }
    return 0;
}
