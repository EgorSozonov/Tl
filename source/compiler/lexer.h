#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <stdbool.h>
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stackHeader.h"
#include "lexerConstants.h"

typedef struct {
    unsigned int tp : 6;
    unsigned int lenBytes: 26;
    unsigned int startByte;
    unsigned int payload1;
    unsigned int payload2;
} Token;


typedef struct {
    unsigned int tp;
    int numberOfToken;
    bool wasOriginallyColon;
} RememberedToken;


DEFINE_STACK_HEADER(RememberedToken)
DEFINE_STACK_HEADER(int)

typedef struct _Lexer Lexer;
typedef void (*LexerFunc)(Lexer*); // LexerFunc = &(Lexer* => void)
typedef int (*ReservedProbe)(int, int, Lexer*);

struct _Lexer {
    int i;
    String* inp;
    int totalTokens;
    bool wasError;
    String* errMsg;
    
    Arr(Token) tokens;

    StackRememberedToken* backtrack;
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    
    Stackint* newlines;
    int nextInd; // the  index for the next token to be added
    int capacity; // current capacity of token storage
    Arena* arena;
};



Lexer* createLexer(String* inp, Arena* ar);
void addToken(Token t, Lexer* lexer);



/**
 * There is a closed set of operators in the language.
 *
 * For added flexibility, most operators are extended into two more planes,
 * for example '+' may be extended into '+.', while '/' may be extended into '/.'.
 * These extended operators are declared by the language, and may be defined
 * for any type by the user, with the return type being arbitrary.
 * For example, the type of 3D vectors may have two different multiplication
 * operators: * for vector product and *. for scalar product.
 *
 * Plus, all the extensible operators (and only them) have automatic assignment counterparts.
 * For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
 *
 * This OperatorToken class records the base type of operator, its extension (0, 1 or 2),
 * and whether it is the assignment version of itself.
 * In the token stream, both of these values are stored inside the 32-bit payload2 of the Token.
 */
typedef struct {
    unsigned int opType : 6;
    unsigned int extended : 2;
    unsigned int isAssignment: 1;
} OperatorToken;

typedef struct {
    String* name;
    byte bytes[4];
    int precedence;
    int arity;
    /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
    bool extensible;
    /* Indices of operators that act as functions in the built-in bindings array.
     * Contains -1 for non-functional operators
     */
    int binding;
    bool overloadable;
    bool assignable;
} OpDef;



typedef struct {
    OpDef (*operators)[countOperators];
    LexerFunc (*dispatchTable)[256];
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
} LanguageDefinition;


LanguageDefinition* buildLanguageDefinitions(Arena* ar);
Lexer* lexicallyAnalyze(String* inp, LanguageDefinition* lang, Arena* ar);

#endif
