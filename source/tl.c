#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <setjmp.h>

//{{{ Utils

#define Int int32_t
#define StackInt Stackint32_t
#define private static
#define byte unsigned char
#define Arr(T) T*
#define untt uint32_t

#define LOWER24BITS 0x00FFFFFF
#define LOWER26BITS 0x03FFFFFF
#define LOWER16BITS 0x0000FFFF
#define LOWER32BITS 0x00000000FFFFFFFF
#define THIRTYFIRSTBIT 0x40000000
#define MAXTOKENLEN = 67108864 // 2**26
#define print(...) \
  printf(__VA_ARGS__);\
  printf("\n");


#ifdef DEBUG
#define DBG(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)
#else
#define DBG(fmt, ...) // empty
#endif      

#define s(lit) str(lit, a)

jmp_buf excBuf;

//}}}

//{{{ Arena

#define CHUNK_QUANT 32768

typedef struct ArenaChunk ArenaChunk;

struct ArenaChunk {
    size_t size;
    ArenaChunk* next;
    char memory[]; // flexible array member
};

typedef struct {
    ArenaChunk* firstChunk;
    ArenaChunk* currChunk;
    int currInd;
} Arena;


private size_t minChunkSize() {
    return (size_t)(CHUNK_QUANT - 32);
}

Arena* mkArena() {
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
void* allocateOnArena(size_t allocSize, Arena* ar) {
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


void deleteArena(Arena* ar) {
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

typedef struct {
    int length;
    byte content[];
} String;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define aALower       97
#define aFLower      102
#define aILower      105
#define aLLower      108
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
//}}}



//{{{ Stack
#define DEFINE_STACK_HEADER(T)                                  \
    typedef struct {                                            \
        Int capacity;                                           \
        Int length;                                             \
        Arena* arena;                                           \
        T (* content)[];                                        \
    } Stack##T;                                                 \
    Stack ## T * createStack ## T (Int initCapacity, Arena* ar);\
    bool hasValues ## T (Stack ## T * st);                      \
    T pop ## T (Stack ## T * st);                               \
    T peek ## T(Stack ## T * st);                               \
    void push ## T (T newItem, Stack ## T * st);                \
    void clear ## T (Stack ## T * st);
    
#define DEFINE_STACK(T)                                                                  \
    Stack##T * createStack##T (int initCapacity, Arena* a) {                             \
        int capacity = initCapacity < 4 ? 4 : initCapacity;                              \
        Stack##T * result = allocateOnArena(sizeof(Stack##T), a);                        \
        result->capacity = capacity;                                                     \
        result->length = 0;                                                              \
        result->arena = a;                                                               \
        T (* arr)[] = allocateOnArena(capacity*sizeof(T), a);                            \
        result->content = arr;                                                           \
        return result;                                                                   \
    }                                                                                    \
    bool hasValues ## T (Stack ## T * st) {                                              \
        return st->length > 0;                                                           \
    }                                                                                    \
    T pop##T (Stack ## T * st) {                                                         \
        st->length--;                                                                    \
        return (*st->content)[st->length];                                               \
    }                                                                                    \
    T peek##T(Stack##T * st) {                                                           \
        return (*st->content)[st->length - 1];                                           \
    }                                                                                    \
    void push##T (T newItem, Stack ## T * st) {                                          \
        if (st->length < st->capacity) {                                                 \
            memcpy((T*)(st->content) + (st->length), &newItem, sizeof(T));               \
        } else {                                                                         \
            T (* newContent)[] = allocateOnArena(2*(st->capacity)*sizeof(T), st->arena); \
            memcpy(newContent, st->content, st->length*sizeof(T));                       \
            memcpy((T*)(newContent) + (st->length), &newItem, sizeof(T));                \
            st->capacity *= 2;                                                           \
            st->content = (T(*)[])newContent;                                            \
        }                                                                                \
        st->length++;                                                                    \
    }                                                                                    \
    void clear##T (Stack##T * st) {                                                      \
        st->length = 0;                                                                  \
    }
    
DEFINE_STACK_HEADER(int32_t)
DEFINE_STACK(int32_t)
typedef struct Compiler Compiler;
private void handleFullChunkParser(Compiler* pr);
private void setSpanLengthParser(Int, Compiler* pr);
#define handleFullChunk(X) _Generic((X), Lexer*: handleFullChunkLexer, Compiler*: handleFullChunkParser)(X)
#define setSpanLength(A, X) _Generic((X), Lexer*: setSpanLengthLexer, Compiler*: setSpanLengthParser)(A, X)
//}}}


//{{{ Int Hashmap

typedef struct {
    Arr(int*) dict;
    int dictSize;
    int length;    
    Arena* a;
} IntMap;

IntMap* createIntMap(int initSize, Arena* a) {
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


void addIntMap(int key, int value, IntMap* hm) {
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
            memcpy(newBucket + 1, p + 1, capacity*2);
            newBucket[0] = ((2*capacity) << 16) + capacity;
            newBucket[2*capacity + 1] = key;
            newBucket[2*capacity + 2] = value;
            *(hm->dict + hash) = newBucket;
        }
    }
}


bool hasKeyIntMap(int key, IntMap* hm) {
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


int getIntMap(int key, int* value, IntMap* hm) {
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
int getUnsafeIntMap(int key, IntMap* hm) {
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


bool hasKeyValueIntMap(int key, int value, IntMap* hm) {
    return false;
}

//}}}

//{{{ String Hashmap

/** Reference to first occurrence of a string identifier within input text */
typedef struct {
    Int length;
    Int indString;
} StringValue;


typedef struct {
    untt capAndLen;
    StringValue content[];
} Bucket;

/** Hash map of all words/identifiers encountered in a source module */
typedef struct {
    Arr(Bucket*) dict;
    int dictSize;
    int length;    
    Arena* a;
} StringStore;


/** Backtrack token, used during lexing to keep track of all the nested stuff */
typedef struct {
    untt tp : 6;
    Int tokenInd;
    Int countClauses;
    untt spanLevel : 3;
    bool wasOrigColon;
} BtToken;


DEFINE_STACK_HEADER(BtToken)
DEFINE_STACK(BtToken)


#define initBucketSize 8

#define pop(X) _Generic((X), \
    StackBtToken*: popBtToken \
    )(X)

#define peek(X) _Generic((X), \
    StackBtToken*: peekBtToken \
    )(X)

#define push(A, X) _Generic((X), \
    StackBtToken*: pushBtToken, \
    Stackint32_t*: pushint32_t \
    )(A, X)
    
#define hasValues(X) _Generic((X), \
    StackBtToken*: hasValuesBtToken \
    )(X)   


StringStore* createStringStore(int initSize, Arena* a) {
    StringStore* result = allocateOnArena(sizeof(StringStore), a);
    int realInitSize = (initSize >= initBucketSize && initSize < 2048) ? initSize : (initSize >= initBucketSize ? 2048 : initBucketSize);
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
    for (int i = 0; i < len; i++) {        
        result = ((result << 5) + result) + p[i]; /* hash*33 + c */
    }

    return result;
}


private bool equalStringRefs(byte* text, Int a, Int b, Int len) {
    return memcmp(text + a, text + b, len) == 0;
}


private void addValueToBucket(Bucket** ptrToBucket, Int startBt, Int lenBts, Int newIndString, Arena* a) {    
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


Int addStringStore(byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable, StringStore* hm) {
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
                && equalStringRefs(text, (*stringTable->content)[stringValues[i].indString], startBt, lenBts)) { // key already present                
                return stringValues[i].indString;
            }
        }
        
        push(startBt, stringTable);
        newIndString = stringTable->length;
        addValueToBucket((hm->dict + hash), startBt, lenBts, newIndString, hm->a);
    }
    return newIndString;
}

/** Returns the index of a string within the string table, or -1 if it's not present */
Int getStringStore(byte* text, String* strToSearch, Stackint32_t* stringTable, StringStore* hm) {
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
                && memcmp(strToSearch->content, text + (*stringTable->content)[stringValues[i].indString], lenBts) == 0) {
                return stringValues[i].indString;
            }
        }
        
        return -1;
    }
}

//}}}

//{{{ Lexer

//{{{ LexerConstants

#define LEXER_INIT_SIZE 2000

/**
 * Regular (leaf) Token types
 */
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt          0
#define tokLong         1
#define tokFloat        2
#define tokBool         3      // pl2 = value (1 or 0)
#define tokString       4
#define tokUnderscore   5
#define tokDocComment   6

// This group requires analysis in the parser
#define tokWord         7      // pl2 = index in the string table
#define tokTypeName     8
#define tokDotWord      9      // ".fieldName", pl's the same as tokWord
#define tokOperator    10      // pl1 = OperatorToken, one of the "opT" constants below
#define tokDispose     11

// This is a temporary Token type for use during lexing only. In the final token stream it's replaced with tokParens
#define tokColon       12 

// Punctuation (inner node) Token types
#define tokScope       13       // denoted by (.)
#define tokStmt        14
#define tokParens      15
#define tokTypeParens  16
#define tokData        17       // data initializer, like (: 1 2 3)
#define tokAssignment  18       // pl1 = 1 if mutable assignment, 0 if immutable 
#define tokReassign    19       // :=
#define tokMutation    20       // pl1 = like in topOperator. This is the "+=", operatorful type of mutations
#define tokArrow       21       // not a real scope, but placed here so the parser can dispatch on it
#define tokElse        22       // not a real scope, but placed here so the parser can dispatch on it
 
// Single-shot core syntax forms. pl1 = spanLevel
#define tokAlias       23
#define tokAssert      24
#define tokAssertDbg   25
#define tokAwait       26
#define tokBreak       27
#define tokCatch       28       // paren "(catch e. e .print)"
#define tokContinue    29
#define tokDefer       30
#define tokEach        31
#define tokEmbed       32       // Embed a text file as a string literal, or a binary resource file // 200
#define tokExport      33
#define tokExposePriv  34
#define tokFnDef       35
#define tokIface       36
#define tokLambda      37
#define tokMeta        38
#define tokPackage     39       // for single-file packages
#define tokReturn      40
#define tokStruct      41
#define tokTry         42       // early exit
#define tokYield       43

// Resumable core forms
#define tokIf          44    // "(if " or "(-i". pl1 = 1 if it's the "(-" variant
#define tokIfPr        45    // like if, but every branch is a value compared using custom predicate
#define tokMatch       46    // "(-m " or "(match " pattern matching on sum type tag 
#define tokImpl        47    // "(-impl " 
#define tokLoop        48    // "(-loop "
// "(-iface"
#define topVerbatimTokenVariant tokUnderscore

/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokScope
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokAlias

#define countCoreForms (tokLoop - tokAlias + 1)


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

/** Span levels */
#define slScope         1 // scopes (denoted by brackets): newlines and commas just have no effect in them
#define slParenMulti    2 // things like "(if)": they're multiline but they cannot contain any brackets
#define slStmt          3 // single-line statements: newlines and commas break 'em
#define slSubexpr       4 // parens and the like: newlines have no effect, dots error out

/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array in the lexer.c file.
 * The order is defined by ASCII.
 */

/** Count of lexical operators, i.e. things that are lexed as operator tokens.
 * must be equal to the count of following constants
 */ 
#define countLexOperators   40
#define countOperators      43 // count of things that are stored as operators, regardless of how they are lexed
#define opTNotEqual          0 // !=
#define opTBoolNegation      1 // !
#define opTSize              2 // #
#define opTToString          3 // $
#define opTRemainder         4 // %
#define opTBinaryAnd         5 // && bitwise and
#define opTTypeAnd           6 // & interface intersection (type-level)
#define opTIsNull            7 // '
#define opTTimesExt          8 // *.
#define opTTimes             9 // *
#define opTIncrement        10 // ++
#define opTPlusExt          11 // +.
#define opTPlus             12 // +
#define opTToFloat          13 // ,
#define opTDecrement        14 // --
#define opTMinusExt         15 // -.
#define opTMinus            16 // -
#define opTDivByExt         17 // /.
#define opTDivBy            18 // /
#define opTBitShiftLeftExt  19 // <<.
#define opTBitShiftLeft     20 // <<
#define opTLTEQ             21 // <=
#define opTComparator       22 // <>
#define opTLessThan         23 // <
#define opTEquality         24 // ==
#define opTIntervalBoth     25 // >=<= inclusive interval check
#define opTIntervalLeft     26 // >=<  left-inclusive interval check
#define opTIntervalRight    27 // ><=  right-inclusive interval check
#define opTIntervalExcl     28 // ><   exclusive interval check
#define opTGTEQ             29 // >=
#define opTBitShiftRightExt 30 // >>.  unsigned right bit shift
#define opTBitshiftRight    31 // >>   right bit shift
#define opTGreaterThan      32 // >
#define opTNullCoalesce     33 // ?:   null coalescing operator
#define opTQuestionMark     34 // ?    nullable type operator
#define opTAccessor         35 // @
#define opTExponentExt      36 // ^.   exponentiation extended
#define opTExponent         37 // ^    exponentiation
#define opTBoolOr           38 // ||   bitwise or
#define opTXor              39 // |    bitwise xor
#define opTAnd              40
#define opTOr               41
#define opTNegation         42

/** Reserved words of Tl in ASCII byte form */
#define countReservedLetters         25 // length of the interval of letters that may be init for reserved words (A to Y)
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
static const byte reservedBytesLoop[]        = { 108, 111, 111, 112 };
static const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
static const byte reservedBytesMut[]         = { 109, 117, 116 }; // replace with "immut" or "val" for struct fields
static const byte reservedBytesOr[]          = { 111, 114 };
static const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
static const byte reservedBytesStruct[]      = { 115, 116, 114, 117, 99, 116 };
static const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
static const byte reservedBytesTry[]         = { 116, 114, 121 };
static const byte reservedBytesYield[]       = { 121, 105, 101, 108, 100 };


const char errorNonAscii[]                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!";
const char errorPrematureEndOfInput[]        = "Premature end of input";
const char errorUnrecognizedByte[]           = "Unrecognized byte in source code!";
const char errorWordChunkStart[]             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!";
const char errorWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!";
const char errorWordUnderscoresOnlyAtStart[] = "Underscores are only allowed at start of word (snake case is forbidden)!";
const char errorWordWrongAccessor[]          = "Only regular identifier words may be used for data access with []!";
const char errorWordReservedWithDot[]        = "Reserved words may not be called like functions!";
const char errorNumericEndUnderscore[]       = "Numeric literal cannot end with underscore!";
const char errorNumericWidthExceeded[]       = "Numeric literal width is exceeded!";
const char errorNumericBinWidthExceeded[]    = "Integer literals cannot exceed 64 bit!";
const char errorNumericFloatWidthExceeded[]  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!";
const char errorNumericEmpty[]               = "Could not lex a numeric literal, empty sequence!";
const char errorNumericMultipleDots[]        = "Multiple dots in numeric literals are not allowed!";
const char errorNumericIntWidthExceeded[]    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!";
const char errorPunctuationExtraOpening[]    = "Extra opening punctuation";
const char errorPunctuationExtraClosing[]    = "Extra closing punctuation";
const char errorPunctuationOnlyInMultiline[] = "The dot separator is only allowed in multi-line syntax forms like []";
const char errorPunctuationUnmatched[]       = "Unmatched closing punctuation";
const char errorPunctuationWrongOpen[]       = "Wrong opening punctuation";
const char errorPunctuationScope[]           = "Scopes may only be opened in multi-line syntax forms";
const char errorOperatorUnknown[]            = "Unknown operator";
const char errorOperatorAssignmentPunct[]    = "Incorrect assignment operator: must be directly inside an ordinary statement, after the binding name!";
const char errorOperatorTypeDeclPunct[]      = "Incorrect type declaration operator placement: must be the first in a statement!";
const char errorOperatorMultipleAssignment[] = "Multiple assignment / type declaration operators within one statement are not allowed!";
const char errorOperatorMutableDef[]         = "Definition of a mutable var should look like this: `mut x = 10`";
const char errorCoreNotInsideStmt[]          = "Core form must be directly inside statement";
const char errorCoreMisplacedArrow[]         = "The arrow separator (=>) must be inside an if, ifEq, ifPr or match form";
const char errorCoreMisplacedElse[]          = "The else statement must be inside an if, ifEq, ifPr or match form";
const char errorCoreMissingParen[]           = "Core form requires opening parenthesis/curly brace before keyword!"; 
const char errorCoreNotAtSpanStart[]         = "Reserved word must be at the start of a parenthesized span";
const char errorIndentation[]                = "Indentation error: must be divisible by 4 (tabs also count as 4) and not greater than the current indentation level!";
const char errorDocComment[]                 = "Doc comments must have the syntax: (*comment)";


/** All the symbols an operator may start with. "-" is absent because it's handled by lexMinus, "=" is handled by lexEqual
 */
const int operatorStartSymbols[] = {
    aExclamation, aSharp, aDollar, aPercent, aAmp, aApostrophe, aTimes, aPlus, aComma, aDivBy, 
    aLT, aGT, aQuestion, aAt, aCaret, aPipe
};

//}}}


#define CURR_BT inp[lx->i]
#define NEXT_BT inp[lx->i + 1]
#define VALIDATEL(cond, errMsg) if (!(cond)) { throwExcLexer(errMsg, lx); }
    
 
typedef struct {
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;
} Token;


/**
 * There is a closed set of operators in the language.
 *
 * For added flexibility, some operators may be extended into one more planes,
 * for example '+' may be extended into '+.', while '/' may be extended into '/.'.
 * These extended operators are declared by the language, and may be defined
 * for any type by the user, with the return type being arbitrary.
 * For example, the type of 3D vectors may have two different multiplication
 * operators: *. for vector product and * for scalar product.
 *
 * Plus, many have automatic assignment counterparts.
 * For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
 */
typedef struct {
    String* name;
    byte bytes[4];
    Int arity;
    /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
    bool overloadable;
    bool assignable;
    Int overs; // count of built-in overloads for this operator
} OpDef;


typedef struct LanguageDefinition LanguageDefinition;
typedef struct Lexer Lexer;
typedef void (*LexerFunc)(Lexer*, Arr(byte)); // LexerFunc = &(Lexer* => void)
typedef Int (*ReservedProbe)(int, int, struct Lexer*);


struct Lexer {
    Int i;                     // index in the input text
    String* inp;
    Int inpLength;
    Int totalTokens;
    Int lastClosingPunctInd;   // the index of the last encountered closing punctuation sign, used for statement length
    Int lastLineInitToken;     // index of the last token that was initial in a line of text
    
    LanguageDefinition* langDef;
    
    Arr(Token) tokens;
    Int capacity;              // current capacity of token storage
    Int nextInd;               // the  index for the next token to be added    
    
    Arr(int) newlines;
    Int newlinesCapacity;
    Int newlinesNextInd;
    
    Arr(byte) numeric;          // [aTmp]
    Int numericCapacity;
    Int numericNextInd;

    StackBtToken* backtrack;    // [aTmp]
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    
    Stackint32_t* stringTable;   // The table of unique strings from code. Contains only the startByte of each string.
    StringStore* stringStore;    // A hash table for quickly deduplicating strings. Points into stringTable    
    
    bool wasError;
    String* errMsg;

    Arena* arena;
    Arena* aTmp;
};


struct LanguageDefinition {
    OpDef (*operators)[countOperators];
    LexerFunc (*dispatchTable)[256];
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    Int (*reservedParensOrNot)[countCoreForms];
};


Int getStringStore(byte* text, String* strToSearch, Stackint32_t* stringTable, StringStore* hm);

Lexer* createLexer(String* inp, LanguageDefinition* langDef, Arena* ar);
void add(Token t, Lexer* lexer);

LanguageDefinition* buildLanguageDefinitions(Arena* a);
Lexer* lexicallyAnalyze(String*, LanguageDefinition*, Arena*);


typedef union {
    uint64_t i;
    double   d;
} FloatingBits;

int64_t longOfDoubleBits(double);

void printLexer(Lexer*);
int equalityLexer(Lexer a, Lexer b);




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
    VALIDATEL(lx->i + requiredSymbols <= lx->inpLength, errorPrematureEndOfInput)
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunkLexer(Lexer* lexer) {
    Token* newStorage = allocateOnArena(lexer->capacity*2*sizeof(Token), lexer->arena);
    memcpy(newStorage, lexer->tokens, (lexer->capacity)*(sizeof(Token)));
    lexer->tokens = newStorage;

    lexer->capacity *= 2;
}


void add(Token t, Lexer* lexer) {
    lexer->tokens[lexer->nextInd] = t;
    lexer->nextInd++;
    if (lexer->nextInd == lexer->capacity) {
        handleFullChunk(lexer);
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

    // This is for correctly calculating lengths of statements when they are ended by parens or brackets
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
    PROBERESERVED(reservedBytesLoop, tokLoop)
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
        Arr(int) newNewlines = allocateOnArena(lx->newlinesCapacity*2*sizeof(int), lx->arena);
        memcpy(newNewlines, lx->newlines, lx->newlinesCapacity);
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
    VALIDATEL(hasValues(bt), errorPunctuationExtraClosing)
    BtToken top = pop(bt);
    // since a closing bracket might be closing something with statements inside it, like a lex scope
    // or a core syntax form, we need to close the last statement before closing its parent
    if (bt->length > 0 && top.spanLevel != slScope 
          && ((*bt->content)[bt->length - 1].spanLevel <= slParenMulti)) {

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
        memcpy(newNumeric, lx->numeric, lx->numericCapacity);
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
            throwExcLexer(errorNumericEndUnderscore, lx);            
        } else {
            break;
        }
        VALIDATEL(lx->numericNextInd <= 16, errorNumericBinWidthExceeded)
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

int64_t longOfDoubleBits(double d) {
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
            VALIDATEL(j != (lx->inpLength - 1) && isDigit(inp[j + 1]), errorNumericEndUnderscore)
        } else if (cByte == aDot) {
            if (j + 1 < maximumInd && !isDigit(inp[j + 1])) { // this dot is not decimal - it's a statement closer
                break;
            }
            
            VALIDATEL(!metDot, errorNumericMultipleDots)
            metDot = true;
        } else {
            break;
        }
        j++;
    }

    VALIDATEL(j >= lx->inpLength || !isDigit(inp[j]), errorNumericWidthExceeded)

    if (metDot) {
        double resultValue = 0;
        Int errorCode = calcFloating(&resultValue, -digitsAfterDot, lx, inp);
        VALIDATEL(errorCode == 0, errorNumericFloatWidthExceeded)

        int64_t bitsOfFloat = longOfDoubleBits((isNegative) ? (-resultValue) : resultValue);
        add((Token){ .tp = tokFloat, .pl1 = (bitsOfFloat >> 32), .pl2 = (bitsOfFloat & LOWER32BITS),
                    .startBt = lx->i, .lenBts = j - lx->i}, lx);
    } else {
        int64_t resultValue = 0;
        Int errorCode = calcInteger(&resultValue, lx);
        VALIDATEL(errorCode == 0, errorNumericIntWidthExceeded)

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
        VALIDATEL(!hasValues(bt) || peek(bt).spanLevel == slScope, errorCoreNotInsideStmt)
        addStatement(reservedWordType, startBt, lx);
    } else if (expectations == 1) { // the "(core" case
        VALIDATEL(hasValues(bt), errorCoreMissingParen)
        
        BtToken top = peek(bt);
        VALIDATEL(top.tokenInd == lx->nextInd - 1, errorCoreNotAtSpanStart) // if this isn't the first token inside the parens
        
        if (bt->length > 1 && (*bt->content)[bt->length - 2].tp == tokStmt) {
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
        (*bt->content)[bt->length - 1].tp = reservedWordType;
        (*bt->content)[bt->length - 1].spanLevel = top.tp == tokScope ? slScope : slParenMulti;
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
    } else VALIDATEL(isLowercaseLetter(CURR_BT), errorWordChunkStart)
    lx->i++; // CONSUME the first letter of the word

    while (lx->i < lx->inpLength && isAlphanumeric(CURR_BT)) {
        lx->i++; // CONSUME alphanumeric characters
    }
    VALIDATEL(lx->i >= lx->inpLength || CURR_BT != aUnderscore, errorWordUnderscoresOnlyAtStart)
    return result;
}

/** Closes the current statement. Consumes no tokens */
private void closeStatement(Lexer* lx) {
    BtToken top = peek(lx->backtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errorPunctuationOnlyInMultiline)
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
                VALIDATEL(!wasCapitalized, errorWordCapitalizationOrder)
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

    VALIDATEL(wordType != tokDotWord, errorWordReservedWithDot)

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
        } else if (mbReservedWord == tokDispose) {
            wrapInAStatementStarting(startBt, lx, inp);
            add((Token){.tp=tokDispose, .pl2=0, .startBt=realStartByte, .lenBts=7}, lx);
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
        throwExcLexer(errorOperatorMultipleAssignment, lx);
    } else if (currSpan.spanLevel != slStmt) {
        throwExcLexer(errorOperatorAssignmentPunct, lx);
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
    (*lx->backtrack->content)[lx->backtrack->length - 1] = (BtToken){
        .tp = tok->tp, .spanLevel = slStmt, .tokenInd = tokenInd }; 
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
    VALIDATEL(opType > -1, errorOperatorUnknown)
    
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


private void lexEqual(Lexer* lx, Arr(byte) inp) {
    checkPrematureEnd(2, lx);
    byte nextBt = NEXT_BT;
    if (nextBt == aEqual) {
        lexOperator(lx, inp); // ==        
    } else if (nextBt == aGT) { // => is a statement terminator inside if-like scopes        
        // arrows are only allowed inside "if"s and the like
        VALIDATEL(lx->backtrack->length >= 2, errorCoreMisplacedArrow)
        
        BtToken grandparent = (*lx->backtrack->content)[lx->backtrack->length - 2];
        VALIDATEL(grandparent.tp == tokIf, errorCoreMisplacedArrow)
        closeStatement(lx);
        add((Token){ .tp = tokArrow, .startBt = lx->i, .lenBts = 2 }, lx);
        lx->i += 2;  // CONSUME the arrow "=>"
    } else {
        processAssignment(0, 0, lx);
        lx->i++; // CONSUME the =
    }
}

/**
 *  Doc comments, syntax is (* Doc comment ).
 *  Precondition: we are past the opening "(*"
 */
private void docComment(Lexer* lx, Arr(byte) inp) {
    Int startBt = lx->i;
    
    Int parenLevel = 1;
    Int j = lx->i;
    for (; j < lx->inpLength; j++) {
        byte cByte = inp[j];
        // Doc comments may contain arbitrary UTF-8 symbols, but we care only about newlines and parentheses
        if (cByte == aNewline) {
            addNewLine(j, lx);
        } else if (cByte == aParenLeft) {
            parenLevel++;
        } else if (cByte == aParenRight) {
            parenLevel--;
            if (parenLevel == 0) {
                j++; // CONSUME the right parenthesis
                break;
            }
        }
    }
    VALIDATEL(parenLevel == 0, errorPunctuationExtraOpening)
    if (j > lx->i) {
        add((Token){.tp = tokDocComment, .startBt = lx->i - 2, .lenBts = j - lx->i + 2}, lx);
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
            (*lx->backtrack->content)[lx->backtrack->length - 1].countClauses++;
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

/** An opener for a scope or a scopeful core form. Precondition: we are past the "(-".
 * Consumes zero or 1 byte
 */
private void openScope(Lexer* lx, Arr(byte) inp) {
    Int startBt = lx->i;
    lx->i += 2; // CONSUME the "(."
    VALIDATEL(!hasValues(lx->backtrack) || peek(lx->backtrack).spanLevel == slScope, errorPunctuationScope)
    VALIDATEL(lx->i < lx->inpLength, errorPrematureEndOfInput)
    byte currBt = CURR_BT;
    if (currBt == aLLower && testForWord(lx->inp, lx->i, reservedBytesLoop, 4)) {
            openPunctuation(tokLoop, slScope, startBt, lx);
            lx->i += 4; // CONSUME the "loop"
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

/** Handles the "(*" case (doc-comment), the "(:" case (data initializer) as well as the common subexpression case */
private void lexParenLeft(Lexer* lx, Arr(byte) inp) {
    mbIncrementClauseCount(lx);
    Int j = lx->i + 1;
    VALIDATEL(j < lx->inpLength, errorPunctuationUnmatched)
    if (inp[j] == aColon) { // "(:" starts a new data initializer        
        openPunctuation(tokData, slSubexpr, lx->i, lx);
        lx->i += 2; // CONSUME the "(:"
    } else if (inp[j] == aTimes) {
        lx->i += 2; // CONSUME the "(*"
        docComment(lx, inp);
    } else if (inp[j] == aDot) {
        openScope(lx, inp);
    } else {
        wrapInAStatement(lx, inp);
        openPunctuation(tokParens, slSubexpr, lx->i, lx);
        lx->i++; // CONSUME the left parenthesis
    }
}


private void lexParenRight(Lexer* lx, Arr(byte) inp) {
    // TODO handle syntax like "(foo 5).field" and "arr[5].field"
    Int startInd = lx->i;
    closeRegularPunctuation(tokParens, lx);
    
    if (!hasValues(lx->backtrack)) return;    
    mbCloseCompoundCoreForm(lx);
    
    lx->lastClosingPunctInd = startInd;    
}


void lexSpace(Lexer* lx, Arr(byte) inp) {
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
    VALIDATEL(j != lx->inpLength, errorPrematureEndOfInput)
    add((Token){.tp=tokString, .startBt=(lx->i), .lenBts=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


void lexUnexpectedSymbol(Lexer* lx, Arr(byte) inp) {
    throwExcLexer(errorUnrecognizedByte, lx);
}

void lexNonAsciiError(Lexer* lx, Arr(byte) inp) {
    throwExcLexer(errorNonAscii, lx);
}

/** Must agree in order with token types in LexerConstants.h */
const char* tokNames[] = {
    "Int", "Long", "Float", "Bool", "String", "_", "DocComment", 
    "word", "Type", ".word", "operator", "dispose", 
    ":", "(.", "stmt", "(", "type(", "(:", "=", ":=", "mutation", "=>", "else",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue", 
    "defer", "each", "embed", "export", "exposePriv", "fn", "interface", 
    "lambda", "meta", "package", "return", "struct", "try", "yield",
    "if", "ifPr", "match", "impl", "loop"
};


void printLexer(Lexer* a) {
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
int equalityLexer(Lexer a, Lexer b) {
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
            printf("\n\nUNEQUAL RESULTS\n", i);
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

    for (Int i = 0; i < sizeof(operatorStartSymbols); i++) {
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
    p[ 0] = (OpDef){.name=s("!="),   .arity=2, .overs=4, .bytes={aExclamation, aEqual, 0, 0 } };
    p[ 1] = (OpDef){.name=s("!"),    .arity=1, .overs=2, .bytes={aExclamation, 0, 0, 0 } };
    p[ 2] = (OpDef){.name=s("#"),    .arity=1, .overs=3, .bytes={aSharp, 0, 0, 0 }, .overloadable=true};    
    p[ 3] = (OpDef){.name=s("$"),    .arity=1, .overs=3, .bytes={aDollar, 0, 0, 0 }, .overloadable=true};
    p[ 4] = (OpDef){.name=s("%"),    .arity=2, .overs=1, .bytes={aPercent, 0, 0, 0 } };
    p[ 5] = (OpDef){.name=s("&&"),   .arity=2, .overs=2, .bytes={aAmp, aAmp, 0, 0 }, .assignable=true};
    p[ 6] = (OpDef){.name=s("&"),    .arity=1, .overs=1, .bytes={aAmp, 0, 0, 0 } };
    p[ 7] = (OpDef){.name=s("'"),    .arity=1, .overs=1, .bytes={aApostrophe, 0, 0, 0 } };
    p[ 8] = (OpDef){.name=s("*."),   .arity=2, .overs=2, .bytes={aTimes, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[ 9] = (OpDef){.name=s("*"),    .arity=2, .overs=2, .bytes={aTimes, 0, 0, 0 }, .assignable = true, .overloadable = true};
    p[10] = (OpDef){.name=s("++"),   .arity=1, .overs=1, .bytes={aPlus, aPlus, 0, 0}, .overloadable=true };
    p[11] = (OpDef){.name=s("+."),   .arity=2, .overs=0, .bytes={aPlus, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[12] = (OpDef){.name=s("+"),    .arity=2, .overs=3, .bytes={aPlus, 0, 0, 0 }, .assignable = true, .overloadable = true};
    p[13] = (OpDef){.name=s(","),    .arity=3, .overs=1, .bytes={aComma, 0, 0, 0}};    
    p[14] = (OpDef){.name=s("--"),   .arity=1, .overs=1, .bytes={aMinus, aMinus, 0, 0}, .overloadable=true};
    p[15] = (OpDef){.name=s("-."),   .arity=2, .overs=0, .bytes={aMinus, aDot, 0, 0}, .assignable = true, .overloadable = true};    
    p[16] = (OpDef){.name=s("-"),    .arity=2, .overs=2, .bytes={aMinus, 0, 0, 0}, .assignable = true, .overloadable = true };
    p[17] = (OpDef){.name=s("/."),   .arity=2, .overs=0, .bytes={aDivBy, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[18] = (OpDef){.name=s("/"),    .arity=2, .overs=2, .bytes={aDivBy, 0, 0, 0}, .assignable = true, .overloadable = true};
    p[19] = (OpDef){.name=s("<<."),  .arity=2, .overs=0, .bytes={aLT, aLT, aDot, 0}, .assignable = true, .overloadable = true};
    p[20] = (OpDef){.name=s("<<"),   .arity=2, .overs=1, .bytes={aLT, aLT, 0, 0}, .assignable = true, .overloadable = true };    
    p[21] = (OpDef){.name=s("<="),   .arity=2, .overs=3, .bytes={aLT, aEqual, 0, 0}};    
    p[22] = (OpDef){.name=s("<>"),   .arity=2, .overs=3, .bytes={aLT, aGT, 0, 0}};    
    p[23] = (OpDef){.name=s("<"),    .arity=2, .overs=3, .bytes={aLT, 0, 0, 0 } };
    p[24] = (OpDef){.name=s("=="),   .arity=2, .overs=4, .bytes={aEqual, aEqual, 0, 0 } };
    p[25] = (OpDef){.name=s(">=<="), .arity=3, .overs=2, .bytes={aGT, aEqual, aLT, aEqual } };
    p[26] = (OpDef){.name=s(">=<"),  .arity=3, .overs=2, .bytes={aGT, aEqual, aLT, 0 } };
    p[27] = (OpDef){.name=s("><="),  .arity=3, .overs=2, .bytes={aGT, aLT, aEqual, 0 } };
    p[28] = (OpDef){.name=s("><"),   .arity=3, .overs=2, .bytes={aGT, aLT, 0, 0 } };
    p[29] = (OpDef){.name=s(">="),   .arity=2, .overs=3, .bytes={aGT, aEqual, 0, 0 } };
    p[30] = (OpDef){.name=s(">>."),  .arity=2, .overs=1, .bytes={aGT, aGT, aDot, 0}, .assignable = true, .overloadable = true};
    p[31] = (OpDef){.name=s(">>"),   .arity=2, .overs=1, .bytes={aGT, aGT, 0, 0}, .assignable = true, .overloadable = true };
    p[32] = (OpDef){.name=s(">"),    .arity=2, .overs=3, .bytes={aGT, 0, 0, 0 }};
    p[33] = (OpDef){.name=s("?:"),   .arity=2, .overs=1, .bytes={aQuestion, aColon, 0, 0 } };
    p[34] = (OpDef){.name=s("?"),    .arity=1, .overs=1, .bytes={aQuestion, 0, 0, 0 } };
    p[35] = (OpDef){.name=s("@"),    .arity=1, .overs=1, .bytes={aAt, 0, 0, 0 } };
    p[36] = (OpDef){.name=s("^."),   .arity=2, .overs=0, .bytes={aCaret, aDot, 0, 0}, .assignable = true, .overloadable = true};
    p[37] = (OpDef){.name=s("^"),    .arity=2, .overs=2, .bytes={aCaret, 0, 0, 0}, .assignable = true, .overloadable = true};
    p[38] = (OpDef){.name=s("||"),   .arity=2, .overs=2, .bytes={aPipe, aPipe, 0, 0}, .assignable=true, };
    p[39] = (OpDef){.name=s("|"),    .arity=2, .overs=1, .bytes={aPipe, 0, 0, 0}};
    p[40] = (OpDef){.name=s("and"),  .arity=2, .overs=1, .bytes={0, 0, 0, 0 }, .assignable=true};
    p[41] = (OpDef){.name=s("or"),   .arity=2, .overs=1, .bytes={0, 0, 0, 0 }, .assignable=true};
    p[42] = (OpDef){.name=s("neg"),  .arity=2, .overs=2, .bytes={0, 0, 0, 0 }};
    return result;
}



/*
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    result->possiblyReservedDispatch = tabulateReservedBytes(a);
    result->dispatchTable = tabulateDispatch(a);
    result->operators = tabulateOperators(a);
    result->reservedParensOrNot = tabulateReserved(a);
    return result;
}


Lexer* createLexer(String* inp, LanguageDefinition* langDef, Arena* a) {
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
    VALIDATEL(top.spanLevel != slScope && !hasValues(lx->backtrack), errorPunctuationExtraOpening)

    setStmtSpanLength(top.tokenInd, lx);    
    deleteArena(lx->aTmp);
}


Lexer* lexicallyAnalyze(String* input, LanguageDefinition* langDef, Arena* a) {
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

//{{{ ParserConstants
#define nodInt          0
#define nodLong         1
#define nodFloat        2
#define nodBool         3      // pl2 = value (1 or 0)
#define nodString       4
#define nodUnderscore   5
#define nodDocComment   6

#define nodId           7      // pl1 = index of binding, pl2 = index of name
#define nodCall         8      // pl1 = index of binding, pl2 = arity
#define nodBinding      9      // pl1 = index of binding
#define nodTypeId      10      // pl1 = index of binding
#define nodAnd         11
#define nodOr          12


// Punctuation (inner node)
#define nodScope       13       // (: This is resumable but trivially so, that's why it's not grouped with the others
#define nodExpr        14
#define nodAssignment  15       // pl1 = 1 if mutable assignment, 0 if immutable
#define nodReassign    16       // :=
#define nodMutation    17       // pl1 = like in topOperator. This is the "+=", operatorful type of mutations


// Single-shot core syntax forms
#define nodAlias       18       
#define nodAssert      19       
#define nodAssertDbg   20       
#define nodAwait       21       
#define nodBreak       22       
#define nodCatch       23       // "(catch e. e,print)"
#define nodContinue    24       
#define nodDefer       25       
#define nodEach        26       
#define nodEmbed       27       // noParen. Embed a text file as a string literal, or a binary resource file
#define nodExport      28       
#define nodExposePriv  29       
#define nodFnDef       30       // pl1 = index of binding
#define nodIface       31
#define nodLambda      32
#define nodMeta        33       
#define nodPackage     34       // for single-file packages
#define nodReturn      35       
#define nodStruct      36       
#define nodTry         37       
#define nodYield       38       
#define nodIfClause    39       // pl1 = number of tokens in the left side of the clause
#define nodElse        40
#define nodLoop        41       //
#define nodLoopCond    42

// Resumable core forms
#define nodIf          43       // paren
#define nodIfPr        44       // like if, but every branch is a value compared using custom predicate
#define nodImpl        45       
#define nodMatch       46       // pattern matching on sum type tag


const char errorBareAtom[]                    = "Malformed token stream (atoms and parentheses must not be bare)";
const char errorImportsNonUnique[]            = "Import names must be unique!";
const char errorLengthOverflow[]              = "AST nodes length overflow";
const char errorPrematureEndOfTokens[]        = "Premature end of input";
const char errorUnexpectedToken[]             = "Unexpected token";
const char errorInconsistentSpan[]            = "Inconsistent span length / structure of token scopes!";
const char errorCoreFormTooShort[]            = "Core syntax form too short";
const char errorCoreFormUnexpected[]          = "Unexpected core form";
const char errorCoreFormAssignment[]          = "A core form may not contain any assignments!";
const char errorCoreFormInappropriate[]       = "Inappropriate reserved word!";
const char errorIfLeft[]                      = "A left-hand clause in an if can only contain variables, boolean literals and expressions!";
const char errorIfRight[]                     = "A right-hand clause in an if can only contain atoms, expressions, scopes and some core forms!";
const char errorIfEmpty[]                     = "Empty `if` expression";
const char errorIfMalformed[]                 = "Malformed `if` expression, should look like (if pred => `true case` else `default`)";
const char errorFnNameAndParams[]             = "Function signature must look like this: `(-f fnName ReturnType(x Type1 y Type2). body...)`";
const char errorFnMissingBody[]               = "Function definition must contain a body which must be a Scope immediately following its parameter list!";
const char errorLoopSyntaxError[]             = "A loop should look like `(.loop (< x 101) (x 0). loopBody)`";
const char errorLoopHeader[]                  = "A loop header should contain 1 or 2 items: the condition and, optionally, the var declarations";
const char errorLoopEmptyBody[]               = "Empty loop body!";
const char errorBreakContinueTooComplex[]     = "This statement is too complex! Continues and breaks may contain one thing only: the number of enclosing loops to continue/break!";
const char errorBreakContinueInvalidDepth[]   = "Invalid depth of break/continue! It must be a positive 32-bit integer!"; 
const char errorDuplicateFunction[]           = "Duplicate function declaration: a function with same name and arity already exists in this scope!";
const char errorExpressionInfixNotSecond[]    = "An infix expression must have the infix operator in second position (not counting possible prefix operators)!";
const char errorExpressionError[]             = "Cannot parse expression!";
const char errorExpressionCannotContain[]     = "Expressions cannot contain scopes or statements!";
const char errorExpressionFunctionless[]      = "Functionless expression!";
const char errorExpressionHeadFormOperators[] = "Incorrect number of active operators at end of a head-function subexpression, should be exactly one!";
const char errorTypeDeclCannotContain[]       = "Type declarations may only contain types (like A), type params (like a), type constructors (like .List) and parentheses!";
const char errorTypeDeclError[]               = "Cannot parse type declaration!";
const char errorUnknownType[]                 = "Unknown type";
const char errorUnknownTypeFunction[]         = "Unknown type constructor";
const char errorOperatorWrongArity[]          = "Wrong number of arguments for operator!";
const char errorUnknownBinding[]              = "Unknown binding!";
const char errorUnknownFunction[]             = "Unknown function!";
const char errorIncorrectPrefixSequence[]     = "A prefix atom-expression must look like `!!a`, that is, only prefix operators followed by one ident or literal";
const char errorOperatorUsedInappropriately[] = "Operator used in an inappropriate location!";
const char errorAssignment[]                  = "Cannot parse assignment, it must look like `freshIdentifier` = `expression`";
const char errorAssignmentShadowing[]         = "Assignment error: existing identifier is being shadowed";
const char errorReturn[]                      = "Cannot parse return statement, it must look like `return ` {expression}";
const char errorScope[]                       = "A scope may consist only of expressions, assignments, function definitions and other scopes!";
const char errorLoopBreakOutside[]            = "The break keyword can only be used outside a loop scope!";
const char errorTypeUnknownLastArg[]          = "The type of last argument to a call must be known, otherwise I can't resolve the function overload!";
const char errorTypeZeroArityOverload[]       = "A function with no parameters cannot be overloaded.";
const char errorTypeNoMatchingOverload[]      = "No matching function overload was found";
const char errorTypeWrongArgumentType[]       = "Wrong argument type";

// temporary, delete it when the parser is finished
const char errorTemp[]                        = "Not implemented yet";


//}}}
#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser(errMsg, pr); }

typedef struct {
    untt tp : 6;
    Int startNodeInd;
    Int sentinelToken;
    void* scopeStackFrame; // only for tp = scope or expr
} ParseFrame;

DEFINE_STACK_HEADER(ParseFrame)
DEFINE_STACK(ParseFrame)
                                 

#define pop(X) _Generic((X), \
    StackParseFrame*: popParseFrame, \
    Stackint32_t*: popint32_t \
    )(X)

#define peek(X) _Generic((X), \
    StackParseFrame*: peekParseFrame, \
    Stackint32_t*: peekint32_t \
    )(X)

#define push(A, X) _Generic((X), \
    StackParseFrame*: pushParseFrame, \
    Stackint32_t*: pushint32_t \
    )((A), X)
    
#define hasValues(X) _Generic((X), \
    StackParseFrame*: hasValuesParseFrame, \
    Stackint32_t*:  hasValuesint32_t \
    )(X)   

typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct ScopeChunk ScopeChunk;
struct ScopeChunk {
    ScopeChunk *next;
    int length; // length is divisible by 4
    Int content[];   
};

/** 
 * Either currChunk->next == NULL or currChunk->next->next == NULL
 */
typedef struct {
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    ScopeStackFrame* topScope;
    Int length;
    int nextInd; // next ind inside currChunk, unit of measurement is 4 bytes
} ScopeStack;

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


ScopeStack* createScopeStack() {    
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
void pushScope(ScopeStack* scopeStack) {
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
            
            memcpy(topScope->bindings, newContent, initLength*sizeof(Int));
            topScope->bindings = newContent;                        
        }
    } else if (newLength == 256) {
        longjmp(excBuf, 1);
    }
}

void addBinding(int nameId, int bindingId, Arr(int) activeBindings, ScopeStack* scopeStack) {
    ScopeStackFrame* topScope = scopeStack->topScope;
    resizeScopeArrayIfNecessary(64, topScope, scopeStack);
    
    topScope->bindings[topScope->length] = nameId;
    topScope->length++;
    
    activeBindings[nameId] = bindingId;
}

/**
 * Pops a scope or expr frame. For a scope type of frame, also deactivates its bindings.
 * Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any
 */
void popScopeFrame(Arr(int) activeBindings, ScopeStack* scopeStack) {  
    ScopeStackFrame* topScope = scopeStack->topScope;
    if (topScope->bindings) {
        for (int i = 0; i < topScope->length; i++) {
            activeBindings[*(topScope->bindings + i)] = -1;
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


#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser(errMsg, pr); }

typedef struct Compiler Compiler;
typedef struct {
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;   
} Node;


typedef struct {
    Int typeId;
    Int nameId;
    bool appendUnderscore;
    bool emitAsPrefix;
    bool emitAsOperator;
    bool emitAsMethod;
} Entity;


typedef struct {
    String* name;
    Entity entity;
} EntityImport;


typedef struct {
    String* name;
    Int count;
} OverloadImport;


typedef void (*ParserFunc)(Token, Arr(Token), Compiler*);
typedef void (*ResumeFunc)(Token*, Arr(Token), Compiler*);

#define countSyntaxForms (tokLoop + 1)
#define firstResumableForm nodIf
#define countResumableForms (nodMatch - nodIf + 1)

typedef struct {
    ParserFunc (*nonResumableTable)[countSyntaxForms];
    ResumeFunc (*resumableTable)[countResumableForms];
    bool (*allowedSpanContexts)[countResumableForms + countSyntaxForms][countResumableForms];
    OpDef (*operators)[countOperators];
} ParserDefinition;

/*
 * PARSER DATA
 * 
 * 1) a = Arena for the results
 * AST (i.e. the resulting code)
 * Entitys
 * Types
 *
 * 2) aBt = Arena for the temporary stuff (backtrack). Freed after end of parsing
 *
 * 3) ScopeStack (temporary, but knows how to free parts of itself, so in a separate arena)
 *
 * WORKFLOW
 * The "stringTable" is frozen: it was filled by the lexer. The "bindings" table is growing
 * with every new assignment form encountered. "Nodes" is obviously growing with the new AST nodes
 * emitted.
 * Any new span (scope, expression, assignment etc) means 
 * - pushing a ParseFrame onto the "backtrack" stack
 * - if the new frame is a lexical scope, also pushing a scope onto the "scopeStack"
 * The end of a span means popping from "backtrack" and also, if needed, popping from "scopeStack".
 *
 * ENTITIES AND OVERLOADS
 * In the parser, all non-functions are bound to Entities, but functions are instead bound to Overloads.
 * Binding ids go: 0, 1, 2... Overload ids go: -2, -3, -4...
 * Those overloads are used for counting how many functions for each name there are.
 * Then they are resolved at the stage of the typer, after which there are only the Entities.
 */
struct Compiler {
    String* text;
    Lexer* inp;
    Int inpLength;
    ParserDefinition* parDef;
    StackParseFrame* backtrack;// [aTmp] 
    ScopeStack* scopeStack;
    Int i;                     // index of current token in the input

    Arr(Node) nodes; 
    Int capacity;               // current capacity of node storage
    Int nextInd;                // the index for the next token to be added    

    Arr(Entity) entities;      // growing array of all bindings ever encountered
    Int entNext;
    Int entCap;
    Int entOverloadZero;       // the index of the first parsed (as opposed to being built-in or imported) overloaded binding
    Int entBindingZero;        // the index of the first parsed (as opposed to being built-in or imported) non-overloaded binding

    Arr(Int) overloadCounts;   // [aTmp] growing array of counts of all fn name definitions encountered (for the typechecker to use)
    Int overlCNext;
    Int overlCCap;

    Stackint32_t* types;
    Int typeNext;
    Int typeCap;

    Stackint32_t* expStack;    // [aTmp] temporary scratch space for type checking/resolving an expression

    // Current bindings and overloads in scope. -1 means "not active"
    // Var & type bindings are nameId (index into stringTable) -> bindingId
    // Function bindings are nameId -> (-overloadId - 2). So negative values less than -1 mean "function is active"
    Arr(int) activeBindings;

    Stackint32_t* stringTable; // The table of unique strings from code. Contains only the startByte of each string.       
    StringStore* stringStore;  // A hash table for quickly deduplicating strings. Points into stringTable 
    Int strLength;             // length of stringTable    

    bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aTmp;
};

#define BIG 70000000

_Noreturn private void throwExcParser(const char errMsg[], Compiler* pr) {   
    
    pr->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d\n", pr->i);
#endif    
    pr->errMsg = str(errMsg, pr->a);
    longjmp(excBuf, 1);
}


// Forward declarations
private bool parseLiteralOrIdentifier(Token tok, Compiler* pr);


/** Validates a new binding (that it is unique), creates an entity for it, and adds it to the current scope */
Int createBinding(Token bindingToken, Entity b, Compiler* pr) {
    VALIDATEP(bindingToken.tp == tokWord, errorAssignment)

    Int nameId = bindingToken.pl2;
    Int mbBinding = pr->activeBindings[nameId];
    VALIDATEP(mbBinding == -1, errorAssignmentShadowing)
    
    pr->entities[pr->entNext] = b;
    Int newBindingId = pr->entNext;
    pr->entNext++;
    if (pr->entNext == pr->entCap) {
        Arr(Entity) newBindings = allocateOnArena(2*pr->entCap*sizeof(Entity), pr->a);
        memcpy(newBindings, pr->entities, pr->entCap*sizeof(Entity));
        pr->entities = newBindings;
        pr->entCap *= 2;
    }    
    
    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (pr->scopeStack->length > 0) {
            addBinding(nameId, newBindingId, pr->activeBindings, pr->scopeStack); // adds it to the ScopeStack
        }
        pr->activeBindings[nameId] = newBindingId; // makes it active
    }
    
    return newBindingId;
}

/** Processes the name of a defined function. Creates an overload counter, or increments it if it exists. Consumes no tokens. */
void encounterFnDefinition(Int nameId, Compiler* pr) {    
    Int activeValue = (nameId > -1) ? pr->activeBindings[nameId] : -1;
    VALIDATEP(activeValue < 0, errorAssignmentShadowing);
    if (activeValue == -1) {
        pr->overloadCounts[pr->overlCNext] = 1;
        pr->activeBindings[nameId] = -pr->overlCNext - 2;
        pr->overlCNext++;
        if (pr->overlCNext == pr->overlCCap) {
            Arr(Int) newOverloads = allocateOnArena(2*pr->overlCCap*4, pr->a);
            memcpy(newOverloads, pr->overloadCounts, pr->overlCCap*4);
            pr->overloadCounts = newOverloads;
            pr->overlCCap *= 2;
        }
    } else {
        pr->overloadCounts[-activeValue - 2]++;
    }
}


Int importEntity(Int nameId, Entity b, Compiler* pr) {
    if (nameId > -1) {
        Int mbBinding = pr->activeBindings[nameId];
        VALIDATEP(mbBinding == -1, errorAssignmentShadowing)
    }

    pr->entities[pr->entNext] = b;
    Int newBindingId = pr->entNext;
    pr->entNext++;
    if (pr->entNext == pr->entCap) {
        Arr(Entity) newBindings = allocateOnArena(2*pr->entCap*sizeof(Entity), pr->a);
        memcpy(newBindings, pr->entities, pr->entCap*sizeof(Entity));
        pr->entities = newBindings;
        pr->entCap *= 2;
    }    
    
    if (pr->scopeStack->length > 0) {
        // adds it to the ScopeStack so it will be cleaned up when the scope is over
        addBinding(nameId, newBindingId, pr->activeBindings, pr->scopeStack); 
    }
    pr->activeBindings[nameId] = newBindingId; // makes it active
           
    return newBindingId;
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunkParser(Compiler* pr) {
    Node* newStorage = allocateOnArena(pr->capacity*2*sizeof(Node), pr->a);
    memcpy(newStorage, pr->nodes, (pr->capacity)*(sizeof(Node)));
    pr->nodes = newStorage;
    pr->capacity *= 2;
}


void addNode(Node n, Compiler* pr) {
    pr->nodes[pr->nextInd] = n;
    pr->nextInd++;
    if (pr->nextInd == pr->capacity) {
        handleFullChunk(pr);
    }
}

/**
 * Finds the top-level punctuation opener by its index, and sets its node length.
 * Called when the parsing of a span is finished.
 */
private void setSpanLengthParser(Int nodeInd, Compiler* pr) {
    pr->nodes[nodeInd].pl2 = pr->nextInd - nodeInd - 1;
}


private void parseVerbatim(Token tok, Compiler* pr) {
    addNode((Node){.tp = tok.tp, .startBt = tok.startBt, .lenBts = tok.lenBts, 
        .pl1 = tok.pl1, .pl2 = tok.pl2}, pr);
}

private void parseErrorBareAtom(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private ParseFrame popFrame(Compiler* pr) {    
    ParseFrame frame = pop(pr->backtrack);
    if (frame.tp == nodScope) {
        popScopeFrame(pr->activeBindings, pr->scopeStack);
    }
    setSpanLength(frame.startNodeInd, pr);
    return frame;
}

/** Calculates the sentinel token for a token at a specific index */
private Int calcSentinel(Token tok, Int tokInd) {
    return (tok.tp >= firstPunctuationTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}

/**
 * A single-item subexpression, like "(foo)" or "(!)". Consumes no tokens.
 */
private void exprSingleItem(Token theTok, Compiler* pr) {
    if (theTok.tp == tokWord) {
        Int mbOverload = pr->activeBindings[theTok.pl2];
        VALIDATEP(mbOverload != -1, errorUnknownFunction)
        addNode((Node){.tp = nodCall, .pl1 = mbOverload, .pl2 = 0,
                       .startBt = theTok.startBt, .lenBts = theTok.lenBts}, pr);        
    } else if (theTok.tp == tokOperator) {
        Int operBindingId = theTok.pl1;
        OpDef operDefinition = (*pr->parDef->operators)[operBindingId];
        if (operDefinition.arity == 1) {
            addNode((Node){ .tp = nodId, .pl1 = operBindingId,
                .startBt = theTok.startBt, .lenBts = theTok.lenBts}, pr);
        } else {
            throwExcParser(errorUnexpectedToken, pr);
        }
    } else if (theTok.tp <= topVerbatimTokenVariant) {
        addNode((Node){.tp = theTok.tp, .pl1 = theTok.pl1, .pl2 = theTok.pl2,
                        .startBt = theTok.startBt, .lenBts = theTok.lenBts }, pr);
    } else VALIDATEP(theTok.tp < firstCoreFormTokenType, errorCoreFormInappropriate)
    else {
        throwExcParser(errorUnexpectedToken, pr);
    }
}

/** Counts the arity of the call, including skipping unary operators. Consumes no tokens. */
void exprCountArity(Int* arity, Int sentinelToken, Arr(Token) tokens, Compiler* pr) {
    Int j = pr->i;
    Token firstTok = tokens[j];

    j = calcSentinel(firstTok, j);
    while (j < sentinelToken) {
        Token tok = tokens[j];
        if (tok.tp < firstPunctuationTokenType) {
            j++;
        } else {
            j += tok.pl2 + 1;
        }
        if (tok.tp != tokOperator || (*pr->parDef->operators)[tok.pl1].arity > 1) {            
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
private void exprSubexpr(Token parenTok, Int* arity, Arr(Token) tokens, Compiler* pr) {
    Token firstTok = tokens[pr->i];    
    
    if (parenTok.pl2 == 1) {
        exprSingleItem(tokens[pr->i], pr);
        *arity = 0;
        pr->i++; // CONSUME the single item within parens
    } else {
        exprCountArity(arity, pr->i + parenTok.pl2, tokens, pr);
        
        if (firstTok.tp == tokWord || firstTok.tp == tokOperator) {
            Int mbBindingId = -1;
            if (firstTok.tp == tokWord) {
                mbBindingId = pr->activeBindings[firstTok.pl2];
            } else if (firstTok.tp == tokOperator) {
                VALIDATEP(*arity == (*pr->parDef->operators)[firstTok.pl1].arity, errorOperatorWrongArity)
                mbBindingId = -firstTok.pl1 - 2;
            }
            
            VALIDATEP(mbBindingId < -1, errorUnknownFunction)            

            addNode((Node){.tp = nodCall, .pl1 = mbBindingId, .pl2 = *arity, // todo overload
                           .startBt = firstTok.startBt, .lenBts = firstTok.lenBts}, pr);
            *arity = 0;
            pr->i++; // CONSUME the function or operator call token
        }
    }
}

/**
 * Flushes the finished subexpr frames from the top of the funcall stack.
 * A subexpr frame is finished iff current token equals its sentinel.
 * Flushing includes appending its operators, clearing the operator stack, and appending
 * prefix unaries from the previous subexpr frame, if any.
 */
private void subexprClose(Arr(Token) tokens, Compiler* pr) {
    while (pr->scopeStack->length > 0 && pr->i == peek(pr->backtrack).sentinelToken) {
        popFrame(pr);        
    }
}

/** An operator token in non-initial position is either a funcall (if unary) or an operand. Consumes no tokens. */
private void exprOperator(Token tok, ScopeStackFrame* topSubexpr, Arr(Token) tokens, Compiler* pr) {
    Int bindingId = tok.pl1;
    OpDef operDefinition = (*pr->parDef->operators)[bindingId];

    if (operDefinition.arity == 1) {
        addNode((Node){ .tp = nodCall, .pl1 = -bindingId - 2, .pl2 = 1,
                        .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    } else {
        addNode((Node){ .tp = nodId, .pl1 = -bindingId - 2, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    }
}

/** Parses an expression. Precondition: we are 1 past the span token */
private void parseExpr(Token exprTok, Arr(Token) tokens, Compiler* pr) {
    Int sentinelToken = pr->i + exprTok.pl2;
    Int arity = 0;
    if (exprTok.pl2 == 1) {
        exprSingleItem(tokens[pr->i], pr);
        pr->i++; // CONSUME the single item within parens
        return;
    }

    push(((ParseFrame){.tp = nodExpr, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + exprTok.pl2 }), 
         pr->backtrack);        
    addNode((Node){ .tp = nodExpr, .startBt = exprTok.startBt, .lenBts = exprTok.lenBts }, pr);

    exprSubexpr(exprTok, &arity, tokens, pr);
    while (pr->i < sentinelToken) {
        subexprClose(tokens, pr);
        Token currTok = tokens[pr->i];
        untt tokType = currTok.tp;
        
        ScopeStackFrame* topSubexpr = pr->scopeStack->topScope;
        if (tokType == tokParens) {
            pr->i++; // CONSUME the parens token
            exprSubexpr(currTok, &arity, tokens, pr);
        } else VALIDATEP(tokType < firstPunctuationTokenType, errorExpressionCannotContain)
        else if (tokType <= topVerbatimTokenVariant) {
            addNode((Node){ .tp = currTok.tp, .pl1 = currTok.pl1, .pl2 = currTok.pl2,
                            .startBt = currTok.startBt, .lenBts = currTok.lenBts }, pr);
            pr->i++; // CONSUME the verbatim token
        } else {
            if (tokType == tokWord) {
                Int mbBindingId = pr->activeBindings[currTok.pl2];
                VALIDATEP(mbBindingId != -1, errorUnknownBinding)
                addNode((Node){ .tp = nodId, .pl1 = mbBindingId, .pl2 = currTok.pl2, 
                            .startBt = currTok.startBt, .lenBts = currTok.lenBts}, pr);                
            } else if (tokType == tokOperator) {
                exprOperator(currTok, topSubexpr, tokens, pr);
            }
            pr->i++; // CONSUME any leaf token
        }
    }
    subexprClose(tokens, pr);    
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
private void maybeCloseSpans(Compiler* pr) {
    while (hasValues(pr->backtrack)) { // loop over subscopes and expressions inside FunctionDef
        ParseFrame frame = peek(pr->backtrack);
        if (pr->i < frame.sentinelToken) {
            return;
        } else VALIDATEP(pr->i == frame.sentinelToken, errorInconsistentSpan)
        popFrame(pr);
    }
}

/** Parses top-level function bodies */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Compiler* pr) {
    while (pr->i < sentinelToken) {
        Token currTok = tokens[pr->i];
        untt contextType = peek(pr->backtrack).tp;
        
        // pre-parse hooks that let contextful syntax forms (e.g. "if") detect parsing errors and maintain their state
        if (contextType >= firstResumableForm) {
            ((*pr->parDef->resumableTable)[contextType - firstResumableForm])(&currTok, tokens, pr);
        } else {
            pr->i++; // CONSUME any span token
        }
        ((*pr->parDef->nonResumableTable)[currTok.tp])(currTok, tokens, pr);
        
        maybeCloseSpans(pr);
    }    
}

/** Consumes 0 or 1 tokens. Returns false if didn't parse anything. */
private bool parseLiteralOrIdentifier(Token tok, Compiler* pr) {
    if (tok.tp <= topVerbatimTokenVariant) {
        parseVerbatim(tok, pr);
    } else if (tok.tp == tokWord) {        
        Int nameId = tok.pl2;
        Int mbBinding = pr->activeBindings[nameId];
        VALIDATEP(mbBinding != -1, errorUnknownBinding)    
        addNode((Node){.tp = nodId, .pl1 = mbBinding, .pl2 = nameId,
                       .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    } else {
        return false;        
    }
    pr->i++; // CONSUME the literal or ident token
    return true;
}


private void parseAssignment(Token tok, Arr(Token) tokens, Compiler* pr) {
    const Int rightSideLen = tok.pl2 - 1;
    VALIDATEP(rightSideLen >= 1, errorAssignment)
    
    Int sentinelToken = pr->i + tok.pl2;

    Token bindingToken = tokens[pr->i];
    Int newBindingId = createBinding(bindingToken, (Entity){ }, pr);

    push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
         pr->backtrack);
    addNode((Node){.tp = nodAssignment, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    
    addNode((Node){.tp = nodBinding, .pl1 = newBindingId, 
            .startBt = bindingToken.startBt, .lenBts = bindingToken.lenBts}, pr);
    
    pr->i++; // CONSUME the word token before the assignment sign
    Token rightSideToken = tokens[pr->i];
    if (rightSideToken.tp == tokScope) {
        print("scope %d", rightSideToken.tp);
        //openScope(pr);
    } else {
        if (rightSideLen == 1) {
            VALIDATEP(parseLiteralOrIdentifier(rightSideToken, pr), errorAssignment)
        } else if (rightSideToken.tp == tokIf) {
        } else {
            parseExpr((Token){ .pl2 = rightSideLen, .startBt = rightSideToken.startBt, 
                               .lenBts = tok.lenBts - rightSideToken.startBt + tok.startBt
                       }, 
                       tokens, pr);
        }
    }
}

private void parseReassignment(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

private void parseMutation(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

private void parseAlias(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

private void parseAssert(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseAssertDbg(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseAwait(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

// TODO validate we are inside at least as many loops as we are breaking out of
private void parseBreak(Token tok, Arr(Token) tokens, Compiler* pr) {
    VALIDATEP(tok.pl2 <= 1, errorBreakContinueTooComplex);
    if (tok.pl2 == 1) {
        Token nextTok = tokens[pr->i];
        VALIDATEP(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errorBreakContinueInvalidDepth)
        addNode((Node){.tp = nodBreak, .pl1 = nextTok.pl2, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
        pr->i++; // CONSUME the Int after the "break"
    } else {
        addNode((Node){.tp = nodBreak, .pl1 = 1, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    }    
}


private void parseCatch(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseContinue(Token tok, Arr(Token) tokens, Compiler* pr) {
    VALIDATEP(tok.pl2 <= 1, errorBreakContinueTooComplex);
    if (tok.pl2 == 1) {
        Token nextTok = tokens[pr->i];
        VALIDATEP(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errorBreakContinueInvalidDepth)
        addNode((Node){.tp = nodContinue, .pl1 = nextTok.pl2, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
        pr->i++; // CONSUME the Int after the "continue"
    } else {
        addNode((Node){.tp = nodContinue, .pl1 = 1, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    }    
}


private void parseDefer(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseDispose(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseExposePrivates(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseFnDef(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseInterface(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseImpl(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseLambda(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseLambda1(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseLambda2(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseLambda3(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parsePackage(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseReturn(Token tok, Arr(Token) tokens, Compiler* pr) {
    Int lenTokens = tok.pl2;
    Int sentinelToken = pr->i + lenTokens;        
    
    push(((ParseFrame){ .tp = nodReturn, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
            pr->backtrack);
    addNode((Node){.tp = nodReturn, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    
    Token rightSideToken = tokens[pr->i];
    if (rightSideToken.tp == tokScope) {
        print("scope %d", rightSideToken.tp);
        //openScope(pr);
    } else {        
        if (lenTokens == 1) {
            VALIDATEP(parseLiteralOrIdentifier(rightSideToken, pr), errorReturn)            
        } else {
            parseExpr((Token){ .pl2 = lenTokens, .startBt = rightSideToken.startBt, 
                               .lenBts = tok.lenBts - rightSideToken.startBt + tok.startBt
                       }, 
                       tokens, pr);
        }
    }
}


private void parseSkip(Token tok, Arr(Token) tokens, Compiler* pr) {
    
}


private void parseScope(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

private void parseStruct(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseTry(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}


private void parseYield(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

/** To be called at the start of an "if" clause. It validates the grammar and emits nodes. Consumes no tokens.
 * Precondition: we are pointing at the init token of left side of "if" (i.e. at a tokStmt or the like)
 */
private void ifAddClause(Token tok, Arr(Token) tokens, Compiler* pr) {
    VALIDATEP(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errorIfLeft)
    Int leftTokSkip = (tok.tp >= firstPunctuationTokenType) ? (tok.pl2 + 1) : 1;
    Int j = pr->i + leftTokSkip;
    VALIDATEP(j + 1 < pr->inpLength, errorPrematureEndOfTokens)
    VALIDATEP(tokens[j].tp == tokArrow, errorIfMalformed)
    
    j++; // the arrow
    
    Token rightToken = tokens[j];
    Int rightTokLength = (rightToken.tp >= firstPunctuationTokenType) ? (rightToken.pl2 + 1) : 1;    
    Int clauseSentinelByte = rightToken.startBt + rightToken.lenBts;
    
    ParseFrame clauseFrame = (ParseFrame){ .tp = nodIfClause, .startNodeInd = pr->nextInd, .sentinelToken = j + rightTokLength };
    push(clauseFrame, pr->backtrack);
    addNode((Node){.tp = nodIfClause, .pl1 = leftTokSkip,
         .startBt = tok.startBt, .lenBts = clauseSentinelByte - tok.startBt }, pr);
}


private void parseIf(Token tok, Arr(Token) tokens, Compiler* pr) {
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = pr->nextInd, 
        .sentinelToken = pr->i + tok.pl2 };
    push(newParseFrame, pr->backtrack);
    addNode((Node){.tp = nodIf, .pl1 = tok.pl1, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);

    ifAddClause(tokens[pr->i], tokens, pr);
}


private void parseLoop(Token loopTok, Arr(Token) tokens, Compiler* pr) {
    Token tokenStmt = tokens[pr->i];
    Int sentinelStmt = pr->i + tokenStmt.pl2 + 1;
    VALIDATEP(tokenStmt.tp == tokStmt, errorLoopSyntaxError)    
    VALIDATEP(sentinelStmt < pr->i + loopTok.pl2, errorLoopEmptyBody)
    
    Int indLeftSide = pr->i + 1;
    Token tokLeftSide = tokens[indLeftSide]; // + 1 because pr->i points at the stmt so far

    VALIDATEP(tokLeftSide.tp == tokWord || tokLeftSide.tp == tokBool || tokLeftSide.tp == tokParens, errorLoopHeader)
    Int sentinelLeftSide = calcSentinel(tokLeftSide, indLeftSide); 

    Int startOfScope = sentinelStmt;
    Int startBtScope = tokens[startOfScope].startBt;
    Int indRightSide = -1;
    if (sentinelLeftSide < sentinelStmt) {
        indRightSide = sentinelLeftSide;
        startOfScope = indRightSide;
        Token tokRightSide = tokens[indRightSide];
        startBtScope = tokRightSide.startBt;
        VALIDATEP(calcSentinel(tokRightSide, indRightSide) == sentinelStmt, errorLoopHeader)
    }
    
    Int sentToken = startOfScope - pr->i + loopTok.pl2;
        
    push(((ParseFrame){ .tp = nodLoop, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + loopTok.pl2 }), pr->backtrack);
    addNode((Node){.tp = nodLoop, .pl1 = slScope, .startBt = loopTok.startBt, .lenBts = loopTok.lenBts}, pr);

    push(((ParseFrame){ .tp = nodScope, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + loopTok.pl2 }), pr->backtrack);
    addNode((Node){.tp = nodScope, .startBt = startBtScope,
            .lenBts = loopTok.lenBts - startBtScope + loopTok.startBt}, pr);

    // variable initializations, if any
    if (indRightSide > -1) {
        pr->i = indRightSide + 1;
        while (pr->i < sentinelStmt) {
            Token binding = tokens[pr->i];
            VALIDATEP(binding.tp = tokWord, errorLoopSyntaxError)
            
            Token expr = tokens[pr->i + 1];
            
            VALIDATEP(expr.tp < firstPunctuationTokenType || expr.tp == tokParens, errorLoopSyntaxError)
            
            Int initializationSentinel = calcSentinel(expr, pr->i + 1);
            Int bindingId = createBinding(binding, ((Entity){}), pr);
            Int indBindingSpan = pr->nextInd;
            addNode((Node){.tp = nodAssignment, .pl2 = initializationSentinel - pr->i,
                           .startBt = binding.startBt,
                           .lenBts = expr.lenBts + expr.startBt - binding.startBt}, pr);
            addNode((Node){.tp = nodBinding, .pl1 = bindingId,
                           .startBt = binding.startBt, .lenBts = binding.lenBts}, pr);
                        
            if (expr.tp == tokParens) {
                pr->i += 2;
                parseExpr(expr, tokens, pr);
            } else {
                exprSingleItem(expr, pr);
            }
            setSpanLength(indBindingSpan, pr);
            
            pr->i = initializationSentinel;
        }
    }

    // loop body
    
    Token tokBody = tokens[sentinelStmt];
    if (startBtScope < 0) {
        startBtScope = tokBody.startBt;
    }    
    addNode((Node){.tp = nodLoopCond, .pl1 = slStmt, .pl2 = sentinelLeftSide - indLeftSide,
         .startBt = tokLeftSide.startBt, .lenBts = tokLeftSide.lenBts}, pr);
    pr->i = indLeftSide + 1;
    parseExpr(tokens[indLeftSide], tokens, pr);
    
    
    pr->i = sentinelStmt; // CONSUME the loop token and its first statement
}


private void resumeIf(Token* tok, Arr(Token) tokens, Compiler* pr) {
    if (tok->tp == tokElse) {        
        VALIDATEP(pr->i < pr->inpLength, errorPrematureEndOfTokens)
        pr->i++; // CONSUME the "else"
        *tok = tokens[pr->i];        

        push(((ParseFrame){ .tp = nodElse, .startNodeInd = pr->nextInd,
                           .sentinelToken = calcSentinel(*tok, pr->i)}), pr->backtrack);
        addNode((Node){.tp = nodElse, .startBt = tok->startBt, .lenBts = tok->lenBts }, pr);       
        pr->i++; // CONSUME the token after the "else"
    } else {
        ifAddClause(*tok, tokens, pr);
        pr->i++; // CONSUME the init token of the span
    }
}

private void resumeIfPr(Token* tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

private void resumeImpl(Token* tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
}

private void resumeMatch(Token* tok, Arr(Token) tokens, Compiler* pr) {
    throwExcParser(errorTemp, pr);
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
    p[tokLoop]       = &parseLoop;
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
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
ParserDefinition* buildParserDefinitions(LanguageDefinition* langDef, Arena* a) {
    ParserDefinition* result = allocateOnArena(sizeof(ParserDefinition), a);
    result->resumableTable = tabulateResumableDispatch(a);    
    result->nonResumableTable = tabulateNonresumableDispatch(a);
    result->operators = langDef->operators;
    return result;
}


void importEntities(Arr(EntityImport) impts, Int countBindings, Compiler* pr) {
    for (int i = 0; i < countBindings; i++) {
        Int mbNameId = getStringStore(pr->text->content, impts[i].name, pr->stringTable, pr->stringStore);
        if (mbNameId > -1) {           
            Int newBindingId = importEntity(mbNameId, impts[i].entity, pr);
        }
    }    
}


void importOverloads(Arr(OverloadImport) impts, Int countImports, Compiler* pr) {
    for (int i = 0; i < countImports; i++) {        
        Int mbNameId = getStringStore(pr->text->content, impts[i].name, pr->stringTable, pr->stringStore);
        if (mbNameId > -1) {
            encounterFnDefinition(mbNameId, pr);
        }
    }
}

/* Entities and overloads for the built-in operators, types and functions. */
void importBuiltins(LanguageDefinition* langDef, Compiler* pr) {
    Arr(String*) baseTypes = allocateOnArena(5*sizeof(String*), pr->aTmp);
    baseTypes[tokInt] = str("Int", pr->aTmp);
    baseTypes[tokLong] = str("Long", pr->aTmp);
    baseTypes[tokFloat] = str("Float", pr->aTmp);
    baseTypes[tokString] = str("String", pr->aTmp);
    baseTypes[tokBool] = str("Bool", pr->aTmp);
    for (int i = 0; i < 5; i++) {
        push(0, pr->types);
        Int typeNameId = getStringStore(pr->text->content, baseTypes[i], pr->stringTable, pr->stringStore);
        if (typeNameId > -1) {
            pr->activeBindings[typeNameId] = i;
        }
    }
    
    EntityImport builtins[2] =  {
        (EntityImport) { .name = str("math-pi", pr->a), .entity = (Entity){.typeId = tokFloat} },
        (EntityImport) { .name = str("math-e", pr->a),  .entity = (Entity){.typeId = tokFloat} }
    };    
    for (int i = 0; i < countOperators; i++) {
        pr->overloadCounts[i] = (*langDef->operators)[i].overs;
    }
    pr->overlCNext = countOperators;
    importEntities(builtins, sizeof(builtins)/sizeof(EntityImport), pr);
}


Compiler* createCompiler(Lexer* lx, Arena* a) {
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
        .text = lx->inp, .inp = lx, .inpLength = lx->totalTokens, .parDef = buildParserDefinitions(lx->langDef, a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aTmp), .i = 0,
        
        .nodes = allocateOnArena(sizeof(Node)*initNodeCap, a), .nextInd = 0, .capacity = initNodeCap,
        
        .entities = allocateOnArena(sizeof(Entity)*64, a), .entNext = 0, .entCap = 64,        
        .overloadCounts = allocateOnArena(4*(countOperators + 10), aTmp),
        .overlCNext = 0, .overlCCap = countOperators + 10,        
        .activeBindings = allocateOnArena(4*stringTableLength, a),
        
        .types = createStackint32_t(64, a), .typeNext = 0, .typeCap = 64,
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

/** Parses top-level types but not functions and adds their bindings to the scope */
private void parseToplevelTypes(Lexer* lr, Compiler* pr) {
}

/** Parses top-level constants but not functions, and adds their bindings to the scope */
private void parseToplevelConstants(Lexer* lx, Compiler* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokAssignment) {
            parseUpTo(pr->i + tok.pl2, lx->tokens, pr);
        } else {
            pr->i += (tok.pl2 + 1);
        }
    }    
}

/** Parses top-level function names and adds their bindings to the scope */
private void parseToplevelFunctionNames(Lexer* lx, Compiler* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.pl2;
            VALIDATEP(lenTokens >= 3, errorFnNameAndParams)
            
            Token fnName = lx->tokens[(pr->i) + 2]; // + 2 because we skip over the "fn" and "stmt" span tokens
            VALIDATEP(fnName.tp == tokWord, errorFnNameAndParams)
            
            encounterFnDefinition(fnName.pl2, pr);
        }
        pr->i += (tok.pl2 + 1);        
    } 
}

/** 
 * Parses a top-level function signature.
 * The result is [FnDef EntityName Scope EntityParam1 EntityParam2 ... ]
 */
private void parseFnSignature(Token fnDef, Lexer* lx, Compiler* pr) {
    Int fnSentinel = pr->i + fnDef.pl2 - 1;
    Int byteSentinel = fnDef.startBt + fnDef.lenBts;
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodFnDef, .startNodeInd = pr->nextInd, 
        .sentinelToken = fnSentinel };
    Token fnName = lx->tokens[pr->i];
    pr->i++; // CONSUME the function name token

    // the function's return type, it's optional
    if (lx->tokens[pr->i].tp == tokTypeName) {
        Token fnReturnType = lx->tokens[pr->i];

        VALIDATEP(pr->activeBindings[fnReturnType.pl2] > -1, errorUnknownType)
        
        pr->i++; // CONSUME the function return type token
    }

    // the fnDef scope & node
    push(newParseFrame, pr->backtrack);
    encounterFnDefinition(fnName.pl2, pr);
    addNode((Node){.tp = nodFnDef, .pl1 = pr->activeBindings[fnName.pl2],
                        .startBt = fnDef.startBt, .lenBts = fnDef.lenBts} , pr);

    // the scope for the function body
    VALIDATEP(lx->tokens[pr->i].tp == tokParens, errorFnNameAndParams)
    push(((ParseFrame){ .tp = nodScope, .startNodeInd = pr->nextInd, 
        .sentinelToken = fnSentinel }), pr->backtrack);        
    pushScope(pr->scopeStack);
    Token parens = lx->tokens[pr->i];
    addNode((Node){ .tp = nodScope, 
                    .startBt = parens.startBt, .lenBts = byteSentinel - parens.startBt }, pr);           
    
    Int paramsSentinel = pr->i + parens.pl2 + 1;
    pr->i++; // CONSUME the parens token for the param list            
    
    while (pr->i < paramsSentinel) {
        Token paramName = lx->tokens[pr->i];
        VALIDATEP(paramName.tp == tokWord, errorFnNameAndParams)
        Int newBindingId = createBinding(paramName, (Entity){.nameId = paramName.pl2}, pr);
        Node paramNode = (Node){.tp = nodBinding, .pl1 = newBindingId, 
                                .startBt = paramName.startBt, .lenBts = paramName.lenBts };
        pr->i++; // CONSUME a param name
        
        VALIDATEP(pr->i < paramsSentinel, errorFnNameAndParams)
        Token paramType = lx->tokens[pr->i];
        VALIDATEP(paramType.tp == tokTypeName, errorFnNameAndParams)
        
        Int typeBindingId = pr->activeBindings[paramType.pl2]; // the binding of this parameter's type
        VALIDATEP(typeBindingId > -1, errorUnknownType)
        
        pr->entities[newBindingId].typeId = typeBindingId;
        pr->i++; // CONSUME the param's type name
        
        addNode(paramNode, pr);        
    }
    
    VALIDATEP(pr->i < fnSentinel && lx->tokens[pr->i].tp >= firstPunctuationTokenType, errorFnMissingBody)
}

/** Parses top-level function params and bodies */
private void parseFunctionBodies(Lexer* lx, Compiler* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.pl2;
            Int sentinelToken = pr->i + lenTokens + 1;
            VALIDATEP(lenTokens >= 2, errorFnNameAndParams)
            
            pr->i += 2; // CONSUME the function def token and the stmt token
            parseFnSignature(tok, lx, pr);
            parseUpTo(sentinelToken, lx->tokens, pr);
            pr->i = sentinelToken;
        } else {            
            pr->i += (tok.pl2 + 1);    // CONSUME the whole non-function span
        }
    }
}


Compiler* parseWithCompiler(Lexer* lx, Compiler* pr, Arena* a) {
    ParserDefinition* pDef = pr->parDef;
    int inpLength = lx->totalTokens;
    int i = 0;

    ParserFunc (*dispatch)[countSyntaxForms] = pDef->nonResumableTable;
    ResumeFunc (*dispatchResumable)[countResumableForms] = pDef->resumableTable;
    if (setjmp(excBuf) == 0) {
        parseToplevelTypes(lx, pr);
        parseToplevelConstants(lx, pr);
        parseToplevelFunctionNames(lx, pr);
        parseFunctionBodies(lx, pr);
    }
    return pr;
}

/** Parses a single file in 4 passes, see docs/parser.txt */
Compiler* parse(Lexer* lx, Arena* a) {
    Compiler* pr = createCompiler(lx, a);
    return parseWithCompiler(lx, pr, a);
}

//}}}


//{{{ Typer

/** Allocates the table with overloads and changes the overloadCounts table to contain indices into the new table (not counts) */
Arr(Int) createOverloads(Compiler* pr) {
    Int neededCount = 0;
    for (Int i = 0; i < pr->overlCNext; i++) {
        neededCount += (2*pr->overloadCounts[i] + 1); // a typeId and an entityId for each overload, plus a length field for the list
    }
    print("total number of overloads needed: %d", neededCount);
    Arr(Int) overloads = allocateOnArena(neededCount*4, pr->a);

    Int j = 0;
    for (Int i = 0; i < pr->overlCNext; i++) {
        overloads[j] = pr->overloadCounts[i]; // length of the overload list (which will be filled during type check/resolution)
        pr->overloadCounts[i] = j;
        j += (2*pr->overloadCounts[i] + 1);
    }
    return overloads;
}


/** Function types are stored as: (length, return type, paramType1, paramType2, ...) */
void addFunctionType(Int arity, Arr(Int) paramsAndReturn, Compiler* pr) {
    Int neededLen = pr->typeNext + arity + 2;
    while (pr->typeCap < neededLen) {
        StackInt* newTypes = createStackint32_t(pr->typeCap*2*4, pr->a);
        memcpy(newTypes, pr->types, pr->typeNext);
        pr->types = newTypes;
        pr->typeCap *= 2;
    }
    (*pr->types->content)[pr->typeNext] = arity + 1;
    memcpy(pr->types + pr->typeNext + 1, paramsAndReturn, arity + 1);
    pr->typeNext += (arity + 2);
}


/** Shifts elements from start and until the end to the left.
 * E.g. the call with args (5, 3) takes the stack from [x x x x x 1 2 3] to [x x 1 2 3]
 */
void shiftTypeStackLeft(Int startInd, Int byHowMany, Compiler* cm) {
    cm->expStack->length -= byHowMany;
}

private void printExpSt(Compiler *cm) {
    for (int i = 0; i < cm->expStack->length; i++) {
        printf("%d ", (*cm->expStack->content)[i]);
    }
}

/** Typechecks and type-resolves a single expression */
Int typeCheckResolveExpr(Int indExpr, Compiler* cm) {
    Node expr = cm->nodes[indExpr];
    Int sentinelNode = indExpr + expr.pl2 + 1;
    Int currAhead = 0; // how many elements ahead we are compared to the token array
    StackInt* st = cm->expStack;
    // populate the stack with types, known and unknown
    for (int i = indExpr + 1; i < sentinelNode; ++i) {
        Node nd = cm->nodes[i];
        if (nd.tp <= nodString) {
            push((Int)nd.tp, st);
        } else if (nd.tp == nodCall) {
            push(BIG + nd.pl2, st); // signifies that it's a call, and its arity
            push(pr->overloadCounts[nd.pl1], st); // this is not the count of overloads anymore, but an index into the overloads
            currAhead++;
        } else if (nd.pl1 > -1) { // bindingId            
            push(pr->entities[nd.pl1].typeId, st);
        } else { // overloadId
            push(nd.pl1, st); // overloadId            
        }
    }
    print("expStack len is %d", st->length)

    // now go from back to front, resolving the calls, typechecking & collapsing args, and replacing calls with their return types
    Int j = expr.pl2 + currAhead - 1;
    Arr(Int) cont = *st->content;
    while (j > -1) {        
        if (cont[j] >= BIG) { // a function call. cont[j] contains the arity, cont[j + 1] the index into overloads table
            Int arity = cont[j] - BIG;
            Int o = cont[j + 1]; // index into the table of overloads
            Int overlCount = pr->overloadCounts[o];
            if (arity == 0) {
                VALIDATEP(overlCount == 1, errorTypeZeroArityOverload)
                Int functionTypeInd = pr->overloadCounts[o + 1];
                Int typeLength = (*pr->types->content)[functionTypeInd];
                if (typeLength == 1) { // the function returns something
                    cont[j] = (*pr->types->content)[functionTypeInd + 1]; // write the return type
                } else {
                    shiftTypeStackLeft(j + 1, 1, cm); // the function returns nothing, so there's no return type to write
                }
                --j;
            } else {
                Int typeLastArg = cont[j + arity + 1]; // + 1 for the element with the overloadId of the func
                VALIDATEP(typeLastArg > -1, errorTypeUnknownLastArg)
                Int ov = o + overlCount;
                while (ov > o && pr->overloads[ov] != typeLastArg) {
                    --ov;
                }
                VALIDATEP(ov > o, errorTypeNoMatchingOverload)
                
                Int typeOfFunc = pr->overloads[ov];
                VALIDATEP(pr->types[typeOfFunc] - 1 == arity, errorTypeNoMatchingOverload) // last param matches, but not arity

                // We know the type of the function, now to validate arg types against param types
                for (int k = j + arity; k > j + 1; k--) { // not "j + arity + 1", because we've already checked the last param
                    if (cont[k] > -1) {
                        VALIDATEP(cont[k] == pr->types[ov + k - j], errorTypeWrongArgumentType)
                    } else {
                        Int argBindingId = pr->nodes[indExpr + k - currAhead].pl1;
                        print("argBindingId %d", argBindingId)
                        pr->bindings[argBindingId].typeId = pr->types[ov + k - j];
                    }
                }
                cont[j] = pr->types[ov + 1]; // the function return type
                shiftTypeStackLeft(j + arity + 2, arity + 1);
                j -= 2;
            }
                    } else {
            j--;
        }
    }
}

//}}}
