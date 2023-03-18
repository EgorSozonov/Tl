#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include "lexer.h"
#include "lexerConstants.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stack.h"
#include <setjmp.h>

#define CURR_BT inp[lr->i]
#define NEXT_BT inp[lr->i + 1]

DEFINE_STACK(RememberedToken)
DEFINE_STACK(int)

jmp_buf excBuf;

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
    return  tType == tokLexScope || tType == tokTypeDecl || tType == tokParens
            || tType == tokAssignment;
}

/** Predicate for an element of a scope, which can be any type of statement, or a split statement (such as an "if" or "match" form) */
bool isScopeElementVal(unsigned int tType) {
    return tType == tokLexScope
            || tType == tokTypeDecl
            || tType == tokParens
            || tType == tokAssignment;
}

/** Sets i to beyond input's length to communicate to callers that lexing is over */
private void throwExc(const char errMsg[], Lexer* lr) {   
    lr->wasError = true;
    printf("Error on i = %d\n", lr->i);
    lr->errMsg = allocLit(lr->arena, errMsg);
    longjmp(excBuf, 1);
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private void checkPrematureEnd(int requiredSymbols, Lexer* lr) {
    if (lr->i + requiredSymbols > lr->inpLength) {
        throwExc(errorPrematureEndOfInput, lr);
    }
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

/** For all the dollars at the top of the backtrack, turns them into parentheses, sets their lengths and closes them */
private void closeDollars(Lexer* lr) {
    while (hasValuesRememberedToken(lr->backtrack) && peekRememberedToken(lr->backtrack).wasOriginallyColon) {
        RememberedToken top = popRememberedToken(lr->backtrack);
        int j = top.numberOfToken;
        lr->tokens[j].tp = tokParens;
        lr->tokens[j].lenBytes = lr->i - lr->tokens[j].startByte;
        lr->tokens[j].payload2 = lr->nextInd - j - 1;
    }
}

/**
 * Finds the top-level punctuation opener by its index, and sets its lengths.
 * Called when the matching closer is lexed.
 */
private void setSpanLength(int tokenInd, Lexer* lr) {    
    lr->tokens[tokenInd].lenBytes = lr->i - lr->tokens[tokenInd].startByte;
    lr->tokens[tokenInd].payload2 = lr->nextInd - tokenInd - 1;
}

/**
 * Validation to catch unmatched closing punctuation
 */
private void validateClosingPunct(uint closingType, uint openType, Lexer* lr) {
    if (closingType == tokParens) {
        if (openType < firstCoreFormTokenType && openType != tokParens && openType != tokColon) {
            throwExc(errorPunctuationUnmatched, lr);
        }
    } else if (closingType == tokBrackets) {
        if (openType != tokBrackets && openType != tokAccessor) throwExc(errorPunctuationUnmatched, lr);
    } else if (closingType == tokStmt) {
        if (openType != tokStmt && openType != tokAssignment) throwExc(errorPunctuationUnmatched, lr);
    }
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a ] or a dot.
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 * TODO correctly check if it's a multi-line statement or not, and whether it should be ended
 */
private void closeRegularPunctuation(int closingType, Lexer* lr) {
    closeDollars(lr);
    if (!hasValuesRememberedToken(lr->backtrack)) {
        throwExc(errorPunctuationExtraClosing, lr);
    }

    RememberedToken top = popRememberedToken(lr->backtrack);
    if (closingType == tokParens && top.tp == tokStmt) {
        // since a closing parenthesis might be closing something with statements inside it, like a lex scope
        // or a core syntax form, we need to close the last statement before closing its parent
        setSpanLength(top.numberOfToken, lr);
        top = popRememberedToken(lr->backtrack);
    }

    validateClosingPunct(closingType, top.tp, lr);
    setSpanLength(top.numberOfToken, lr);

    lr->i++;
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


private int determineReservedC(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesCatch, tokStmtCatch)
    PROBERESERVED(reservedBytesContinue, tokContinue)
    return 0;
}


private int determineReservedE(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesEmbed, tokStmtEmbed)
    return 0;
}


private int determineReservedF(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesFalse, reservedFalse)
    return 0;
}


private int determineReservedI(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesIf, tokStmtIf)
    PROBERESERVED(reservedBytesIfEq, tokStmtIfEq)
    PROBERESERVED(reservedBytesIfPr, tokStmtIfPr)
    PROBERESERVED(reservedBytesImpl, tokStmtImpl)
    PROBERESERVED(reservedBytesInterface, tokStmtIface)
    return 0;
}


private int determineReservedL(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesLoop, tokLoop)
    return 0;
}


private int determineReservedM(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesMatch, tokStmtMatch)
    PROBERESERVED(reservedBytesMut, tokStmtMut)
    return 0;
}


private int determineReservedN(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesNodestruct, tokNodestruct)
    return 0;
}


private int determineReservedR(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesReturn, tokStmtReturn)
    return 0;
}


private int determineReservedS(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesStruct, tokStmtStruct)
    return 0;
}


private int determineReservedT(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesTest, tokStmtTest)
    PROBERESERVED(reservedBytesTrue, reservedTrue)
    PROBERESERVED(reservedBytesTry, tokStmtTry)
    PROBERESERVED(reservedBytesType, tokStmtType)
    return 0;
}

private int determineReservedY(int startByte, int lenBytes, Lexer* lr) {
    int lenReser;
    PROBERESERVED(reservedBytesYield, tokYield)
    return 0;
}

private int determineUnreserved(int startByte, int lenBytes, Lexer* lr) {
    return -1;
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 */
private void setSpanType(int tokenInd, unsigned int tType, Lexer* lr) {
    lr->tokens[tokenInd].tp = tType;
}

private void setSpanLambda(int tokenInd, unsigned int tType, Lexer* lr) {
    lr->tokens[tokenInd].tp = tType;
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
    if (hasValuesRememberedToken(lr->backtrack)) {
        uint topType = peekRememberedToken(lr->backtrack).tp;
        if (topType == tokLexScope || topType >= firstCoreFormTokenType) {
            pushRememberedToken((RememberedToken){ .tp = tokStmt, .numberOfToken = lr->nextInd, 
                                                                  .isMultiline=isMultiline }, lr->backtrack);
            add((Token){ .tp = tokStmt, .startByte = lr->i},  lr);
        }                
    } else {
        pushRememberedToken((RememberedToken){ .tp = tokStmt, .isMultiline=isMultiline, 
                                                              .numberOfToken = lr->nextInd}, lr->backtrack);
        add((Token){ .tp = tokStmt, .startByte = lr->i},  lr);
    }
}


private void convertGrandparentToScope(Lexer* lr) {
    int indPenultimate = lr->backtrack->length - 2;
    if (indPenultimate <= -1) return;
    
    uint gpType = (*lr->backtrack->content)[indPenultimate].tp;
    if (gpType == tokColon) {
        throwExc(errorPunctuationInsideColon, lr);
    } else if (gpType == tokParens) {
        (*lr->backtrack->content)[indPenultimate].tp = tokLexScope;
        setSpanType((*lr->backtrack->content)[indPenultimate].numberOfToken, tokLexScope, lr);
    }    
}

/** 
 *  TODO handle syntax like "(foo 5).field" and "foo[5].field"
 */
private void lexDot(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    convertGrandparentToScope(lr);
    closeRegularPunctuation(tokStmt, lr);
}


private void addNumeric(byte b, Lexer* lr) {
    if (lr->numericNextInd < lr->numericCapacity) {
        lr->numeric[lr->numericNextInd] = b;
    } else {
        Arr(byte) newNumeric = allocateOnArena(lr->numericCapacity*2, lr->arena);
        memcpy(newNumeric, lr->numeric, lr->numericCapacity);
        newNumeric[lr->numericCapacity] = b;
        
        lr->numeric = newNumeric;
        lr->numericCapacity *= 2;       
    }
    lr->numericNextInd++;
}


private int64_t calcIntegerWithinLimits(Lexer* lr) {
    int64_t powerOfTen = (int64_t)1;
    int64_t result = 0;
    int j = lr->numericNextInd - 1;

    int loopLimit = -1;
    while (j > loopLimit) {
        result += powerOfTen*lr->numeric[j];
        powerOfTen *= 10;
        j--;
    }
    return result;
}

/**
 * Checks if the current numeric <= b if they are regarded as arrays of decimal digits (0 to 9).
 */
private bool integerWithinDigits(const byte* b, int bLength, Lexer* lr){
    if (lr->numericNextInd != bLength) return (lr->numericNextInd < bLength);
    for (int j = 0; j < lr->numericNextInd; j++) {
        if (lr->numeric[j] < b[j]) return true;
        if (lr->numeric[j] > b[j]) return false;
    }
    return true;
}


private int calcInteger(int64_t* result, Lexer* lr) {
    if (lr->numericNextInd > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lr)) return -1;
    *result = calcIntegerWithinLimits(lr);
    return 0;
}


private int64_t calcBinNumber(Lexer* lr){
    int64_t result = 0;
    int64_t powerOfTwo = 1;
    int j = lr->numericNextInd - 1;

    // If the literal is full 64 bits long, then its upper bit is the sign bit, so we don't read it
    int loopLimit = (lr->numericNextInd == 64) ? 0 : -1;
    while (j > loopLimit) {
        if (lr->numeric[j] > 0) {
            result += powerOfTwo;
        }
        powerOfTwo = powerOfTwo << 1;
        j--;
    }
    if (lr->numericNextInd == 64 && lr->numeric[0] > 0) {
        result += (int64_t)(-9223372036854775807-1);
    }
    return result;
}

private int64_t calcHexNumber(Lexer* lr) {
    int64_t result = 0;
    int64_t powerOfSixteen = 1;
    int j = lr->numericNextInd - 1;

    // If the literal is full 16 bits long, then its upper sign contains the sign bit
    int loopLimit = -1; //if (byteBuf.ind == 16) { 0 } else { -1 }
    while (j > loopLimit) {
        result += powerOfSixteen*lr->numeric[j];
        powerOfSixteen = powerOfSixteen << 4;
        j--;
    }
    return result;
}

/**
 * Lexes a hexadecimal numeric literal (integer or floating-point).
 * Examples of accepted expressions: 0xCAFE_BABE, 0xdeadbeef, 0x123_45A
 * Examples of NOT accepted expressions: 0xCAFE_babe, 0x_deadbeef, 0x123_
 * Checks that the input fits into a signed 64-bit fixnum.
 * TODO add floating-point literals like 0x12FA.
 */
private void hexNumber(Lexer* lr, Arr(byte) inp) {
    checkPrematureEnd(2, lr);
    lr->numericNextInd = 0;
    int j = lr->i + 2;
    while (j < lr->inpLength) {
        byte cByte = inp[j];
        if (isDigit(cByte)) {
            addNumeric(cByte - aDigit0, lr);
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            addNumeric(cByte - aALower + 10, lr);
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            addNumeric(cByte - aAUpper + 10, lr);
        } else if (cByte == aUnderscore && (j == lr->inpLength - 1 || isHexDigit(inp[j + 1]))) {            
            throwExc(errorNumericEndUnderscore, lr);            
        } else {
            break;
        }
        if (lr->numericNextInd > 16) {
            throwExc(errorNumericBinWidthExceeded, lr);
        }
        j++;
    }
    int64_t resultValue = calcHexNumber(lr);
    add((Token){ .tp = tokInt, .payload1 = resultValue >> 32, .payload2 = resultValue & LOWER32BITS, 
                .startByte = lr->i, .lenBytes = j - lr->i }, lr);
    lr->numericNextInd = 0;
    lr->i = j;
}

private void binNumber(Lexer* lr, Arr(byte) inp) {
    lr->numericNextInd = 0;

    int j = lr->i + 2;
    while (j < lr->inpLength) {
        byte cByte = inp[j];
        if (cByte == aDigit0) {
            addNumeric(0, lr);
        } else if (cByte == aDigit1) {
            addNumeric(1, lr);
        } else if (cByte == aUnderscore) {
            if ((j == lr->inpLength - 1 || (inp[j + 1] != aDigit0 && inp[j + 1] != aDigit1))) {
                throwExc(errorNumericEndUnderscore, lr);
            }
        } else {
            break;
        }
        if (lr->numericNextInd > 64) {
            throwExc(errorNumericBinWidthExceeded, lr);
        }
        j++;
    }
    if (j == lr->i + 2) {
        throwExc(errorNumericEmpty, lr);
    }

    int64_t resultValue = calcBinNumber(lr);
    add((Token){ .tp = tokInt, .payload1 = resultValue >> 32, .payload2 = resultValue & LOWER32BITS, 
                .startByte = lr->i, .lenBytes = j - lr->i }, lr);
    lr->i = j;
}

/**
 * Parses the floating-point numbers using just the "fast path" of David Gay's "strtod" function,
 * extended to 16 digits.
 * I.e. it handles only numbers with 15 digits or 16 digits with the first digit not 9,
 * and decimal powers within [-22; 22].
 * Parsing the rest of numbers exactly is a huge and pretty useless effort. Nobody needs these
 * floating literals in text form: they are better input in binary, or at least text-hex or text-binary.
 * Input: array of bytes that are digits (without leading zeroes), and the negative power of ten.
 * So for '0.5' the input would be (5 -1), and for '1.23000' (123000 -5).
 * Example, for input text '1.23' this function would get the args: ([1 2 3] 1)
 * Output: a 64-bit floating-point number, encoded as a long (same bits)
 */
private int calcFloating(double* result, int powerOfTen, Lexer* lr, Arr(byte) inp){
    int indTrailingZeroes = lr->numericNextInd - 1;
    int ind = lr->numericNextInd;
    while (indTrailingZeroes > -1 && lr->numeric[indTrailingZeroes] == 0) { 
        indTrailingZeroes--;
    }

    // how many powers of 10 need to be knocked off the significand to make it fit
    int significandNeeds = ind - 16 >= 0 ? ind - 16 : 0;
    // how many power of 10 significand can knock off (these are just trailing zeroes)
    int significantCan = ind - indTrailingZeroes - 1;
    // how many powers of 10 need to be added to the exponent to make it fit
    int exponentNeeds = -22 - powerOfTen;
    // how many power of 10 at maximum can be added to the exponent
    int exponentCanAccept = 22 - powerOfTen;

    if (significantCan < significandNeeds || significantCan < exponentNeeds || significandNeeds > exponentCanAccept) {
        return -1;
    }

    // Transfer of decimal powers from significand to exponent to make them both fit within their respective limits
    // (10000 -6) -> (1 -2); (10000 -3) -> (10 0)
    int transfer = (significandNeeds >= exponentNeeds) ? (
             (ind - significandNeeds == 16 && significandNeeds < significantCan && significandNeeds + 1 <= exponentCanAccept) ? 
                (significandNeeds + 1) : significandNeeds
        ) : exponentNeeds;
    lr->numericNextInd -= transfer;
    int finalPowerTen = powerOfTen + transfer;

    if (!integerWithinDigits(maximumPreciselyRepresentedFloatingInt, sizeof(maximumPreciselyRepresentedFloatingInt), lr)) {
        return -1;
    }

    int64_t significandInt = calcIntegerWithinLimits(lr);
    double significand = (double)significandInt; // precise
    double exponent = pow(10.0, (double)(abs(finalPowerTen)));

    *result = (finalPowerTen > 0) ? (significand*exponent) : (significand/exponent);
    return 0;
}

int64_t longOfDoubleBits(double d) {
    FloatingBits un = {.d = d};
    return un.i;
}

/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO: add support for the '1.23E4' format
 */
private void decNumber(bool isNegative, Lexer* lr, Arr(byte) inp) {
    int j = (isNegative) ? (lr->i + 1) : lr->i;
    int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
    bool metDot = false;
    bool metNonzero = false;
    int maximumInd = (lr->i + 40 > lr->inpLength) ? (lr->i + 40) : lr->inpLength;
    while (j < maximumInd) {
        byte cByte = inp[j];

        if (isDigit(cByte)) {
            if (metNonzero) {
                addNumeric(cByte - aDigit0, lr);
            } else if (cByte != aDigit0) {
                metNonzero = true;
                addNumeric(cByte - aDigit0, lr);
            }
            if (metDot) digitsAfterDot++;
        } else if (cByte == aUnderscore) {
            if (j == (lr->inpLength - 1) || !isDigit(inp[j + 1])) {
                throwExc(errorNumericEndUnderscore, lr);
            }
        } else if (cByte == aDot) {
            if (metDot) {
                throwExc(errorNumericMultipleDots, lr);
            }
            metDot = true;
        } else {
            break;
        }
        j++;
    }

    if (j < lr->inpLength && isDigit(inp[j])) {
        throwExc(errorNumericWidthExceeded, lr);
    }

    if (metDot) {
        double resultValue = 0;
        int errorCode = calcFloating(&resultValue, -digitsAfterDot, lr, inp);
        if (errorCode != 0) {
            throwExc(errorNumericFloatWidthExceeded, lr);
        }

        int64_t bitsOfFloat = longOfDoubleBits((isNegative) ? (-resultValue) : resultValue);
        add((Token){ .tp = tokFloat, .payload1 = (bitsOfFloat >> 32), .payload2 = (bitsOfFloat & LOWER32BITS),
                    .startByte = lr->i, .lenBytes = j - lr->i}, lr);
    } else {
        int64_t resultValue = 0;
        int errorCode = calcInteger(&resultValue, lr);
        if (errorCode != 0) {
            throwExc(errorNumericIntWidthExceeded, lr);
        }
        if (isNegative) resultValue = -resultValue;
        add((Token){ .tp = tokInt, .payload1 = resultValue >> 32, .payload2 = resultValue & LOWER32BITS, 
                .startByte = lr->i, .lenBytes = j - lr->i }, lr);
    }
    lr->i = j;
}

private void lexNumber(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    byte cByte = CURR_BT;
    if (lr->i == lr->inpLength - 1 && isDigit(cByte)) {
        add((Token){ .tp = tokInt, .payload2 = cByte - aDigit0, .startByte = lr->i, .lenBytes = 1 }, lr);
        lr->i++;
        return;
    }
    
    byte nByte = NEXT_BT;
    if (nByte == aXLower) {
        hexNumber(lr, inp);
    } else if (nByte == aBLower) {
        binNumber(lr, inp);
    } else {
        decNumber(false, lr, inp);
    }
    lr->numericNextInd = 0;
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
        (RememberedToken){ .tp = tType, .numberOfToken = lr->nextInd, .wasOriginallyColon = tType == tokColon},
        lr->backtrack
    );
    add((Token) {.tp = tType, .startByte = (lr->i + 1) }, lr);
    lr->i++;
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
    if (!hasValuesRememberedToken(lr->backtrack) || popRememberedToken(lr->backtrack).numberOfToken < (lr->nextInd - 1)) {
        add((Token){.tp=tokReserved, .payload2=reservedWordType, .startByte=startInd, .lenBytes=(lr->i - startInd)}, lr);
        return;
    }

    RememberedToken currSpan = peekRememberedToken(lr->backtrack);
    if (currSpan.numberOfToken < (lr->nextInd - 1) || currSpan.tp >= firstCoreFormTokenType) {
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

    if (isCapitalLetter(CURR_BT)) {
        result = true;
    } else if (!isLowercaseLetter(CURR_BT)) {
        throwExc(errorWordChunkStart, lr);
    }
    lr->i++;

    while (lr->i < lr->inpLength && isAlphanumeric(CURR_BT)) {
        lr->i++;
    }
    if (lr->i < lr->inpLength && CURR_BT == aUnderscore) {
        throwExc(errorWordUnderscoresOnlyAtStart, lr);
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
                    throwExc(errorWordCapitalizationOrder, lr);
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
    if (wordType != tokWord && isAlsoAccessor) {
        throwExc(errorWordWrongAccessor, lr);
    }
        
    if (wordType == tokAtWord || firstByte < aALower || firstByte > aYLower) {
        add((Token){.tp=wordType, .payload2=paylCapitalized, .startByte=realStartInd, .lenBytes=lenBytes}, lr);
        if (isAlsoAccessor) {
            openPunctuation(tokAccessor, lr);
        }
    } else {
        int mbReservedWord = (*lr->possiblyReservedDispatch)[firstByte - aALower](startInd, lr->i - startInd, lr);
        if (mbReservedWord > 0) {
            if (wordType == tokDotWord) {
                throwExc(errorWordReservedWithDot, lr);
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


private void lexAtWord(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    checkPrematureEnd(2, lr);
    lr->i++;
    wordInternal(tokAtWord, lr, inp);
}


private void lexColon(Lexer* lr, Arr(byte) inp) {
    // TODO
    wrapInAStatement(lr, inp);
    checkPrematureEnd(2, lr);
    lr->i++;
    wordInternal(tokAtWord, lr, inp);
}


private void addOperator(int opType, int isExtensionAssignment, int startByte, int lenBytes, Lexer* lr) {
    add((Token){ .tp = tokOperator, .payload1 = (isExtensionAssignment + (opType << 2)), 
                .startByte = startByte, .lenBytes = lenBytes }, lr);
}


private void setSpanAssignment(int tokenInd, int isExtensionAssignment, uint opType, Lexer* lr) {
    int j = tokenInd;
    lr->tokens[j].tp = tokAssignment;
    lr->tokens[j].payload1 = isExtensionAssignment + (opType << 2);
}


private void processAssignmentOperator(uint opType, int isExtensionAssignment, Lexer* lr) {
    RememberedToken currSpan = peekRememberedToken(lr->backtrack);

    if (currSpan.tp == tokAssignment) {
        throwExc(errorOperatorMultipleAssignment, lr);
    } else if (currSpan.tp != tokStmt) {
        throwExc(errorOperatorAssignmentPunct, lr);
    }
    convertGrandparentToScope(lr);

    setSpanAssignment(currSpan.numberOfToken, isExtensionAssignment, opType, lr);
    (*lr->backtrack->content)[lr->backtrack->length - 1] = (RememberedToken){.tp = tokAssignment, .numberOfToken = currSpan.numberOfToken};
}


private void lexOperator(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    
    byte firstSymbol = CURR_BT;
    byte secondSymbol = (lr->inpLength > lr->i + 1) ? inp[lr->i + 1] : 0;
    byte thirdSymbol = (lr->inpLength > lr->i + 2) ? inp[lr->i + 2] : 0;
    byte fourthSymbol = (lr->inpLength > lr->i + 3) ? inp[lr->i + 3] : 0;
    int k = 0;
    int opType = -1; // corresponds to the opT... operator types
    OpDef (*operators)[countOperators] = lr->langDef->operators;
    while (k < countOperators && (*operators)[k].bytes[0] != firstSymbol) {
        k++;
    }
    while (k < countOperators && (*operators)[k].bytes[0] == firstSymbol) {
        OpDef opDef = (*operators)[k];
        byte secondTentative = opDef.bytes[1];
        if (secondTentative != 0 && secondTentative != secondSymbol) {
            k++;
            continue;
        }
        byte thirdTentative = opDef.bytes[2];
        if (thirdTentative != 0 && thirdTentative != thirdSymbol) {
            k++;
            continue;
        }
        byte fourthTentative = opDef.bytes[3];
        if (fourthTentative != 0 && fourthTentative != fourthSymbol) {
            k++;
            continue;
        }
        opType = k;
        break;
    }
    if (opType < 0) {
        throwExc(errorOperatorUnknown, lr);
    }
    
    OpDef opDef = (*operators)[opType];
    bool isExtensible = opDef.extensible;
    int isExtensionAssignment = 0;
    
    int lengthOfBaseOper = (opDef.bytes[1] == 0) ? 1 : (opDef.bytes[2] == 0 ? 2 : (opDef.bytes[3] == 0 ? 3 : 4));
    int j = lr->i + lengthOfBaseOper;
    if (isExtensible && j < lr->inpLength && inp[j] == aDot) {
        isExtensionAssignment += 2;
        j++;        
    }
    if ((isExtensible || opDef.assignable) && j < lr->inpLength && inp[j] == aEqual) {
        isExtensionAssignment++;
        j++;
    }
    if (opType == opTDefinition || opType == opTMutation || (isExtensionAssignment & 1 > 0)) {
        processAssignmentOperator(opType, isExtensionAssignment, lr);
    } else {
        addOperator(opType, isExtensionAssignment, lr->i, j - lr->i, lr);
    }
    lr->i = j;
}


void lexMinus(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    int j = lr->i + 1;
    if (j < lr->inpLength && isDigit(inp[j])) {
        decNumber(true, lr, inp);
        lr->numericNextInd = 0;
    } else {
        lexOperator(lr, inp);
    }
}

void lexDollar(Lexer* lr, Arr(byte) inp) {
    throwExc(errorUnrecognizedByte, lr);
}


private void lambda(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);

    int j = lr->i + 1;
    
    lr->i++;
    RememberedToken top = peekRememberedToken(lr->backtrack);
    setSpanLambda(top.numberOfToken, tokLambda, lr);
    top.tp = tokLambda;

    pushRememberedToken((RememberedToken){.tp=tokStmt, .numberOfToken=lr->nextInd}, lr->backtrack);    
    add((Token){ .tp = tokStmt, .startByte = lr->i }, lr);
}


void lexParenLeft(Lexer* lr, Arr(byte) inp) {
    if (lr->i < lr->inpLength - 1) {
        byte nextBt = NEXT_BT;
        if (nextBt == aDot) {
            lambda(lr, inp);
            return;
        } // TODO generator, (=) lambda
    }
    wrapInAStatement(lr, inp);
    openPunctuation(tokParens, lr);
}


void lexParenRight(Lexer* lr, Arr(byte) inp) {
    closeRegularPunctuation(tokParens, lr);
}


void lexBracketLeft(Lexer* lr, Arr(byte) inp) {
    wrapInAStatement(lr, inp);
    openPunctuation(tokBrackets, lr);
}

void lexBracketRight(Lexer* lr, Arr(byte) inp) {
    closeRegularPunctuation(tokBrackets, lr);
}


void lexSpace(Lexer* lr, Arr(byte) inp) {
    lr->i++;
    while (lr->i < lr->inpLength && CURR_BT == aSpace) {
        if (CURR_BT != aSpace) return;
        lr->i++;
    }
}

/** A newline in a Stmt ends it. A newline in a StmtMulti, or in something nested within it, has no effect.  */
private void lexNewline(Lexer* lr, Arr(byte) inp) {
    addNewLine(lr->i, lr);
    lr->i++;

    RememberedToken top = peekRememberedToken(lr->backtrack);
    if (top.tp == tokStmt && !top.isMultiline) {
        closeRegularPunctuation(tokStmt, lr);
    }
    while (lr->i < lr->inpLength && CURR_BT == aSpace) {
        lr->i++;
    }    
}

private void lexStringLiteral(Lexer* lr, Arr(byte) inp) {
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
    throwExc(errorPrematureEndOfInput, lr);
}

/**
 * Appends a doc comment token if it's the first one, or elongates the existing token if there was already
 * a doc comment on the previous line.
 * This logic handles multiple consecutive lines of doc comments.
 * NOTE: this is the only function in the whole lexer that needs the endByte instead of lenBytes!
 */
private void appendDocComment(int startByte, int endByte, Lexer* lr) {
    if (lr->nextInd == 0 || lr->tokens[lr->nextInd - 1].tp != tokDocComment) {
        add((Token){.tp = tokDocComment, .startByte = startByte, .lenBytes = endByte - startByte + 1}, lr);
    } else {        
        lr->tokens[lr->nextInd - 1].lenBytes = endByte - (lr->tokens[lr->nextInd - 1].startByte) + 1;
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
        docComment(lr, inp);
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
    throwExc(errorUnrecognizedByte, lr);
}

void lexNonAsciiError(Lexer* lr, Arr(byte) inp) {
    throwExc(errorNonAscii, lr);
}



private LexerFunc (*tabulateDispatch(Arena* a))[256] {
    LexerFunc (*result)[256] = allocateOnArena(256*sizeof(LexerFunc), a);
    LexerFunc* p = *result;
    for (int i = 0; i < 128; i++) {
        p[i] = &lexUnexpectedSymbol;
    }
    for (int i = 128; i < 256; i++) {
        p[i] = &lexNonAsciiError;
    }
    for (int i = aDigit0; i <= aDigit9; i++) {
        p[i] = &lexNumber;
    }

    for (int i = aALower; i <= aZLower; i++) {
        p[i] = &lexWord;
    }
    for (int i = aAUpper; i <= aZUpper; i++) {
        p[i] = &lexWord;
    }
    p[aUnderscore] = lexWord;
    p[aDot] = &lexDot;
    p[aAt] = &lexAtWord;
    p[aColon] = &lexColon;


    for (int i = 0; i < countOperatorStartSymbols; i++) {
        p[operatorStartSymbols[i]] = &lexOperator;
    }
    p[aMinus] = &lexMinus;
    p[aDollar] = &lexDollar;
    p[aParenLeft] = &lexParenLeft;
    p[aParenRight] = &lexParenRight;
    p[aBracketLeft] = &lexBracketLeft;
    p[aBracketRight] = &lexBracketRight;
    p[aSpace] = &lexSpace;
    p[aCarriageReturn] = &lexSpace;
    p[aNewline] = &lexNewline;
    p[aQuote] = &lexStringLiteral;
    p[aSharp] = &lexComment;
    return result;
}

/**
 * Table for dispatch on the first letter of a word in case it might be a reserved word.
 * It's indexed on the diff between first byte and the letter "a" (the earliest letter a reserved word may start with)
 */
private ReservedProbe (*tabulateReserved(Arena* a))[countReservedLetters] {
    ReservedProbe (*result)[countReservedLetters] = allocateOnArena(countReservedLetters*sizeof(ReservedProbe), a);
    ReservedProbe* p = *result;
    for (int i = 1; i < 25; i++) {
        p[i] = determineUnreserved;
    }
    p[0] = determineReservedA;
    p[2] = determineReservedC;
    p[4] = determineReservedE;
    p[5] = determineReservedF;
    p[7] = determineReservedI;
    p[11] = determineReservedL;
    p[12] = determineReservedM;
    p[13] = determineReservedN;
    p[17] = determineReservedR;
    p[18] = determineReservedS;
    p[19] = determineReservedT;
    p[24] = determineReservedY;
    return result;
}

/** The set of base operators in the language */
private OpDef (*tabulateOperators(Arena* a))[countOperators] {
    OpDef (*result)[countOperators] = allocateOnArena(countOperators*sizeof(OpDef), a);

    OpDef* p = *result;
    /**
    * This is an array of 4-byte arrays containing operator byte sequences.
    * Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
    * It's used to lex operator symbols using left-to-right search.
    */
    p[ 0] = (OpDef){ .name=allocLit(a, "!="), .precedence=11, .arity=2, .binding=0, .bytes={aExclamation, aEqual, 0, 0 } };
    p[ 1] = (OpDef){ .name=allocLit(a, "!"), .precedence=prefixPrec, .arity=1, .binding=1, .bytes={aExclamation, 0, 0, 0 } };
    p[ 2] = (OpDef){ .name=allocLit(a, "%"), .precedence=20, .arity=2, .extensible=true, .binding=3, .bytes={aPercent, 0, 0, 0 } };
    p[ 3] = (OpDef){ .name=allocLit(a, "&&"), .precedence=9, .arity=2, .binding=5, .bytes={aAmp, aAmp, 0, 0 }, .assignable=true };
    p[ 4] = (OpDef){ .name=allocLit(a, "&"), .precedence=prefixPrec, .arity=1, .binding=5, .bytes={aAmp, 0, 0, 0 } };
    p[ 5] = (OpDef){ .name=allocLit(a, "'"), .precedence=prefixPrec, .arity=1, .binding=6, .bytes={aApostrophe, 0, 0, 0 } };
    p[ 6] = (OpDef){ .name=allocLit(a, "*"), .precedence=20, .arity=2, .extensible=true, .binding=7, .bytes={aTimes, 0, 0, 0 } };
    p[ 7] = (OpDef){ .name=allocLit(a, "++"), .precedence=functionPrec, .arity=1, .binding=8, .bytes={aPlus, aPlus, 0, 0 }, .overloadable=true };
    p[ 8] = (OpDef){ .name=allocLit(a, "+"), .precedence=17, .arity=2, .extensible=true, .binding=9, .bytes={aPlus, 0, 0, 0 } };
    p[ 9] = (OpDef){ .name=allocLit(a, "--"), .precedence=functionPrec, .arity=1, .binding=10, .bytes={aMinus, aMinus, 0, 0 }, .overloadable=true };
    p[10] = (OpDef){ .name=allocLit(a, "-"), .precedence=17, .arity=2, .extensible=true, .binding=11, .bytes={aMinus, 0, 0, 0 } };

    p[11] = (OpDef){ .name=allocLit(a, "/"), .precedence=20, .arity=2, .extensible=true, .binding=12, .bytes={aDivBy, 0, 0, 0 } };
    p[12] = (OpDef){ .name=allocLit(a, ":="), .precedence=0, .arity=0, .binding=-1, .bytes={aColon, aEqual, 0, 0 }};
    p[13] = (OpDef){ .name=allocLit(a, ";<"), .precedence=1, .arity=2, .binding=13, .bytes={aSemicolon, aLT, 0, 0 }, .overloadable=true };
    p[14] = (OpDef){ .name=allocLit(a, ";"), .precedence=1, .arity=2, .binding=14, .bytes={aSemicolon, 0, 0, 0 }, .overloadable=true };
    p[15] = (OpDef){ .name=allocLit(a, "<-"), .precedence=0, .arity=0, .binding=-1, .bytes={aLT, aMinus, 0, 0 } };
    p[16] = (OpDef){ .name=allocLit(a, "<="), .precedence=12, .arity=2, .binding=15, .bytes={aLT, aEqual, 0, 0 } };    
    p[17] = (OpDef){ .name=allocLit(a, "<<"), .precedence=14, .arity=2, .binding=16, .extensible=true, .bytes={aTimes, 0, 0, 0 } };
    p[18] = (OpDef){ .name=allocLit(a, "<"), .precedence=12, .arity=2, .binding=17, .bytes={aLT, 0, 0, 0 } };
    p[19] = (OpDef){ .name=allocLit(a, "=="), .precedence=11, .arity=2, .binding=18, .bytes={aEqual, aEqual, 0, 0 } };
    p[20] = (OpDef){ .name=allocLit(a, "="), .precedence=0, .arity=0, .binding=-1, .bytes={aEqual, 0, 0, 0 } };
    p[21] = (OpDef){ .name=allocLit(a, ">=<="), .precedence=12, .arity=3, .binding=19, .bytes={aGT, aEqual, aLT, aEqual } };
    p[22] = (OpDef){ .name=allocLit(a, ">=<"), .precedence=12, .arity=3, .binding=20, .bytes={aGT, aEqual, aLT, 0 } };
    p[23] = (OpDef){ .name=allocLit(a, "><="), .precedence=12, .arity=3, .binding=21, .bytes={aGT, aLT, aEqual, 0 } };
    p[24] = (OpDef){ .name=allocLit(a, "><"), .precedence=12, .arity=3, .binding=22, .bytes={aGT, aLT, 0, 0 } };
    p[25] = (OpDef){ .name=allocLit(a, ">="), .precedence=12, .arity=2, .binding=23, .bytes={aGT, aEqual, 0, 0 } };
    p[26] = (OpDef){ .name=allocLit(a, ">>"), .precedence=14, .arity=2, .binding=24, .extensible=true, .bytes={aGT, aGT, 0, 0 } };
    p[27] = (OpDef){ .name=allocLit(a, ">"), .precedence=12, .arity=2, .binding=25, .bytes={aGT, 0, 0, 0 } };
    p[28] = (OpDef){ .name=allocLit(a, "?:"), .precedence=1, .arity=2, .binding=26, .bytes={aQuestion, aColon, 0, 0 } };
    p[29] = (OpDef){ .name=allocLit(a, "?"), .precedence=prefixPrec, .arity=1, .binding=27, .bytes={aQuestion, 0, 0, 0 } };
    p[30] = (OpDef){ .name=allocLit(a, "\\"), .precedence=prefixPrec, .arity=1, .binding=28, .bytes={aBackslash, 0, 0, 0 } };    
    p[31] = (OpDef){ .name=allocLit(a, "^"), .precedence=21, .arity=2, .binding=29, .bytes={aCaret, 0, 0, 0 } };
    p[32] = (OpDef){ .name=allocLit(a, "||"), .precedence=3, .arity=2, .binding=30, .bytes={aPipe, aPipe, 0, 0 }, .assignable=true };
    p[33] = (OpDef){ .name=allocLit(a, "|"), .precedence=9, .arity=2, .binding=31, .bytes={aPipe, 0, 0, 0 } };    
    p[34] = (OpDef){ .name=allocLit(a, "~"), .precedence=prefixPrec, .arity=1, .binding=32, .bytes={aTilde, 0, 0, 0 } };
    return result;
}

/*
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    result->possiblyReservedDispatch = tabulateReserved(a);
    result->dispatchTable = tabulateDispatch(a);
    result->operators = tabulateOperators(a);

    return result;
}


Lexer* createLexer(String* inp, Arena* a) {
    Lexer* result = allocateOnArena(sizeof(Lexer), a);    

    result->langDef = buildLanguageDefinitions(a);    
    result->arena = a;
    result->inp = inp;
    result->inpLength = inp->length;    
    
    result->tokens = allocateOnArena(LEXER_INIT_SIZE*sizeof(Token), a);
    result->capacity = LEXER_INIT_SIZE;
        
    result->newlines = allocateOnArena(1000*sizeof(int), a);
    result->newlinesCapacity = 1000;
    
    result->numeric = allocateOnArena(50*sizeof(int), a);
    result->numericCapacity = 50;
    
    result->backtrack = createStackRememberedToken(16, a);

    result->errMsg = &empty;

    return result;
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

/**
 * Finalizes the lexing of a single input: checks for unclosed scopes, and closes an open statement, if any.
 */
private void finalize(Lexer* lr) {
    if (!hasValuesRememberedToken(lr->backtrack)) return;
    closeDollars(lr);
    uint top = peekRememberedToken(lr->backtrack).tp;    
    if (lr->backtrack->length == 1 && (top == tokStmt || top == tokAssignment)) {
        closeRegularPunctuation(tokStmt, lr);
    } else {
        throwExc(errorPunctuationExtraOpening, lr);
    }    
}


Lexer* lexicallyAnalyze(String* input, LanguageDefinition* lang, Arena* ar) {
    Lexer* result = createLexer(input, ar);

    int inpLength = input->length;
    Arr(byte) inp = input->content;
    
    if (inpLength == 0) {
        throwExc("Empty input", result);
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
    if (setjmp(excBuf) == 0) {
        while (result->i < inpLength) {
            ((*dispatch)[inp[result->i]])(result, inp);
        }
        finalize(result);
    }
    result->totalTokens = result->nextInd;
    return result;
}
