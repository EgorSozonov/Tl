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
#include "eyr.internal.h"

//}}}
//{{{ Language definition
//{{{ Lexical structure

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


private LexerFn LEX_TABLE[256]; // filled in by "tabulateLexer"

//}}}
//{{{ Operators

private OpDef OPERATORS[countOperators] = {
    { .arity=1, .bytes={ aExclamation, aDot, 0, 0 } },   // !.
    { .arity=2, .bytes={ aExclamation, aEqual, 0, 0 } }, // !=
    { .arity=1, .bytes={ aExclamation, 0, 0, 0 } },      // !
    { .arity=1, .bytes={ aSharp, aSharp, 0, 0 }, .overloadable=true }, // ##
    { .arity=1, .bytes={ aDollar, 0, 0, 0 } },     // $
    { .arity=2, .bytes={ aPercent, 0, 0, 0 } },    // %
    { .arity=2, .bytes={ aAmp, aAmp, aDot, 0 } },  // &&.
    { .arity=2, .bytes={ aAmp, aAmp, 0, 0 }, .assignable=true }, // &&
    { .arity=1, .bytes={ aAmp, 0, 0, 0 }, .isTypelevel=true },   // &
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
    { .arity=1, .bytes={ aLT, aDigit0, 0, 0} },    // <0
    { .arity=2, .bytes={ aLT, aLT,    0, 0}, .overloadable = true }, // <<
    { .arity=2, .bytes={ aLT, aEqual, 0, 0} },     // <=
    { .arity=2, .bytes={ aLT, aGT, 0, 0} },        // <>
    { .arity=2, .bytes={ aLT, 0, 0, 0 } },         // <
    { .arity=2, .bytes={ aEqual, aEqual, aEqual, 0 } }, // ===
    { .arity=1, .bytes={ aEqual, aDigit0, 0, 0 } }, // =0
    { .arity=2, .bytes={ aEqual, aEqual, 0, 0 } }, // ==
    { .arity=3, .bytes={ aGT, aEqual, aLT, aEqual } }, // >=<=
    { .arity=3, .bytes={ aGT, aLT, aEqual, 0 } },  // ><=
    { .arity=3, .bytes={ aGT, aEqual, aLT, 0 } },  // >=<
    { .arity=2, .bytes={ aGT, aGT, aDot, 0}, .assignable=true, .overloadable = true}, // >>.
    { .arity=1, .bytes={ aGT, aDigit0, 0, 0 } },   // >0
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
}; // real operator overloads filled in by "buildOperators"

const int operatorStartSymbols[13] = {
    // Symbols an operator may start with. "-" is absent because it's handled by lexMinus,
    // "=" - by lexEqual, "|" by lexPipe, "/" by "lexDivBy"
    aExclamation, aSharp, aDollar, aPercent, aAmp, aTimes, aPlus,
    aDivBy, aLT, aGT, aQuestion, aAt, aCaret
};

//}}}
//{{{ Syntactical structure

private ParserFn PARSE_TABLE[countSyntaxForms]; // filled in by "tabulateParser"

//}}}
//{{{ Code generator structure

private CodegenFn CODEGEN_TABLE[countAstForms]; // filled in by "tabulateCodegen"

//}}}
//{{{ Runtime (virtual machine)

InterpreterFn INTERPRETER_TABLE[countInstructions]; // filled in by "tabulateInterpreter"
#define countBuiltins 1
BuiltinFn BUILTINS_TABLE[countBuiltins]; // filled in by "tabulateBuiltins"

//}}}
//}}}
//{{{ Utils

jmp_buf excBuf;

//{{{ Arena

#define CHUNK_QUANT 32768

private size_t minChunkSize(void) {
    return (size_t)(CHUNK_QUANT - 32);
}

testable Arena* createArena(void) { //:createArena
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

testable void* allocateOnArena(size_t allocSize, Arena* a) { //:allocateOnArena
// Allocate memory in the arena, malloc'ing a new chunk if needed
    if ((size_t)a->currInd + allocSize >= a->currChunk->size) {
        if (a->currChunk->next != null && a->currChunk->next->size < allocSize) {
            // the next chunk is big enough, so we skip the rest of this chunk and move on
            print("reusing cleared memory from the arena!")
            a->currChunk = a->currChunk->next;
            a->currInd = 0;
        } else { // we need to allocate new chunk
            
            size_t newSize = calculateChunkSize(allocSize);
            ArenaChunk* newChunk = malloc(newSize);
            if (!newChunk) {
                perror("malloc error when allocating arena chunk");
                exit(EXIT_FAILURE);
            };
            // sizeof counts everything but the flexible array member, that's why we subtract it
            newChunk->size = newSize - sizeof(ArenaChunk);
            newChunk->next = a->currChunk->next; // if the arena has a (small) tail, don't lose it

            a->currChunk->next = newChunk;
            a->currChunk = newChunk;
            a->currInd = 0;
        }
    
    }
    void* result = (void*)(a->currChunk->memory + (a->currInd));
    a->currInd += allocSize;
    if (allocSize % 4 != 0)  {
        a->currInd += (4 - (allocSize % 4));
    }
    return result;
}


testable void deleteArena(Arena* ar) { //:deleteArena
// Returns memory of the arena to the OS
    ArenaChunk* curr = ar->firstChunk;
    while (curr != null) {
        ArenaChunk* nextToFree = curr->next;
        free(curr);
        curr = nextToFree;
    }
    free(ar);
}


testable void clearArena(Arena* a) { //:clearArena
// Clears the memory of the arena for reuse. Does not free memory.
    a->currChunk = a->firstChunk;
    a->currInd = 0;
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
        st->len -= 1;\
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
        st->len += 1;\
    }\


#ifdef TEST
private void dbgStackNode(StackNode*, Arena*);
#endif


void saveNodes(Int startInd, StackNode* scr, StackSourceLoc* locs, Compiler* cm) { //:saveNodes
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
        memcpy(newContent, cm->nodes.cont + startInd, cm->nodes.len*sizeof(Node));
        memcpy((Node*)(newContent) + (cm->nodes.len),
                scr->cont + startInd,
                pushCount*sizeof(Node));
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

void ensureCapacityTokenBuf(Int neededSpace, StackToken* st, Compiler* cm) {
//:ensureCapacityTokenBuf Reserve space in the temp buffer used to shuffle tokens
    st->len = 0;
    if (neededSpace >= st->cap) {
        Arr(Token) newContent = allocateOnArena(2*(st->cap)*sizeof(Token), cm->a);
        st->cap *= 2;
        st->cont = newContent;
    }
}


private void ensureCapacityTokens(Int neededSpace, Compiler* cm) {
//:ensureCapacityNodes Reserve space in the main nodes list
    if (cm->tokens.len + neededSpace - 1 >= cm->tokens.cap) {
        const Int newCap = (2*(cm->tokens.cap) > cm->tokens.cap + neededSpace)
            ? 2*cm->tokens.cap
            : cm->tokens.cap + neededSpace;
        Arr(Token) newContent = allocateOnArena(newCap*sizeof(Token), cm->a);
        memcpy(newContent, cm->tokens.cont, cm->tokens.len*sizeof(Token));
        cm->tokens.cap = newCap;
        cm->tokens.cont = newContent;
    }
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
        cm->fieldName.len += 1;\
    }
//}}}
//{{{ AssocList

void dbgType(TypeId typeId, Compiler* cm);

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
        freeStep += 1;
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

void dbgRawOverload(Int listInd, Compiler* cm) { //:dbgRawOverload
    MultiAssocList* ml = cm->rawOverloads;
    Int len = ml->cont[listInd]/2;
    printf("[");
    for (Int j = 0; j < len; j++) {
        printf("%d: %d ", ml->cont[listInd + 2 + 2*j], ml->cont[listInd + 2*j + 3]);
    }
    print("]");
    printf("types: ");
    for (Int j = 0; j < len; j++) {
        dbgType(ml->cont[listInd + 2 + 2*j], cm);
        printf("\n");
    }
}

#endif

//}}}
//{{{ Datatypes a la carte

DEFINE_STACK(int32_t) //:createStackint32_t :pushint32_t :peekint32_t :hasValuesint32_t :popint32_t
DEFINE_STACK(BtToken) //:createStackBtToken
DEFINE_STACK(Token) //:createStackToken
DEFINE_STACK(ParseFrame) //:createStackParseFrame
DEFINE_STACK(ExprFrame) //:createStackExprFrame
DEFINE_STACK(TypeFrame) //:createStackTypeFrame
DEFINE_STACK(SourceLoc) //:createStackSourceLoc

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Token) //:createInListToken
DEFINE_INTERNAL_LIST(tokens, Token, a) //:pushIntokens

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Toplevel)  //:createInListToplevel
DEFINE_INTERNAL_LIST(toplevels, Toplevel, a) //:pushIntoplevels

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Node) //:createInListNode
DEFINE_INTERNAL_LIST(nodes, Node, a) //:pushInnodes

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Entity) //:createEntity
DEFINE_INTERNAL_LIST(entities, Entity, a) //:pushInentities

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Int) //:createInListInt
DEFINE_INTERNAL_LIST(newlines, Int, a) //:pushInnewlines
DEFINE_INTERNAL_LIST(numeric, Int, a) //:pushInnumeric
DEFINE_INTERNAL_LIST(importNames, Int, a) //:pushInimportNames
DEFINE_INTERNAL_LIST(overloads, Int, a) //:pushInoverloads
DEFINE_INTERNAL_LIST(types, Int, a) //:pushIntypes

DEFINE_INTERNAL_LIST_CONSTRUCTOR(Ulong) //:createInListUlong
DEFINE_INTERNAL_LIST(bytecode, Ulong, a) //:pushInbytecode

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
        len += 1;

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
        Int capacity = (Unt)(*p) >> 16;
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


private Unt hashCode(Byte* start, Int len) {
    Unt result = 5381;
    Byte* p = start;
    for (Int i = 0; i < len; i++) {
        result = ((result << 5) + result) + p[i]; // hash*33 + c
    }

    return result;
}


private void addValueToBucket(Bucket** ptrToBucket, Int newIndString, Unt hash, Arena* a) {
    Bucket* p = *ptrToBucket;
    Int capacity = (p->capAndLen) >> 16;
    Int lenBucket = (p->capAndLen & 0xFFFF);
    if (lenBucket + 1 < capacity) {
        *(p->cont + lenBucket) = (StringValue){.hash = hash, .indString = newIndString};
        (p->capAndLen) += 1;
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
    Unt hash = hashCode(text + startBt, lenBts);
    Int hashOffset = hash % (hm->dictSize);
    Int newIndString;
    Bucket* bu = *(hm->dict + hashOffset);

    if (bu == null) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = cap, right u16 = len
        StringValue* firstElem = (StringValue*)newBucket->cont;

        newIndString = stringTable->len;
        Unt newName = ((Unt)(lenBts) << 24) + (Unt)startBt;
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
        Unt newName = ((Unt)(lenBts) << 24) + (Unt)startBt;
        push(newName, stringTable);
        addValueToBucket(hm->dict + hashOffset, newIndString, hash, hm->a);
    }
    return newIndString;
}


testable Int getStringDict(Byte* text, String* strToSearch, Stackint32_t* stringTable, StringDict* hm) {
// Returns the index of a string within the string table, or -1 if it's not present
    Int lenBts = strToSearch->len;
    Unt hash = hashCode(strToSearch->cont, lenBts);
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
        i += 1;
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
            prevInd += 1;
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
            prevInd += 1;
            list->cont[prevInd] = currVal;
        }
    }
    list->len = prevInd + 1;
}


private Int minPositiveOf(Int count, ...) { //:minPositiveOf
// Returns the minimum positive integer of a list, or 0 if none of them are positive
    if (count == 0) {
        return 0;
    }
    Int result = 0;
    va_list args;
    va_start(args, count);
    for (Int j = count; j > 0; j -= 1)  {
        Int n = va_arg(args, Int);
        if (n > 0 && (n < result || result == 0))  {
            result = n;
        }
    }
    va_end(args);
    return result;
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
#define iErrorArrayElemButShouldBePtr   13 // An assignment with list accessor on left should be ptr

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
const char errPunctuationParamList[]       = "The intro separator `:` must be directly in a statement";
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
const char errPrematureEndOfTokens[]       = "Premature end of tokens";
const char errUnexpectedToken[]            = "Unexpected token";
const char errCoreFormTooShort[]           = "Core syntax form too short";
const char errCoreFormUnexpected[]         = "Unexpected core form";
const char errCoreFormAssignment[]         = "A core form may not contain any assignments!";
const char errCoreFormInappropriate[]      = "Inappropriate reserved word!";
const char errIfLeft[]                     = "A left-hand clause in an if can only contain variables, boolean literals and expressions!";
const char errIfRight[]                    = "A right-hand clause in an if can only contain atoms, expressions, scopes and some core forms!";
const char errIfEmpty[]                    = "Empty `if` expression";
const char errIfMalformed[]                = "Malformed `if` expression, should look like (if pred: `true case` else `default`)";
const char errIfElseMustBeLast[]           = "An `else` subexpression must be the last thing in an `if`";
const char errTypeDefCountNames[]          = "Wrong count of names in a type definition!";
const char errFnNameAndParams[]            = "Function signature must look like this: `{x Type1 y Type 2 ->  ReturnType => body...}`";
const char errFnDuplicateParams[]          = "Duplicate parameter names in a function are not allowed";
const char errFnMissingBody[]              = "Function definition must contain a body which must be a Scope immediately following its parameter list!";
const char errLoopSyntaxError[]            = "A loop should look like `for x = 0; x < 101; ( loopBody ) `";
const char errLoopNoCondition[]            = "A loop header should contain a condition";
const char errLoopEmptyStepBody[]          = "Empty loop step code & body, but at least one must be present!";
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
const char errListDifferentEltTypes[]       = "A list's elements must all be of the same type";
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
const char errTypeOfNotList[]               = "Trying to get the element of a type which is not a list";
const char errTypeOfListIndex[]             = "The type of a list/array index must be Int";

//}}}

//}}}
//{{{ Forward decls


#define P_CT Arr(Token) toks, Compiler* cm // parser context signature fragment
#define P_C toks, cm // parser context args fragment
private void closeStatement(Compiler* lx);
testable NameId stToNameId(Int a);
private void exprCopyFromScratch(Int startNodeInd, Compiler* cm);
private Int isFunction(TypeId typeId, Compiler* cm);
private void addRawOverload(NameId nameId, TypeId typeId, EntityId entityId, Compiler* cm);
private TypeId exprUpToWithFrame(ParseFrame fr, SourceLoc loc, P_CT);
testable void typeAddHeader(TypeHeader hdr, Compiler* cm);
testable TypeHeader typeReadHeader(TypeId typeId, Compiler* cm);
testable void typeAddTypeParam(Int paramInd, Int arity, Compiler* cm);
private Int typeEncodeTag(Unt sort, Int depth, Int arity, Compiler* cm);
private FirstArgTypeId getFirstParamType(TypeId funcTypeId, Compiler* cm);
private TypeId getFunctionReturnType(TypeId funcTypeId, Compiler* cm);
private bool isFunctionWithParams(TypeId typeId, Compiler* cm);
private OuterTypeId typeGetOuter(FirstArgTypeId typeId, Compiler* cm);
private Int typeGetTyrity(TypeId typeId, Compiler* cm);
testable Int typeCheckBigExpr(Int indExpr, Int sentinel, Compiler* cm);
private TypeId typecheckList(Int startInd, Compiler* cm);
private TypeId tDefinition(StateForTypes* st, Int sentinel, Compiler* cm);
private TypeId tGetIndexOfFnFirstParam(TypeId fnType, Compiler* cm);
private TypeId tCreateSingleParamTypeCall(TypeId outer, TypeId param, Compiler* cm);
private Int tGetFnArity(TypeId fnType, Compiler* cm);
private void* inDeref0(Ptr address, Interpreter* rt);
#define inDeref(ptr) inDeref0(ptr, rt)
private void inMoveHeapTop(Unt sz, Interpreter* in);
#ifdef DEBUG
void printParser(Compiler* cm, Arena* a);
void dbgType(TypeId typeId, Compiler* cm);
private void dbgExprFrames(StateForExprs* st);
private void printStackInt(StackInt* st);
void dbgTypeFrames(StackTypeFrame* st);
void dbgCallFrames(Interpreter* rt);
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


const char standardText[] = "aliasassertbreakcatchcontinuedoeacheifelsefalsefor"
                            "ifimplimportmatchpubreturntraittruetry"
                            // reserved words end here; what follows may have arbitrary order
                            "IntLongDoubleBoolStrVoidFLArrayDRecEnumTuPromiselencapf1f2print"
                            "printErrmath:pimath:eTU"
#ifdef TEST
                            "foobarinner"
#endif
                            ;
// The :standardText prepended to all source code inputs and the hash table to provide a built-in
// string set. Eyr's reserved words must be at the start and sorted lexicographically.
// Also they must agree with the "standardStr" in eyr.internal.h

const Int standardStringLens[] = {
     5, 6, 5, 5, 8,
     2, 4, 3, 4, 5,
     3, 2, 4, 6, 5,
     3, 6, 5, 4, 3,
     // reserved words end here
     3, 4, 6, 4, 3, // Str(ing)
     4, 1, 1, 5, 1, // D(ict)
     3, 4, 2, 7, 3, // len
     3, 2, 2, 5, 8, // printErr
     7, 6, 1, 1, // U
#ifdef TEST
     3, 3, 5
#endif
};

const Int standardKeywords[] = {
    tokAlias,     tokAssert,  keywBreak,  tokCatch,   keywContinue,
    tokScope,     tokEach,    tokElseIf,  tokElse,    keywFalse,
    tokFor,       tokIf,      tokImpl,    tokImport,  tokMatch,
    tokMisc,      tokReturn,  tokTrait,   keywTrue,   tokTry
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
void dbgLexBtrack(Compiler* lx);

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
        len += 1;
    }

    String* result = allocateOnArena(lenStandard + len + 1 + sizeof(String), a); // +1 for the \0
    result->len = lenStandard + len;
    memcpy(result->cont, standardText, lenStandard);
    memcpy(result->cont + lenStandard, content, len + 1); // + 1 to copy the \0
    return result;
}

private Unt nameOfToken(Token tk) { //:nameOfToken
    return ((Unt)tk.lenBts << 24) + (Unt)tk.pl1;
}


testable Unt nameOfStandard(Int strId) { //:nameOfStandard
// Builds a name (nameId + length) for a standardString after "strFirstNonreserved"
    Int length = standardStringLens[strId];
    Int nameId = strId + countOperators;
    return ((Unt)length << 24) + (Unt)(nameId);
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
    printf("Internal error %d at line %d\n", errInd, lineNumber);
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
// Correctly calculates the lenBts for a single-line, statement-type span.
// This is for correctly calculating lengths of statements when they are ended by parens in
// case of a gap before ")", for example:
//  ` (do asdf    <- statement actually ended here
//    )`        <- but we are in this position now
//  Does not consume any characters.
    Token lastToken = lx->tokens.cont[lx->tokens.len - 1];
    Int byteAfterLastToken = lastToken.startBt + lastToken.lenBts;
    Int byteAfterLastPunct = lx->stats.lastClosingPunctInd + 1;
    Int lenBts = (byteAfterLastPunct > byteAfterLastToken ? byteAfterLastPunct : byteAfterLastToken)
                    - lx->tokens.cont[tokenInd].startBt;
    lx->tokens.cont[tokenInd].lenBts = lenBts;
    lx->tokens.cont[tokenInd].pl2 = lx->tokens.len - tokenInd - 1;
}


private void addStatementSpan(Unt stmtType, Int startBt, Compiler* lx) {
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
        j -= 1;
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
        j -= 1;
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
        j += 1;
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
        indTrailingZeroes -= 1;
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
                digitsAfterDot += 1;
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
        j += 1;
    }

    VALIDATEL(j >= lx->stats.inpLength || !isDigit(source[j]), errNumericWidthExceeded)

    if (metDot) {
        double resultValue = 0;
        Int errorCode = calcFloating(&resultValue, -digitsAfterDot, source, lx);
        VALIDATEL(errorCode == 0, errNumericFloatWidthExceeded)

        Long bitsOfFloat = longOfDoubleBits((isNegative) ? (-resultValue) : resultValue);
        pushIntokens((Token){ .tp = tokDouble, .pl1 = (bitsOfFloat >> 32),
                    .pl2 = (bitsOfFloat & LOWER32BITS), .startBt = lx->i, .lenBts = j - lx->i}, lx);
    } else {
        int64_t resultValue = 0;
        Int errorCode = calcInteger(&resultValue, lx);
        VALIDATEL(errorCode == 0, errNumericIntWidthExceeded)

        if (isNegative) resultValue = -resultValue;
        pushIntokens(
            (Token){ .tp = tokInt, .pl1 = resultValue >> 32, .pl2 = resultValue & LOWER32BITS,
            .startBt = lx->i, .lenBts = j - lx->i }, lx);
    }
    lx->i = j; // CONSUME the decimal number
}


private void lexNumber(Arr(Byte) source, Compiler* lx) { //:lexNumber
    wrapInAStatement(source, lx);
    Byte cByte = CURR_BT;
    if (lx->i == lx->stats.inpLength - 1 && isDigit(cByte)) {
        pushIntokens((Token){ .tp = tokInt, .pl2 = cByte - aDigit0,
                .startBt = lx->i, .lenBts = 1 }, lx);
        lx->i += 1; // CONSUME the single-digit number
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


private void openPunctuation(Unt tType, Unt spanLevel, Int startBt, Compiler* lx) {
//:openPunctuation Adds a token which serves punctuation purposes, i.e. either a ( or  a [
// These tokens are used to define the structure, that is, nesting within the AST.
// Upon addition, they are saved to the backtracking stack to be updated with their length
// once it is known. Consumes no bytes
    push(((BtToken){ .tp = tType, .tokenInd = lx->tokens.len, .spanLevel = spanLevel}),
            lx->lexBtrack);
    pushIntokens((Token) {.tp = tType, .pl1 = (tType < firstScopeTokenType) ? 0 : spanLevel,
                          .startBt = startBt }, lx);
}


private void lexElseElseIf(Unt reservedWordType, Int startBt,
               Arr(Byte) source, Compiler* lx) { //:lexElseElseIf
    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(bt->len >= 1, errCoreFormInappropriate)
    if (peek(bt).tp != tokIf) {
        // if not directly inside "if", then we may be only be in:
        // [If], [If stmt], [ElseIf] or [ElseIf stmt]
        VALIDATEL(bt->len > 1, errCoreFormInappropriate)
        if (peek(bt).spanLevel == slStmt) {
            closeStatement(lx);
        }
        VALIDATEL(bt->len > 0, errCoreFormInappropriate)
        BtToken top = peek(bt);
        if (top.tp == tokElseIf) {
            setStmtSpanLength(top.tokenInd, lx);
            pop(lx->lexBtrack);
        }
    }
    openPunctuation(reservedWordType, slScope, startBt, lx);
}


private void lexSyntaxForm(Unt reservedWordType, Int startBt,
                           Arr(Byte) source, Compiler* lx) { //:lexSyntaxForm
// Lexer action for a paren-type or statement-type syntax form.
// Precondition: we are looking at the character immediately after the keyword
    StackBtToken* bt = lx->lexBtrack;
    if (reservedWordType == tokElseIf || reservedWordType == tokElse) {
        lexElseElseIf(reservedWordType, startBt, source, lx);
    } else if (reservedWordType >= firstScopeTokenType) {
        // A reserved word must be the first inside parentheses, but parentheses are always wrapped
        // in statements, so we need to check the TWO last tokens and two top BtTokens
        VALIDATEL(bt->len >= 2 && peek(bt).tp == tokParens
          && bt->cont[bt->len - 2].tp == tokStmt, errCoreFormInappropriate)
        const Int indLastToken = lx->tokens.len - 1;
        VALIDATEL(lx->tokens.cont[indLastToken].tp == tokParens
          && lx->tokens.cont[indLastToken - 1].tp == tokStmt, errCoreFormInappropriate)
        lx->tokens.cont[indLastToken - 1].tp = reservedWordType;
        lx->tokens.cont[indLastToken - 1].pl1 = slScope;
        lx->tokens.len -= 1;
        bt->cont[bt->len - 2].tp = reservedWordType;
        bt->cont[bt->len - 2].spanLevel = slScope;
        bt->len -= 1;
        skipSpaces(source, lx);
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

    lx->i += 1; // CONSUME the first letter of the word
    while (lx->i < lx->stats.inpLength && isAlphanumeric(CURR_BT)) {
        lx->i += 1; // CONSUME alphanumeric characters
    }
    return result;
}


private void mbCloseAssignRight(BtToken* top, Compiler* cm) { //:mbCloseAssignRight
// Handles the case we are closing a tokAssignRight: we need to close its parent tokAssignment!
    if (top->tp != tokAssignRight) {
        return;
    }
    setStmtSpanLength(top->tokenInd, cm);
#ifdef SAFETY
    VALIDATEI(hasValues(cm->lexBtrack) && peek(cm->lexBtrack).tp == tokAssignment,
                iErrorInconsistentSpans)
#endif
    *top = pop(cm->lexBtrack);
    setStmtSpanLength(top->tokenInd, cm);
}


private void lxCloseFnDef(BtToken* top, Compiler* cm) { //:lxCloseFnDef
// Handles the case we are closing a function definition: we need to close its parent tokAssignment!
    StackBtToken* bt = cm->lexBtrack;
    setStmtSpanLength(top->tokenInd, cm);
    if (!hasValues(bt) || peek(bt).tp != tokAssignRight) {
        return;
    }
    *top = pop(bt); // the tokAssignRight
    setStmtSpanLength(top->tokenInd, cm);

#ifdef SAFETY
    VALIDATEI(hasValues(bt) && peek(bt).tp == tokAssignment, iErrorInconsistentSpans)
#endif
    *top = pop(bt); // the tokAssignment
    setStmtSpanLength(top->tokenInd, cm);
}


private void closeStatement(Compiler* lx) { //:closeStatement
// Closes the current statement. Consumes no tokens
    BtToken top = peek(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errPunctuationOnlyInMultiline)
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->lexBtrack);
        mbCloseAssignRight(&top, lx);
    }
}


private void tryOpenAccessor(Arr(Byte) source, Compiler* lx) { //:tryOpenAccessor
// Checks whether there is a `[` right after a word, in which case it's an accessor
    if (lx->i < lx->stats.inpLength && CURR_BT == aBracketLeft) { // `a[i][j]`
        openPunctuation(tokAccessor, slSubexpr, lx->i, lx);
        lx->i += 1; // CONSUME the `[`
    }
}


private void tryChangeParensToTypeCall(Compiler* lx) {
    if (!hasValues(lx->lexBtrack) || peek(lx->lexBtrack).tp != tokParens
            || lx->tokens.cont[lx->tokens.len - 1].tp != tokParens) {
        return;
    }
    lx->lexBtrack->cont[lx->lexBtrack->len - 1].tp = tokTypeCall;
    lx->tokens.cont[lx->tokens.len - 1].tp = tokTypeCall;
}


private void wordNormal(Unt wordType, Int uniqueStringId, Int startBt, Int realStartBt,
                        bool wasCapitalized, Arr(Byte) source, Compiler* lx) { //:wordNormal
    Int lenBts = lx->i - realStartBt;
    Token newToken = (Token){ .tp = wordType, .pl1 = uniqueStringId,
                             .startBt = realStartBt, .lenBts = lenBts };
    if (wordType == tokWord) {
        wrapInAStatementStarting(startBt, source, lx);
        if (wasCapitalized) {
            tryChangeParensToTypeCall(lx);
            newToken.tp = tokTypeName;
        }
    }
    pushIntokens(newToken, lx);

    if (wordType == tokWord && !wasCapitalized) {
        tryOpenAccessor(source, lx); // `word[i]` is parsed as list accessor
    }
}


private void wordReserved(Unt wordType, Int wordId, Int startBt, Int realStartBt,
                          Arr(Byte) source, Compiler* lx) { //:wordReserved
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

            push(((BtToken){ .tp = tokBreakCont, .tokenInd = lx->tokens.len, .spanLevel = slStmt}),
                    lx->lexBtrack);
            pushIntokens((Token) {.tp = tokBreakCont, .pl1 = 0, .startBt = realStartBt }, lx);
        } else if (keywordTp == keywContinue) {
            push(((BtToken){ .tp = tokBreakCont, .tokenInd = lx->tokens.len, .spanLevel = slStmt}),
                    lx->lexBtrack);
            pushIntokens((Token) {.tp = tokBreakCont, .pl1 = 1, .startBt = realStartBt }, lx);
        }
    } else {
        lexSyntaxForm(keywordTp, realStartBt, source, lx);
    }
}


private void wordInternal(Unt wordType, Arr(Byte) source, Compiler* lx) { //:wordInternal
// Lexes a word (both reserved and identifier) according to Eyr's rules.
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
                lx->i += 1; // CONSUME the minus
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
    Int stringId = addStringDict(source, startBt, lenString, lx->stringTable, lx->stringDict);
    if (stringId - countOperators < strFirstNonReserved)  {
        wordReserved(wordType, stringId - countOperators, startBt, realStartBt, source, lx);
    } else {
        wrapInAStatementStarting(startBt, source, lx);
        wordNormal(wordType, stringId, startBt, realStartBt, wasCapitalized, source, lx);
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
    lx->i += 1; // CONSUME the dot
    VALIDATEL(prevTok.tp == tokWord && lx->i == (prevTok.startBt + prevTok.lenBts + 1),
            errWordFreeFloatingFieldAcc);
    wordInternal(tokFieldAcc, source, lx);
}


private void lexAssignment(Int opType, Compiler* lx) { //:lexAssignment
// Params: opType is the operator for mutations (like `*=`), -1 for normal assignments.
// Handles the "=", and "+=" tokens (for the latter, inserts the operator and duplicates the
// tokens from the left side). Changes existing stmt token into tokAssignment and opens up a new
// tokAssignRight span. Doesn't consume anything
    BtToken currSpan = peek(lx->lexBtrack);
    VALIDATEL(currSpan.tp == tokStmt, errOperatorAssignmentPunct);

    Int assignmentStartInd = currSpan.tokenInd;
    Token* tok = (lx->tokens.cont + assignmentStartInd);
    tok->tp = tokAssignment;
    lx->lexBtrack->cont[lx->lexBtrack->len - 1].tp = tokAssignment;

    if (opType == -1 && lx->tokens.cont[assignmentStartInd + 1].tp == tokTypeName){ // type assign
        tok->pl1 = assiType;
    }

    openPunctuation(tokAssignRight, slStmt, lx->i, lx);
    if (opType > -1) {
        // -2 because we've already opened the right side span
        const Int countLeftSide = lx->tokens.len - assignmentStartInd - 2;
        ensureCapacityTokens(countLeftSide + 1, lx); // + 1 for the operator

        pushIntokens((Token){ .tp = tokOperator, .pl1 = opType,
                    .pl2 = 0, .startBt = lx->i, .lenBts = OPERATORS[opType].lenBts}, lx);
        memcpy(lx->tokens.cont + lx->tokens.len,
               lx->tokens.cont + assignmentStartInd + 1, countLeftSide*sizeof(Token));
        lx->tokens.len += countLeftSide;
    }
}


private void lexColon(Arr(Byte) source, Compiler* lx) { //:lexColon
// Handles keyword arguments ":asdf" and intros like param lists "(\\x Int : ...)"
    if (lx->i < lx->stats.inpLength - 1 && isLowercaseLetter(NEXT_BT)) {
        lx->i += 1; // CONSUME the ":"
        wordInternal(tokKwArg, source, lx);
    } else {
        VALIDATEL(hasValues(lx->lexBtrack), errPunctuationOnlyInMultiline)
        BtToken top = peek(lx->lexBtrack);
        VALIDATEL(top.spanLevel == slStmt, errPunctuationParamList)

        lx->stats.lastClosingPunctInd = lx->i;
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->lexBtrack);
        mbCloseAssignRight(&top, lx);

        Token* tok = lx->tokens.cont + top.tokenInd;
        tok->pl1 = tok->tp;
        tok->tp = tokIntro;

        lx->i += 1; // CONSUME the ":"
    }
}


private void lexApostrophe(Arr(Byte) source, Compiler* lx) { //:lexApostrophe
// Handles keyword arguments ":asdf" and param lists "{{x Int : ...}}"
    VALIDATEL(lx->i < lx->stats.inpLength - 1 && isLetter(NEXT_BT), errUnexpectedToken)
    lx->i += 1; // CONSUME the "'"
    wordInternal(tokTypeVar, source, lx);
}


private void lexSemicolon(Arr(Byte) source, Compiler* lx) { //:lexSemicolon
    VALIDATEL(hasValues(lx->lexBtrack), errPunctuationOnlyInMultiline)
    BtToken top = peek(lx->lexBtrack);
    VALIDATEL(top.spanLevel != slSubexpr, errPunctuationOnlyInMultiline);
    lx->stats.lastClosingPunctInd = lx->i;
    if (top.spanLevel == slScope) {
        pushIntokens((Token){ .tp = tokStmt, .pl1 = 0, .pl2 = 0, .startBt = lx->i, .lenBts = 1 },
                        lx);
    } else {
        closeStatement(lx);
    }
    lx->i += 1;  // CONSUME the ";"
}


private void lexTilde(Arr(Byte) source, Compiler* lx) { //:lexTilde
    const Int lastInd = lx->tokens.len - 1;
    VALIDATEL(lx->tokens.len > 0, errWordTilde)
    Token lastTk = lx->tokens.cont[lastInd];
    VALIDATEL(lx->tokens.cont[lastInd].tp == tokWord && (lastTk.startBt + lastTk.lenBts == lx->i),
              errWordTilde)
    lx->tokens.cont[lastInd].pl2 = 1;
    lx->tokens.cont[lastInd].lenBts = lastTk.lenBts + 1;
    lx->i += 1;  // CONSUME the "~"
}


private void lexOperator(Arr(Byte) source, Compiler* lx) { //:lexOperator
    wrapInAStatement(source, lx);

    Byte firstSymbol = CURR_BT;
    Byte secondSymbol = (lx->stats.inpLength > lx->i + 1) ? source[lx->i + 1] : 0;
    Byte thirdSymbol = (lx->stats.inpLength > lx->i + 2) ? source[lx->i + 2] : 0;
    Byte fourthSymbol = (lx->stats.inpLength > lx->i + 3) ? source[lx->i + 3] : 0;
    Int k = 0;
    Int opType = -1; // corresponds to the op... operator types
    while (k < countOperators && OPERATORS[k].bytes[0] < firstSymbol) {
        k += 1;
    }
    while (k < countOperators && OPERATORS[k].bytes[0] == firstSymbol) {
        OpDef opDef = OPERATORS[k];
        Byte secondTentative = opDef.bytes[1];
        if (secondTentative != 0 && secondTentative != secondSymbol) {
            k += 1;
            continue;
        }
        Byte thirdTentative = opDef.bytes[2];
        if (thirdTentative != 0 && thirdTentative != thirdSymbol) {
            k += 1;
            continue;
        }
        Byte fourthTentative = opDef.bytes[3];
        if (fourthTentative != 0 && fourthTentative != fourthSymbol) {
            k += 1;
            continue;
        }
        opType = k;
        break;
    }
    VALIDATEL(opType > -1, errOperatorUnknown)

    OpDef opDef = OPERATORS[opType];
    bool isAssignment = false;

    Int lengthOfBaseOper = (opDef.bytes[1] == 0) ? 1
        : (opDef.bytes[2] == 0 ? 2 : (opDef.bytes[3] == 0 ? 3 : 4));
    Int j = lx->i + lengthOfBaseOper;
    if (opDef.assignable && j < lx->stats.inpLength && source[j] == aEqual) {
        isAssignment = true;
        j += 1;
    }
    if (isAssignment) { // mutation operators like "*=" or "*.="
        lexAssignment(opType, lx);
    } else {
        pushIntokens((Token){ .tp = tokOperator, .pl1 = opType,
                    .pl2 = 0, .startBt = lx->i, .lenBts = j - lx->i}, lx);
    }
    lx->i = j; // CONSUME the operator
}


private void lexEqual(Arr(Byte) source, Compiler* lx) { //:lexEqual
// The humble "=" can be the definition statement or a comparison "=="
    checkPrematureEnd(2, lx);
    Byte nextBt = NEXT_BT;
    if (nextBt == aEqual || nextBt == aDigit0) {
        lexOperator(source, lx); // == or =0
    } else {
        lexAssignment(-1, lx);
        lx->i += 1; // CONSUME the =
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
        lx->i += 1; // CONSUME the "_"
    }
}


private void lexNewline(Arr(Byte) source, Compiler* lx) { //:lexNewline
    pushInnewlines(lx->i, lx);

    lx->i += 1;     // CONSUME the LF
    while (lx->i < lx->stats.inpLength) {
        if (!isSpace(CURR_BT)) {
            break;
        }
        lx->i += 1; // CONSUME a space or tab
    }
}


private void lexComment(Arr(Byte) source, Compiler* lx) { //:lexComment
// Eyr separates between documentation comments (which live in meta info and are
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
            wrapInAStatement(source, lx);
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
    if (NEXT_BT == aParenLeft) {
        openPunctuation(tokFn, slScope, lx->i, lx);
        lx->i += 2; // CONSUME the "(("
    } else {
        wrapInAStatement(source, lx);
        openPunctuation(tokParens, slSubexpr, lx->i, lx);
        lx->i += 1; // CONSUME the left parenthesis
    }
}


private void lexParenRight(Arr(Byte) source, Compiler* lx) { //:lexParenRight
// A closing parenthesis may close the following configurations of lexer backtrack:
// 1. [scope stmt] - if it's just a scope nested within another scope or a function
// 2. [coreForm stmt] - eg. if it's closing the function body
// 3. [if else/elseIf stmt]
// 4. [if else/elseIf ]
    const Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;

    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);

    mbCloseAssignRight(&top, lx);

    // since a closing paren may be closing something with statements inside it, like a lex scope
    // or a function, we need to close that statement before closing its parent
    if (top.spanLevel == slStmt) {
        VALIDATEL(hasValues(bt) && bt->cont[bt->len - 1].spanLevel == slScope,
                  errPunctuationUnmatched);
        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }

    lx->stats.lastClosingPunctInd = startInd; // this must be here, after possible statement close
    if (top.tp == tokFn
            && lx->i + 1 < lx->stats.inpLength && NEXT_BT == aParenRight) {
        lx->stats.lastClosingPunctInd += 1;
        lxCloseFnDef(&top, lx);
        lx->i += 2; // CONSUME the closing "))"
        return;
    }

    if (hasValues(bt) && (top.tp == tokElse || top.tp == tokElseIf)) {
        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }

    VALIDATEL(top.spanLevel == slSubexpr || top.spanLevel == slScope, errPunctuationUnmatched)

    setSpanLengthLexer(top.tokenInd, lx);
    lx->i += 1; // CONSUME the closing ")"
}


private void lexCurlyLeft(Arr(Byte) source, Compiler* lx) { //:lexCurlyLeft
// Handles objects/maps/sets "{}"
    Int j = lx->i + 1;
    VALIDATEL(j < lx->stats.inpLength, errPunctuationUnmatched)
    openPunctuation(tokDataMap, slSubexpr, lx->i, lx);
    lx->i += 1; // CONSUME the left bracket
}


private void lexCurlyRight(Arr(Byte) source, Compiler* lx) { //:lexCurlyRight
    Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);
    VALIDATEL(top.tp == tokDataMap, errPunctuationUnmatched)

    setSpanLengthLexer(top.tokenInd, lx);

    if (hasValues(bt)) { lx->stats.lastClosingPunctInd = startInd; }
    lx->i += 1; // CONSUME the closing "}"
}



private void lexBracketLeft(Arr(Byte) source, Compiler* lx) { //:lexBracketLeft
    wrapInAStatement(source, lx);
    openPunctuation(tokDataList, slSubexpr, lx->i, lx);
    lx->i += 1; // CONSUME the `[`
}


private void lexBracketRight(Arr(Byte) source, Compiler* lx) { //:lexBracketRight
    Int startInd = lx->i;
    StackBtToken* bt = lx->lexBtrack;
    VALIDATEL(hasValues(bt), errPunctuationExtraClosing)
    BtToken top = pop(bt);
    VALIDATEL(top.tp == tokDataList || top.tp == tokAccessor, errPunctuationUnmatched)

    setSpanLengthLexer(top.tokenInd, lx);

    if (hasValues(bt)) { lx->stats.lastClosingPunctInd = startInd; }
    lx->i += 1; // CONSUME the closing "]"

    tryOpenAccessor(source, lx);
}


private void lexPipe(Arr(Byte) source, Compiler* lx) { //:lexPipe
// Closes the current statement and changes its type to tokIntro
    Int j = lx->i + 1;
    if (j < lx->stats.inpLength && NEXT_BT == aPipe) {
        lexOperator(source, lx);
    } else {
        throwExcLexer(errUnexpectedToken);
        // TODO function chaining
    }
}


private void lexSpace(Arr(Byte) source, Compiler* lx) { //:lexSpace
    lx->i += 1; // CONSUME the space
    while (lx->i < lx->stats.inpLength && isSpace(CURR_BT)) {
        lx->i += 1; // CONSUME a space
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


private void tabulateLexer() { //:tabulateLexer
    LexerFn* p = LEX_TABLE;
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
    p[aApostrophe] = &lexApostrophe; // to handle type variables "'a"
    p[aSemicolon] = &lexSemicolon; // the statement terminator
    p[aTilde] = &lexTilde;

    p[aSpace] = &lexSpace;
    p[aNewline] = &lexNewline; // to make the newline a statement terminator sometimes
    p[aBacktick] = &lexStringLiteral;
    //return result;
}


private void setOperatorsLengths() { //:setOperatorsLengths
// The set of operators in the language. Must agree in order with eyr.internal.h
    for (Int k = 0; k < countOperators; k++) {
        Int m = 0;
        for (; m < 4 && OPERATORS[k].bytes[m] > 0; m++) {}
        OPERATORS[k].lenBts = m;
    }
}

//}}}

//}}}
//{{{ Parser
//{{{ Parser utils

#define VALIDATEP(cond, errMsg) if (!(cond)) { throwExcParser0(errMsg, __LINE__, cm); }

private TypeId exprUpTo(Int sentinelToken, SourceLoc loc, P_CT);
private void eClose(StateForExprs* s, Compiler* cm);
private void addBinding(int nameId, int bindingId, Compiler* cm);
private void mbCloseSpans(Compiler* cm);
private void popScopeFrame(Compiler* cm);
private EntityId importActivateEntity(Entity ent, Compiler* cm);
private void createBuiltins(Compiler* cm);
testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a);
private void eLinearize(Int sentinel, P_CT);
private TypeId exprHeadless(Int sentinel, SourceLoc loc, P_CT);
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


private EntityId createEntity(Unt name, Emit emit, Byte class, Compiler* cm) { //:createEntity
// Validates a new binding (that it is unique), creates an entity for it,
// and adds it to the current scope
    Int nameId = name & LOWER24BITS;
    Int mbBinding = cm->activeBindings[nameId];
    VALIDATEP(mbBinding < 0, errAssignmentShadowing)
    // if it's a binding, it should be -1, and if overload, < -1

    Int newEntityId = cm->entities.len;
    pushInentities(((Entity){ .name = name, .emit = emit, .class = class }), cm);
    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (cm->scopeStack->len > 0) {
            addBinding(nameId, newEntityId, cm); // adds it to the ScopeStack
        }
        cm->activeBindings[nameId] = newEntityId; // makes it active
    }
    return newEntityId;
}


private EntityId createEntityWithType(Unt name, TypeId typeId, Emit emit, Byte class,
                                 Compiler* cm) { //:createEntityWithType
    EntityId newEntityId = createEntity(name, emit, class, cm);
    cm->entities.cont[newEntityId].typeId = typeId;
    return newEntityId;
}


private Int calcSentinel(Token tok, Int tokInd) { //:calcSentinel
// Calculates the sentinel token for a token at a specific index
    return (tok.tp >= firstSpanTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}


testable void addNode(Node node, SourceLoc loc, Compiler* cm) { //:addNode
    pushInnodes(node, cm);
    push(loc, cm->sourceLocs);
}


private void pAddAccessorCall(Int sentinel, SourceLoc loc, StateForExprs* s) {
//:pAddAccessorCall Pushes a list accessor call to the temporary stacks during expression parsing
    push(((ExprFrame) { .tp = exfrCall, .startNode = -1, .sentinel = sentinel,
                        .argCount = 1 }), s->frames);
    push(((Node) { .tp = nodCall, .pl1 = opGetElem }), s->calls);
    push(loc, s->locsCalls);
}


private void pAddUnaryCall(Token tok, StateForExprs* s) {
//:pAddUnaryCall Pushes a unary, prefix call to the temporary stacks during expression parsing
    push(((ExprFrame) { .tp = exfrUnaryCall, .startNode = -1, .sentinel = -1,
                        .argCount = 1 }), s->frames);
    push(((Node) { .tp = nodCall, .pl1 = tok.pl1 }), s->calls);
    push(((SourceLoc) { .startBt = tok.startBt, .lenBts = tok.lenBts }), s->locsCalls);
}


private void pAddFunctionCall(Token tk, Int sentinel, StateForExprs* s) {
//:pAddFunctionCall Pushes a call to the temporary stacks during expression parsing
    push(
        ((ExprFrame) {.tp = exfrCall, .sentinel = sentinel, .argCount = 0 }),
        s->frames);
    push(((Node) { .tp = nodCall, .pl1 = tk.pl1 }), s->calls);
    push(((SourceLoc) { .startBt = tk.startBt, .lenBts = tk.lenBts }), s->locsCalls);
}

//}}}
//{{{ Forward decls

testable void pushLexScope(ScopeStack* scopeStack);
testable Int pTypeDef(P_CT);

#ifdef TEST
void printIntArrayOff(Int startInd, Int count, Arr(Int) arr);
#endif

//}}}

private SourceLoc locOf(Token tk) {
    return (SourceLoc){.startBt = tk.startBt, .lenBts = tk.lenBts};
}

private void openParsedScope(Int sentinelToken, SourceLoc loc, Compiler* cm) {
//:openParsedScope Performs coordinated insertions to start a scope within the parser
    push(((ParseFrame){
            .tp = nodScope, .startNodeInd = cm->nodes.len, .sentinel = sentinelToken }),
        cm->backtrack);
    addNode((Node){.tp = nodScope}, loc, cm);
    pushLexScope(cm->scopeStack);
}


private void openFnScope(EntityId fnEntity, NameId name, TypeId fnType, Token fnTk, Int sentinel,
                         Compiler* cm) {
//:openFnScope Performs coordinated insertions to start a function definition
    push(((ParseFrame){ .tp = nodFnDef, .startNodeInd = cm->nodes.len, .sentinel = sentinel,
                        .typeId = fnType }), cm->backtrack);
    pushLexScope(cm->scopeStack); // a function body is also a lexical scope
    addNode((Node){ .tp = nodFnDef, .pl1 = fnEntity, .pl3 = (name & LOWER24BITS)},
            locOf(fnTk), cm);
}

private void parseMisc(Token tok, P_CT) {
}


private void pScope(Token tok, P_CT) { //:pScope
    openParsedScope(cm->i + tok.pl2, locOf(tok), cm);
}

private void parseTry(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void ifFindNextClause(Int start, Int sentinel, OUT Int* nextTokenInd, OUT Int* lastLastByte,
                              Arr(Token) toks) { //:ifFindNextClause
// Finds the next clause inside an "if" syntax form. Returns the index of that clause's first token
// and the last byte (of course exclusive, so the byte after) of the last token/span before that
// clause. Returns 0s if there is no next clause.
// Precondition: we are 1 token past the tokIntro span
    if (start >= sentinel)  {
        *nextTokenInd = 0;
        *lastLastByte = 0;
        return;
    }
    Int j = start;
    Token curr;
    for (curr = toks[j]; curr.tp != tokElseIf && curr.tp != tokElse; ){
        j = calcSentinel(curr, j);
        if (j >= sentinel) {
            break;
        }
        *lastLastByte = curr.startBt + curr.lenBts;
        curr = toks[j];
    }
    if (j == sentinel)  {
        *nextTokenInd = 0;
        *lastLastByte = 0;
    } else {
        *nextTokenInd = j;
    }
}


private void ifCondition(Token tok, P_CT) { //:ifCondition
// Precondition: we are 1 past the "stmt" token, which is the first parameter
    Int leftSentinel = calcSentinel(tok, cm->i - 1);
    VALIDATEP(tok.tp == tokIntro, errIfLeft)
    VALIDATEP(leftSentinel + 1 < cm->stats.inpLength, errPrematureEndOfTokens)

    TypeId typeLeft = pExprWorker(tok, P_C);
    VALIDATEP(typeLeft == tokBool, errTypeMustBeBool)
    mbCloseSpans(cm);
}


private void ifOpenSpan(Unt tp, Int sentinel, Int indNextClause, SourceLoc loc, Compiler* cm) {

    ParseFrame newParseFrame = (ParseFrame){ .tp = tp, .startNodeInd = cm->nodes.len,
            .sentinel = sentinel };
    push(newParseFrame, cm->backtrack);
    addNode((Node){.tp = tp, .pl3 = indNextClause > 0 ? (indNextClause - cm->i + 1) : 0},
            loc, cm);
}

private void pIf(Token tok, P_CT) { //:pIf
    const Int ifSentinel = cm->i + tok.pl2;
    Int indNextClause;
    Int lastByteBeforeNextClause;
    ifFindNextClause(cm->i, ifSentinel, OUT &indNextClause, OUT &lastByteBeforeNextClause, toks);

    ifOpenSpan(nodIf, ifSentinel, indNextClause, locOf(tok), cm);

    Token stmtTok = toks[cm->i];
    cm->i += 1; // CONSUME the stmt token
    ifCondition(stmtTok, P_C);

    Token scopeTok = toks[cm->i];
    if (indNextClause > 0) {
        VALIDATEP(indNextClause > cm->i, errIfEmpty)
        SourceLoc loc = { .startBt = scopeTok.startBt,
                          .lenBts = lastByteBeforeNextClause - scopeTok.startBt};
        openParsedScope(indNextClause, loc, cm);
    } else {
        lastByteBeforeNextClause = tok.startBt + tok.lenBts;
        openParsedScope(ifSentinel, locOf(scopeTok), cm);
    }
}


private void pElseIf(Token tok, P_CT) { //:pElseIf
// ElseIf spans go inside an if: (if expr (scope ...) (elseif expr (scope ...))
    const Int ifSentinel = cm->i + tok.pl2;
    ifOpenSpan(nodElseIf, ifSentinel, 0, locOf(tok), cm);

    Token stmtTok = toks[cm->i];
    cm->i += 1; // CONSUME the stmt token
    ifCondition(stmtTok, P_C);
    openParsedScope(ifSentinel, locOf(tok), cm);
}


private void pElse(Token tok, P_CT) { //:pElse
// "Else" is a special case of "ElseIf" marked with .pl3 = 0
    mbCloseSpans(cm);

    const Int ifSentinel = cm->i + tok.pl2;
    ifOpenSpan(nodElseIf, ifSentinel, 0, locOf(tok), cm);
    openParsedScope(ifSentinel, locOf(tok), cm);
}


private TypeId assignmentComplexLeftSide(Int sentinel, P_CT) { //:assignmentComplexLeftSide
// A left side with more than one token must consist of a known var with a series of accessors.
// It gets transformed like this:
// arr[i][j*2][k + 3] ==> arr i getElem j 2 *(2) getElem k 3 +(2) getElemPtr
// Returns the type of the left side
    Token firstTk = toks[cm->i];
    VALIDATEP(firstTk.tp == tokWord, errAssignmentLeftSide)
    //EntityId leftEntityId = getActiveVar(firstTk.pl1, cm);
    Int j = cm->i + 1;
    StackInt* sc = cm->stateForExprs->exp;
    sc->len = 0;

    while (j < sentinel) {
        Token accessorTk = toks[j];
        VALIDATEP(accessorTk.tp == tokAccessor, errAssignmentLeftSide)
        j = calcSentinel(accessorTk, j);
        push(j, sc);
    }

    Int startBt = firstTk.startBt;
    Int lastBt = toks[j - 1].startBt + toks[j - 1].lenBts;
    TypeId leftType = exprUpToWithFrame(
            (ParseFrame){ .tp = nodExpr, .startNodeInd = cm->nodes.len, .sentinel = sentinel },
            (SourceLoc){.startBt = startBt, .lenBts = lastBt - startBt }, P_C);

    // we also need to mutate the last opGetElem to opGetElemPtr, but we do that in
    // "assignmentMutateComplexLeft"
    return leftType;
}


private void assignmentMutateComplexLeft(Int rightNodeInd, P_CT) {
//:assignmentMutateComplexLeft Mutating the last opGetElem to opGetElemPtr, but only after the whole
// left side has been copied to the right (in case we are in a mutation like `x[i] += 2`)
    Node lastNode = cm->nodes.cont[rightNodeInd - 1];
    if (lastNode.tp == nodCall)  {
#ifdef SAFETY
        VALIDATEI(lastNode.pl1 == opGetElem, iErrorArrayElemButShouldBePtr)
#endif
        cm->nodes.cont[rightNodeInd - 1].pl1 = opGetElemPtr;
    }
}


private TypeId pAssignmentRight(TypeId leftType, Token rightTk, Int sentinel, P_CT) {
//:pAssignmentRight The right side of an assignment
    if (rightTk.tp == tokFn) {
        return -1;
        // TODO
    } else {
        TypeId rightType = exprUpToWithFrame(
                (ParseFrame){ .tp = nodExpr, .startNodeInd = cm->nodes.len, .sentinel = sentinel },
                locOf(rightTk), P_C);
        VALIDATEP(rightType != -2, errAssignment)
        return rightType;
    }
}


private void pAssignment(Token tok, P_CT) { //:pAssignment
    if (tok.pl1 == assiType) {
        pTypeDef(P_C);
        return;
    }

    TypeId leftType = -1;
    Int j = cm->i;
    for (; j < cm->stats.inpLength && toks[j].tp != tokAssignRight;
            j += 1) {
    }
    const Int rightInd = j;
    const Int sentinel = calcSentinel(tok, cm->i - 1);
    const Int countLeftSide = j - cm->i;
    Token rightTk = toks[j];

#ifdef SAFETY
    VALIDATEI(countLeftSide < (tok.pl2 - 1), iErrorInconsistentSpans)
#endif

    EntityId entityId = -1;
    const Int assignmentNodeInd = cm->nodes.len;
    push(((ParseFrame){.tp = nodAssignment, .startNodeInd = assignmentNodeInd,
                       .sentinel = sentinel}), cm->backtrack);
    addNode((Node){ .tp = nodAssignment}, locOf(tok), cm);
    if (countLeftSide == 1)  {
        Token nameTk = toks[cm->i];
        Unt newName = ((Unt)nameTk.lenBts << 24) + (Unt)nameTk.pl1; // nameId + lenBts
        entityId = cm->activeBindings[nameTk.pl1];
        if (entityId > -1) {
            VALIDATEP(cm->entities.cont[entityId].class == classMut,
                    errCannotMutateImmutable)
            leftType = cm->entities.cont[entityId].typeId;
        } else {
            entityId = createEntity(
                    newName,
                    (Emit){.kind = uncallableVar, .body = {}},
                    nameTk.pl2 == 1 ? classMut: classImmut, cm
            );
        }
        addNode((Node){ .tp = nodBinding, .pl1 = entityId, .pl2 = 0 }, locOf(nameTk), cm);
    } else if (countLeftSide > 1) {
        leftType = assignmentComplexLeftSide(cm->i + countLeftSide, P_C);
    }

    cm->i = rightInd + 1; // CONSUME the left side of an assignment and the assignRight
    cm->nodes.cont[assignmentNodeInd].pl3 = cm->nodes.len - assignmentNodeInd;
    const Int rightNodeInd = cm->nodes.len;

    if (countLeftSide > 1) {
        assignmentMutateComplexLeft(rightNodeInd, P_C);
    }
    const TypeId rightType = pAssignmentRight(leftType, rightTk, sentinel, P_C);

    if (entityId > -1 && rightType > -1 && leftType == -1) {
        cm->entities.cont[entityId].typeId = rightType; // inferring the type of left binding
    } else if (leftType > -1 && rightType > -1) {
        VALIDATEP(leftType == rightType, errTypeMismatch)
    }

    mbCloseSpans(cm);
}


private void preambleFor(Int sentinel, OUT Int* condInd, OUT Int* stepInd, OUT Int* bodyInd,
                         P_CT) { //:preambleFor
// Pre-processes a "for" loop and finds its key tokens: the loop condition and the intro
// Every out index is either positive or 0 for "not found".
// One of "stepInd" and "bodyInd" is guaranteed to be found & positive. If they are both present,
// this function performs important token twiddling: it reorders the step to be after the body.
// A "for" syntax form is quadripartite:
// 1) var inits (they must all be assignments),
// 2) the condition (must be an expression),
// 3) statements for stepping to the next iteration (must be expressions, assignments or asserts),
// 4) loop body (arbitrary syntax forms).
// The stepping statements (3) just need to be appended to the body (4), but the parser works in a
// unidirectional way: t'would be hard to make it jump back to parse a couple of stmts. This is why
// this function reorders the tokens:
//
// [inits | cond | tokStmt tokIntro | tokBody1 ... tokBodyN]
//                 ^ step1 ^ step2    ^ loop body
//
//   |    |    |    |    |    |    |    |    |
//   v    v    v    v    v    v    v    v    v
// [inits | cond | tokBody1 ... tokBodyN tokStep1 tokStep2 ] <- restores original tp of tokIntro
    VALIDATEP(cm->i < sentinel, errLoopEmptyStepBody)
    Int j = cm->i;
    for (Token currTok = toks[j];
         (currTok.tp == tokAssignment || currTok.tp == tokAssignRight);
         currTok = toks[j]) {
        j = calcSentinel(currTok, j);
        VALIDATEP(j < sentinel, errLoopEmptyStepBody)
    }
    Token condTok = toks[j];
    VALIDATEP((condTok.tp == tokStmt || condTok.tp == tokIntro) && condTok.pl2 > 0,
              errLoopNoCondition)
    *condInd = j;

    if (condTok.tp == tokIntro)  {
        VALIDATEP(condTok.pl1 == tokStmt, errLoopNoCondition) // a cond must be an expression
        *stepInd = 0;
    } else {
        j = calcSentinel(condTok, j); // skipping the cond
        *stepInd = j;
        for (Token currTok = toks[j]; j < sentinel && currTok.tp != tokIntro; currTok = toks[j]) {
            j = calcSentinel(currTok, j);
        }
        VALIDATEP(j < sentinel, errLoopSyntaxError)
    }
    toks[j].tp = toks[j].pl1; // restore the original token type of the tokIntro
    j = calcSentinel(toks[j], j);
    *bodyInd = (j < sentinel) ? j : 0;

    VALIDATEP((*stepInd) + (*bodyInd) > 0, errLoopEmptyStepBody)
    // re-order
    if (*stepInd > 0 && *bodyInd > 0)  {
        const Int lenBody = sentinel - *bodyInd;
        const Int lenStep = *bodyInd - (*stepInd);
        StackToken* buf = cm->stateForExprs->reorderBuf;
        ensureCapacityTokenBuf(lenBody, buf, cm);
        memcpy(buf->cont, toks + *bodyInd, lenBody*sizeof(Token));
        memcpy(toks + sentinel - lenStep, toks + (*stepInd), lenStep*sizeof(Token));
        memcpy(toks + (*stepInd), buf->cont, lenBody*sizeof(Token));
    }
}


private void pFor(Token forTk, P_CT) { //:pFor
// For loops. Look like "(for x~ = 0;  x < 100; x += 1:  ... )"
//                            ^initInd ^condInd ^stepInd ^bodyInd
// At least a step or a body is syntactically required.
// End result of a parse looks like:
// nodFor
//     scope (pl3 = length of nodes to condinner scope)(
//         initializations
//         expr (the cond - if present)
//         scope (if any inits are present)
//             body
//             step
    const Int initInd = cm->i;

    cm->stats.loopCounter += 1;
    const Int sentinel = cm->i + forTk.pl2;

    Int condInd; // index of condition
    Int stepInd; // index of iteration stepping code
    Int bodyInd; // index of loop body
    const Int forNodeInd = cm->nodes.len;
    preambleFor(sentinel, OUT &condInd, OUT &stepInd, OUT &bodyInd, P_C);

    push(((ParseFrame){ .tp = nodFor, .startNodeInd = cm->nodes.len, .sentinel = sentinel,
                        .typeId = cm->stats.loopCounter }), cm->backtrack);
    addNode((Node){.tp = nodFor}, locOf(forTk), cm);

    // variable initialization
    Int sndInd = minPositiveOf(3, condInd, stepInd, bodyInd);
    if (sndInd > initInd) {
        openParsedScope(sentinel, locOf(forTk), cm);
        for (cm->i = initInd; cm->i < sndInd;) {
            // parse assignments
            Token tok = toks[cm->i];
            cm->i += 1; // CONSUME the assignment span marker
            pAssignment(tok, P_C);
        }
    }
    // loop condition
    if (condInd != 0)  {
        Token condTok = toks[condInd];

// TODO handle single-token conditions, like a single var or a literal
        cm->i = condInd + 1; // +1 cause the expression parser needs to be 1 past the exprToken

        TypeId condTypeId = exprUpToWithFrame(
                (ParseFrame){ .tp = nodExpr, .startNodeInd = cm->nodes.len,
                              .sentinel = minPositiveOf(3, stepInd, bodyInd, sentinel),
                              .typeId = cm->stats.loopCounter },
                locOf(condTok), P_C);
        VALIDATEP(condTypeId == tokBool, errTypeMustBeBool)
    }

    // readying to parse the body + step statements
    Int bodyStartBt = toks[sndInd].startBt;
    const Int bodyNodeInd = cm->nodes.len;

    cm->nodes.cont[forNodeInd].pl3 = bodyNodeInd - forNodeInd; // distance to inner scope
    openParsedScope(sentinel, (SourceLoc) {.startBt = bodyStartBt,
                                           .lenBts = forTk.lenBts - bodyStartBt + forTk.startBt },
                    cm);

    cm->i = minPositiveOf(2, stepInd, bodyInd); // CONSUME the "for" until the loop body + step
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
    if (frame.tp == nodScope || frame.tp == nodFnDef) { // a nodFnDef acts as a scope itself
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
        Int varId = getActiveVar(tk.pl1, cm);
        typeId = getTypeOfVar(varId, cm);
        addNode((Node){.tp = nodId, .pl1 = varId, .pl2 = tk.pl1}, locOf(tk), cm);
    } else if (tk.tp == tokOperator) {
        Int operBindingId = tk.pl1;
        OpDef operDefinition = OPERATORS[operBindingId];
        VALIDATEP(operDefinition.arity == 1, errOperatorWrongArity)
        addNode((Node){ .tp = nodId, .pl1 = operBindingId}, locOf(tk), cm);
        // TODO add the type when we support first-class functions
    } else if (tk.tp <= topVerbatimType) {
        addNode((Node){.tp = tk.tp, .pl1 = tk.pl1, .pl2 = tk.pl2}, locOf(tk), cm);
        typeId = tk.tp;
    } else {
        throwExcParser(errUnexpectedToken);
    }
    return typeId;
}


private void subexDataAllocation(ExprFrame frame, StateForExprs* stEx, Compiler* cm) {
//:subexDataAllocation Creates an assignment in main. Then walks over the data allocator nodes
// and counts elements that are subexpressions. Then copies the nodes from scratch to main,
// careful to wrap subexpressions in a nodExpr. Finally, replaces the copied nodes in scr with
// an id linked to the new entity
    StackNode* scr = stEx->scr;  // ((ind in scr) (count of nodes in subexpr))
    Node rawNd = pop(stEx->calls);
    SourceLoc rawLoc = pop(stEx->locsCalls);

    const EntityId newEntityId = cm->entities.len;
    pushInentities(((Entity) { .class = classImmut }), cm);

    Int countElements = 0;
    Int countNodes = scr->len - frame.startNode;
    for (Int j = frame.startNode; j < scr->len; ++j)  {
        Node nd = scr->cont[j];
        countElements += 1;

        if (nd.tp == nodExpr) {
            j += nd.pl2;
        }
    }

    addNode((Node){.tp = nodAssignment, .pl1 = 0, .pl2 = countNodes + 2, .pl3 = 2}, rawLoc, cm);
    addNode((Node){.tp = nodBinding, .pl1 = newEntityId, .pl2 = -1}, rawLoc, cm);
    addNode((Node){.tp = nodDataAlloc, .pl1 = rawNd.pl1, .pl2 = countNodes, .pl3 = countElements },
            rawLoc, cm);

    Int startNodeInd = cm->nodes.len;
    saveNodes(frame.startNode, scr, stEx->locsScr, cm);
    if (countNodes > 0)  {
        TypeId eltType = typecheckList(startNodeInd, cm);
        cm->entities.cont[newEntityId].typeId = tCreateSingleParamTypeCall(
            cm->activeBindings[stToNameId(strL)], eltType, cm
        );
    }

    scr->len = frame.startNode + 1;
    stEx->locsScr->len = frame.startNode + 1;
    scr->cont[frame.startNode] = (Node){ .tp = nodId, .pl1 = newEntityId, .pl2 = -1 };
}


private void eBumpArgCount(StateForExprs* stEx) { //:eBumpArgCount
    const Int ind = stEx->frames->len - 1;
    const Int tp = stEx->frames->cont[ind].tp;
    if (tp == exfrCall || tp == exfrDataAlloc) {
        stEx->frames->cont[ind].argCount += 1;
    }
}


private void ePopUnaryCalls(StateForExprs* stEx) { //:ePopUnaryCalls
    ExprFrame* zero = stEx->frames->cont;
    ExprFrame* parent = zero + (stEx->frames->len - 1);
    Bool poppedAnyUnaries = false;
    for (; parent >= zero && parent->tp == exfrUnaryCall;
           parent -= 1) {
        pop(stEx->frames);
        Node call = pop(stEx->calls);
        call.pl2 = 1;
        push(call, stEx->scr);
        push(pop(stEx->locsCalls), stEx->locsScr);
        poppedAnyUnaries = true;
    }
}


private void eClose(StateForExprs* stEx, Compiler* cm) { //:eClose
// Flushes the finished subexpr frames from the top of the funcall stack. Handles data allocations
    StackNode* scr = stEx->scr;
    while (stEx->frames->len > 0 && cm->i == peek(stEx->frames).sentinel) {
        ExprFrame frame = pop(stEx->frames);
        if (frame.tp == exfrCall)  {
            Node call = pop(stEx->calls);
            call.pl2 = frame.argCount;
            if (frame.startNode > -1 && scr->cont[frame.startNode].tp == nodExpr) {
                // Call inside a data allocator. It was wrapped in a nodExpr, now we set its length
                scr->cont[frame.startNode].pl2 = scr->len - frame.startNode - 1;
            }

            push(call, scr);
            push(pop(stEx->locsCalls), stEx->locsScr);
        } else if (frame.tp == exfrDataAlloc)  {
            subexDataAllocation(frame, stEx, cm);
        } else if (frame.tp == exfrParen) {
            ePopUnaryCalls(stEx);
            if (stEx->frames->len > 0) {
                eBumpArgCount(stEx);
            }
            if (scr->cont[frame.startNode].tp == nodExpr) {
                // Parens inside data allocator. It was wrapped in a nodExpr, now we set its length
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


private void eLinearize(Int sentinel, P_CT) { //:eLinearize
// The core code of the general, long expression parse. Starts at cm->i and parses until
// "sentinel". Produces a linear sequence of operands and operators with arg counts in
// Reverse Polish Notation. Handles data allocations, too. But not single-item exprs
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

    push(((ExprFrame){ .tp = exfrParen, .sentinel = sentinel}), frames);

    for (; cm->i < sentinel; cm->i += 1) { // CONSUME any expression token
        eClose(stEx, cm);
        ExprFrame parent = peek(frames);
        Token cTk = toks[cm->i];
        SourceLoc loc = locOf(cTk);
        Unt tokType = cTk.tp;
        if (tokType == tokWord && (cm->i + 1) < sentinel && toks[cm->i + 1].tp == tokAccessor) {
            // accessor like `a[i]`, but on this iteration we parse only the `a`
            Int accSentinel = calcSentinel(toks[cm->i + 1], cm->i + 1);
            push(((ExprFrame){ .tp = exfrParen, .startNode = scr->len,
                    .sentinel = accSentinel }), frames);
            EntityId varId = getActiveVar(cTk.pl1, cm);
            push(((Node){ .tp = nodId, .pl1 = varId, .pl2 = cTk.pl1 }), scr);
            push(locOf(cTk), locsScr);
        } else if (tokType == tokAccessor) {
            // accessor like `a[i]`, now we parse the `[i]`
            const Int accSentinel = calcSentinel(cTk, cm->i);
            pAddAccessorCall(accSentinel, loc, stEx);
        } else if ((tokType == tokWord || tokType == tokOperator) && parent.tp == exfrParen) {
            pAddFunctionCall(cTk, parent.sentinel, stEx);
        } else if (tokType <= topVerbatimTokenVariant || tokType == tokWord) {
            if (tokType == tokWord) {
                EntityId varId = getActiveVar(cTk.pl1, cm);
                push(((Node){ .tp = nodId, .pl1 = varId, .pl2 = cTk.pl1 }), scr);
            } else {
                push(((Node){ .tp = cTk.tp, .pl1 = cTk.pl1, .pl2 = cTk.pl2 }), scr);
            }
            push(loc, locsScr);
            ePopUnaryCalls(stEx);
            eBumpArgCount(stEx);
        } else if (tokType == tokParens) {
            Int parensSentinel = calcSentinel(cTk, cm->i);
            push(((ExprFrame){
                    .tp = exfrParen, .startNode = scr->len, .sentinel = parensSentinel }), frames);

            if (parent.tp == exfrDataAlloc) { // inside a data allocator, subexpressions need to
                                              // be marked with nodExpr for t-checking & codegen
                push(((Node){ .tp = nodExpr, .pl1 = 0 }), scr);
                push(loc, locsScr);
            }
        } else if (tokType == tokOperator) {
            if (OPERATORS[cTk.pl1].arity == 1) {
                pAddUnaryCall(cTk, stEx);
            } else {
                push(((Node){ .tp = nodId, .pl2 = cTk.pl1 }), scr);
                push(loc, locsScr);
                eBumpArgCount(stEx);
                ePopUnaryCalls(stEx);
            }
        } else if (tokType == tokDataList) {
            stEx->metAnAllocation = true;
            eBumpArgCount(stEx);
            push(((ExprFrame) { .tp = exfrDataAlloc, .startNode = scr->len,
                               .sentinel = calcSentinel(cTk, cm->i)}), frames
            );
            push(((Node) { .tp = nodDataAlloc, .pl1 = stToNameId(strL) }), calls);
            push(loc, locsCalls);
        } else {
            throwExcParser(errExpressionCannotContain);
        }
    }
    eClose(stEx, cm);
}


private TypeId exprUpToWithFrame(ParseFrame frame, SourceLoc loc, P_CT) {
//:exprUpToWithFrame The main "big" expression parser. Parses an expression whether there is a
// token or not. Starts from cm->i and goes up to the sentinel token. Returns the expression's type
// Precondition: we are looking 1 past the tokExpr or tokParens
    if (cm->i + 1 == frame.sentinel) { // the [stmt 1, tokInt] case
        Token singleToken = toks[cm->i];
        if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) {
            cm->i += 1; // CONSUME the single literal
            return exprSingleItem(singleToken, cm);
        }
    }

    const Int startNodeInd = cm->nodes.len;
    push(frame, cm->backtrack);
    addNode((Node){ .tp = nodExpr}, loc, cm);

    eLinearize(frame.sentinel, P_C);
    exprCopyFromScratch(startNodeInd, cm);
    Int exprType = typeCheckBigExpr(startNodeInd, cm->nodes.len, cm);
    mbCloseSpans(cm);
    return exprType;
}


private TypeId exprUpTo(Int sentinelToken, SourceLoc loc, P_CT) { //:exprUpTo
// The main "big" expression parser. Parses an expression whether there is a token or not.
// Precondition: we are looking 1 past the tokExpr or tokParens
// Starts from cm->i and goes up to the sentinel token.
// Emits a nodExpr and opens a corresponding parse frame
// Returns the expression's type
    Int startNodeInd = cm->nodes.len;
    push(((ParseFrame){.tp = nodExpr, .startNodeInd = startNodeInd,
                .sentinel = sentinelToken }), cm->backtrack);
    addNode((Node){ .tp = nodExpr}, loc, cm);

    eLinearize(sentinelToken, P_C);
    exprCopyFromScratch(startNodeInd, cm);
    Int exprType = typeCheckBigExpr(startNodeInd, cm->nodes.len, cm);
    return exprType;
}


private TypeId exprHeadless(Int sentinel, SourceLoc loc, P_CT) { //:exprHeadless
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
    return exprUpTo(sentinel, loc, P_C);
}


private Int pExprWorker(Token tok, P_CT) { //:pExprWorker
// Precondition: we are looking 1 past the first token of expr, which is the first parameter.
// Consumes 1 or more tokens. Handles single items also Returns the type of parsed expression
    if (tok.tp == tokStmt || tok.tp == tokParens || tok.tp == tokIntro) {
        if (tok.pl2 == 1) {
            Token singleToken = toks[cm->i];
            if (singleToken.tp <= topVerbatimTokenVariant || singleToken.tp == tokWord) {
                // [stmt 1, tokInt]
                cm->i += 1; // CONSUME the single literal token
                return exprSingleItem(singleToken, cm);
            }
        }

        return exprUpTo(cm->i + tok.pl2, locOf(tok), P_C);
    } else {
        return exprSingleItem(tok, cm);
    }
}


private void pExpr(Token tok, P_CT) {
    pExprWorker(tok, P_C);
}


private void mbCloseSpans(Compiler* cm) { //:mbCloseSpans
// When we are at the end of a function parsing a parse frame, we might be at the end of said frame
// (otherwise => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
// in which case this function handles all the corresponding stack poppin'.
// It also always handles updating all inner frames with consumed tokens
// This is safe to call anywhere, pretty much
    while (hasValues(cm->backtrack)) { // loop over subscopes and expressions inside FunctionDef
        ParseFrame frame = peek(cm->backtrack);
        if (cm->i < frame.sentinel) {
            return;
        }
#ifdef SAFETY
        if (cm->i != frame.sentinel) {
            print("Span inconsistency i %d  frame.sentinelToken %d frametp %d startInd %d",
                cm->i, frame.sentinel, frame.tp, frame.startNodeInd);
        }
        VALIDATEI(cm->i == frame.sentinel, iErrorInconsistentSpans)
#endif
        popFrame(cm);
    }
}


private void parseUpTo(Int sentinelToken, P_CT) { //:parseUpTo
// Parses anything from current cm->i to "sentinelToken"
    while (cm->i < sentinelToken) {
        Token currTok = toks[cm->i];
        cm->i += 1;
        (PARSE_TABLE[currTok.tp])(currTok, P_C);
        mbCloseSpans(cm);
    }
}


private void setClassToMutated(Int bindingId, Compiler* cm) { //:setClassToMutated
// Changes a mutable variable to mutated. Throws an exception for an immutable one
    Int class = cm->entities.cont[bindingId].class;
    VALIDATEP(class < classImmut, errCannotMutateImmutable);
    if (class % 2 == 0) {
        cm->entities.cont[bindingId].class += 1;
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
// Returns the number of levels to break/continue to, or 1 if there weren't any specified
    VALIDATEP(tok.pl2 <= 1, errBreakContinueTooComplex);
    Int unwindLevel = 1;
    *sentinel = cm->i;
    if (tok.pl2 == 1) {
        Token nextTok = toks[cm->i];
        VALIDATEP(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errBreakContinueInvalidDepth)

        unwindLevel = nextTok.pl2;
        (*sentinel) += 1; // CONSUME the Int after the `break`
    }
    if (unwindLevel == 1) {
        return 1;
    }

    Int j = cm->backtrack->len;
    while (j > -1) {
        Int pfType = cm->backtrack->cont[j].tp;
        if (pfType == nodFor) {
            unwindLevel -= 1;
            if (unwindLevel == 0) {
                ParseFrame loopFrame = cm->backtrack->cont[j];
                Int loopId = loopFrame.typeId;
                cm->nodes.cont[loopFrame.startNodeInd].pl1 = loopId;
                return unwindLevel == 1 ? -1 : loopId;
            }
        }
        j -= 1;
    }
    throwExcParser(errBreakContinueInvalidDepth);
}


private void pBreakCont(Token tok, P_CT) { //:pBreakCont
    Int sentinel = cm->i;
    Int loopId = breakContinue(tok, &sentinel, P_C);
    if (tok.pl1 > 0) { // continue
        loopId += BIG;
    }
    addNode((Node){.tp = nodBreakCont, .pl1 = loopId}, locOf(tok), cm);
    cm->i = sentinel; // CONSUME the whole break statement
}


private void parseCatch(Token tok, P_CT) {
    throwExcParser(errTemp);
}

private void parseDefer(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void pTrait(Token tok, P_CT) {
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


private void parsePackage(Token tok, P_CT) {
    throwExcParser(errTemp);
}


private void typecheckFnReturn(Int typeId, Compiler* cm) { //:typecheckFnReturn
    Int j = cm->backtrack->len - 1;
    while (j > -1 && cm->backtrack->cont[j].tp != nodFnDef) {
        j -= 1;
    }
    const Int fnTypeInd = cm->backtrack->cont[j].typeId;
    const TypeId returnType = getFunctionReturnType(fnTypeInd, cm);
    VALIDATEP(returnType == typeId, errTypeWrongReturnType)
}


private void pReturn(Token tok, P_CT) { //:pReturn
    Int lenTokens = tok.pl2;
    Int sentinelToken = cm->i + lenTokens;

    push(((ParseFrame){ .tp = nodReturn, .startNodeInd = cm->nodes.len,
                        .sentinel = sentinelToken }), cm->backtrack);
    addNode((Node){.tp = nodReturn}, locOf(tok), cm);

    Token rTk = toks[cm->i];
    SourceLoc loc = {.startBt = rTk.startBt, .lenBts = tok.lenBts - rTk.startBt + tok.startBt};
    Int typeId = exprHeadless(sentinelToken, loc, P_C);
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


testable void importEntities(Arr(Entity) impts, Int countEntities,
                             Compiler* cm) { //:importEntities
    for (int j = 0; j < countEntities; j++) {
        importActivateEntity(impts[j], cm);
    }
    cm->stats.countNonparsedEntities = cm->entities.len;
}


private void tabulateParser() { //:tabulateParser
    ParserFn* p = PARSE_TABLE;
    int i = 0;
    while (i <= firstSpanTokenType) {
        p[i] = &parseErrorBareAtom;
        i += 1;
    }
    p[tokScope]       = &pScope;
    p[tokStmt]        = &pExpr;
    p[tokParens]      = &parseErrorBareAtom;
    p[tokAssignment]  = &pAssignment;
    p[tokMisc]        = &parseMisc;

    p[tokAlias]       = &pAlias;
    p[tokAssert]      = &parseAssert;
    p[tokBreakCont]   = &pBreakCont;
    p[tokCatch]       = &pAlias;
    p[tokFn]          = &pAlias;
    p[tokTrait]       = &pAlias;
    p[tokImport]      = &pAlias;
    p[tokReturn]      = &pReturn;
    p[tokTry]         = &pAlias;

    p[tokIf]          = &pIf;
    p[tokElseIf]      = &pElseIf;
    p[tokElse]        = &pElse;
    p[tokFor]         = &pFor;
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
// - types that are sufficient for the built-in operators
// - entities with the built-in operator entities
// - overloadIds with counts
    Compiler* proto = allocateOnArena(sizeof(Compiler), a);
    (*proto) = (Compiler){
        .entities = createInListEntity(32, a),
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
    setStmtSpanLength(top.tokenInd, lx);
    mbCloseAssignRight(&top, lx);
    VALIDATEL(top.spanLevel != slScope && !hasValues(lx->lexBtrack), errPunctuationExtraOpening)
}


testable Compiler* lexicallyAnalyze(String* sourceCode, Compiler* proto, Arena* a) {
//:lexicallyAnalyze Main lexer function. Precondition: the input Byte array has been prepended
// with StandardText
    Compiler* lx = createLexerFromProto(sourceCode, proto, a);
    const Int inpLength = lx->stats.inpLength;
    Arr(Byte) inp = lx->sourceCode->cont;
    VALIDATEL(inpLength > 0, "Empty input")

    // Main loop over the input
    if (setjmp(excBuf) == 0) {
        while (lx->i < inpLength) {
            (LEX_TABLE[inp[lx->i]])(inp, lx);
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
    scopeStack->len += 1;
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
    topScope->len += 1;

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
#ifdef TRACE
                printf("ScopeStack is freeing a chunk of memory at %p next = %p\n", ch, nextToDelete);
#endif
                free(ch);

                if (nextToDelete == null) break;
                ch = nextToDelete;

            } while (ch != null);
        }
    }
    scopeStack->len -= 1;
}


private void addRawOverload(NameId nameId, TypeId typeId, EntityId entityId, Compiler* cm) {
//:addRawOverload Adds an overload of a function to the [rawOverloads] and activates it, if needed
    Int mbListId = -cm->activeBindings[nameId] - 2;
    FirstArgTypeId firstParamType = getFirstParamType(typeId, cm);
    if (mbListId == -1) {
        Int newListId = listAddMultiAssocList(firstParamType, entityId, cm->rawOverloads);
        cm->activeBindings[nameId] = -newListId - 2;
        cm->stats.countOverloadedNames += 1;
    } else {
        Int updatedListId = addMultiAssocList(firstParamType, entityId, mbListId,
                                              cm->rawOverloads);
        if (updatedListId != -1) {
            cm->activeBindings[nameId] = -updatedListId - 2;
        }
    }
    cm->stats.countOverloads += 1;
}


private TypeId mergeTypeWorker(Int startInd, Int lenInts, Compiler* cm) { //:mergeTypeWorker
    Arr(Int) types = cm->types.cont;
    StringDict* hm = cm->typesDict;
    const Int lenBts = lenInts*4;
    Unt theHash = hashCode((Byte*)(types + startInd), lenBts);
    Int hashOffset = theHash % (hm->dictSize);

    if (*(hm->dict + hashOffset) == null) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue),
                hm->a);
        newBucket->capAndLen = (initBucketSize << 16) + 1; // left u16 = cap, right u16 = len
        StringValue* firstElem = (StringValue*)newBucket->cont;
        *firstElem = (StringValue){.hash = theHash, .indString = startInd };
        *(hm->dict + hashOffset) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hashOffset);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->cont;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].hash == theHash
                  && memcmp(types + stringValues[i].indString, types + startInd, lenBts) == 0) {
                // key already present
                cm->types.len -= lenInts;
                return stringValues[i].indString;
            }
        }
        addValueToBucket((hm->dict + hashOffset), startInd, lenInts, hm->a);
    }
    hm->len += 1;
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


private Unt stToFullName(Int sta, Compiler* cm) { //:stToFullName
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
    pushInentities((Entity){ .typeId = typeId, .class = classImmut }, cm);
    addRawOverload(operId, typeId, newEntityId, cm);
}


private void buildOperators(Compiler* cm) { //:buildOperators
// Operators are the first-ever functions to be defined. This function builds their @types,
// @functions and overload counts. The order must agree with the order of operator
// definitions in eyr.internal.h, and every operator must have at least one type defined
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
    buildOperator(opToString,     strOfInt, cm); // TODO
    buildOperator(opToString,     strOfBool, cm);
    buildOperator(opToString,     strOfFloat, cm);
    buildOperator(opRemainder,    intOfIntInt, cm);
    buildOperator(opBitwiseAnd,   boolOfBoolBool, cm); // dummy
    buildOperator(opBoolAnd,      boolOfBoolBool, cm);
    buildOperator(opPtr,          boolOfBoolBool, cm); // dummy
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
    buildOperator(opLTZero,       boolOfIntInt, cm);
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
    buildOperator(opIsNull,       boolOfIntInt, cm); // dummy
    buildOperator(opEquality,     boolOfIntInt, cm);
    buildOperator(opIntervalBo,   boolOfIntIntInt, cm); // dummy
    buildOperator(opIntervalBo,   boolOfFlFlFl, cm); // dummy
    buildOperator(opIntervalR,    boolOfIntIntInt, cm); // dummy
    buildOperator(opIntervalR,    boolOfFlFlFl, cm); // dummy
    buildOperator(opIntervalL,    boolOfIntIntInt, cm); // dummy
    buildOperator(opIntervalL,    boolOfFlFlFl, cm); // dummy
    buildOperator(opBitShiftR,    boolOfBoolBool, cm); // dummy
    buildOperator(opGTZero,       boolOfIntInt, cm);
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
    buildOperator(opUnused,       flOfFlFl, cm); // a dummy type
    buildOperator(opBitwiseOr,    intOfIntInt, cm); // dummy
    buildOperator(opBoolOr,       flOfFl, cm); // dummy
    buildOperator(opGetElem,      flOfFl, cm); // dummy
    buildOperator(opGetElemPtr,   flOfFl, cm); // dummy
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

    Entity imports[6] =  {
        (Entity){ .name = nameOfStandard(strMathPi), .typeId = tokDouble, .class = classPubImmut,
                   .emit = {.kind = uncallableLiteral,
                            .body = {.literal = longOfDoubleBits(3.14159265358)} }
        },
        (Entity){ .name = nameOfStandard(strMathE), .typeId = tokDouble, .class = classPubImmut,
                   .emit = {.kind = uncallableLiteral, .body = {.literal = longOfDoubleBits(2.71)}}
        },
        (Entity){ .name = nameOfStandard(strPrint), .typeId = strToVoid, .class = classPubImmut,
                   .emit = {.kind = callableInstr, .body = {.opcode = iPrint} }
        },
        // TODO define print x = print $x as AST nodes injected into every build
        (Entity){ .name = nameOfStandard(strPrint), .typeId = intToVoid,
                   .emit = {.kind = callableDefined, .body = {.nodeInd = -1} }
        },
        (Entity){ .name = nameOfStandard(strPrint), .typeId = floatToVoid,
                   .emit = {.kind = callableDefined, .body = {.nodeInd = -1} }
        },
        (Entity){ .name = nameOfStandard(strPrintErr), .typeId = strToVoid,
                   .emit = {.kind = callableInstr, .body = {.opcode = iPrintErr} }
        }
       // TODO functions for casting (int, double, unsigned)
    };
    // These base types occupy the first places in the stringTable and in the types table.
    // So for them nameId == typeId, unlike type funcs like L(ist) and A(rray)
    for (Int j = strInt; j <= strVoid; j++) {
        cm->activeBindings[j - strInt + countOperators] = j - strInt;
    }

    importEntities(imports, sizeof(imports)/sizeof(Entity), cm);
}


testable Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a) {
//:createLexerFromProto A proto compiler contains just the built-in definitions and tables. This fn
// copies it and performs initialization. Post-condition: i has been incremented by the
// standardText size
    Compiler* lx = allocateOnArena(sizeof(Compiler), a);
    Arena* aTmp = createArena();
    (*lx) = (Compiler){
        // this assumes that the source code is prefixed with the "standardText"
        .i = sizeof(standardText) - 1,
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
        .locsCalls = createStackSourceLoc(16*sizeof(SourceLoc), a),
        .reorderBuf = createStackToken(16*sizeof(Token), a)
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
        o += 1;
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
        if (tok.tp == tokAssignment && toks[cm->i + 1].tp == tokTypeName) {
            cm->i += 1; // CONSUME the assignment token
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
        if (tok.tp == tokAssignment) {
            Token rightTk = toks[cm->i + 3]; // 3 to skip the binding, tokAssignRight and tokFn
            if (rightTk.tp != tokFn) {
                cm->i += 1; // CONSUME the left and right assignment
                pAssignment(tok, P_C);
                //parseUpTo(cm->i + toks[cm->i + 1].pl2 + 1, P_C);
            } else {
                cm->i = calcSentinel(tok, cm->i); // CONSUME the top-level assignment
            }
        } else {
            cm->i = calcSentinel(tok, cm->i);
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


testable void pFnSignature(Token fnDef, bool isToplevel, Unt name, Int voidToVoid,
                               Compiler* cm) { //:pFnSignature
// Parses a function signature. Emits no nodes, adds data to @toplevels, @functions, @overloads.
// Pre-condition: we are 1 token past the tokFn
    Int fnStartTokenId = cm->i - 1;
    Int fnSentinelToken = calcSentinel(fnDef, fnStartTokenId);

    NameId fnNameId = name & LOWER24BITS;

    pushIntypes(0, cm); // will overwrite it with the type's length once we know it

    Arr(Token) toks = cm->tokens.cont;
    TypeId newFnTypeId = voidToVoid; // default for nullary functions
    StateForTypes* st = cm->stateForTypes;
    if (toks[cm->i].tp == tokIntro) {
        Token paramListTk = toks[cm->i];
        Int sentinel = calcSentinel(paramListTk, cm->i);
        cm->i += 1; // CONSUME the paramList
        st->frames->len = 0;
        push(((TypeFrame) { .tp = sorFunction, .sentinel = sentinel }), st->frames);
        newFnTypeId = tDefinition(st, sentinel, cm);
    }

    EntityId newFnEntityId = cm->entities.len;
    pushInentities(((Entity){ .class = classImmut, .typeId = newFnTypeId }), cm);
    addRawOverload(fnNameId, newFnTypeId, newFnEntityId, cm);
    pushIntoplevels((Toplevel){.indToken = fnStartTokenId, .sentinelToken = fnSentinelToken,
            .name = name, .entityId = newFnEntityId }, cm);
}


private void pToplevelBody(Int indToplevel, Arr(Token) toks, Compiler* cm) {
//:pToplevelBody Parses a top-level function. The result is the AST
//[ FnDef ParamList body... ]
    Toplevel toplevelSignature = cm->toplevels.cont[indToplevel];
    cm->toplevels.cont[indToplevel].indNode = cm->nodes.len;
    Int fnStartInd = toplevelSignature.indToken;

    const Int fnSentinel = toplevelSignature.sentinelToken;
    EntityId fnEntity = toplevelSignature.entityId;
    TypeId fnType = cm->entities.cont[fnEntity].typeId;

    cm->i = fnStartInd; // tokFn
    Token fnTk = toks[cm->i];

    openFnScope(fnEntity, toplevelSignature.name, fnType, fnTk, fnSentinel, cm);

    cm->i += 1; // CONSUME the tokFn token
    Token mbParamsTk = toks[cm->i];
    if (mbParamsTk.tp == tokIntro) {
        const Int paramsSentinel = cm->i + mbParamsTk.pl2 + 1;
        cm->i += 1; // CONSUME the tokIntro
        TypeId paramTypeInd = tGetIndexOfFnFirstParam(fnType, cm);
        while (cm->i < paramsSentinel) {
            // need to get params type from the function type we got, not
            // from tokens (where they may be generic)
            Token paramName = toks[cm->i];
            if (paramName.tp == tokMisc) {
                break;
            }
            Int newEntityId = createEntityWithType(
                    nameOfToken(paramName), cm->types.cont[paramTypeInd], (Emit){
                        .kind = callableDefined, .body = {.nodeInd = -1} // to be filled in later
                    },
                    paramName.pl1 == 1 ? classMut : classImmut, cm
            );
            addNode(((Node){.tp = nodBinding, .pl1 = newEntityId, .pl2 = 0}), locOf(paramName), cm);

            cm->i += 1; // CONSUME the param name
            cm->i = calcSentinel(toks[cm->i], cm->i); // CONSUME the tokens of param type
            paramTypeInd += 1;
        }
        cm->i = paramsSentinel;
    }
    parseUpTo(fnSentinel, P_C);
}


private void pFunctionBodies(Arr(Token) toks, Compiler* cm) { //:pFunctionBodies
// Parses top-level function params and bodies
    for (int j = 0; j < cm->toplevels.len; j++) {
        cm->stats.loopCounter = 0;
        pToplevelBody(j, P_C);
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
        nextI = calcSentinel(tok, cm->i);
        if (tok.tp != tokAssignment || tok.pl2 <= 3) { // toplevel func has at least 4 tokens
            continue;
        }
        Token nameTk = toks[cm->i + 1];
        VALIDATEP(nameTk.tp == tokWord && nameTk.pl2 == 0, errAssignmentToplevelFn)
        Token rightTk = toks[cm->i + 2]; // the token after tokAssignRight
        const Int fnInd = cm->i + 3; // skipping the name & the tokAssignRight
        if (rightTk.tp != tokAssignRight || toks[fnInd].tp != tokFn) {
            continue;
        }

        // since this is an immutable definition tokAssignment, its pl1 is the nameId and
        // its bytes are same as the var name in source code
        Unt name =  ((Unt)nameTk.lenBts << 24) + (Unt)nameTk.pl1;
        cm->i = fnInd + 1; // CONSUME the left side, tokAssignmentRight and tokFn
        pFnSignature(toks[fnInd], true, name, voidToVoid, cm);
    }
}


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
    clearArena(cm->aTmp);
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

private Int typeEncodeTag(Unt sort, Int depth, Int arity, Compiler* cm) {
    return (Int)((Unt)(sort << 16) + (depth << 8) + arity);
}


testable void typeAddHeader(TypeHeader hdr, Compiler* cm) { //:typeAddHeader
// Writes the bytes for the type header to the tail of the cm->types table.
// Adds two 4-byte elements
    pushIntypes((Int)((Unt)((Unt)hdr.sort << 16) + ((Unt)hdr.arity << 8) + hdr.tyrity), cm);
    pushIntypes((Int)hdr.nameAndLen, cm);
}


testable TypeHeader typeReadHeader(TypeId typeId, Compiler* cm) { //:typeReadHeader
// Reads a type header from the type array
    Int tag = cm->types.cont[typeId + 1];
    return (TypeHeader){ .sort = ((Unt)tag >> 16) & LOWER16BITS,
            .arity = (tag >> 8) & 0xFF, .tyrity = tag & 0xFF,
            .nameAndLen = (Unt)cm->types.cont[typeId + 2] };
}


private Int typeGetTyrity(TypeId typeId, Compiler* cm) { //:typeGetTyrity
    if (cm->types.cont[typeId] == 0) {
        return 0;
    }
    Int tag = cm->types.cont[typeId + 1];
    return tag & 0xFF;
}


private OuterTypeId typeGetOuter(FirstArgTypeId typeId, Compiler* cm) { //:typeGetOuter
// A          => A  (concrete types)
// (A B)      => A  (concrete generic types)
// A + (A B)  => -2 (param generic types)
// (F A -> B) => (F A -> B)
    if (typeId <= topVerbatimType) {
        return typeId;
    }
    TypeHeader hdr = typeReadHeader(typeId, cm);
    if (hdr.sort <= sorFunction) {
        return typeId;
    } else if (hdr.sort == sorTypeCall) {
        return hdr.nameAndLen;
    } else {
        return cm->types.cont[typeId + hdr.tyrity + TYPE_PREFIX_LEN] & LOWER24BITS;
    }
}

private TypeId getGenericParam(TypeId t, Int ind, Compiler* cm) { //:getGenericParam
// (L Foo) -> Foo
    return cm->types.cont[t + TYPE_PREFIX_LEN + ind];
}


private TypeId tGetIndexOfFnFirstParam(TypeId fnType, Compiler* cm) { //:tGetIndexOfFnFirstParam
    TypeHeader hdr = typeReadHeader(fnType, cm);
#ifdef SAFETY
    VALIDATEI(hdr.sort == sorFunction, iErrorNotAFunction);
#endif
    return fnType + TYPE_PREFIX_LEN + hdr.tyrity;
}


private Int tGetFnArity(TypeId fnType, Compiler* cm) { //:tGetFnArity
    TypeHeader hdr = typeReadHeader(fnType, cm);
#ifdef SAFETY
    VALIDATEI(hdr.sort == sorFunction, iErrorNotAFunction);
#endif
    return hdr.arity;
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
            midInd -= 1;
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
    const Int countNames = names->len;
    names->len = 0;
    return countNames;
}


/*
private TypeId typeCreateRecord(StateForTypes* st, Int startInd, Unt nameAndLen,
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
    return mergeType(tentativeTypeId, cm);
}


private TypeId tCreateSingleParamTypeCall(TypeId outer, TypeId param, Compiler* cm) {
//:tCreateSingleParamTypeCall Creates a type like (L Int)
    const Int tentativeTypeId = cm->types.len;
    pushIntypes(0, cm);
    TypeId listType = cm->activeBindings[stToNameId(strL)];
    typeAddHeader((TypeHeader){
        .sort = sorTypeCall, .tyrity = 1, .arity = 0, .nameAndLen = listType }, cm);
    pushIntypes(param, cm);

    cm->types.cont[tentativeTypeId] = cm->types.len - tentativeTypeId - 1;
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
                                  Unt fnNameAndLen, Compiler* cm) { //:tCreateFnSignature
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


private void teClose(StateForTypes* st, Compiler* cm) { //:teClose
// Flushes the finished subexpr frames from the top of the funcall stack. Handles data allocations
    StackInt* exp = st->exp;
    StackTypeFrame* frames = st->frames;
    while (frames->len > 0 && peek(frames).sentinel == cm->i) {
        TypeFrame frame = pop(frames);

        Int startInd = exp->len - frame.countArgs;
        Int newTypeId = -1;
        if (frame.tp == sorFunction) {
            newTypeId = tCreateFnSignature(st, startInd, -1, cm);
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
// Parses a type definition like `Rec(id Int name Str)` or `a Double b (L Bool) -> String`.
// Precondition: we are looking at the first token in actual type definition.
// @typeStack may be non-empty (for example, when parsing a function signature,
// it will contain one element with .tp = sorFunction). Produces a linear, RPN sequence.
    StackInt* exp = st->exp;
    StackInt* params = st->params;
    StackTypeFrame* frames = st->frames;
    exp->len = 0;
    params->len = 0;
    Bool metArrow = false;
    Bool isFnSignature = hasValues(frames) && peek(frames).tp == sorFunction;

    while (cm->i < sentinel) {
        teClose(st, cm);

        Token cTk = cm->tokens.cont[cm->i];
        cm->i += 1; // CONSUME the current token

        VALIDATEP(hasValues(frames), errTypeDefError)
        if (cTk.tp == tokWord) {
            VALIDATEP(cm->i < sentinel, errTypeDefError)
            Int ctxType = peek(frames).tp;
            VALIDATEP(ctxType == sorRecord || ctxType == sorFunction, errTypeDefError)

            Token nextTk = cm->tokens.cont[cm->i];
            VALIDATEP(nextTk.tp == tokTypeName || nextTk.tp == tokTypeCall, errTypeDefError)
            push(cTk.pl1, st->names);
            continue;
        } else if (cTk.tp == tokMisc) { // "->", function return type marker
            VALIDATEP(cTk.pl1 == miscArrow && peek(frames).tp == sorFunction, errTypeDefError);
            VALIDATEP(!metArrow, errTypeFnSingleReturnType)
            metArrow = true;

            if (cm->i < sentinel) { // no return type specified = void type
                Unt nextTp = cm->tokens.cont[cm->i].tp;
                VALIDATEP(nextTp == tokTypeName || nextTp == tokTypeCall, errTypeDefError)
            } else {
                push((cm->activeBindings[stToNameId(strVoid)]), exp);
                frames->cont[frames->len - 1].countArgs += 1;
            }

            continue;
        }

        frames->cont[frames->len - 1].countArgs += 1;

        if (cTk.tp == tokTypeName) {
            // arg count
            Int mbParamId = typeParamBinarySearch(cTk.pl1, cm);
            if (mbParamId == -1) {
                push(cm->activeBindings[cTk.pl1], exp);
            } else {
                push(-mbParamId - 1, exp); // index of this param in @params
            }
        } else if (cTk.tp == tokTypeCall) {
            VALIDATEP(cm->i < sentinel, errTypeDefError)
            Token typeFuncTk = cm->tokens.cont[cm->i];

            Int nameId = typeFuncTk.pl1 & LOWER24BITS;

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

    teClose(st, cm);

    VALIDATEI(exp->len == 1, iErrorInconsistentTypeExpr);
    return exp->cont[0];
}


private void typeNameNewType(TypeId newTypeId, Unt name, Compiler* cm) { //:typeNameNewType
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
    VALIDATEP(toks[cm->i + 1].tp == tokAssignRight, errAssignmentLeftSide)
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
// 4. outerType >= BIG: function types (generic or concrete), e.g. "(F Int -> String)" => BIG + 1
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
            j += 1;
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
            k -= 1;
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
// expression. First we need to skip possible internal assignments (in cases with data allocations)
    StackInt* exp = cm->stateForExprs->exp;
    exp->len = 0;
    Int j = indExpr + 1;
    for (; j < sentinelNode; ) {
        Node nd = cm->nodes.cont[j];
        if (nd.tp != nodAssignment)  {
            break;
        }
        j += (nd.pl2 + 1);
    }
    for (; j < sentinelNode; ++j) {
        Node nd = cm->nodes.cont[j];
        if (nd.tp <= tokString) {
            push((Int)nd.tp, exp);
        } else if (nd.tp == nodCall) {
            if (nd.pl1 == opGetElem)  { // accessors are handled specially
                push(2*BIG, exp);
            } else {
                const Int argCount = nd.pl2;
                push(BIG + argCount, exp); // signifies that it's a call, and its arg count
                // index into overloadIds, or, for 0-arity fns, entityId directly
                push((argCount > 0 ? -nd.pl1 - 2 : nd.pl1), exp);
            }
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
    const TypeId listType = cm->activeBindings[stToNameId(strL)];

    //printStackInt(exp);
    for (Int j = 0; j < expSentinel; ++j) {
        if (cont[j] < BIG) { // it's not a function call because function call indicators
                             // have BIG in them
            continue;
        } else if (cont[j] == 2*BIG) { // a list accessor
            TypeId type1 = cont[j - 2];
            TypeId outer1 = typeGetOuter(type1, cm);
            VALIDATEP(outer1 == listType, errTypeOfNotList)

            TypeId type2 = cont[j - 1];
            VALIDATEP(type2 == tokInt, errTypeOfListIndex)

            TypeId eltType = getGenericParam(type1, 0, cm);

            j -= 2;
            shiftStackLeft(j + 1, 2, cm);
            currAhead += 2;
            cont[j] = eltType;
            expSentinel -= 2;
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
                print("Overload not found, stack is:")
                printStackInt(exp);
            }
#endif //}}}
            VALIDATEP(ovFound, errTypeNoMatchingOverload)
            cm->nodes.cont[j + indExpr + currAhead].pl1 = entityId;

            cont[j] = getFunctionReturnType(cm->entities.cont[entityId].typeId, cm);
            shiftStackLeft(j + 1, 1, cm);
            expSentinel -= 1;

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
            // first parm matches, but not arity
            VALIDATEP(typeReadHeader(typeOfFunc, cm).arity == argCount, errTypeNoMatchingOverload)

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

            // the type-resolved function of the call
            cm->nodes.cont[j + indExpr + (currAhead)].pl1 = entityId;

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


private TypeId typecheckAndProcessListElt(Int* j, Compiler* cm) { //:typecheckAndProcessListElt
// Also updates the current index to skip the current element
    Node nd = cm->nodes.cont[*j];
    if (nd.tp <= topVerbatimTokenVariant) {
        *j += 1;
        return nd.tp;
    } else if (nd.tp == nodId) {
        *j += 1;
        return cm->entities.cont[nd.pl1].typeId;
    } else {
        Int sentinel = (*j) + nd.pl2 + 1;
        TypeId exprType = typeCheckBigExpr(*j, sentinel, cm);
        *j = sentinel;
        return exprType;
    }
}


private TypeId typecheckList(Int startInd, Compiler* cm) { //:typecheckList
    Int j = startInd;
    TypeId fstType = typecheckAndProcessListElt(&j, cm);
    for (; j < cm->nodes.len; ) {
        TypeId eltType = typecheckAndProcessListElt(&j, cm);
        VALIDATEP(eltType == fstType, errListDifferentEltTypes)
    }
    return fstType;
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
        (*ind) += 1;
        toSkip -= 1;
    }
}

#define typeGenTag(val) ((val >> 24) & 0xFF)

private Int typeEltArgcount(Int typeElt) {
// Get type-arg-count from a type element (which may be a type call, concrete type, type param etc)
    Unt genericTag = typeGenTag(typeElt);
    if (genericTag == 255) {
        return typeElt & 0xFF;
    } else {
        return genericTag;
    }
}


private Int typeMergeTypeCall(Int startInd, Int len, Compiler* cm) {
// Takes a single call from within a concrete type, and merges it into the type dictionary
// as a whole, separate type. Returns the unique typeId
    return -1;
}

//}}}
//}}}
//{{{ Built-ins

private void buiToStringInt(Interpreter* rt) { //:buiToStringInt
// Reads the int right after call stack header and writes the string to the heap top, then
// overwrites the int with the heap address
    Unt* arg = (Unt*)inDeref(rt->currFrame) + 2;

    Ptr targetAddr = rt->heapTop;
    char* target = (char*)inDeref(targetAddr);
    Int charsWritten = sprintf(target + 4, "%d", *arg);
    // it also wrote +1 char, the zero char, but we don't care. Eyr strings store their length
    // with them, so this zero char is safe to overwrite

    *((Unt*)target) = charsWritten; // the length of the string
    inMoveHeapTop(charsWritten + 4, rt); // +4 for the length that precedes the char
    *arg = targetAddr; // store ref to new string on the stack
}

//}}}
//{{{ Codegen
//{{{ Definitions

DEFINE_STACK(BtCodegen) //:StackBtCodegen

typedef struct { //:CgExpInterval
    Int start; // starting node ind in the AST
    Int end; // exclusive
    Int size; // the size in memory (units of 4 bytes), either of an operand, or of a call result
} CgExpInterval;

//}}}
//{{{ Generating instructions

testable Ulong instrArithmetic(Byte opCode, Short dest, Short operand1, Short operand2) {
    return ((Ulong)opCode << 58) + ((Ulong)dest << 32) + ((Ulong)operand1 << 16) + operand2;
}

testable Ulong instrConstArithmetic(Byte opCode, Int increment) {
    return ((Ulong)opCode << 58) + increment;
}


testable Ulong instrGetFld(Short dest, Short obj, Int offset) {
    return ((Ulong)iGetFld << 58) + ((Ulong)dest << 40) + ((Ulong)obj << 24)
        + (offset & LOWER24BITS);
}


testable Ulong instrNewString(Short dest, Short start, Int len) {
    return ((Ulong)iNewstring << 58) + ((Ulong)dest << 40) + ((Ulong)start << 24)
        + ((Ulong)len & LOWER24BITS);
}


testable Ulong instrConcatStrings(Short dest, Short op1, Short op2) {
    return ((Ulong)iConcatStrs << 58) + ((Ulong)dest << 32) + ((Ulong)op1 << 16)
        + ((Ulong)op2);
}

testable Ulong instrReverseString(Short dest, Short src) {
    return ((Ulong)iReverseString << 58) + ((Ulong)dest << 16) + ((Ulong)src << 16);
}

testable Ulong instrPrint(StackAddr strAddress) {
    return ((Ulong)iPrint << 58) + (Ulong)(Ushort)strAddress;
}

testable Ulong instrSetLocal(StackAddr dest, Int value) {
    Ulong a = (((Ulong)iSetLocal) << 58) + (((Ulong)((uint16_t)dest)) << 32) + (Ulong)(Unt)value;
    return a;
}


testable Ulong instrBuiltinCall(BuiltinFn fn) {
    for (Int j = 0; j < countBuiltins; j++) {
        if (BUILTINS_TABLE[j] == fn) {
            return (((Ulong)iBuiltinCall) << 58) + (Ulong)j;
        }
    }
    return 0;
}


testable Ulong instrCall(Short newFramePtr, Unt newIp) {
    Ulong a = (((Ulong)iCall) << 58) + (((Ulong)((uint16_t)newFramePtr)) << 32) + (Ulong)newIp;
    return a;
}


testable Ulong instrReturn(Byte returnSize) { //:instrReturn
// "returnSize" is either 0, 1 or 2 ("words", ie. 4-byte chunks).
    Ulong a = (((Ulong)iReturn) << 58) + (Ulong)returnSize;
    return a;
}

//}}}
//{{{ Codegen main

private void cgExp(Node nd, Arr(Node) nodes, Compiler* cg) {
     
}

//}}}
//{{{ Codegen init

private void tabulateCodegen() { //:tabulateCodegen
    CodegenFn* p = CODEGEN_TABLE;
    p[nodExpr]        = &cgExp;
}


testable void initializeCodegen(Compiler* cm, Arena* a) { //:initializeCodegen
// Turns a parser into a code generator
    if (cm->stats.wasError) {
        return;
    }

    Compiler* cg = cm;
    cg->i = 0;
    cg->cgBtrack = createStackBtCodegen(16, cm->aTmp);
    cg->bytecode = createInListUlong(128, a);
}


private void tmpGetCode(Interpreter* rt, Arena* a) {
// Super-simple bytecode for writing a barebones codegen:
// Code = "a = 77; print $a;"
// Bytecode = "setLocal; call built-in; call instruction"
    const Int len = 4;
    Arr(Ulong) theCode = ((Ulong[]) {
        len,
        instrSetLocal(2, 77),
        instrBuiltinCall(&buiToStringInt),
        instrPrint(2),
        instrReturn(0)
    });



//    const Int len = 4;
//    Arr(Ulong) theCode = ((Ulong[]) {
//        len,
//        instrSetLocal(stackFrameStart, 3),
//        instrNewString(stackFrameStart + 4, stackFrameStart, 4),
//        instrPrint(stackFrameStart + 4),
//        (((Ulong)iReturn) << 58)
//    });
    Int lenBytes = (len + 1)*sizeof(Ulong);
    void* codeAddr = allocateOnArena(lenBytes, a);

    memcpy(codeAddr, theCode, lenBytes);
    rt->code = (Arr(Ulong))codeAddr;
}

//}}}


private void writeFunction(Int indNode, Compiler* cm) { //:writeFunction
// Generate bytecode for a single function
    //rt->i = indNode;
    //const Int sentinel = indNode + cm->nodes.cont[indNode].pl2 + 1;
    
}

private void codegen(Compiler* cm) { //:codegen
// Generate bytecode for a whole module
    print("codegen")
    initializeCodegen(cm, cm->a);
    Compiler* cg = cm;

    for (Int j = 0; j < cg->toplevels.len; j++) {
        writeFunction(cg->toplevels.cont[j].indNode, cg);
    }
}

//}}}
//{{{ Interpreter
//{{{ Utils

private void* inDeref0(Ptr address, Interpreter* rt) { //:inDeref0
    return (void*)rt->memory + ((Ulong)address)*4;
}


private Unt inGetFromStack(StackAddr addr, Interpreter* rt) { //:inGetFromStack
    Unt* p = (Unt*)(inDeref(rt->currFrame + (Unt)addr));
    return *p;
}


private void inSetOnStack(Short dest, Unt value, Interpreter* rt) { //:inSetOnStack
    Unt* p = (Unt*)(inDeref(rt->currFrame + (Unt)dest));
    *p = value;
}


private void inMoveHeapTop(Unt sz, Interpreter* in) { //:inMoveHeapTop
// Moves the top of the heap after an allocation. "sz" is total size in bytes
    in->heapTop += sz / 4;
    if (sz % 4 > 0)  {
        in->heapTop += 1;
    }
}


//}}}
//{{{ Code running

private Unt runPlus(Ulong instr, Unt ip, Interpreter* rt) {
    return ip + 1;
}

private Unt runMinus(Ulong instr, Unt ip, Interpreter* rt) {

    return ip + 1;
}

private Unt runTimes(Ulong instr, Unt ip, Interpreter* rt) {
    return ip + 1;
}


private Unt runDivBy(Ulong instr, Unt ip, Interpreter* rt) {
    return ip + 1;
}


private Unt runNewString(Ulong instr, Unt ip, Interpreter* rt) { //:runNewString
// Creates a new string as a substring of the static text. Stores pointer to the new string
// in the stack
    Short dest = (Short)((instr >> 40) & LOWER16BITS);
    Short startAddr = (Short)((instr >> 24) & LOWER16BITS); // address within the frame
    Int len = (Int)(instr & LOWER24BITS);
    Unt start = inGetFromStack(startAddr, rt); // actual starting symbol within the static text

    char* copyFrom = (char*)inDeref(rt->textStart) + 4 + start;

    const Ptr targetAddr = rt->heapTop;
    Unt* target = (Unt*)(inDeref(targetAddr));

    *target = len;
    memcpy(target + 1, copyFrom, len);

    inMoveHeapTop(len, rt); // update the heap top after this allocation
    inSetOnStack(dest, targetAddr, rt);

    return ip + 1;
}


private Unt runConcatStrings(Ulong instr, Unt ip, Interpreter* rt) { //:runConcatStrings
    return ip + 1;
}

private Unt runReverseString(Ulong instr, Unt ip, Interpreter* rt) { //:runReverseString
    return ip + 1;
}

private Unt runSetLocal(Ulong instr, Unt ip, Interpreter* rt) { //:runSetLocal
    StackAddr dest = (instr >> 32) & LOWER16BITS;
    Unt* address = (Unt*)(inDeref(rt->currFrame + (Unt)dest));
    *address = (Unt)(instr & LOWER32BITS);
    return ip + 1;
}


private Unt runBuiltinCall(Ulong instr, Unt ip, Interpreter* rt) { //:runBuiltinCall
    BUILTINS_TABLE[instr & (0xFF)](rt);
    return ip + 1;
}


private Unt runCall(Ulong instr, Unt ip, Interpreter* rt) { //:runCall
// Creates and activates a new call frame. Stack frame layout (all are 4-byte sized):
// prevFrame  - Ptr index into the runtime stack
// ip         - Unt index into @Interpreter.code
    Unt newFn = (Unt)(instr & LOWER32BITS);
    StackAddr newFrameRef = (Short)((instr >> 32) & LOWER16BITS);
    Ptr oldFrameRef = rt->currFrame;
    Unt* oldFrame = inDeref(oldFrameRef);

    // save the ip of old function to its frame
    print("saving old ip = %d", ip);
    *(oldFrame + 1) = ip;

    print("frame before call:")
    dbgCallFrames(rt);

    rt->currFrame = (Ptr)(oldFrameRef + (Int)newFrameRef);
    Unt* newFrame = (Unt*)inDeref(rt->currFrame);
    *newFrame = *oldFrame;

    print("frame after call:")
    dbgCallFrames(rt);

    return newFn;
}


private Unt runReturn(Ulong instr, Unt ip, Interpreter* rt) { //:runReturn
// Return from function. The return value, if any, will be stored right after the header
    Int returnSize = ip & (0xFF);
    Int* prevFrame = inDeref(rt->currFrame);
    if (*prevFrame == -1) {
        return -1;
    }

    rt->topOfFrame = rt->currFrame + stackFrameStart + returnSize;
    rt->currFrame = *prevFrame; // take a call off the stack
    Unt* callerIp = (Unt*)inDeref(*prevFrame) + 1;
    return *callerIp;
}


private Unt runPrint(Ulong instr, Unt ip, Interpreter* rt) { //:runPrint
    Unt* ref = (Unt*)(inDeref(rt->currFrame + (Int)(instr & LOWER16BITS)));
    Unt* actualString = inDeref(*ref);
    fwrite(actualString + 1, 1, *actualString, stdout);
    printf("\n");
    return ip + 1;
}

//}}}
//{{{ Interpreter init

private void tmpGetText(Interpreter* rt) {
    char txt[] = "asdfBBCC";
    const Int len = sizeof(txt) - 1;
    Unt* p = (Unt*)(inDeref(rt->heapTop));
    *p = len;
    p += 1;
    memcpy(p, txt, len);
    rt->textStart = rt->heapTop;
    inMoveHeapTop(sizeof(txt), rt);
}


private void tabulateInterpreter() { //:tabulateInterpreter
    InterpreterFn* p = INTERPRETER_TABLE;
    p[iPlus]        = &runPlus;
    p[iTimes]       = &runTimes;
    p[iMinus]       = &runMinus;
    p[iDivBy]       = &runDivBy;
   /*
    p[iPlusFl]      = &runPlus;
    p[iMinusFl]       = &runMinusFl;
    p[iTimesFl]       = &runTimesFl;
    p[iDivByFl]       = &runDivByFl;
    p[iPlusConst]       = &runPlusConst;
    p[iMinusConst]       = &runMinusConst;
    p[iTimesConst]       = &runTimesConst;
    p[iDivByConst]       = &runDivByConst;
    p[iPlusFlConst]       = &runPlusFlConst;
    p[iMinusFlConst]       = &runMinusFlConst;
    p[iTimesFlConst]       = &runTimesFlConst;
    p[iDivByFlConst]       = &runDivByFlConst;
    p[iIndexOfSubstring]       = &runIndexOfSubstring;
    p[iGetFld]       = &runGetFld;
    p[iNewList]       = &runNewList;
    p[iSubstring]       = &runSubstring;
   */
    p[iNewstring]      = &runNewString;
    p[iConcatStrs]     = &runConcatStrings;
    p[iReverseString]  = &runReverseString;
    p[iSetLocal]       = &runSetLocal;
    p[iBuiltinCall]    = &runBuiltinCall;
    p[iCall]           = &runCall;
    p[iReturn]         = &runReturn;
    p[iPrint]          = &runPrint;
}


private void tabulateBuiltins() { //:tabulateBuiltins
    BuiltinFn* p = BUILTINS_TABLE;
    p[0]         = &buiToStringInt;
}


private Interpreter* createInterpreter(Compiler* cg) { //:createInterpreter
    Arena* a = cg->a;
    Interpreter* rt = allocateOnArena(sizeof(Interpreter), a);
    (*rt) = (Interpreter)  {
        .memory = (Arr(char))allocateOnArena(1000000, a),
        .fns = allocateOnArena(4, a),
        .heapTop = 50000,  // skipped the 200k of stack space
        .currFrame = 0,
        .textStart = 0,
        .code = cg->bytecode.cont
    };
    
    //tmpGetText(rt);
    rt->fns[0] = 0;
    Int* zeroPtr = (Int*)inDeref(rt->currFrame);
    (*zeroPtr) = -1; // for the "main" call, there is no previous frame
    return rt;
}

//}}}
//}}}
//{{{ Utils for tests & debugging

#ifdef DEBUG

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


void printNameAndLen(Unt unsign, Compiler* cm) { //:printNameAndLen
    Int startBt = unsign & LOWER24BITS;
    Int len = (unsign >> 24) & 0xFF;
    fwrite(cm->sourceCode->cont + startBt, 1, len, stdout);
}


void printName(NameId nameId, Compiler* cm) { //:printName
    Unt unsign = cm->stringTable->cont[nameId];
    printNameAndLen(unsign, cm);
    printf("\n");
}

//}}}
//{{{ Lexer testing

// Must agree in order with token types in eyr.internal.h
const char* tokNames[] = {
    "Int", "Long", "Double", "Bool", "String", "misc",
    "word", "Type", "'var", ":kwarg", "operator", ".field",
    "stmt", "()", "(T ...)", "intro:", "[]", "{}", "a[]",
    "=", "=...", "alias", "assert", "breakCont",
    "trait", "import", "return",
    "(do", "(\\", "(try", "(catch",
    "(if", "match{", "elseIf", "else", "(impl", "(for", "(each"
};


Int pos(Compiler* lx) {
    return lx->i - sizeof(standardText) + 1;
}


Int posInd(Int ind) {
    return ind - sizeof(standardText) + 1;
}


void dbgLexBtrack(Compiler* lx) { //:dbgLexBtrack
    StackBtToken* bt = lx->lexBtrack;
    printf("lexBtTrack = [");

    for (Int k = 0; k < bt->len; k++) {
        printf("%s ", tokNames[bt->cont[k].tp]);
    }
    printf("  ]\n");

    printf("lexBtTrack StartInds = [");

    for (Int k = 0; k < bt->len; k++) {
        printf("%d ", bt->cont[k].tokenInd);
    }
    printf("  ]\n");
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
            indent -= 1;
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
            indent += 1;
        }
    }
}

//}}}
//{{{ Parser testing

// Must agree in order with node types in eyr.internal.h
const char* nodeNames[] = {
    "Int", "Long", "Double", "Bool", "String", "_", "misc",
    "id", "call", "binding", ".fld", "GEP", "GElem",
    "(do", "Expr", "=", "[]",
    "alias", "assert", "breakCont", "catch", "defer",
    "import", "(\\ fn)", "trait", "return", "try",
    "for", "if", "eif", "impl", "match"
};


void printParser(Compiler* cm, Arena* a) { //:printParser
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
            indent -= 1;
        }

        if (i < 10) printf(" ");
        printf("%d: ", i);
        for (int j = 0; j < indent; j++) {
            printf("  ");
        }
        Int startBt = loc.startBt - stText.len;
        if (nod.tp == nodCall) {
            printf("call %d [%d; %d] type = \n", nod.pl1, startBt, loc.lenBts);
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
            indent += 1;
        }
    }
}

private void dbgStackNode(StackNode* st, Arena* a) { //:dbgStackNode
    Int indent = 0;
    Stackint32_t* sentinels = createStackint32_t(16, a);
    for (int i = 0; i < st->len; i++) {
        Node nod = st->cont[i];
        for (int m = sentinels->len - 1; m > -1 && sentinels->cont[m] == i; m--) {
            popint32_t(sentinels);
            indent -= 1;
        }

        if (i < 10) printf(" ");
        printf("%d: ", i);
        for (int j = 0; j < indent; j++) {
            printf("  ");
        }
        if (nod.tp == nodCall) {
            printf("call %d type = \n", nod.pl1);
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
            indent += 1;
        }
    }
}


private void dbgExprFrames(StateForExprs* st) { //:dbgExprFrames
    print(">>> Expr frames");
    for (Int j = 0; j < st->frames->len; j += 1) {
        ExprFrame fr = st->frames->cont[j];
        if (fr.tp == exfrCall) {
            printf("Call argc");
        } else if (fr.tp == exfrDataAlloc) {
            printf("DataAlloc");
        } else if (fr.tp == exfrParen) {
            printf("Paren");
        }
        printf(" %d %d; ", fr.argCount, fr.sentinel);
    }
    printf(">>>\n\n");
}

//}}}
//{{{ Types testing

private void dbgTypeGenElt(Int v) { //:dbgTypeGenElt
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

void dbgTypeOuter(TypeId currT, Compiler* cm) { //:dbgTypeOuter
// Print the name of the outer type. `(Tu Int Double)` -> `Tu`
    Arr(Int) t = cm->types.cont;
    TypeHeader currHdr = typeReadHeader(currT, cm);
    if (currHdr.sort == sorFunction) {
        printf("[F ");
    } else if (currHdr.sort == sorTypeCall) {
        const TypeId genericTypeId = currHdr.nameAndLen;
        const TypeHeader genericHdr = typeReadHeader(genericTypeId, cm);
        printf("(");
        printNameAndLen(genericHdr.nameAndLen, cm);
        printf(" ");
    } else {
        printName(t[currT + TYPE_PREFIX_LEN], cm);
    }
}


void dbgType(TypeId typeId, Compiler* cm) { //:dbgType
// Print a single type fully for debugging purposes
    //printf("Printing the type [ind = %d, len = %d]\n", typeId, cm->types.cont[typeId]);
    if (typeId <= topVerbatimType)  {
        printf("%s", nodeNames[typeId]);
        return;
    }
    printIntArrayOff(typeId, 6, cm->types.cont);

    StackTypeLoc* st = createStackTypeLoc(16, cm->aTmp);
    TypeLoc* top = null;

    Int sentinel = typeId + cm->types.cont[typeId] + 1;
    dbgTypeOuter(typeId, cm);
    pushTypeLoc(((TypeLoc){ .currPos = typeId + TYPE_PREFIX_LEN, .sentinel = sentinel }), st);
    top = st->cont;

    Int countIters = 0;
    while (top != null && countIters < 10)  {
        Int currT = cm->types.cont[top->currPos];
        top->currPos += 1;
        if (currT > -1) {
            if (currT <= topVerbatimType)  {
                printf("%s ", currT != tokMisc ? nodeNames[currT] : "Void");
            } else {
                Int newSentinel = currT + cm->types.cont[currT] + 1;
                pushTypeLoc(
                    ((TypeLoc){ .currPos = currT + TYPE_PREFIX_LEN, .sentinel = newSentinel}), st);
                top = st->cont + (st->len - 1);
                dbgTypeOuter(currT, cm);
            }
        } else { // type param
            printf("%d ", -currT - 1);
        }
        while (top != null && top->currPos == top->sentinel) {
            popTypeLoc(st);
            top = hasValuesTypeLoc(st) ? st->cont + (st->len - 1) : null;
            printf(")");
        }

        countIters += 1;
    }
    printf("\n");
}


void dbgTypeFrames(StackTypeFrame* st) { //:dbgTypeFrames
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


void dbgOverloads(Int nameId, Compiler* cm) { //:dbgOverloads
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
//{{{ Interpreter utils


// Must agree in order with instruction types in eyr.internal.h
const char* instructionNames[] = {
    "Int", "Long", "Double", "Bool", "String", "_", "misc",
    "id", "call", "binding", ".fld", "GEP", "GElem",
    "(do", "Expr", "=", "[]",
    "alias", "assert", "breakCont", "catch", "defer",
    "import", "(\\ fn)", "trait", "return", "try",
    "for", "if", "eif", "impl", "match"
};

void dbgBytecode(Compiler* cg) { //:dbgBytecode
// Print the bytecode
    
}

void dbgCallFrames(Interpreter* rt) { //:dbgCallFrame
// Print the current call frame header, and the previous frame too (if applicable)
    Unt* framePtr = inDeref(rt->currFrame);
    Ptr prevFrame = *framePtr;
    Ptr fnCode = *(framePtr + 1);
    printf("Current call frame: prevFrame = %d, fn code at %d\n", prevFrame, fnCode);
    if ((Int)prevFrame != -1) {
        Unt* prevFramePtr = inDeref(prevFrame);
        Ptr prevPrevFrame = *prevFramePtr;
        Ptr prevFnCode = *(prevFramePtr + 1);
        Ptr prevIp = *(prevFramePtr + 2);
        printf("Prev call frame: ancestorFrame = %d, fn code at %d, execution stopped at ip %d",
                prevPrevFrame, prevFnCode, prevIp);
    }
}

//}}}

#endif

//}}}
//{{{ Main

Int eyrInitCompiler() { //:eyrInitCompiler
// Definition of the operators, lexer dispatch, parser dispatch etc tables for the compiler.
// This function should only be called once, at compiler init. Its results are global shared const.
    setOperatorsLengths();
    tabulateLexer();
    tabulateParser();
    tabulateCodegen();
    tabulateInterpreter();
    tabulateBuiltins();
    return 0;
}

Int eyrCompile(unsigned char* fn) { //:eyrCompile
//    String* sourceCode = readSourceFile(fn, a);
//    if (sourceCode == NULL) {
//        goto cleanup;
//    }
    return 0;
}


private void inPrintString(Unt address, Interpreter* rt) {
    Unt* len = (Unt*)(inDeref(address));
    fwrite(len + 1, 1, *len, stdout);
    printf("\n");
}


private Int interpretCode(Interpreter* in) {
    Int ip = 1; // skipping the function size
    while (ip > -1) {
        //print("ip = %d", ip);
        Ulong instr = in->code[ip];
        //print("instr %lx op code %d", instr, instr>>58);
        ip = (INTERPRETER_TABLE[instr >> 58])(instr, ip, in);
    }
    return 0;
}


#ifndef TEST

Int main(int argc, char** argv) { //:main

    eyrInitCompiler();
    Arena* a = createArena();
    
    
    String* sourceCode = s("main = (( a = 78; print a))");

    Compiler* proto = createProtoCompiler(a);
    Compiler* cm = lexicallyAnalyze(sourceCode, proto, a);
    cm = parse(cm, proto, a);
    codegen(cm);
    Interpreter* rt = createInterpreter(cm);
    Int interpResult = 0;//interpretCode(rt);
    
    printf("Interpretation result = %d\n", interpResult);

    cleanup:
    deleteArena(a);
    return 0;
}

#endif

//}}}
