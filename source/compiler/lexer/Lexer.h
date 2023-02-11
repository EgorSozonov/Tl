#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <stdbool.h>
#include "../utils/String.h"
#include "../utils/StackHeader.h"

#define LEX_CHUNK_SIZE 10000


typedef struct {
    unsigned int tp : 6;
    unsigned int lenBytes: 26;
    unsigned int startByte;
    unsigned int payload1;
    unsigned int payload2;
} Token;

typedef struct {
    Token* tokens;
    int totalTokens;
    bool wasError;
    String* errMsg;
    
    int currInd; // for reading a previously filled token storage. This is the absolute index (i.e. not within a single array)
    
    int chunkCapacity;
    int chunkCount;
    Token** storage; // chunkCapacity-length array of pointers to LEX_CHUNK_SIZE-sized arrays of Token
    
    Token* currChunk; // the current token array being added to
    int nextInd; // the relative (i.e. withing a single token array, currChunk) index for the next token to be added
    
    Arena* arena;
} Lexer;

Lexer* createLexer(Arena* ar);
void addToken(Token t, Lexer* lexer);

/** 
 * Regular (leaf) Token types
 */
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt 0
#define tokFloat 1
#define tokBool 2
#define tokString 3
#define tokUnderscore 4

// This group requires analysis in the parser
#define tokDocComment 5
#define tokCompoundString 6
#define tokWord 7              // payload2: 1 if the word is all capitals
#define tokDotWord 8           // payload2: 1 if the word is all capitals
#define tokAtWord 9
#define tokReserved 10         // payload2: value of a constant from the 'reserved*' group
#define tokOperator 11         // payload2: OperatorToken encoded as an Int

/** 
 * Punctuation (inner node) Token types
 */
#define tokCurlyBraces 12
#define tokBrackets 13
#define tokParens 14
#define tokStmtAssignment 15 // payload1: (number of tokens before the assignment operator) shl 16 + (OperatorType)
#define tokStmtTypeDecl 16
#define tokLexScope 17
#define tokStmtFn 18
#define tokStmtReturn 19
#define tokStmtIf 20
#define tokStmtLoop 21
#define tokStmtBreak 22
#define tokStmtIfEq 23
#define tokStmtIfPred 24

/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType = tokCurlyBraces
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType = tokStmtFn


/*

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


// & + - / * ! ~ $ % ^ | > < ? =
 
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

*/


#endif
