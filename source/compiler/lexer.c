#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "lexer.h"
#include "lexerConstants.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stack.h"


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

const OpDef noFun = {
    .name = &empty,
    .bytes = {0, 0, 0, 0},
    .precedence = 0,
    .arity = 0,
    .extensible = false,
    .bindingIndex = -1,
    .overloadable = false
};


#define TESTRESERVED(reservedBytesName, nameLen)    \
    lenReser = nameLen; \
    if (lenBytes == lenReser && testByteSequence(lr->inp, startByte, reservedBytesCatch, lenReser)) \
        return tokStmtBreak;


private int determineReservedB(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesBreak, 5)
    return 0;
}


private int determineReservedC(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesCatch, 5)
    TESTRESERVED(reservedBytesContinue, 8)
    return 0;
}


private int determineReservedE(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesEmbed, 5)
    return 0;
}


private int determineReservedF(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesFalse, 5)
    TESTRESERVED(reservedBytesFn, 2)
    TESTRESERVED(reservedBytesFor, 3)
    return 0;
}


private int determineReservedI(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesIf, 2)
    TESTRESERVED(reservedBytesIfEq, 4)
    TESTRESERVED(reservedBytesIfPr, 4)
    TESTRESERVED(reservedBytesImpl, 4)
    TESTRESERVED(reservedBytesInterface, 9)
    return 0;
}


private int determineReservedM(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesMatch, 5)
    return 0;
}


private int determineReservedO(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesOpen, 4)
    return 0;
}


private int determineReservedR(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesReturn, 6)
    return 0;
}


private int determineReservedS(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesStruct, 6)
    return 0;
}


private int determineReservedT(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    TESTRESERVED(reservedBytesTest, 4)
    TESTRESERVED(reservedBytesTrue, 4)
    TESTRESERVED(reservedBytesTry, 3)
    TESTRESERVED(reservedBytesType, 4)
    return 0;
}

private int determineUnreserved(int startByte, int lenBytes, Lexer* lr) {
    return 0;
}


void lexDotSomething(Lexer* lr) {
    exitWithError(errorUnrecognizedByte, lr);
}

private void lexNumber(Lexer* lr) {
    exitWithError(errorUnrecognizedByte, lr);
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 */
private void setSpanType(int tokenInd, unsigned int tType, Lexer* lr) {
    lr->tokens[tokenInd].tp = tType;
}


private void convertGrandparentToScope(Lexer* lr) {
    int indPenultimate = lr->backtrack->length - 2;
    if (indPenultimate > -1) {
        RememberedToken penult = (*lr->backtrack->content)[indPenultimate];
        if (penult.tp != tokLexScope){
            (*lr->backtrack->content)[indPenultimate].tp = tokLexScope;
            setSpanType(penult.numberOfToken, tokLexScope, lr);
        }
    }
}

/**
 * Lexer action for a reserved word. If at the start of a statement/expression, it mutates the type of said expression,
 * otherwise it emits a corresponding token.
 * Implements the validation 2 from Syntax.txt
 */
private void lexReservedWord(int reservedWordType, int startInd, Lexer* lr) {
    if (reservedWordType == tokStmtBreak) {
        addToken((Token){.tp=tokStmtBreak, .startByte=startInd, .lenBytes=5}, lr);
        return;
    }
    if (!hasValuesRememberedToken(lr->backtrack) || popRememberedToken(lr->backtrack).numberOfToken < (lr->totalTokens - 1)) {
        addToken((Token){.tp=tokReserved, .payload2=reservedWordType, .startByte=startInd, .lenBytes=(lr->i - startInd)}, lr);
        return;
    }

    RememberedToken currSpan = peekRememberedToken(lr->backtrack);
    if (currSpan.numberOfToken < (lr->totalTokens - 1) || currSpan.tp >= firstCoreFormTokenType) {
        // if this is not the first token inside this span, or if it's already been converted to core form, add
        // the reserved word as a token
        addToken((Token){ .tp=tokReserved, .payload2=reservedWordType, .startByte=startInd, .lenBytes=lr->i - startInd}, lr);
        return;
    }

    setSpanType(currSpan.numberOfToken, reservedWordType, lr);
    if (reservedWordType == tokStmtReturn) {
        convertGrandparentToScope(lr);
    }
    (*lr->backtrack->content)[lr->backtrack->length - 1] = (RememberedToken){
        .tp=reservedWordType, .numberOfToken=currSpan.numberOfToken
    };
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
        int mbReservedWord = (*lr->possiblyReservedDispatch)[(firstByte - aBLower)](startInd, lr->i - startInd, lr);
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

void lexAtWord(Lexer* lr) {
    exitWithError(errorUnrecognizedByte, lr);
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
    exitWithError(errorUnrecognizedByte, lr);
}

void lexBracketRight(Lexer* lr) {
    exitWithError(errorUnrecognizedByte, lr);
}

void lexUnexpectedSymbol(Lexer* lr) {
    exitWithError(errorUnrecognizedByte, lr);
}

void lexNonAsciiError(Lexer* lr) {
    exitWithError(errorNonAscii, lr);
}



private LexerFunc (*buildDispatch(Arena* ar))[256] {
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


    for (int i = 0; i < countOperatorStartSymbols; i++) {
        p[operatorStartSymbols[i]] = lexOperator;
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

/**
    * Table for dispatch on the first letter of a word in case it might be a reserved word.
    * It's indexed on the diff between first byte and the letter "c" (the earliest letter a reserved word may start with)
    */
private ReservedProbe (*buildReserved(Arena* ar))[countReservedLetters] {
    ReservedProbe (*result)[countReservedLetters] = allocateOnArena(ar, countReservedLetters*sizeof(ReservedProbe));
    ReservedProbe* p = *result;
    for (int i = 2; i < 18; i++) {
        p[i] = determineUnreserved;
    }
    p[0] = determineReservedB;
    p[1] = determineReservedC;
    p[3] = determineReservedE;
    p[4] = determineReservedF;
    p[6] = determineReservedI;
    p[11] = determineReservedM;
    p[13] = determineReservedO;
    p[16] = determineReservedR;
    p[17] = determineReservedS;
    p[18] = determineReservedT;
    return result;
}

private OpDef (*buildOperators(Arena* ar))[countOperators] {
    OpDef (*result)[countOperators] = allocateOnArena(ar, countOperators*sizeof(OpDef));

    OpDef* p = *result;
    /**
    * This is an array of 4-byte arrays containing operator byte sequences.
    * Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
    * It's used to lex operator symbols using left-to-right search.
    */
    p[ 0] = (OpDef){ .name = allocateLiteral(ar, "!="), .precedence=11, .arity=2,
                                    .bindingIndex=0, .bytes={aExclamation, aEqual, 0, 0 } };
    p[ 1] = (OpDef){ .name = allocateLiteral(ar, "!"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=1, .bytes={aExclamation, 0, 0, 0 } };
    p[ 2] = (OpDef){ .name = allocateLiteral(ar, "#"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=2, .bytes={aSharp, 0, 0, 0 } };
    p[ 3] = (OpDef){ .name = allocateLiteral(ar, "$"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=3, .bytes={aDollar, 0, 0, 0 } };
    p[ 4] = (OpDef){ .name = allocateLiteral(ar, "%"), .precedence=20, .arity=2, .extensible=true,
                                    .bindingIndex=4, .bytes={aPercent, 0, 0, 0 } };
    p[ 5] = (OpDef){ .name = allocateLiteral(ar, "&&"), .precedence=9, .arity=2, .extensible=true,
                                    .bindingIndex=5, .bytes={aAmpersand, aAmpersand, 0, 0 } };
    p[ 6] = (OpDef){ .name = allocateLiteral(ar, "&"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=6, .bytes={aAmpersand, 0, 0, 0 } };
    p[ 7] = (OpDef){ .name = allocateLiteral(ar, "*"), .precedence=20, .arity=2, .extensible=true,
                                    .bindingIndex=7, .bytes={aTimes, 0, 0, 0 } };
    p[ 8] = (OpDef){ .name = allocateLiteral(ar, "++"), .precedence=functionPrec, .arity=1,
                                    .bindingIndex=8, .bytes={aPlus, aPlus, 0, 0 } };
    p[ 9] = (OpDef){ .name = allocateLiteral(ar, "+"), .precedence=17, .arity=2, .extensible=true,
                                    .bindingIndex=9, .bytes={aPlus, 0, 0, 0 } };
    p[10] = (OpDef){ .name = allocateLiteral(ar, "--"), .precedence=functionPrec, .arity=1,
                                    .bindingIndex=10, .bytes={aMinus, aMinus, 0, 0 } };
    p[11] = (OpDef){ .name = allocateLiteral(ar, "-"), .precedence=17, .arity=2, .extensible=true,
                                    .bindingIndex=11, .bytes={aMinus, 0, 0, 0 } };
    p[12] = (OpDef){ .name = allocateLiteral(ar, "/"), .precedence=20, .arity=2, .extensible=true,
                                    .bindingIndex=12, .bytes={aDivBy, 0, 0, 0 } };
    p[13] = (OpDef){ .name = allocateLiteral(ar, ":="), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aColon, aEqual, 0, 0 }};
    p[14] = (OpDef){ .name = allocateLiteral(ar, ";<"), .precedence=1, .arity=2,
                                    .bindingIndex=13, .bytes={aSemicolon, aLessThan, 0, 0 } };
    p[15] = (OpDef){ .name = allocateLiteral(ar, ";"), .precedence=1, .arity=2,
                                    .bindingIndex=14, .bytes={aSemicolon, 0, 0, 0 } };
    p[16] = (OpDef){ .name = allocateLiteral(ar, "<="), .precedence=12, .arity=2,
                                    .bindingIndex=15, .bytes={aLessThan, aEqual, 0, 0 } };
    p[17] = (OpDef){ .name = allocateLiteral(ar, "<<"), .precedence=14, .arity=2, .extensible=true,
                                    .bindingIndex=16, .bytes={aTimes, 0, 0, 0 } };
    p[18] = (OpDef){ .name = allocateLiteral(ar, "<-"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aLessThan, aMinus, 0, 0 } };
    p[19] = (OpDef){ .name = allocateLiteral(ar, "<"), .precedence=12, .arity=2,
                                    .bindingIndex=17, .bytes={aLessThan, 0, 0, 0 } };
    p[20] = (OpDef){ .name = allocateLiteral(ar, "=>"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aEqual, aGreaterThan, 0, 0 } };
    p[21] = (OpDef){ .name = allocateLiteral(ar, "=="), .precedence=11, .arity=2,
                                    .bindingIndex=18, .bytes={aEqual, aEqual, 0, 0 } };
    p[22] = (OpDef){ .name = allocateLiteral(ar, "="), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aEqual, 0, 0, 0 } };
    p[23] = (OpDef){ .name = allocateLiteral(ar, ">=<="), .precedence=12, .arity=3,
                                    .bindingIndex=19, .bytes={aGreaterThan, aEqual, aLessThan, aEqual } };
    p[24] = (OpDef){ .name = allocateLiteral(ar, ">=<"), .precedence=12, .arity=3,
                                    .bindingIndex=20, .bytes={aGreaterThan, aEqual, aLessThan, 0 } };
    p[25] = (OpDef){ .name = allocateLiteral(ar, "><="), .precedence=12, .arity=3,
                                    .bindingIndex=21, .bytes={aGreaterThan, aLessThan, aEqual, 0 } };
    p[26] = (OpDef){ .name = allocateLiteral(ar, "><"), .precedence=12, .arity=3,
                                    .bindingIndex=22, .bytes={aGreaterThan, aLessThan, 0, 0 } };
    p[27] = (OpDef){ .name = allocateLiteral(ar, ">="), .precedence=12, .arity=2,
                                    .bindingIndex=23, .bytes={aGreaterThan, aEqual, 0, 0 } };
    p[28] = (OpDef){ .name = allocateLiteral(ar, ">>"), .precedence=14, .arity=2, .extensible=true,
                                    .bindingIndex=24, .bytes={aGreaterThan, aGreaterThan, 0, 0 } };
    p[29] = (OpDef){ .name = allocateLiteral(ar, ">"), .precedence=12, .arity=2,
                                    .bindingIndex=25, .bytes={aGreaterThan, 0, 0, 0 } };
    p[30] = (OpDef){ .name = allocateLiteral(ar, "?"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=26, .bytes={aQuestion, 0, 0, 0 } };
    p[31] = (OpDef){ .name = allocateLiteral(ar, "\\"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aBackslash, 0, 0, 0 } };
    p[32] = (OpDef){ .name = allocateLiteral(ar, "^"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=27, .bytes={aCaret, 0, 0, 0 } };
    p[33] = (OpDef){ .name = allocateLiteral(ar, "||"), .precedence=3, .arity=2, .extensible=true,
                                    .bindingIndex=28, .bytes={aPipe, aPipe, 0, 0 } };
    p[34] = (OpDef){ .name = allocateLiteral(ar, "|"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aPipe, 0, 0, 0 } };
    return result;
}


LanguageDefinition* buildLanguageDefinitions(Arena* ar) {
    LanguageDefinition* result = allocateOnArena(ar, sizeof(LanguageDefinition));
    /*
    * Definition of the operators with info for those that act as functions. The order must be exactly the same as
    * OperatorType.
    */

    result->possiblyReservedDispatch = buildReserved(ar);
    result->dispatchTable = buildDispatch(ar);
    result->operators = buildOperators(ar);

    return result;
}


Lexer* createLexer(String* inp, Arena* ar) {
    Lexer* result = allocateOnArena(ar, sizeof(Lexer));

    result->inp = inp;
    result->capacity = LEXER_INIT_SIZE;
    result->tokens = allocateOnArena(ar, LEXER_INIT_SIZE*sizeof(Token));

    result->backtrack = createStackRememberedToken(ar, 8);

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
    if (!this->wasError && !hasValuesRememberedToken(this->backtrack)) {
        exitWithError(errorPunctuationExtraOpening, this);
    }
}


Lexer* lexicallyAnalyze(String* inp, LanguageDefinition* lang, Arena* ar) {
    Lexer* result = createLexer(inp, ar);
    if (inp->length == 0) {
        exitWithError("Empty input", result);
    }

    // Check for UTF-8 BOM at start of file
    if (inp->length >= 3
        && (unsigned char)inp->content[0] == 0xEF
        && (unsigned char)inp->content[1] == 0xBB
        && (unsigned char)inp->content[2] == 0xBF) {
        result->i = 3;
    }
    LexerFunc (*dispatch)[256] = lang->dispatchTable;
    result->possiblyReservedDispatch = lang->possiblyReservedDispatch;
    // Main loop over the input
    while (result->i < inp->length) {
        ((*dispatch)[inp->content[result->i]])(result);
    }

    finalize(result);
    return result;
}



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

