#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "lexer.h"
#include "lexerConstants.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stack.h"


#define CURR_BT inp[lr->i]
#define NEXT_BT inp[lr->i + 1]

DEFINE_STACK(RememberedToken)
DEFINE_STACK(int)


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
    res->errMsg = allocLit(res->arena, errMsg);
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private void checkPrematureEnd(int requiredSymbols, Lexer* lr) {
    if (lr->i > (lr->inpLength - requiredSymbols)) {
        exitWithError(errorPrematureEndOfInput, lr);
    }
}

const OpDef noFun = {
    .name = &empty,
    .bytes = {0, 0, 0, 0},
    .precedence = 0,
    .arity = 0,
    .extensible = false,
    .binding = -1,
    .overloadable = false
};


#define PROBERESERVED(reservedBytesName, returnVarName)    \
    lenReser = sizeof(reservedBytesName); \
    if (lenBytes == lenReser && testByteSequence(lr->inp, startByte, reservedBytesCatch, lenReser)) \
        return returnVarName;


private int determineReservedA(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesAlias, tokStmtAlias)
    PROBERESERVED(reservedBytesAwait, tokStmtAwait)
    return 0;
}

private int determineReservedB(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesBreak)
    return 0;
}
private int determineReservedB(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesBreak)
    return 0;
}


private int determineReservedC(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesCatch)
    PROBERESERVED(reservedBytesContinue)
    return 0;
}


private int determineReservedE(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesEmbed)
    return 0;
}


private int determineReservedF(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesFalse)
    PROBERESERVED(reservedBytesFor)
    return 0;
}


private int determineReservedI(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesIf)
    PROBERESERVED(reservedBytesIfEq)
    PROBERESERVED(reservedBytesIfPr)
    PROBERESERVED(reservedBytesImpl)
    PROBERESERVED(reservedBytesInterface)
    return 0;
}


private int determineReservedM(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesMatch)
    PROBERESERVED(reservedBytesMut)
    return 0;
}


private int determineReservedN(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesNodestruct)
    return 0;
}


private int determineReservedR(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesReturn, tokStmtReturn)
    return 0;
}


private int determineReservedS(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesStruct)
    return 0;
}


private int determineReservedT(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesTest)
    PROBERESERVED(reservedBytesTrue)
    PROBERESERVED(reservedBytesTry)
    PROBERESERVED(reservedBytesType)
    return 0;
}

private int determineReservedY(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesYield)
    return 0;
}

private int determineUnreserved(int startByte, int lenBytes, Lexer* lr) {
    return 0;
}

private void addNewLine(int j, Lexer* lr) {
    lr->newlines[lr->newlinesNextInd] = j;
    lr->newlinesNextInd++;
    if (lr->newlinesNextInd == lr->newlinesCapacity) {
        Arr(int) newNewlines = allocateOnArena(lr->newlinesCapacity*2*sizeof(int), lr->arena);
        memcpy(newNewlines, lr->newlines, lr->newlinesCapacity);
        lr->newlines = newNewlines;
        lr->newlinesCapacity *= 2;
    }
}

/** A paren starts a multi-line statement, anything else - a regular statement. */
private void wrapInAStatement(Lexer* lr, Arr(byte) inp) {
    bool isMultiline = CURR_BT == aParenLeft;
    if (hasValuesRememberedToken(lr->backtrack.isEmpty())) {
        unsigned int topType = peekRememberedToken(lr->backtrack).tp;
        if (topType == tokLexScope || topType >= firstCoreFormTok) {
            pushRememberedToken((RembemberedToken){.tp = tokStmt, .numberOfToken = lr->totalTokens, 
                                                                  .isMultiline=isMultiline }, lr->backtrack);
            add((Token) .tp = tokStmt, .startByte = i},  lr);
        }                
    } else {
        pushRememberedToken((RembemberedToken){.tp = tokStmt, .isMultiline=isMultiline, 
                                                              .numberOfToken = lr->totalTokens}, lr->backtrack);
        add((Token) .tp = tokStmt, .startByte = i},  lr);
    }
}


private void lexDot(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}

private void lexNumber(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 */
private void setSpanType(int tokenInd, unsigned int tType, Lexer* lr) {
    lr->tokens[tokenInd].tp = tType;
}

/** Append a punctuation token */
private void appendPunctuation(unsigned int tType, int startByte, Lexer* lr) {
    add((Token) {.tp = tType, .startByte = startByte}, lr);
}

/**
 * Adds a token which serves punctuation purposes, i.e. either a (, a {, a [, a .[ or a $
 * These tokens are used to define the structure, that is, nesting within the AST.
 * Upon addition, they are saved to the backtracking stack to be updated with their length
 * once it is known.
 * The startByte & lengthBytes don't include the opening and closing delimiters, and
 * the lenTokens also doesn't include the punctuation token itself - only the insides of the
 * scope. Thus, for '(asdf)', the opening paren token will have a byte length of 4 and a
 * token length of 1.
 */
private void openPunctuation(unsigned int tType, Lexer* lr) {
    pushRememberedToken( 
        (RememberedToken){ .tp = tType, .numberOfToken = lr->totalTokens, .wasOriginallyColon = tType == tokColon},
        lr->backtrack
    );

    appendPunctuation(tType, i + 1, lr);
    lr->i++;
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
        add((Token){.tp=tokStmtBreak, .startByte=startInd, .lenBytes=5}, lr);
        return;
    }
    if (!hasValuesRememberedToken(lr->backtrack) || popRememberedToken(lr->backtrack).numberOfToken < (lr->totalTokens - 1)) {
        add((Token){.tp=tokReserved, .payload2=reservedWordType, .startByte=startInd, .lenBytes=(lr->i - startInd)}, lr);
        return;
    }

    RememberedToken currSpan = peekRememberedToken(lr->backtrack);
    if (currSpan.numberOfToken < (lr->totalTokens - 1) || currSpan.tp >= firstCoreFormTokenType) {
        // if this is not the first token inside this span, or if it's already been converted to core form, add
        // the reserved word as a token
        add((Token){ .tp=tokReserved, .payload2=reservedWordType, .startByte=startInd, .lenBytes=lr->i - startInd}, lr);
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
 * Returns True if the lexed chunk was capitalized
 */
private bool wordChunk(Lexer* lr, Arr(byte) inp) {
    bool result = false;

    if (CURR_BT == aUnderscore) {
        checkPrematureEnd(2, lr);
        lr->i++;
    } else {
        checkPrematureEnd(1, lr);
    }
    if (lr->wasError) return false;

    if (isCapitalLetter(CURR_BT)) {
        result = true;
    } else if (!isLowercaseLetter(CURR_BT)) {
        exitWithError(errorWordChunkStart, lr);
    }
    lr->i++;

    while (lr->i < lr->inpLength && isAlphanumeric(CURR_BT)) {
        lr->i++;
    }
    if (lr->i < lr->inpLength && CURR_BT == aUnderscore) {
        exitWithError(errorWordUnderscoresOnlyAtStart, lr);
    }
    return result;
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Examples of acceptable expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of unacceptable expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private void wordInternal(uint wordType, Lexer* lr, Arr(byte) inp) {
    int startInd = lr->i;
    bool wasCapitalized = wordChunk(lr, inp);
    bool isAlsoAccessor = false;
    while (lr->i < (lr->inpLength - 1)) {
        byte currBt = CURR_BT;
        if (currBt == aBracketLeft) {
            isAlsoAccessor = true; // data accessor like a[5]
            break;
        } else if (currBt == aDot) {
            byte nextBt = NEXT_BT;
            if (isLetter(nextBt) || nextBt == aUnderscore) {
                lr->i++;
                bool isCurrCapitalized = wordChunk(lr, inp);
                if (wasCapitalized && isCurrCapitalized) {
                    exitWithError(errorWordCapitalizationOrder, lr);
                }
                wasCapitalized = isCurrCapitalized;
            } else {
                break;
            }
        } else {
            break;
        }        
    }
    
    int realStartInd = (wordType == tokWord) ? startInd : (startInd - 1); // accounting for the . or @ at the start
    bool paylCapitalized = wasCapitalized ? 1 : 0;

    byte firstByte = lr->inp->content[startInd];
    int lenBytes = lr->i - realStartInd;
    if (wordType != tokWord && isAlsoAccessor) exitWithError(errorWordWrongAccessor, lr);
    if (wordType == tokAtWord || firstByte < aALower || firstByte > aYLower) {
        add((Token){.tp=wordType, .payload2=paylCapitalized, .startByte=realStartInd, .lenBytes=lenBytes}, lr);
        if (isAlsoAccessor) {
            openPunctuation(tokAccessor);
        }
    } else {
        int mbReservedWord = (*lr->possiblyReservedDispatch)[(firstByte - aBLower)](startInd, lr->i - startInd, lr);
        if (mbReservedWord > 0) {
            if (wordType == tokDotWord) {
                exitWithError(errorWordReservedWithDot, lr);
            } else if (mbReservedWord == reservedTrue) {
                add((Token){.tp=tokBool, .payload2=1, .startByte=realStartInd, .lenBytes=lenBytes}, lr);
            } else if (mbReservedWord == reservedFalse) {
                add((Token){.tp=tokBool, .payload2=0, .startByte=realStartInd, .lenBytes=lenBytes}, lr);
            } else {
                lexReservedWord(mbReservedWord, realStartInd, lr);
            }
        } else  {
            add((Token){.tp=wordType, .payload2=paylCapitalized, .startByte=realStartInd, .lenBytes=lenBytes }, lr);
        }
    }
}


private void lexWord(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    wordInternal(tokWord, lr, inp);
}


void lexAtWord(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    checkPrematureEnd(2, inp);
    lr->i++;
    wordInternal(tokAtWord, lr, inp);
}


void lexOperator(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}


void lexMinus(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}


void lexColon(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}


void lexParenLeft(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}


void lexParenRight(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}


void lexCurlyLeft(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}


void lexCurlyRight(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}

void lexBracketLeft(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}

void lexBracketRight(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}

void lexSpace(Lexer* lr, Arr(byte) inp) {
    lr->i++;
    while (lr->i < lr->inpLength) {
        if (CURR_BT != aSpace) return;
        lr->i++;
    }
}

/** A newline in a Stmt ends it. A newline in a StmtMulti, or in something nested within it, has no effect.  */
void lexNewline(Lexer* lr, Arr(byte) inp) {
    addNewLine(lr->i);
    lr->i++;
    // TODO check statement type to maybe end it
    while (lr->i < lr->inpLength) {
        if (CURR_BT != aSpace) return;
        lr->i++;
    }    
}

void lexStringLiteral(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    
    int j = lr->i + 1;
    while (j < lr->inpLength) {
        byte cByte = inp[j];
        if (cByte == aQuote) {
            add((Token){.tp=tokString, .startByte=(lr->i + 1), .lenBytes=(j - lr->i - 1)}, lr);
            lr->i = j + 1;
            return;
        } else {
            j++;
        }
    }
    exitWithError(errorPrematureEndOfInput, lr);
}

/**
 * Appends a doc comment token if it's the first one, or elongates the existing token if there was already
 * a doc comment on the previous line.
 * This logic handles multiple consecutive lines of doc comments.
 * NOTE: this is the only function in the whole lexer that needs the endByte instead of lenBytes!
 */
private void appendDocComment(int startByte, int endByte, Lexer* lr) {
    if (lr->nextInd == 0 || lr->tokens[lr->nextInd - 1].tp != tokDocComment) {
        add((Token){.tp = tokDocComment, .startByte = startByte, .lenBytes = endByte - startByte + 1});
    } else {        
        lr->tokens[nextInd - 1].lenBytes = endByte - (tokens[nextInd - 1].startByte) + 1;
    }
}

/**
 *  Doc comments, syntax is ## Doc comment
 */
private void docComment(Lexer* lr, Arr(byte) inp) {
    int startByte = lr->i + 2; // +2 for the '##'
    for (int j = startByte; j < lr->inpLength; j++) {
        if (inp[j] == aNewline) {
            addNewLine(j, lr);
            if (j > startByte) {
                appendDocComment(startByte, j - 1, lr);
            }
            lr->i = j + 1;
            return;
        }
    }
    lr->i = lr->inpLength; // if we're here, then we haven't met a newline, hence we are at the document's end
    appendDocComment(startByte, lr->inpLength - 1, lr);
}

/**
 *  Doc comments, syntax is # The comment
 */
private void lexComment(Lexer* lr, Arr(byte) inp) {
    int j = lr->i + 1;
    if (j < lr->inpLength && inp[j] == aSharp) {
        lexDocComment(lr, inp);
        return;
    }
    while (j < lr->inpLength) {
        byte cByte = inp[j];
        if (cByte == aNewline) {
            lr->i = j + 1;
            return;
        } else {
            j++;
        }
    }
    lr->i = j;
}

void lexUnexpectedSymbol(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorUnrecognizedByte, lr);
}

void lexNonAsciiError(Lexer* lr, Arr(byte) inp) {
    exitWithError(errorNonAscii, lr);
}



private LexerFunc (*buildDispatch(Arena* a))[256] {
    LexerFunc (*result)[256] = allocateOnArena(256*sizeof(LexerFunc), a);
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
    p[aDot] = lexDot;
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
    p[aSpace] = lexSpace;
    p[aCarriageReturn] = lexSpace;
    p[aNewline] = lexNewline;
    p[aApostrophe] = lexStringLiteral;
    p[aDivBy] = lexComment;
    return result;
}

/**
 * Table for dispatch on the first letter of a word in case it might be a reserved word.
 * It's indexed on the diff between first byte and the letter "a" (the earliest letter a reserved word may start with)
 */
private ReservedProbe (*buildReserved(Arena* a))[countReservedLetters] {
    ReservedProbe (*result)[countReservedLetters] = allocateOnArena(countReservedLetters*sizeof(ReservedProbe), a);
    ReservedProbe* p = *result;
    for (int i = 2; i < 25; i++) {
        p[i] = determineUnreserved;
    }
    p[0] = determineReservedA;
    p[1] = determineReservedB;
    p[2] = determineReservedC;
    p[4] = determineReservedE;
    p[5] = determineReservedF;
    p[7] = determineReservedI;
    p[12] = determineReservedM;
    p[13] = determineReservedN;
    p[14] = determineReservedO;
    p[17] = determineReservedR;
    p[18] = determineReservedS;
    p[19] = determineReservedT;
    p[24] = determineReservedY;
    return result;
}

/** The set of base operators in the language */
private OpDef (*buildOperators(Arena* a))[countOperators] {
    OpDef (*result)[countOperators] = allocateOnArena(countOperators*sizeof(OpDef), a);

    OpDef* p = *result;
    /**
    * This is an array of 4-byte arrays containing operator byte sequences.
    * Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
    * It's used to lex operator symbols using left-to-right search.
    */
    p[ 0] = (OpDef){ .name=allocLit(a, "!="), .precedence=11, .arity=2, .binding=0, .bytes={aExclamation, aEqual, 0, 0 } };
    p[ 1] = (OpDef){ .name=allocLit(a, "!"), .precedence=prefixPrec, .arity=1, .binding=1, .bytes={aExclamation, 0, 0, 0 } };
    p[ 2] = (OpDef){ .name=allocLit(a, "$"), .precedence=prefixPrec, .arity=1, .binding=2, .bytes={aDollar, 0, 0, 0 }, .overloadable=true };
    p[ 3] = (OpDef){ .name=allocLit(a, "%"), .precedence=20, .arity=2, .extensible=true, .binding=3, .bytes={aPercent, 0, 0, 0 } };
    p[ 4] = (OpDef){ .name=allocLit(a, "&&"), .precedence=9, .arity=2, .binding=5, .bytes={aAmp, aAmp, 0, 0 }, .assignable=true };
    p[ 5] = (OpDef){ .name=allocLit(a, "&"), .precedence=prefixPrec, .arity=1, .binding=5, .bytes={aAmp, 0, 0, 0 } };
    p[ 6] = (OpDef){ .name=allocLit(a, "'"), .precedence=prefixPrec, .arity=1, .binding=6, .bytes={aApostrophe, 0, 0, 0 } };
    p[ 7] = (OpDef){ .name=allocLit(a, "*"), .precedence=20, .arity=2, .extensible=true, .binding=7, .bytes={aTimes, 0, 0, 0 } };
    p[ 8] = (OpDef){ .name=allocLit(a, "++"), .precedence=functionPrec, .arity=1, .binding=8, .bytes={aPlus, aPlus, 0, 0 }, .overloadable=true };
    p[ 9] = (OpDef){ .name=allocLit(a, "+"), .precedence=17, .arity=2, .extensible=true, .binding=9, .bytes={aPlus, 0, 0, 0 } };
    p[10] = (OpDef){ .name=allocLit(a, "--"), .precedence=functionPrec, .arity=1, .binding=10, .bytes={aMinus, aMinus, 0, 0 }, .overloadable=true };
    p[11] = (OpDef){ .name=allocLit(a, "-"), .precedence=17, .arity=2, .extensible=true, .binding=11, .bytes={aMinus, 0, 0, 0 } };
    p[12] = (OpDef){ .name=allocLit(a, "/"), .precedence=20, .arity=2, .extensible=true, .binding=12, .bytes={aDivBy, 0, 0, 0 } };
    p[13] = (OpDef){ .name=allocLit(a, ":="), .precedence=0, .arity=0, .binding=-1, .bytes={aColon, aEqual, 0, 0 }};
    p[14] = (OpDef){ .name=allocLit(a, ";<"), .precedence=1, .arity=2, .binding=13, .bytes={aSemicolon, aLT, 0, 0 }, .overloadable=true };
    p[15] = (OpDef){ .name=allocLit(a, ";"), .precedence=1, .arity=2, .binding=14, .bytes={aSemicolon, 0, 0, 0 }, .overloadable=true };
    p[16] = (OpDef){ .name=allocLit(a, "<-"), .precedence=0, .arity=0, .binding=-1, .bytes={aLT, aMinus, 0, 0 } };
    p[17] = (OpDef){ .name=allocLit(a, "<="), .precedence=12, .arity=2, .binding=15, .bytes={aLT, aEqual, 0, 0 } };    
    p[18] = (OpDef){ .name=allocLit(a, "<<"), .precedence=14, .arity=2, .binding=16, .extensible=true, .bytes={aTimes, 0, 0, 0 } };
    p[19] = (OpDef){ .name=allocLit(a, "<"), .precedence=12, .arity=2, .binding=17, .bytes={aLT, 0, 0, 0 } };
    p[20] = (OpDef){ .name=allocLit(a, "=="), .precedence=11, .arity=2, .binding=18, .bytes={aEqual, aEqual, 0, 0 } };
    p[21] = (OpDef){ .name=allocLit(a, "="), .precedence=0, .arity=0, .binding=-1, .bytes={aEqual, 0, 0, 0 } };
    p[22] = (OpDef){ .name=allocLit(a, ">=<="), .precedence=12, .arity=3, .binding=19, .bytes={aGT, aEqual, aLT, aEqual } };
    p[23] = (OpDef){ .name=allocLit(a, ">=<"), .precedence=12, .arity=3, .binding=20, .bytes={aGT, aEqual, aLT, 0 } };
    p[24] = (OpDef){ .name=allocLit(a, "><="), .precedence=12, .arity=3, .binding=21, .bytes={aGT, aLT, aEqual, 0 } };
    p[25] = (OpDef){ .name=allocLit(a, "><"), .precedence=12, .arity=3, .binding=22, .bytes={aGT, aLT, 0, 0 } };
    p[26] = (OpDef){ .name=allocLit(a, ">="), .precedence=12, .arity=2, .binding=23, .bytes={aGT, aEqual, 0, 0 } };
    p[27] = (OpDef){ .name=allocLit(a, ">>"), .precedence=14, .arity=2, .binding=24, .extensible=true, .bytes={aGT, aGT, 0, 0 } };
    p[28] = (OpDef){ .name=allocLit(a, ">"), .precedence=12, .arity=2, .binding=25, .bytes={aGT, 0, 0, 0 } };
    p[29] = (OpDef){ .name=allocLit(a, "?:"), .precedence=1, .arity=2, .binding=26, .bytes={aQuestion, aColon, 0, 0 } };
    p[30] = (OpDef){ .name=allocLit(a, "?"), .precedence=prefixPrec, .arity=1, .binding=27, .bytes={aQuestion, 0, 0, 0 } };
    p[31] = (OpDef){ .name=allocLit(a, "\\"), .precedence=prefixPrec, .arity=1, .binding=28, .bytes={aBackslash, 0, 0, 0 } };    
    p[32] = (OpDef){ .name=allocLit(a, "^"), .precedence=21, .arity=2, .binding=29, .bytes={aCaret, 0, 0, 0 } };
    p[33] = (OpDef){ .name=allocLit(a, "||"), .precedence=3, .arity=2, .binding=30, .bytes={aPipe, aPipe, 0, 0 }, .assignable=true };
    p[34] = (OpDef){ .name=allocLit(a, "|"), .precedence=9, .arity=2, .binding=31, .bytes={aPipe, 0, 0, 0 } };
    p[35] = (OpDef){ .name=allocLit(a, "~"), .precedence=[prefixPrec], .arity=1, .binding=32, .bytes={aTilde, 0, 0, 0 } };
    return result;
}

/*
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);

    result->possiblyReservedDispatch = buildReserved(a);
    result->dispatchTable = buildDispatch(a);
    result->operators = buildOperators(a);

    return result;
}


Lexer* createLexer(String* inp, Arena* a) {
    Lexer* result = allocateOnArena(sizeof(Lexer), a);

    result->inp = inp;
    result->inpLength = inp->length;
    result->tokens = allocateOnArena(LEXER_INIT_SIZE*sizeof(Token), a);
    result->capacity = LEXER_INIT_SIZE;
    
    result->newlines = allocateOnArena(a, 1000*sizeof(int));
    result->newlinesCapacity = 1000;
    
    result->backtrack = createStackRememberedToken(16, a);

    result->errMsg = &empty;

    result->arena = a;

    return result;
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunk(Lexer* lexer) {
    Token* newStorage = allocateOnArena(lexer->capacity*2*sizeof(Token), lexer->arena);
    memcpy(newStorage, lexer->tokens, (lexer->capacity)*(sizeof(Token)));
    lexer->tokens = newStorage;

    lexer->capacity *= 2;
}


void add(Token t, Lexer* lexer) {
    lexer->tokens[lexer->nextInd] = t;
    lexer->nextInd++;
    if (lexer->nextInd == lexer->capacity) {
        handleFullChunk(lexer);
    }
}


private Lexer* buildLexer(int totalTokens, String* inp, Arena *a, /* Tokens */ ...) {
    Lexer* result = createLexer(inp, a);
    if (result == NULL) return result;

    result->totalTokens = totalTokens;

    va_list tokens;
    va_start (tokens, a);

    for (int i = 0; i < totalTokens; i++) {
        add(va_arg(tokens, Token), result);
    }

    va_end(tokens);
    return result;
}


private void finalize(Lexer* this) {
    if (!this->wasError && !hasValuesRememberedToken(this->backtrack)) {
        exitWithError(errorPunctuationExtraOpening, this);
    }
}


Lexer* lexicallyAnalyze(String* input, LanguageDefinition* lang, Arena* ar) {
    Lexer* result = createLexer(input, ar);
    int inpLength = input->length;
    Arr(byte) inp = input->content;
    
    if (inpLength == 0) {
        exitWithError("Empty input", result);
    }

    // Check for UTF-8 BOM at start of file
    if (inpLength >= 3
        && (unsigned char)inp[0] == 0xEF
        && (unsigned char)inp[1] == 0xBB
        && (unsigned char)inp[2] == 0xBF) {
        result->i = 3;
    }
    LexerFunc (*dispatch)[256] = lang->dispatchTable;
    result->possiblyReservedDispatch = lang->possiblyReservedDispatch;

    // Main loop over the input
    while (result->i < inpLength) {
        ((*dispatch)[inp[result->i]])(result, inp);
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
