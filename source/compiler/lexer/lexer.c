#include "lexer.h"
#include "lexerConstants.h"
#include "../languageDefinition.h"
#include "../../utils/aliases.h"
#include "../../utils/arena.h"
#include "../../utils/string.h"
#include "../../utils/stack.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


DEFINE_STACK(RememberedToken)

typedef void (*LexerFunc)(Lexer*); // LexerFunc = &(Lexer* => void)

void lexNumber(Lexer*);
void lexWord(Lexer*);
void lexDotSomething(Lexer*);
void lexAtWord(Lexer*);
void lexOperator(Lexer*);
void lexMinus(Lexer*);
void lexColon(Lexer*);
void lexParenLeft(Lexer*);
void lexParenRight(Lexer*);
void lexCurlyLeft(Lexer*);
void lexCurlyRight(Lexer*);
void lexBracketLeft(Lexer*);
void lexBracketRight(Lexer*);
void lexUnexpectedSymbol(Lexer*);
void lexNonAsciiError(Lexer*);

LexerFunc (*buildLexerDispatch(Arena* ar))[256] {
    printf("Building lexer dispatch table, sizeof(LexerFunc) = %lu", sizeof(LexerFunc));
    LexerFunc (*result)[256] = allocateOnArena(ar, 256*sizeof(LexerFunc));
    LexerFunc* p = *result;
    for (int i = 0; i < 128; i++) {
        p[i] = lexUnexpectedSymbol;
    }
    for (int i = 128; i < 256; i++) {
        p[i] = lexNonAsciiError;
    }
    for (int i = aDigit0; i <= aDigit9; i++) {
        p[i] = lexNumber;
    }

    for (int i = aALower; i <= aZLower; i++) {
        p[i] = lexWord;
    }
    for (int i = aAUpper; i <= aZUpper; i++) {
        p[i] = lexWord;
    }
    p[aUnderscore] = lexWord;
    p[aDot] = lexDotSomething;
    p[aAt] = lexAtWord;
    const int opStartSize = sizeof(operatorStartSymbols);

    printf("opStartSize = %d\n", opStartSize);

    for (int i = 0; i < opStartSize; i++) {
        p[i] = lexOperator;
    }
    p[aMinus] = lexMinus;
    p[aColon] = lexColon;
    p[aParenLeft] = lexParenLeft;
    p[aParenRight] = lexParenRight;
    p[aCurlyLeft] = lexCurlyLeft;
    p[aCurlyRight] = lexCurlyRight;
    p[aBracketLeft] = lexBracketLeft;
    p[aBracketRight] = lexBracketRight;
    return result;
}


Lexer* createLexer(String* inp, Arena* ar) {
    Lexer* result = allocateOnArena(ar, sizeof(Lexer));
    result->inp = inp;
    result->capacity = LEXER_INIT_SIZE;
    result->tokens = allocateOnArena(ar, LEXER_INIT_SIZE*sizeof(Token));
    result->errMsg = &empty;
    result->arena = ar;

    return result;
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunk(Lexer* lexer) {
    Token* newStorage = allocateOnArena(lexer->arena, lexer->capacity*2*sizeof(Token));
    memcpy(newStorage, lexer->tokens, (lexer->capacity)*(sizeof(Token)));
    lexer->tokens = newStorage;

    lexer->capacity *= 2;
}

void addToken(Token t, Lexer* lexer) {
    lexer->tokens[lexer->nextInd] = t;
    lexer->nextInd++;
    if (lexer->nextInd == lexer->capacity) {
        handleFullChunk(lexer);
    }
}

private Lexer* buildLexer(int totalTokens, String* inp, Arena *ar, /* Tokens */ ...) {
    Lexer* result = createLexer(inp, ar);
    if (result == NULL) return result;
    
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, ar);
    
    for (int i = 0; i < totalTokens; i++) {
        addToken(va_arg(tokens, Token), result);
    }
    
    va_end(tokens);
    return result;
}

/** Sets i to beyond input's length to communicate to callers that lexing is over */
private void exitWithError(const char errMsg[], Lexer* res) {
    res->i = res->inp->length;
    res->wasError = true;
    res->errMsg = allocateLiteral(res->arena, errMsg);
}


private void finalize(Lexer* this) {
    if (!hasValuesRememberedToken(this->backtrack)) {
        exitWithError(errorPunctuationExtraOpening, this);
    }
}


Lexer* lexicallyAnalyze(String* inp, LanguageDefinition* lang, Arena* ar) {
    Lexer* result = createLexer(ar);
    if (inp->length == 0) {
        exitWithError("Empty input", result);
    }

    // Check for UTF-8 BOM at start of file
    int i = 0;
    if (inp->length >= 3
        && (unsigned char)inp->content[0] == 0xEF
        && (unsigned char)inp->content[1] == 0xBB
        && (unsigned char)inp->content[2] == 0xBF) {
        i = 3;
    }
    LexDispatch (*dispatchTable)[256] = buildLexerDispatch(ar);

    // Main loop over the input
    while (i < inp->length && !result->wasError) {
        (*dispatchTable[inp->content[i]])(result);
    }

    finalize(result);
    return result;
}




//~ DEFINE_STACK(RememberedToken)

//~ LexResult* mkLexResult(Arena* ar) {
    //~ LexResult* result = arenaAllocate(ar, sizeof(LexResult));
    //~ result->arena = ar;
    //~ LexChunk* firstChunk = arenaAllocate(ar, sizeof(LexChunk));
    //~ result->firstChunk = firstChunk;
    //~ result->currChunk = firstChunk;
    //~ result->currInd = 0;
    //~ result->totalNumber = 0;
    //~ return result;
//~ }

//~ void addToken(LexResult* lr, Token tok) {
    //~ if (lr->currInd < (LEX_CHUNK_SIZE - 1)) {
        //~ lr->currInd++;
        //~ lr->currChunk->tokens[lr->currInd] = tok;
    //~ } else {
        //~ LexChunk* nextChunk = arenaAllocate(lr->arena, sizeof(LexChunk));
        //~ nextChunk->tokens[0] = tok;
        //~ lr->currChunk->next = nextChunk;
        //~ lr->currChunk = nextChunk;
        //~ lr->currInd = 1;
    //~ }
    //~ lr->totalNumber++;
//~ }




//~ bool _isLexicographicallyLE(char* byteStart, char digits[19]) {
    //~ for (int i = 0; i < 19; i++) {
        //~ if (*(byteStart + i) < digits[i]) { return true; }
        //~ if (*(byteStart + i) < digits[i]) { return false; }
    //~ }
    //~ return true;
//~ }

//~ bool checkIntRange(char* byteStart, int byteLength, bool isNegative) {
    //~ if (byteLength != 19) return byteLength < 19;
    //~ return isNegative ? _isLexicographicallyLE(byteStart, minInt) : _isLexicographicallyLE(byteStart, maxInt);
//~ }

//~ uint64_t intOfDigits(char* byteStart, int byteLength) {
    //~ uint64_t result = 0;
    //~ uint64_t powerOfTen = 1;
    //~ for (char* ind = byteStart + byteLength - 1; ind >= byteStart; --byteStart) {
        //~ uint64_t digitValue = (*ind - ASCII.digit0)*powerOfTen;
        //~ result += digitValue;
        //~ powerOfTen *= 10;
    //~ }
    //~ return result;
//~ }




//~ LexResult* lexicallyAnalyze(String* inp, Arena* ar) {
    //~ LexResult* result = mkLexResult(ar);
    //~ StackRememberedToken* backtrack = mkStackRememberedToken(ar, 100);
    //~ const int len = inp->length;

    //~ // scratch space to write out digits excluding underscores from number literals
    //~ String* digitsScratchPad = allocateScratchSpace(ar, 16);

    //~ for (int i = 0; i < len; i++) {
        //~ unsigned char cByte = inp->content[i];
        //~ unsigned char nextByte = inp->content[i + 1];
        //~ if (cByte > 127) {
            //~ errorOut("Non-ASCII characters are only allowed in comments and string litersl!", result);
            //~ return result;
        //~ }
        //~ if (cByte == ASCII.space || cByte == ASCII.emptyCR) {
            //~ i++;
        //~ } else if (cByte == ASCII.emptyLF) {
            //~ if (hasValuesRememberedToken(backtrack)) {
                //~ RememberedToken* a = backtrack->content[backtrack->length - 1];
                //~ if (a->token->tokType == TokType.curlyBraces && true) {
                    //~ // we are in a CurlyBraces context, so a newline means closing current statement and opening a new one


                //~ }
                //~ i++;
            //~ }
        //~ } else if (cByte == ASCII.colonSemi) {

        //~ } else if (cByte == ASCII.curlyOpen) {

        //~ } else if (cByte == ASCII.curlyClose) {

        //~ } else if (cByte == ASCII.parenthesisOpen) {

        //~ } else if (cByte == ASCII.parenthesisClose) {

        //~ } else if (cByte == ASCII.bracketOpen) {

        //~ } else if (cByte == ASCII.bracketClose)

    //~ }

    //~ return result;
//~ }
