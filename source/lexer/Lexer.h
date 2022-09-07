#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../utils/String.h"
#include "../../utils/StackHeader.h"

#define LEX_CHUNK_SIZE 5000

typedef enum {
    parens,
    curlyBraces,
    squareBrackets,
    intLiteral,
    floatLiteral,
    strLiteral,
    word,
    dotWord,
    operator,
    comment,
    nopToken,
} TokenType;


/**
 * & + - / * ! ~ $ % ^ | > < ? =
 */
typedef enum  {
    ampersand,
    plus,
    minus,
    slash,
    asterisk,
    exclamation,
    tilde,
    dollar,
    percent,
    caret,
    pipe,
    lt,
    gt,
    question,
    equals,
    colon,
    notASymb,
} OperatorSymb;

typedef enum {
    statement,
    dataInitializer,
    scope,
    subExpr,
} LexicalContext;

typedef struct {
    TokenType tokType;
    uint64_t payload;
    uint32_t startChar;
    uint32_t lenChars;
    uint32_t lenTokens;
} Token;

typedef struct LexChunk {
    struct LexChunk* next;
    Token tokens[LEX_CHUNK_SIZE];
} LexChunk;

typedef struct {
    Token* token;
    int numberOfToken;
} RememberedToken;
DEFINE_STACK_HEADER(RememberedToken)

typedef struct {
    LexChunk* firstChunk;
    LexChunk* currChunk;
    int currInd;
    int totalNumber;
    bool wasError;
    String* errMsg;
    Arena* arena;
} LexResult;


LexResult* mkLexResult(Arena* ar);
LexResult* lexicallyAnalyze(String* inp, Arena* ar);




#endif
