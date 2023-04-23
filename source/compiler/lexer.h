#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/structures/stackHeader.h"
#include "lexerConstants.h"

typedef struct {
    untt tp : 6;
    untt lenBytes: 26;
    untt startByte;
    untt payload1;
    untt payload2;
} Token;


typedef struct {
    untt tp;
    intt tokenInd;
    intt countClauses;
    intt isMultiline;
    bool wasOriginallyColon;
} RememberedToken;


DEFINE_STACK_HEADER(RememberedToken)

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
 * Plus, all the extensible operators (and only them) have automatic assignment counterparts.
 * For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
 *
 * This OperatorToken class records the base type of operator, its extension (0, 1 or 2),
 * and whether it is the assignment version of itself.
 * In the token stream, both of these values are stored inside the 32-bit payload2 of the Token.
 */
typedef struct {
    untt opType : 6;
    untt extended : 2;
    untt isAssignment: 1;
} OperatorToken;


typedef struct {
    String* name;
    byte bytes[4];
    intt precedence;
    intt arity;
    /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
    bool extensible;
    bool overloadable;
    bool assignable;
} OpDef;

typedef struct LanguageDefinition LanguageDefinition;
typedef struct Lexer Lexer;
typedef void (*LexerFunc)(Lexer*, Arr(byte)); // LexerFunc = &(Lexer* => void)
typedef intt (*ReservedProbe)(int, int, struct Lexer*);


struct Lexer {
    intt i;                     // index in the input text
    String* inp;
    intt inpLength;
    intt totalTokens;
    intt lastClosingPunctInd;   // the index of the last encountered closing punctuation sign, used for statement length
    
    LanguageDefinition* langDef;
    
    Arr(Token) tokens;
    intt capacity;              // current capacity of token storage
    intt nextInd;               // the  index for the next token to be added    
    
    Arr(int) newlines;
    intt newlinesCapacity;
    intt newlinesNextInd;
    
    Arr(byte) numeric;
    intt numericCapacity;
    intt numericNextInd;

    StackRememberedToken* backtrack;
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    
    bool wasError;
    String* errMsg;

    Arena* arena;
};

struct LanguageDefinition {
    OpDef (*operators)[countOperators];
    LexerFunc (*dispatchTable)[256];
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    intt (*reservedParensOrNot)[countCoreForms];
};

Lexer* createLexer(String* inp, Arena* ar);
void add(Token t, Lexer* lexer);


LanguageDefinition* buildLanguageDefinitions(Arena* a);
Lexer* lexicallyAnalyze(String*, LanguageDefinition*, Arena*);
typedef union {
    uint64_t i;
    double   d;
} FloatingBits;

int64_t longOfDoubleBits(double);

#endif
