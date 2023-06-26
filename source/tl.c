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

#define DEFINE_STACK(T)                                                                  \
    Stack##T * createStack##T (int initCapacity, Arena* a) {                             \
        int capacity = initCapacity < 4 ? 4 : initCapacity;                              \
        Stack##T * result = allocateOnArena(sizeof(Stack##T), a);                        \
        result->capacity = capacity;                                                     \
        result->length = 0;                                                              \
        result->arena = a;                                                               \
        T* arr = allocateOnArena(capacity*sizeof(T), a);                                 \
        result->content = arr;                                                           \
        return result;                                                                   \
    }                                                                                    \
    bool hasValues ## T (Stack ## T * st) {                                              \
        return st->length > 0;                                                           \
    }                                                                                    \
    T pop##T (Stack ## T * st) {                                                         \
        st->length--;                                                                    \
        return st->content[st->length];                                                  \
    }                                                                                    \
    T peek##T(Stack##T * st) {                                                           \
        return st->content[st->length - 1];                                              \
    }                                                                                    \
    void push##T (T newItem, Stack ## T * st) {                                          \
        if (st->length < st->capacity) {                                                 \
            memcpy((T*)(st->content) + (st->length), &newItem, sizeof(T));               \
        } else {                                                                         \
            T* newContent = allocateOnArena(2*(st->capacity)*sizeof(T), st->arena); \
            memcpy(newContent, st->content, st->length*sizeof(T));                       \
            memcpy((T*)(newContent) + (st->length), &newItem, sizeof(T));                \
            st->capacity *= 2;                                                           \
            st->content = newContent;                                                  \
        }                                                                                \
        st->length++;                                                                    \
    }                                                                                    \
    void clear##T (Stack##T * st) {                                                      \
        st->length = 0;                                                                  \
    }                                             
DEFINE_STACK(int32_t)
DEFINE_STACK(BtToken)
DEFINE_STACK(ParseFrame)
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

private void handleFullChunkParser(Compiler* cm);
private void setSpanLengthParser(Int, Compiler* cm);
#define handleFullChunk(X) _Generic((X), Lexer*: handleFullChunkLexer, Compiler*: handleFullChunkParser)(X)
#define setSpanLength(A, X) _Generic((X), Lexer*: setSpanLengthLexer, Compiler*: setSpanLengthParser)(A, X)

//}}}

//{{{ Int Hashmap

private IntMap* createIntMap(int initSize, Arena* a) {
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


private void addIntMap(int key, int value, IntMap* hm) {
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


private bool hasKeyIntMap(int key, IntMap* hm) {
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
                && equalStringRefs(text, stringTable->content[stringValues[i].indString], startBt, lenBts)) { // key already present                
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

#define errorInternalInconsistentSpans 1 // Inconsistent span length / structure of token scopes!;

//}}}
//{{{ Syntax errors
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
const char errorPunctuationOnlyInMultiline[] = "The dot separator is only allowed in multi-line syntax forms like (: )";
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
const char errorDocComment[]                 = "Doc comments must have the syntax: (*comment)";
const char errorBareAtom[]                   = "Malformed token stream (atoms and parentheses must not be bare)";
const char errorImportsNonUnique[]           = "Import names must be unique!";
const char errorLengthOverflow[]             = "AST nodes length overflow";
const char errorPrematureEndOfTokens[]       = "Premature end of input";
const char errorUnexpectedToken[]            = "Unexpected token";
const char errorCoreFormTooShort[]           = "Core syntax form too short";
const char errorCoreFormUnexpected[]         = "Unexpected core form";
const char errorCoreFormAssignment[]         = "A core form may not contain any assignments!";
const char errorCoreFormInappropriate[]      = "Inappropriate reserved word!";
const char errorIfLeft[]                     = "A left-hand clause in an if can only contain variables, boolean literals and expressions!";
const char errorIfRight[]                    = "A right-hand clause in an if can only contain atoms, expressions, scopes and some core forms!";
const char errorIfEmpty[]                    = "Empty `if` expression";
const char errorIfMalformed[]                = "Malformed `if` expression, should look like (if pred => `true case` else `default`)";
const char errorFnNameAndParams[]            = "Function signature must look like this: `(-f fnName ReturnType(x Type1 y Type2). body...)`";
const char errorFnMissingBody[]              = "Function definition must contain a body which must be a Scope immediately following its parameter list!";
const char errorLoopSyntaxError[]            = "A loop should look like `(.loop (< x 101) (x 0). loopBody)`";
const char errorLoopHeader[]                 = "A loop header should contain 1 or 2 items: the condition and, optionally, the var declarations";
const char errorLoopEmptyBody[]              = "Empty loop body!";
const char errorBreakContinueTooComplex[]    = "This statement is too complex! Continues and breaks may contain one thing only: the number of enclosing loops to continue/break!";
const char errorBreakContinueInvalidDepth[]  = "Invalid depth of break/continue! It must be a positive 32-bit integer!"; 
const char errorDuplicateFunction[]          = "Duplicate function declaration: a function with same name and arity already exists in this scope!";
const char errorExpressionInfixNotSecond[]   = "An infix expression must have the infix operator in second position (not counting possible prefix operators)!";
const char errorExpressionError[]            = "Cannot parse expression!";
const char errorExpressionCannotContain[]     = "Expressions cannot contain scopes or statements!";
const char errorExpressionFunctionless[]      = "Functionless expression!";
const char errorTypeDeclCannotContain[]       = "Type declarations may only contain types (like Int), type params (like A), type constructors (like List) and parentheses!";
const char errorTypeDeclError[]               = "Cannot parse type declaration!";
const char errorUnknownType[]                 = "Unknown type";
const char errorUnknownTypeFunction[]         = "Unknown type constructor";
const char errorOperatorWrongArity[]          = "Wrong number of arguments for operator!";
const char errorUnknownBinding[]              = "Unknown binding!";
const char errorUnknownFunction[]             = "Unknown function!";
const char errorOperatorUsedInappropriately[] = "Operator used in an inappropriate location!";
const char errorAssignment[]                  = "Cannot parse assignment, it must look like `freshIdentifier` = `expression`";
const char errorAssignmentShadowing[]         = "Assignment error: existing identifier is being shadowed";
const char errorReturn[]                      = "Cannot parse return statement, it must look like `return ` {expression}";
const char errorScope[]                       = "A scope may consist only of expressions, assignments, function definitions and other scopes!";
const char errorLoopBreakOutside[]            = "The break keyword can only be used outside a loop scope!";
const char errorTemp[]                        = "Temporary, delete it when finished";

//}}}

//{{{ Type errors

const char errorTypeUnknownLastArg[]          = "The type of last argument to a call must be known, otherwise I can't resolve the function overload!";
const char errorTypeZeroArityOverload[]       = "A function with no parameters cannot be overloaded.";
const char errorTypeNoMatchingOverload[]      = "No matching function overload was found";
const char errorTypeWrongArgumentType[]       = "Wrong argument type";

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
static const byte reservedBytesLoop[]        = { 108, 111, 111, 112 };
static const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
static const byte reservedBytesMut[]         = { 109, 117, 116 }; // replace with "immut" or "val" for struct fields
static const byte reservedBytesOr[]          = { 111, 114 };
static const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
static const byte reservedBytesStruct[]      = { 115, 116, 114, 117, 99, 116 };
static const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
static const byte reservedBytesTry[]         = { 116, 114, 121 };
static const byte reservedBytesYield[]       = { 121, 105, 101, 108, 100 };

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
#ifdef TRACE    
    printf("Error on i = %d: %s\n", cm->i, errMsg);
#endif    
    cm->errMsg = stringOfInt(errInd, cm->a);
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
    VALIDATEL(lx->i + requiredSymbols <= lx->inpLength, errorPrematureEndOfInput)
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
        
        BtToken grandparent = lx->backtrack->content[lx->backtrack->length - 2];
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
    VALIDATEL(j != lx->inpLength, errorPrematureEndOfInput)
    add((Token){.tp=tokString, .startBt=(lx->i), .lenBts=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


private void lexUnexpectedSymbol(Lexer* lx, Arr(byte) inp) {
    throwExcLexer(errorUnrecognizedByte, lx);
}

private void lexNonAsciiError(Lexer* lx, Arr(byte) inp) {
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
    p[13] = (OpDef){.name=s(","),    .arity=3, .bytes={aComma, 0, 0, 0}};    
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



/*
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
testable LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    result->possiblyReservedDispatch = tabulateReservedBytes(a);
    result->dispatchTable = tabulateDispatch(a);
    result->operators = tabulateOperators(a);
    result->reservedParensOrNot = tabulateReserved(a);
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
    VALIDATEL(top.spanLevel != slScope && !hasValues(lx->backtrack), errorPunctuationExtraOpening)

    setStmtSpanLength(top.tokenInd, lx);    
    deleteArena(lx->aTmp);
}


testable Lexer* lexicallyAnalyze(String* input, LanguageDefinition* langDef, Arena* a) {
    print("lexically analyze")
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


private ScopeStack* createScopeStack() {    
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
private void pushScope(ScopeStack* scopeStack) {
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
            Int bindingOrOverload = cm->activeBindings[*(topScope->bindings + i)];
            if (bindingOrOverload > -1) {
                cm->activeBindings[*(topScope->bindings + i)] = -1;
            } else {
                --cm->overloadCounts[-bindingOrOverload - 2];
            }            
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


#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser(errMsg, cm); }
#define BIG 70000000

_Noreturn private void throwExcParser(const char errMsg[], Compiler* cm) {   
    cm->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d\n", cm->i);
#endif    
    cm->errMsg = str(errMsg, cm->a);
    longjmp(excBuf, 1);
}


// Forward declarations
private bool parseLiteralOrIdentifier(Token tok, Compiler* cm);


/** Validates a new binding (that it is unique), creates an entity for it, and adds it to the current scope */
private Int createBinding(Token bindingToken, Entity b, Compiler* cm) {
    VALIDATEP(bindingToken.tp == tokWord, errorAssignment)

    Int nameId = bindingToken.pl2;
    Int mbBinding = cm->activeBindings[nameId];
    VALIDATEP(mbBinding == -1, errorAssignmentShadowing)
    
    cm->entities[cm->entNext] = b;
    Int newBindingId = cm->entNext;
    cm->entNext++;
    if (cm->entNext == cm->entCap) {
        Arr(Entity) newBindings = allocateOnArena(2*cm->entCap*sizeof(Entity), cm->a);
        memcpy(newBindings, cm->entities, cm->entCap*sizeof(Entity));
        cm->entities = newBindings;
        cm->entCap *= 2;
    }    
    
    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (cm->scopeStack->length > 0) {
            addBinding(nameId, newBindingId, cm); // adds it to the ScopeStack
        }
        cm->activeBindings[nameId] = newBindingId; // makes it active
    }
    
    return newBindingId;
}

/** Processes the name of a defined function. Creates an overload counter, or increments it if it exists. Consumes no tokens. */
private void encounterFnDefinition(Int nameId, Compiler* cm) {    
    Int activeValue = (nameId > -1) ? cm->activeBindings[nameId] : -1;
    VALIDATEP(activeValue < 0, errorAssignmentShadowing);
    if (activeValue == -1) { // this is the first-registered overload of this function
        cm->overloadCounts[cm->overlCNext] = SIXTEENPLUSONE;
        cm->activeBindings[nameId] = -cm->overlCNext - 2;
        cm->overlCNext++;
        if (cm->overlCNext == cm->overlCCap) {
            Arr(Int) newOverloadCounts = allocateOnArena(2*cm->overlCCap*4, cm->a);
            memcpy(newOverloadCounts, cm->overloadCounts, cm->overlCCap*4);
            cm->overloadCounts = newOverloadCounts;
            cm->overlCCap *= 2;
        }
    } else { // other overloads have already been registered, so just increment the counts
        cm->overloadCounts[-activeValue - 2] += SIXTEENPLUSONE;
    }
}


private Int importEntity(Int nameId, Entity ent, Compiler* cm) {
    Int mbBinding = cm->activeBindings[nameId];
    Int typeLength = cm->types.content[ent.typeId];
    VALIDATEP(mbBinding == -1 || typeLength > 0, errorAssignmentShadowing) // either the name unique, or it's a function
    
    cm->entities[cm->entNext] = ent;
    Int newEntityId = cm->entNext;
    cm->entNext++;
    if (cm->entNext == cm->entCap) {
        Arr(Entity) newBindings = allocateOnArena(2*cm->entCap*sizeof(Entity), cm->a);
        memcpy(newBindings, cm->entities, cm->entCap*sizeof(Entity));
        cm->entities = newBindings;
        cm->entCap *= 2;
    }    

    if (typeLength == 0) { // not a function
        if (cm->scopeStack->length > 0) {
            // adds it to the ScopeStack so it will be cleaned up when the scope is over
            addBinding(nameId, newEntityId, cm); 
        }
        cm->activeBindings[nameId] = newEntityId; // makes it active
    } else {
        encounterFnDefinition(nameId, cm);
    }
           
    return newEntityId;
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunkParser(Compiler* cm) {
    Node* newStorage = allocateOnArena(cm->capacity*2*sizeof(Node), cm->a);
    memcpy(newStorage, cm->nodes, (cm->capacity)*(sizeof(Node)));
    cm->nodes = newStorage;
    cm->capacity *= 2;
}


testable void addNode(Node n, Compiler* cm) {
    cm->nodes[cm->nextInd] = n;
    cm->nextInd++;
    if (cm->nextInd == cm->capacity) {
        handleFullChunk(cm);
    }
}

/**
 * Finds the top-level punctuation opener by its index, and sets its node length.
 * Called when the parsing of a span is finished.
 */
private void setSpanLengthParser(Int nodeInd, Compiler* cm) {
    cm->nodes[nodeInd].pl2 = cm->nextInd - nodeInd - 1;
}


private void parseVerbatim(Token tok, Compiler* cm) {
    addNode((Node){.tp = tok.tp, .startBt = tok.startBt, .lenBts = tok.lenBts, 
        .pl1 = tok.pl1, .pl2 = tok.pl2}, cm);
}

private void parseErrorBareAtom(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
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
 * A single-item subexpression, like "(foo)" or "(!)". Consumes no tokens.
 */
private void exprSingleItem(Token theTok, Compiler* cm) {
    if (theTok.tp == tokWord) {
        Int mbOverload = cm->activeBindings[theTok.pl2];
        VALIDATEP(mbOverload != -1, errorUnknownFunction)
        addNode((Node){.tp = nodCall, .pl1 = mbOverload, .pl2 = 0,
                       .startBt = theTok.startBt, .lenBts = theTok.lenBts}, cm);        
    } else if (theTok.tp == tokOperator) {
        Int operBindingId = theTok.pl1;
        OpDef operDefinition = (*cm->parDef->operators)[operBindingId];
        if (operDefinition.arity == 1) {
            addNode((Node){ .tp = nodId, .pl1 = operBindingId,
                .startBt = theTok.startBt, .lenBts = theTok.lenBts}, cm);
        } else {
            throwExcParser(errorUnexpectedToken, cm);
        }
    } else if (theTok.tp <= topVerbatimTokenVariant) {
        addNode((Node){.tp = theTok.tp, .pl1 = theTok.pl1, .pl2 = theTok.pl2,
                        .startBt = theTok.startBt, .lenBts = theTok.lenBts }, cm);
    } else VALIDATEP(theTok.tp < firstCoreFormTokenType, errorCoreFormInappropriate)
    else {
        throwExcParser(errorUnexpectedToken, cm);
    }
}

/** Counts the arity of the call, including skipping unary operators. Consumes no tokens. */
private void exprCountArity(Int* arity, Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
    Int j = cm->i;
    Token firstTok = tokens[j];

    j = calcSentinel(firstTok, j);
    while (j < sentinelToken) {
        Token tok = tokens[j];
        if (tok.tp < firstPunctuationTokenType) {
            j++;
        } else {
            j += tok.pl2 + 1;
        }
        if (tok.tp != tokOperator || (*cm->parDef->operators)[tok.pl1].arity > 1) {            
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
private void exprSubexpr(Token parenTok, Int* arity, Arr(Token) tokens, Compiler* cm) {
    Token firstTok = tokens[cm->i];    
    
    if (parenTok.pl2 == 1) {
        exprSingleItem(tokens[cm->i], cm);
        *arity = 0;
        cm->i++; // CONSUME the single item within parens
    } else {
        exprCountArity(arity, cm->i + parenTok.pl2, tokens, cm);
        
        if (firstTok.tp == tokWord || firstTok.tp == tokOperator) {
            Int mbBindingId = -1;
            if (firstTok.tp == tokWord) {
                mbBindingId = cm->activeBindings[firstTok.pl2];
            } else if (firstTok.tp == tokOperator) {
                VALIDATEP(*arity == (*cm->parDef->operators)[firstTok.pl1].arity, errorOperatorWrongArity)
                mbBindingId = -firstTok.pl1 - 2;
            }
            
            VALIDATEP(mbBindingId < -1, errorUnknownFunction)            

            addNode((Node){.tp = nodCall, .pl1 = mbBindingId, .pl2 = *arity, // todo overload
                           .startBt = firstTok.startBt, .lenBts = firstTok.lenBts}, cm);
            *arity = 0;
            cm->i++; // CONSUME the function or operator call token
        }
    }
}

/**
 * Flushes the finished subexpr frames from the top of the funcall stack.
 * A subexpr frame is finished iff current token equals its sentinel.
 * Flushing includes appending its operators, clearing the operator stack, and appending
 * prefix unaries from the previous subexpr frame, if any.
 */
private void subexprClose(Arr(Token) tokens, Compiler* cm) {
    while (cm->scopeStack->length > 0 && cm->i == peek(cm->backtrack).sentinelToken) {
        popFrame(cm);        
    }
}

/** An operator token in non-initial position is either a funcall (if unary) or an operand. Consumes no tokens. */
private void exprOperator(Token tok, ScopeStackFrame* topSubexpr, Arr(Token) tokens, Compiler* cm) {
    Int bindingId = tok.pl1;
    OpDef operDefinition = (*cm->parDef->operators)[bindingId];

    if (operDefinition.arity == 1) {
        addNode((Node){ .tp = nodCall, .pl1 = -bindingId - 2, .pl2 = 1,
                        .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    } else {
        addNode((Node){ .tp = nodId, .pl1 = -bindingId - 2, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    }
}

/** Parses an expression. Precondition: we are 1 past the span token */
private void parseExpr(Token exprTok, Arr(Token) tokens, Compiler* cm) {
    Int sentinelToken = cm->i + exprTok.pl2;
    Int arity = 0;
    if (exprTok.pl2 == 1) {
        exprSingleItem(tokens[cm->i], cm);
        cm->i++; // CONSUME the single item within parens
        return;
    }

    push(((ParseFrame){.tp = nodExpr, .startNodeInd = cm->nextInd, .sentinelToken = cm->i + exprTok.pl2 }), 
         cm->backtrack);        
    addNode((Node){ .tp = nodExpr, .startBt = exprTok.startBt, .lenBts = exprTok.lenBts }, cm);

    exprSubexpr(exprTok, &arity, tokens, cm);
    while (cm->i < sentinelToken) {
        subexprClose(tokens, cm);
        Token currTok = tokens[cm->i];
        untt tokType = currTok.tp;
        
        ScopeStackFrame* topSubexpr = cm->scopeStack->topScope;
        if (tokType == tokParens) {
            cm->i++; // CONSUME the parens token
            exprSubexpr(currTok, &arity, tokens, cm);
        } else VALIDATEP(tokType < firstPunctuationTokenType, errorExpressionCannotContain)
        else if (tokType <= topVerbatimTokenVariant) {
            addNode((Node){ .tp = currTok.tp, .pl1 = currTok.pl1, .pl2 = currTok.pl2,
                            .startBt = currTok.startBt, .lenBts = currTok.lenBts }, cm);
            cm->i++; // CONSUME the verbatim token
        } else {
            if (tokType == tokWord) {
                Int mbBindingId = cm->activeBindings[currTok.pl2];
                VALIDATEP(mbBindingId != -1, errorUnknownBinding)
                addNode((Node){ .tp = nodId, .pl1 = mbBindingId, .pl2 = currTok.pl2, 
                            .startBt = currTok.startBt, .lenBts = currTok.lenBts}, cm);                
            } else if (tokType == tokOperator) {
                exprOperator(currTok, topSubexpr, tokens, cm);
            }
            cm->i++; // CONSUME any leaf token
        }
    }
    subexprClose(tokens, cm);    
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
        } else VALIDATEI(cm->i == frame.sentinelToken, errorInternalInconsistentSpans)
        popFrame(cm);
    }
}

/** Parses top-level function bodies */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Compiler* cm) {
    while (cm->i < sentinelToken) {
        Token currTok = tokens[cm->i];
        untt contextType = peek(cm->backtrack).tp;
        
        // pre-parse hooks that let contextful syntax forms (e.g. "if") detect parsing errors and maintain their state
        if (contextType >= firstResumableForm) {
            ((*cm->parDef->resumableTable)[contextType - firstResumableForm])(&currTok, tokens, cm);
        } else {
            cm->i++; // CONSUME any span token
        }
        ((*cm->parDef->nonResumableTable)[currTok.tp])(currTok, tokens, cm);
        
        maybeCloseSpans(cm);
    }    
}

/** Consumes 0 or 1 tokens. Returns false if didn't parse anything. */
private bool parseLiteralOrIdentifier(Token tok, Compiler* cm) {
    if (tok.tp <= topVerbatimTokenVariant) {
        parseVerbatim(tok, cm);
    } else if (tok.tp == tokWord) {        
        Int nameId = tok.pl2;
        Int mbBinding = cm->activeBindings[nameId];
        VALIDATEP(mbBinding != -1, errorUnknownBinding)    
        addNode((Node){.tp = nodId, .pl1 = mbBinding, .pl2 = nameId,
                       .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    } else {
        return false;        
    }
    cm->i++; // CONSUME the literal or ident token
    return true;
}


private void parseAssignment(Token tok, Arr(Token) tokens, Compiler* cm) {
    const Int rightSideLen = tok.pl2 - 1;
    VALIDATEP(rightSideLen >= 1, errorAssignment)
    
    Int sentinelToken = cm->i + tok.pl2;

    Token bindingToken = tokens[cm->i];
    Int newBindingId = createBinding(bindingToken, (Entity){ }, cm);

    push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = cm->nextInd, .sentinelToken = sentinelToken }), 
         cm->backtrack);
    addNode((Node){.tp = nodAssignment, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    
    addNode((Node){.tp = nodBinding, .pl1 = newBindingId, 
            .startBt = bindingToken.startBt, .lenBts = bindingToken.lenBts}, cm);
    
    cm->i++; // CONSUME the word token before the assignment sign
    Token rightSideToken = tokens[cm->i];
    if (rightSideToken.tp == tokScope) {
        print("scope %d", rightSideToken.tp);
        //openScope(pr);
    } else {
        if (rightSideLen == 1) {
            VALIDATEP(parseLiteralOrIdentifier(rightSideToken, cm), errorAssignment)
        } else if (rightSideToken.tp == tokIf) {
        } else {
            parseExpr((Token){ .pl2 = rightSideLen, .startBt = rightSideToken.startBt, 
                               .lenBts = tok.lenBts - rightSideToken.startBt + tok.startBt
                       }, 
                       tokens, cm);
        }
    }
}

private void parseReassignment(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

private void parseMutation(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

private void parseAlias(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

private void parseAssert(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseAssertDbg(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseAwait(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

// TODO validate we are inside at least as many loops as we are breaking out of
private void parseBreak(Token tok, Arr(Token) tokens, Compiler* cm) {
    VALIDATEP(tok.pl2 <= 1, errorBreakContinueTooComplex);
    if (tok.pl2 == 1) {
        Token nextTok = tokens[cm->i];
        VALIDATEP(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errorBreakContinueInvalidDepth)
        addNode((Node){.tp = nodBreak, .pl1 = nextTok.pl2, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
        cm->i++; // CONSUME the Int after the "break"
    } else {
        addNode((Node){.tp = nodBreak, .pl1 = 1, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    }    
}


private void parseCatch(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseContinue(Token tok, Arr(Token) tokens, Compiler* cm) {
    VALIDATEP(tok.pl2 <= 1, errorBreakContinueTooComplex);
    if (tok.pl2 == 1) {
        Token nextTok = tokens[cm->i];
        VALIDATEP(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errorBreakContinueInvalidDepth)
        addNode((Node){.tp = nodContinue, .pl1 = nextTok.pl2, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
        cm->i++; // CONSUME the Int after the "continue"
    } else {
        addNode((Node){.tp = nodContinue, .pl1 = 1, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    }    
}


private void parseDefer(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseDispose(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseExposePrivates(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseFnDef(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseInterface(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseImpl(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseLambda(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseLambda1(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseLambda2(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseLambda3(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parsePackage(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseReturn(Token tok, Arr(Token) tokens, Compiler* cm) {
    Int lenTokens = tok.pl2;
    Int sentinelToken = cm->i + lenTokens;        
    
    push(((ParseFrame){ .tp = nodReturn, .startNodeInd = cm->nextInd, .sentinelToken = sentinelToken }), 
            cm->backtrack);
    addNode((Node){.tp = nodReturn, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);
    
    Token rightSideToken = tokens[cm->i];
    if (rightSideToken.tp == tokScope) {
        print("scope %d", rightSideToken.tp);
        //openScope(pr);
    } else {        
        if (lenTokens == 1) {
            VALIDATEP(parseLiteralOrIdentifier(rightSideToken, cm), errorReturn)            
        } else {
            parseExpr((Token){ .pl2 = lenTokens, .startBt = rightSideToken.startBt, 
                               .lenBts = tok.lenBts - rightSideToken.startBt + tok.startBt
                       }, 
                       tokens, cm);
        }
    }
}


private void parseSkip(Token tok, Arr(Token) tokens, Compiler* cm) {
    
}


private void parseScope(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

private void parseStruct(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseTry(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}


private void parseYield(Token tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

/** To be called at the start of an "if" clause. It validates the grammar and emits nodes. Consumes no tokens.
 * Precondition: we are pointing at the init token of left side of "if" (i.e. at a tokStmt or the like)
 */
private void ifAddClause(Token tok, Arr(Token) tokens, Compiler* cm) {
    VALIDATEP(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errorIfLeft)
    Int leftTokSkip = (tok.tp >= firstPunctuationTokenType) ? (tok.pl2 + 1) : 1;
    Int j = cm->i + leftTokSkip;
    VALIDATEP(j + 1 < cm->inpLength, errorPrematureEndOfTokens)
    VALIDATEP(tokens[j].tp == tokArrow, errorIfMalformed)
    
    j++; // the arrow
    
    Token rightToken = tokens[j];
    Int rightTokLength = (rightToken.tp >= firstPunctuationTokenType) ? (rightToken.pl2 + 1) : 1;    
    Int clauseSentinelByte = rightToken.startBt + rightToken.lenBts;
    
    ParseFrame clauseFrame = (ParseFrame){ .tp = nodIfClause, .startNodeInd = cm->nextInd, .sentinelToken = j + rightTokLength };
    push(clauseFrame, cm->backtrack);
    addNode((Node){.tp = nodIfClause, .pl1 = leftTokSkip,
         .startBt = tok.startBt, .lenBts = clauseSentinelByte - tok.startBt }, cm);
}


private void parseIf(Token tok, Arr(Token) tokens, Compiler* cm) {
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = cm->nextInd, 
        .sentinelToken = cm->i + tok.pl2 };
    push(newParseFrame, cm->backtrack);
    addNode((Node){.tp = nodIf, .pl1 = tok.pl1, .startBt = tok.startBt, .lenBts = tok.lenBts}, cm);

    ifAddClause(tokens[cm->i], tokens, cm);
}


private void parseLoop(Token loopTok, Arr(Token) tokens, Compiler* cm) {
    Token tokenStmt = tokens[cm->i];
    Int sentinelStmt = cm->i + tokenStmt.pl2 + 1;
    VALIDATEP(tokenStmt.tp == tokStmt, errorLoopSyntaxError)    
    VALIDATEP(sentinelStmt < cm->i + loopTok.pl2, errorLoopEmptyBody)
    
    Int indLeftSide = cm->i + 1;
    Token tokLeftSide = tokens[indLeftSide]; // + 1 because cm->i points at the stmt so far

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
    
    Int sentToken = startOfScope - cm->i + loopTok.pl2;
        
    push(((ParseFrame){ .tp = nodLoop, .startNodeInd = cm->nextInd, .sentinelToken = cm->i + loopTok.pl2 }), cm->backtrack);
    addNode((Node){.tp = nodLoop, .pl1 = slScope, .startBt = loopTok.startBt, .lenBts = loopTok.lenBts}, cm);

    push(((ParseFrame){ .tp = nodScope, .startNodeInd = cm->nextInd, .sentinelToken = cm->i + loopTok.pl2 }), cm->backtrack);
    addNode((Node){.tp = nodScope, .startBt = startBtScope,
            .lenBts = loopTok.lenBts - startBtScope + loopTok.startBt}, cm);

    // variable initializations, if any
    if (indRightSide > -1) {
        cm->i = indRightSide + 1;
        while (cm->i < sentinelStmt) {
            Token binding = tokens[cm->i];
            VALIDATEP(binding.tp = tokWord, errorLoopSyntaxError)
            
            Token expr = tokens[cm->i + 1];
            
            VALIDATEP(expr.tp < firstPunctuationTokenType || expr.tp == tokParens, errorLoopSyntaxError)
            
            Int initializationSentinel = calcSentinel(expr, cm->i + 1);
            Int bindingId = createBinding(binding, ((Entity){}), cm);
            Int indBindingSpan = cm->nextInd;
            addNode((Node){.tp = nodAssignment, .pl2 = initializationSentinel - cm->i,
                           .startBt = binding.startBt,
                           .lenBts = expr.lenBts + expr.startBt - binding.startBt}, cm);
            addNode((Node){.tp = nodBinding, .pl1 = bindingId,
                           .startBt = binding.startBt, .lenBts = binding.lenBts}, cm);
                        
            if (expr.tp == tokParens) {
                cm->i += 2;
                parseExpr(expr, tokens, cm);
            } else {
                exprSingleItem(expr, cm);
            }
            setSpanLength(indBindingSpan, cm);
            
            cm->i = initializationSentinel;
        }
    }

    // loop body
    
    Token tokBody = tokens[sentinelStmt];
    if (startBtScope < 0) {
        startBtScope = tokBody.startBt;
    }    
    addNode((Node){.tp = nodLoopCond, .pl1 = slStmt, .pl2 = sentinelLeftSide - indLeftSide,
         .startBt = tokLeftSide.startBt, .lenBts = tokLeftSide.lenBts}, cm);
    cm->i = indLeftSide + 1;
    parseExpr(tokens[indLeftSide], tokens, cm);
    
    
    cm->i = sentinelStmt; // CONSUME the loop token and its first statement
}


private void resumeIf(Token* tok, Arr(Token) tokens, Compiler* cm) {
    if (tok->tp == tokElse) {        
        VALIDATEP(cm->i < cm->inpLength, errorPrematureEndOfTokens)
        cm->i++; // CONSUME the "else"
        *tok = tokens[cm->i];        

        push(((ParseFrame){ .tp = nodElse, .startNodeInd = cm->nextInd,
                           .sentinelToken = calcSentinel(*tok, cm->i)}), cm->backtrack);
        addNode((Node){.tp = nodElse, .startBt = tok->startBt, .lenBts = tok->lenBts }, cm);       
        cm->i++; // CONSUME the token after the "else"
    } else {
        ifAddClause(*tok, tokens, cm);
        cm->i++; // CONSUME the init token of the span
    }
}

private void resumeIfPr(Token* tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

private void resumeImpl(Token* tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
}

private void resumeMatch(Token* tok, Arr(Token) tokens, Compiler* cm) {
    throwExcParser(errorTemp, cm);
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
testable ParserDefinition* buildParserDefinitions(LanguageDefinition* langDef, Arena* a) {
    ParserDefinition* result = allocateOnArena(sizeof(ParserDefinition), a);
    result->resumableTable = tabulateResumableDispatch(a);    
    result->nonResumableTable = tabulateNonresumableDispatch(a);
    result->operators = langDef->operators;
    return result;
}


testable void importEntities(Arr(EntityImport) impts, Int countEntities, Compiler* cm) {
    print("import count bindings %d impts %p", countEntities, impts)
    for (int i = 0; i < countEntities; i++) {
        Int mbNameId = getStringStore(cm->text->content, impts[i].name, cm->stringTable, cm->stringStore);
        if (mbNameId > -1) {           
            Int newBindingId = importEntity(mbNameId, impts[i].entity, cm);
        }
    }
    cm->countNonparsedEntities = cm->entNext;
    print("import end")
}


testable void importOverloads(Arr(OverloadImport) impts, Int countImports, Compiler* cm) {
    for (int i = 0; i < countImports; i++) {        
        Int mbNameId = getStringStore(cm->text->content, impts[i].name, cm->stringTable, cm->stringStore);
        if (mbNameId > -1) {
            encounterFnDefinition(mbNameId, cm);
        }
    }
}

/** Function types are stored as: (length, return type, paramType1, paramType2, ...) */
testable void addFunctionType(Int arity, Arr(Int) paramsAndReturn, Compiler* cm) {
    Int neededLen = cm->types.length + arity + 2;
    while (cm->types.capacity < neededLen) {
        StackInt* newTypes = createStackint32_t(cm->types.capacity*2*4, cm->a);
        memcpy(newTypes->content, cm->types.content, cm->types.length);
        cm->types = *newTypes;
        cm->types.capacity *= 2;
    }
    cm->types.content[cm->types.length] = arity + 1;
    memcpy(cm->types.content + cm->types.length + 1, paramsAndReturn, arity + 1);
    cm->types.capacity += (arity + 2);
}

private void buildOperator(Int operId, Int typeId, Compiler* cm) {   
    cm->entities[cm->entNext] = (Entity){.typeId = typeId, .nameId = operId};
    Int newEntityId = cm->entNext;
    cm->entNext++;
    if (cm->entNext == cm->entCap) {
        Arr(Entity) newEntities = allocateOnArena(2*cm->entCap*sizeof(Entity), cm->a);
        memcpy(newEntities, cm->entities, cm->entCap*sizeof(Entity));
        cm->entities = newEntities;
        cm->entCap *= 2;
    }    

    cm->overloadCounts[-operId - 2] += SIXTEENPLUSONE;    
}

/** Operators are the first-ever functions to be defined. This function builds their types, entities and overload counts. */
private void buildInOperators(Compiler* cm) {
    Int boolOfIntInt = cm->types.length;
    addFunctionType(2, (Int[]){tokBool, tokInt, tokInt}, cm);
    Int boolOfIntIntInt = cm->types.length;
    addFunctionType(3, (Int[]){tokBool, tokInt, tokInt, tokInt}, cm);    
    Int boolOfFlFl = cm->types.length;
    addFunctionType(2, (Int[]){tokBool, tokFloat, tokFloat}, cm);
    Int boolOfFlFlFl = cm->types.length;
    addFunctionType(3, (Int[]){tokBool, tokFloat, tokFloat, tokFloat}, cm);
    Int boolOfStrStr = cm->types.length;
    addFunctionType(2, (Int[]){tokBool, tokString, tokString}, cm);
    Int boolOfBool = cm->types.length;
    addFunctionType(1, (Int[]){tokBool, tokBool}, cm);
    Int boolOfBoolBool = cm->types.length;
    addFunctionType(2, (Int[]){tokBool, tokBool, tokBool}, cm);
    Int intOfStr = cm->types.length;
    addFunctionType(1, (Int[]){tokInt, tokString}, cm);
    Int intOfInt = cm->types.length;
    addFunctionType(1, (Int[]){tokInt, tokInt}, cm);
    Int intOfFl = cm->types.length;
    addFunctionType(1, (Int[]){tokInt, tokFloat}, cm);
    Int intOfIntInt = cm->types.length;
    addFunctionType(2, (Int[]){tokInt, tokInt, tokInt}, cm);
    Int intOfFlFl = cm->types.length;
    addFunctionType(2, (Int[]){tokInt, tokFloat, tokFloat}, cm);
    Int intOfStrStr = cm->types.length;
    addFunctionType(2, (Int[]){tokInt, tokString, tokString}, cm);
    Int strOfInt = cm->types.length;
    addFunctionType(1, (Int[]){tokString, tokInt}, cm);
    Int strOfFloat = cm->types.length;
    addFunctionType(1, (Int[]){tokString, tokFloat}, cm);
    Int strOfBool = cm->types.length;
    addFunctionType(1, (Int[]){tokString, tokBool}, cm);
    Int strOfStrStr = cm->types.length;
    addFunctionType(2, (Int[]){tokString, tokString, tokString}, cm);    
    Int flOfFlFl = cm->types.length;
    addFunctionType(2, (Int[]){tokFloat, tokFloat, tokFloat}, cm);
    Int flOfInt = cm->types.length;
    addFunctionType(1, (Int[]){tokFloat, tokInt}, cm);
    Int flOfFl = cm->types.length;
    addFunctionType(1, (Int[]){tokFloat, tokFloat}, cm);

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
    buildOperator(opTBinaryAnd, boolOfBoolBool, cm);
    buildOperator(opTTimes, intOfIntInt, cm);
    buildOperator(opTTimes, flOfFlFl, cm);
    buildOperator(opTIncrement, intOfInt, cm);
    buildOperator(opTPlus, intOfIntInt, cm);    
    buildOperator(opTPlus, flOfFlFl, cm);
    buildOperator(opTToFloat, flOfInt, cm);
    buildOperator(opTDecrement, intOfInt, cm);
    buildOperator(opTMinus, intOfIntInt, cm);
    buildOperator(opTMinus, flOfFlFl, cm);
    buildOperator(opTDivBy, intOfIntInt, cm);
    buildOperator(opTDivBy, flOfFlFl, cm);
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
    buildOperator(opTExponent, intOfIntInt, cm);
    buildOperator(opTExponent, flOfFlFl, cm);
    buildOperator(opTNegation, intOfInt, cm);
    buildOperator(opTNegation, flOfFl, cm);
    cm->overlCNext = countOperators;
    cm->countOperatorEntities = cm->entNext;
}



#define opTRemainder         4 // %
#define opTBinaryAnd         5 // && bitwise and and non-short circuiting and
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

/* Entities and overloads for the built-in operators, types and functions. */
private void importBuiltins(LanguageDefinition* langDef, Compiler* cm) {
    Arr(String*) baseTypes = allocateOnArena(5*sizeof(String*), cm->aTmp);
    baseTypes[tokInt] = str("Int", cm->aTmp);
    baseTypes[tokLong] = str("Long", cm->aTmp);
    baseTypes[tokFloat] = str("Float", cm->aTmp);
    baseTypes[tokString] = str("String", cm->aTmp);
    baseTypes[tokBool] = str("Bool", cm->aTmp);
    for (int i = 0; i < 5; i++) {
        push(0, &cm->types);
        Int typeNameId = getStringStore(cm->text->content, baseTypes[i], cm->stringTable, cm->stringStore);
        if (typeNameId > -1) {
            cm->activeBindings[typeNameId] = i;
        }
    }
    buildInOperators(cm);
    
    EntityImport builtins[2] =  {
        (EntityImport) { .name = str("math-pi", cm->a), .entity = (Entity){.typeId = tokFloat} },
        (EntityImport) { .name = str("math-e", cm->a),  .entity = (Entity){.typeId = tokFloat} }
    };    

    importEntities(builtins, sizeof(builtins)/sizeof(EntityImport), cm);
}


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
        .text = lx->inp, .inp = lx, .inpLength = lx->totalTokens, .parDef = buildParserDefinitions(lx->langDef, a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aTmp), .i = 0,
        
        .nodes = allocateOnArena(sizeof(Node)*initNodeCap, a), .nextInd = 0, .capacity = initNodeCap,
        
        .entities = allocateOnArena(sizeof(Entity)*64, a), .entNext = 0, .entCap = 64,        
        .overloadCounts = allocateOnArena(4*(countOperators + 10), aTmp),
        .overlCNext = 0, .overlCCap = countOperators + 10,        
        .activeBindings = allocateOnArena(4*stringTableLength, a),

        .overloads = (StackInt){.content = NULL, .length = 0, .capacity = 0},
        .types = *createStackint32_t(64, a),
        .expStack = createStackint32_t(16, aTmp),
        
        .stringStore = lx->stringStore, .stringTable = lx->stringTable, .strLength = stringTableLength,
        .wasError = false, .errMsg = &empty, .a = a, .aTmp = aTmp
    };
    if (stringTableLength > 0) {
        memset(result->activeBindings, 0xFF, stringTableLength*sizeof(Int)); // activeBindings is filled with -1
    }
    memset(result->overloadCounts, 0x00, (countOperators + 10)*4);
    importBuiltins(lx->langDef, result);
    return result;    
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

/** Parses top-level function names and adds their bindings to the scope */
private void parseToplevelFunctionNames(Lexer* lx, Compiler* cm) {
    cm->i = 0;
    const Int len = lx->totalTokens;
    while (cm->i < len) {
        Token tok = lx->tokens[cm->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.pl2;
            VALIDATEP(lenTokens >= 3, errorFnNameAndParams)
            
            Token fnName = lx->tokens[(cm->i) + 2]; // + 2 because we skip over the "fn" and "stmt" span tokens
            VALIDATEP(fnName.tp == tokWord, errorFnNameAndParams)
            
            encounterFnDefinition(fnName.pl2, cm);
        }
        cm->i += (tok.pl2 + 1);        
    } 
}

/** 
 * Parses a top-level function signature.
 * The result is [FnDef EntityName Scope EntityParam1 EntityParam2 ... ]
 */
private void parseFnSignature(Token fnDef, Lexer* lx, Compiler* cm) {
    Int fnSentinel = cm->i + fnDef.pl2 - 1;
    Int byteSentinel = fnDef.startBt + fnDef.lenBts;
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodFnDef, .startNodeInd = cm->nextInd, 
        .sentinelToken = fnSentinel };
    Token fnName = lx->tokens[cm->i];
    cm->i++; // CONSUME the function name token

    // the function's return type, it's optional
    if (lx->tokens[cm->i].tp == tokTypeName) {
        Token fnReturnType = lx->tokens[cm->i];

        VALIDATEP(cm->activeBindings[fnReturnType.pl2] > -1, errorUnknownType)
        
        cm->i++; // CONSUME the function return type token
    }

    // the fnDef scope & node
    push(newParseFrame, cm->backtrack);
    encounterFnDefinition(fnName.pl2, cm);
    addNode((Node){.tp = nodFnDef, .pl1 = cm->activeBindings[fnName.pl2],
                        .startBt = fnDef.startBt, .lenBts = fnDef.lenBts} , cm);

    // the scope for the function body
    VALIDATEP(lx->tokens[cm->i].tp == tokParens, errorFnNameAndParams)
    push(((ParseFrame){ .tp = nodScope, .startNodeInd = cm->nextInd, 
        .sentinelToken = fnSentinel }), cm->backtrack);        
    pushScope(cm->scopeStack);
    Token parens = lx->tokens[cm->i];
    addNode((Node){ .tp = nodScope, 
                    .startBt = parens.startBt, .lenBts = byteSentinel - parens.startBt }, cm);           
    
    Int paramsSentinel = cm->i + parens.pl2 + 1;
    cm->i++; // CONSUME the parens token for the param list            
    
    while (cm->i < paramsSentinel) {
        Token paramName = lx->tokens[cm->i];
        VALIDATEP(paramName.tp == tokWord, errorFnNameAndParams)
        Int newBindingId = createBinding(paramName, (Entity){.nameId = paramName.pl2}, cm);
        Node paramNode = (Node){.tp = nodBinding, .pl1 = newBindingId, 
                                .startBt = paramName.startBt, .lenBts = paramName.lenBts };
        cm->i++; // CONSUME a param name
        
        VALIDATEP(cm->i < paramsSentinel, errorFnNameAndParams)
        Token paramType = lx->tokens[cm->i];
        VALIDATEP(paramType.tp == tokTypeName, errorFnNameAndParams)
        
        Int typeBindingId = cm->activeBindings[paramType.pl2]; // the binding of this parameter's type
        VALIDATEP(typeBindingId > -1, errorUnknownType)
        
        cm->entities[newBindingId].typeId = typeBindingId;
        cm->i++; // CONSUME the param's type name
        
        addNode(paramNode, cm);        
    }
    
    VALIDATEP(cm->i < fnSentinel && lx->tokens[cm->i].tp >= firstPunctuationTokenType, errorFnMissingBody)
}

/** Parses top-level function params and bodies */
private void parseFunctionBodies(Lexer* lx, Compiler* cm) {
    cm->i = 0;
    const Int len = lx->totalTokens;
    while (cm->i < len) {
        Token tok = lx->tokens[cm->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.pl2;
            Int sentinelToken = cm->i + lenTokens + 1;
            VALIDATEP(lenTokens >= 2, errorFnNameAndParams)
            
            cm->i += 2; // CONSUME the function def token and the stmt token
            parseFnSignature(tok, lx, cm);
            parseUpTo(sentinelToken, lx->tokens, cm);
            cm->i = sentinelToken;
        } else {            
            cm->i += (tok.pl2 + 1);    // CONSUME the whole non-function span
        }
    }
}


testable Compiler* parseWithCompiler(Lexer* lx, Compiler* cm, Arena* a) {
    ParserDefinition* pDef = cm->parDef;
    int inpLength = lx->totalTokens;
    int i = 0;

    ParserFunc (*dispatch)[countSyntaxForms] = pDef->nonResumableTable;
    ResumeFunc (*dispatchResumable)[countResumableForms] = pDef->resumableTable;
    if (setjmp(excBuf) == 0) {
        parseToplevelTypes(lx, cm);
        parseToplevelConstants(lx, cm);
        parseToplevelFunctionNames(lx, cm);
        parseFunctionBodies(lx, cm);
    }
    return cm;
}

/** Parses a single file in 4 passes, see docs/parser.txt */
private Compiler* parse(Lexer* lx, Arena* a) {
    Compiler* cm = createCompiler(lx, a);
    return parseWithCompiler(lx, cm, a);
}

//}}}

//{{{ Typer

/** Allocates the table with overloads and changes the overloadCounts table to contain indices into the new table (not counts) */
testable void createOverloads(Compiler* cm) {
    print("len of m->overloadCounts %d", cm->overlCNext);
    Int neededCount = 0;
    for (Int i = 0; i < cm->overlCNext; i++) {
        // a typeId and an entityId for each overload, plus a length field for the list
        neededCount += (2*(cm->overloadCounts[i] >> 16) + 1);
    }
    print("total number of overload cells needed: %d", neededCount);
    StackInt* overloads = createStackint32_t(neededCount, cm->a);
    cm->overloads = *overloads;

    Int j = 0;
    for (Int i = 0; i < cm->overlCNext; i++) {
        overloads->content[j] = cm->overloadCounts[i] >> 16; // length of the overload list (to be filled during type check/resolution)
        cm->overloadCounts[i] = j;
        j += (2*(cm->overloadCounts[i] >> 16) + 1);
    }

    populateOverloadsForOperatorsAndImports(cm);
}

private void populateOverloadsForOperatorsAndImports(Compiler* cm) {
}

/** Shifts elements from start and until the end to the left.
 * E.g. the call with args (5, 3) takes the stack from [x x x x x 1 2 3] to [x x 1 2 3]
 */
private void shiftTypeStackLeft(Int startInd, Int byHowMany, Compiler* cm) {
    Int from = startInd;
    Int to = startInd - byHowMany;
    Int sentinel = cm->expStack->length;
    while (from < sentinel) {
        Int pieceSize = MIN(byHowMany, sentinel - from);
        memcpy(cm->expStack + to, cm->expStack + from, pieceSize*4);
        from += pieceSize;
        to += pieceSize;
    }
    cm->expStack->length -= byHowMany;
}

private void printExpSt(Compiler *cm) {
    for (int i = 0; i < cm->expStack->length; i++) {
        printf("%d ", cm->expStack->content[i]);
    }
}

/** Typechecks and type-resolves a single expression */
testable Int typeCheckResolveExpr(Int indExpr, Compiler* cm) {
    Node expr = cm->nodes[indExpr];
    Int sentinelNode = indExpr + expr.pl2 + 1;
    Int currAhead = 0; // how many elements ahead we are compared to the token array
    StackInt* st = cm->expStack;

    // populate the stack with types, known and unknown
    for (int i = indExpr + 1; i < sentinelNode; ++i) {
        Node nd = cm->nodes[i];
        if (nd.tp <= tokString) {
            push((Int)nd.tp, st);
        } else if (nd.tp == nodCall) {
            push(BIG + nd.pl2, st); // signifies that it's a call, and its arity
            print("cm->overloadCounts %p len %d", cm->overloadCounts, nd.pl1)
            push(cm->overloadCounts[nd.pl1], st); // this is not a count of overloads anymore, but an index into the overloads
            currAhead++;
        } else if (nd.pl1 > -1) { // bindingId            
            push(cm->entities[nd.pl1].typeId, st);
        } else { // overloadId
            push(nd.pl1, st); // overloadId            
        }
    }
    print("expStack len is %d", st->length)

    // now go from back to front, resolving the calls, typechecking & collapsing args, and replacing calls with their return types
    Int j = expr.pl2 + currAhead - 1;
    Arr(Int) cont = st->content;
    while (j > -1) {        
        if (cont[j] >= BIG) { // a function call. cont[j] contains the arity, cont[j + 1] the index into overloads table
            Int arity = cont[j] - BIG;
            Int o = cont[j + 1]; // index into the table of overloads
            Int overlCount = cm->overloads.content[o];
            if (arity == 0) {
                VALIDATEP(overlCount == 1, errorTypeZeroArityOverload)
                Int functionTypeInd = cm->overloads.content[o + 1];
                Int typeLength = cm->types.content[functionTypeInd];
                if (typeLength == 1) { // the function returns something
                    cont[j] = cm->types.content[functionTypeInd + 1]; // write the return type
                } else {
                    shiftTypeStackLeft(j + 1, 1, cm); // the function returns nothing, so there's no return type to write
                }
                --j;
            } else {
                Int typeLastArg = cont[j + arity + 1]; // + 1 for the element with the overloadId of the func
                VALIDATEP(typeLastArg > -1, errorTypeUnknownLastArg)
                Int ov = o + overlCount;
                while (ov > o && cm->overloads.content[ov] != typeLastArg) {
                    --ov;
                }
                VALIDATEP(ov > o, errorTypeNoMatchingOverload)
                
                Int typeOfFunc = cm->overloadCounts[ov];
                VALIDATEP(cm->types.content[typeOfFunc] - 1 == arity, errorTypeNoMatchingOverload) // last param matches, but not arity

                // We know the type of the function, now to validate arg types against param types
                for (int k = j + arity; k > j + 1; k--) { // not "j + arity + 1", because we've already checked the last param
                    if (cont[k] > -1) {
                        VALIDATEP(cont[k] == cm->types.content[ov + k - j], errorTypeWrongArgumentType)
                    } else {
                        Int argBindingId = cm->nodes[indExpr + k - currAhead].pl1;
                        print("argBindingId %d", argBindingId)
                        cm->entities[argBindingId].typeId = cm->types.content[ov + k - j];
                    }
                }
                cont[j] = cm->types.content[ov + 1]; // the function return type
                shiftTypeStackLeft(j + arity + 2, arity + 1, cm);
                j -= 2;
            }
        } else {
            j--;
        }
    }
}

//}}}

Int compile(unsigned char *src) {
    return 0;
}
