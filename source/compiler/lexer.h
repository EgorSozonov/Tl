﻿#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stackInt.h"
#include "../utils/structures/stackHeader.h"
#include "lexerConstants.h"

typedef struct {
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;
} Token;

/** Backtrack token, used during lexing to keep track of all the nested stuff */
typedef struct {
    untt tp : 6;
    Int tokenInd;
    Int countClauses;
    untt spanLevel : 3;
    bool wasOrigColon;
} BtToken;



DEFINE_STACK_HEADER(BtToken)


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

#endif
