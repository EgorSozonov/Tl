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

#define CURR_BT lr->inp->content[lr->i]
#define NEXT_BT lr->inp->content[lr->i + 1]

DEFINE_STACK(RememberedToken)

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

/** Predicate for an element of a scope, which can be any type of statement, or a split statement (such as an "if" or "match" form) */
bool isParensElement(unsigned int tType) {
    return  tType == tokLexScope || tType == tokStmtTypeDecl || tType == tokParens
            || tType == tokStmtAssignment;
}

/** Predicate for an element of a scope, which can be any type of statement, or a split statement (such as an "if" or "match" form) */
bool isScopeElementVal(unsigned int tType) {
    return tType == tokLexScope
            || tType == tokStmtTypeDecl
            || tType == tokParens
            || tType == tokStmtAssignment;
}

/** Sets i to beyond input's length to communicate to callers that lexing is over */
private void exitWithError(const char errMsg[], Lexer* res) {
    res->i = res->inp->length;
    res->wasError = true;
    res->errMsg = allocateLiteral(res->arena, errMsg);
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private void checkPrematureEnd(int requiredSymbols, Lexer* lr) {
    if (lr->i > lr->inp->length - requiredSymbols) {
        exitWithError(errorPrematureEndOfInput, lr);
    }
}

private void lexNumber(Lexer* lr) {

}

/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was uncapitalized
 */
private bool lexWordChunk(Lexer* lr) {
    bool result = false;

    if (lr->inp->content[lr->i] == aUnderscore) {
        checkPrematureEnd(2, lr);
        lr->i++;
    } else {
        checkPrematureEnd(1, lr);
    }
    if (lr->wasError) return false;

    if (isLowercaseLetter(CURR_BT)) {
        result = true;
    } else if (!isCapitalLetter(CURR_BT)) {
        exitWithError(errorWordChunkStart, lr);
    }
    lr->i++;

    while (lr->i < lr->inp->length && isAlphanumeric(CURR_BT)) {
        lr->i++;
    }
    if (lr->i < lr->inp->length && CURR_BT == aUnderscore) {
        exitWithError(errorWordUnderscoresOnlyAtStart, lr);
    }

    return result;
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Examples of acceptable expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of unacceptable expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private void lexWordInternal(unsigned int wordType, Lexer* lr) {
    int startInd = lr->i;
    bool metUncapitalized = lexWordChunk(lr);
    while (lr->i < (lr->inp->length - 1) && CURR_BT == aDot && NEXT_BT != aBracketLeft && !lr->wasError) {
        lr->i++;
        bool isCurrUncapitalized = lexWordChunk(lr);
        if (metUncapitalized && !isCurrUncapitalized) {
            exitWithError(errorWordCapitalizationOrder, lr);
        }
        metUncapitalized = isCurrUncapitalized;
    }
    int realStartInd = (wordType == tokWord) ? startInd : (startInd - 1); // accounting for the . or @ at the start
    bool paylCapitalized = metUncapitalized ? 0 : 1;

    byte firstByte = lr->inp->content[startInd];
    int lenBytes = lr->i - realStartInd;
    if (wordType == tokAtWord || firstByte < aBLower || firstByte > aWLower) {
        addToken((Token){.tp=wordType, .payload2=paylCapitalized, .startByte=realStartInd, .lenBytes=lenBytes}, lr);
    } else {
        int mbReservedWord = possiblyReservedDispatch[(firstByte - aBLower)](startInd, lr->i - startInd);
        if (mbReservedWord > 0) {
            if (wordType == tokDotWord) {
                exitWithError(errorWordReservedWithDot, lr);
            } else if (mbReservedWord == reservedTrue) {
                addToken((Token){.tp=tokBool, .payload2=1, .startByte=realStartInd, .lenBytes=lenBytes}, lr);
            } else if (mbReservedWord == reservedFalse) {
                addToken((Token){.tp=tokBool, .payload2=0, .startByte=realStartInd, .lenBytes=lenBytes}, lr);
            } else {
                lexReservedWord(mbReservedWord, realStartInd, lr);
            }
        } else  {
            addToken((Token){.tp=wordType, .payload2=paylCapitalized, .startByte=realStartInd, .lenBytes=lenBytes }, lr);
        }
    }
}


private void lexWord(Lexer* lr) {
    lexWordInternal(tokWord, lr);
}

void lexDotSomething(Lexer* lr) {

}

void lexAtWord(Lexer* lr) {

}

void lexOperator(Lexer* lr) {

}

void lexMinus(Lexer* lr) {

}

void lexColon(Lexer* lr) {

}

void lexParenLeft(Lexer* lr) {

}

void lexParenRight(Lexer* lr) {

}

void lexCurlyLeft(Lexer* lr) {

}

void lexCurlyRight(Lexer* lr) {

}

void lexBracketLeft(Lexer* lr) {

}

void lexBracketRight(Lexer* lr) {

}

void lexUnexpectedSymbol(Lexer* lr) {

}

void lexNonAsciiError(Lexer* lr) {

}

private int determineReservedB(int startByte, int lenBytes, Lexer* lr) {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesBreak)) return tokStmtBreak;
    return 0;
}

private int determineUnreserved(int startByte, int lenBytes, Lexer* lr) {
    return 0;
}

/**
    * Table for dispatch on the first letter of a word in case it might be a reserved word.
    * It's indexed on the diff between first byte and the letter "c" (the earliest letter a reserved word may start with)
    */
private ReservedProbe (*possiblyReservedDispatch(Arena* ar))[19] {
    ReservedProbe (*result)[19] = allocateOnArena(ar, 19*sizeof(ReservedProbe));
    ReservedProbe* p = *result;
    for (int i = 2; i < 18; i++) {
        p[i] = determineUnreserved;
    }
    p[0] = determineReservedB;
    // i -> when(i) {
    //     0 -> Lexer::determineReservedB
    //     1 -> Lexer::determineReservedC
    //     3 -> Lexer::determineReservedE
    //     4 -> Lexer::determineReservedF
    //     7 -> Lexer::determineReservedI
    //     10 -> Lexer::determineReservedL
    //     11 -> Lexer::determineReservedM
    //     16 -> Lexer::determineReservedR
    //     18 -> Lexer::determineReservedT
    //     21 -> Lexer::determineReservedW
    //     else -> Lexer::determineUnreserved
    // }
    return result;
}

private LexerFunc (*buildLexerDispatch(Arena* ar))[256] {
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



private void finalize(Lexer* this) {
    if (!hasValuesRememberedToken(this->backtrack)) {
        exitWithError(errorPunctuationExtraOpening, this);
    }
}


Lexer* lexicallyAnalyze(String* inp, LanguageDefinition* lang, Arena* ar) {
    Lexer* result = createLexer(inp, ar);
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
    LexerFunc (*dispatchTable)[256] = buildLexerDispatch(ar);

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
