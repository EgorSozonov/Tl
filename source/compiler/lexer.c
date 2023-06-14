#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "lexer.h"
#include "lexerConstants.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/structures/stack.h"

#include <setjmp.h>
#include "lexer.internal.h"


#define CURR_BT inp[lx->i]
#define NEXT_BT inp[lx->i + 1]
#define VALIDATE(cond, errMsg) if (!(cond)) { throwExc(errMsg, lx); }
    
 
jmp_buf excBuf;



/** Sets i to beyond input's length to communicate to callers that lexing is over */
_Noreturn private void throwExc(const char errMsg[], Lexer* lx) {   
    lx->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d: %s\n", lx->i, errMsg);
#endif    
    lx->errMsg = str(errMsg, lx->arena);
    longjmp(excBuf, 1);
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private void checkPrematureEnd(Int requiredSymbols, Lexer* lx) {
    VALIDATE (lx->i + requiredSymbols <= lx->inpLength, errorPrematureEndOfInput)
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
private void closeColons(Lexer* lx) {
    while (hasValues(lx->backtrack) && peek(lx->backtrack).wasOrigColon) {
        BtToken top = pop(lx->backtrack);
        Int j = top.tokenInd;        
        lx->tokens[j].lenBytes = lx->i - lx->tokens[j].startByte;
        lx->tokens[j].payload2 = lx->nextInd - j - 1;
    }
}

/**
 * Finds the top-level punctuation opener by its index, and sets its lengths.
 * Called when the matching closer is lexed.
 */
private void setSpanLength(Int tokenInd, Lexer* lx) {
    lx->tokens[tokenInd].lenBytes = lx->i - lx->tokens[tokenInd].startByte + 1;
    lx->tokens[tokenInd].payload2 = lx->nextInd - tokenInd - 1;
}

/**
 * Correctly calculates the lenBytes for a single-line, statement-type span.
 */
private void setStmtSpanLength(Int topTokenInd, Lexer* lx) {
    Token lastToken = lx->tokens[lx->nextInd - 1];
    Int byteAfterLastToken = lastToken.startByte + lastToken.lenBytes;

    // This is for correctly calculating lengths of statements when they are ended by parens or brackets
    Int byteAfterLastPunct = lx->lastClosingPunctInd + 1;
    Int lenBytes = (byteAfterLastPunct > byteAfterLastToken ? byteAfterLastPunct : byteAfterLastToken) 
                    - lx->tokens[topTokenInd].startByte;

    lx->tokens[topTokenInd].lenBytes = lenBytes;
    lx->tokens[topTokenInd].payload2 = lx->nextInd - topTokenInd - 1;
}


#define PROBERESERVED(reservedBytesName, returnVarName)    \
    lenReser = sizeof(reservedBytesName); \
    if (lenBytes == lenReser && testForWord(lx->inp, startByte, reservedBytesName, lenReser)) \
        return returnVarName;


private Int determineReservedA(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesAlias, tokAlias)
    PROBERESERVED(reservedBytesAnd, reservedAnd)
    PROBERESERVED(reservedBytesAwait, tokAwait)
    PROBERESERVED(reservedBytesAssert, tokAssert)
    PROBERESERVED(reservedBytesAssertDbg, tokAssertDbg)
    return 0;
}

private Int determineReservedB(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesBreak, tokBreak)
    return 0;
}

private Int determineReservedC(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesCatch, tokCatch)
    PROBERESERVED(reservedBytesContinue, tokContinue)
    return 0;
}


private Int determineReservedE(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesElse, tokElse)
    PROBERESERVED(reservedBytesEmbed, tokEmbed)
    return 0;
}


private Int determineReservedF(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesFalse, reservedFalse)
    PROBERESERVED(reservedBytesFn, tokFnDef)
    return 0;
}


private Int determineReservedI(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesIf, tokIf)
    PROBERESERVED(reservedBytesIfPr, tokIfPr)
    PROBERESERVED(reservedBytesImpl, tokImpl)
    PROBERESERVED(reservedBytesInterface, tokIface)
    return 0;
}


private Int determineReservedL(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesLambda, tokLambda)
    PROBERESERVED(reservedBytesLoop, tokLoop)
    return 0;
}


private Int determineReservedM(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesMatch, tokMatch)
    return 0;
}


private Int determineReservedO(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesOr, reservedOr)
    return 0;
}


private Int determineReservedR(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesReturn, tokReturn)
    return 0;
}


private Int determineReservedS(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesStruct, tokStruct)
    return 0;
}


private Int determineReservedT(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesTrue, reservedTrue)
    PROBERESERVED(reservedBytesTry, tokTry)
    return 0;
}


private Int determineReservedY(Int startByte, Int lenBytes, Lexer* lx) {
    Int lenReser;
    PROBERESERVED(reservedBytesYield, tokYield)
    return 0;
}

private Int determineUnreserved(Int startByte, Int lenBytes, Lexer* lx) {
    return -1;
}


private void addNewLine(Int j, Lexer* lx) {
    lx->newlines[lx->newlinesNextInd] = j;
    lx->newlinesNextInd++;
    if (lx->newlinesNextInd == lx->newlinesCapacity) {
        Arr(int) newNewlines = allocateOnArena(lx->newlinesCapacity*2*sizeof(int), lx->arena);
        memcpy(newNewlines, lx->newlines, lx->newlinesCapacity);
        lx->newlines = newNewlines;
        lx->newlinesCapacity *= 2;
    }
}

private void addStatement(untt stmtType, Int startByte, Lexer* lx) {
    push(((BtToken){ .tp = stmtType, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->backtrack);
    add((Token){ .tp = stmtType, .startByte = startByte}, lx);
}


private void wrapInAStatementStarting(Int startByte, Lexer* lx, Arr(byte) inp) {    
    if (hasValues(lx->backtrack)) {
        if (peek(lx->backtrack).spanLevel <= slParenMulti) {            
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->backtrack);
            add((Token){ .tp = tokStmt, .startByte = startByte},  lx);
        }
    } else {
        addStatement(tokStmt, startByte, lx);
    }
}


private void wrapInAStatement(Lexer* lx, Arr(byte) inp) {
    if (hasValues(lx->backtrack)) {
        if (peek(lx->backtrack).spanLevel <= slParenMulti) {
            push(((BtToken){ .tp = tokStmt, .tokenInd = lx->nextInd, .spanLevel = slStmt }), lx->backtrack);
            add((Token){ .tp = tokStmt, .startByte = lx->i},  lx);
        }
    } else {
        addStatement(tokStmt, lx->i, lx);
    }
}

private void maybeBreakStatement(Lexer* lx) {
    if (hasValues(lx->backtrack)) {
        BtToken top = peek(lx->backtrack);
        if(top.spanLevel == slStmt) {
            Int len = lx->backtrack->length;
            setStmtSpanLength(top.tokenInd, lx);
            pop(lx->backtrack);
        }
    }
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ) or a ] .
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private void closeRegularPunctuation(Int closingType, Lexer* lx) {
    StackBtToken* bt = lx->backtrack;
    closeColons(lx);
    VALIDATE(hasValues(bt), errorPunctuationExtraClosing)
    BtToken top = pop(bt);
    // since a closing bracket might be closing something with statements inside it, like a lex scope
    // or a core syntax form, we need to close the last statement before closing its parent
    if (bt->length > 0 && top.spanLevel != slScope 
          && ((*bt->content)[bt->length - 1].spanLevel <= slParenMulti)) {

        setStmtSpanLength(top.tokenInd, lx);
        top = pop(bt);
    }
    setSpanLength(top.tokenInd, lx);
    lx->i++; // CONSUME the closing ")" or "]"
}


private void addNumeric(byte b, Lexer* lx) {
    if (lx->numericNextInd < lx->numericCapacity) {
        lx->numeric[lx->numericNextInd] = b;
    } else {
        Arr(byte) newNumeric = allocateOnArena(lx->numericCapacity*2, lx->arena);
        memcpy(newNumeric, lx->numeric, lx->numericCapacity);
        newNumeric[lx->numericCapacity] = b;
        
        lx->numeric = newNumeric;
        lx->numericCapacity *= 2;       
    }
    lx->numericNextInd++;
}


private int64_t calcIntegerWithinLimits(Lexer* lx) {
    int64_t powerOfTen = (int64_t)1;
    int64_t result = 0;
    Int j = lx->numericNextInd - 1;

    Int loopLimit = -1;
    while (j > loopLimit) {
        result += powerOfTen*lx->numeric[j];
        powerOfTen *= 10;
        j--;
    }
    return result;
}

/**
 * Checks if the current numeric <= b if they are regarded as arrays of decimal digits (0 to 9).
 */
private bool integerWithinDigits(const byte* b, Int bLength, Lexer* lx){
    if (lx->numericNextInd != bLength) return (lx->numericNextInd < bLength);
    for (Int j = 0; j < lx->numericNextInd; j++) {
        if (lx->numeric[j] < b[j]) return true;
        if (lx->numeric[j] > b[j]) return false;
    }
    return true;
}


private Int calcInteger(int64_t* result, Lexer* lx) {
    if (lx->numericNextInd > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lx)) return -1;
    *result = calcIntegerWithinLimits(lx);
    return 0;
}


private int64_t calcHexNumber(Lexer* lx) {
    int64_t result = 0;
    int64_t powerOfSixteen = 1;
    Int j = lx->numericNextInd - 1;

    // If the literal is full 16 bits long, then its upper sign contains the sign bit
    Int loopLimit = -1; //if (byteBuf.ind == 16) { 0 } else { -1 }
    while (j > loopLimit) {
        result += powerOfSixteen*lx->numeric[j];
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
 * TODO add floating-pointt literals like 0x12FA.
 */
private void hexNumber(Lexer* lx, Arr(byte) inp) {
    checkPrematureEnd(2, lx);
    lx->numericNextInd = 0;
    Int j = lx->i + 2;
    while (j < lx->inpLength) {
        byte cByte = inp[j];
        if (isDigit(cByte)) {
            addNumeric(cByte - aDigit0, lx);
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            addNumeric(cByte - aALower + 10, lx);
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            addNumeric(cByte - aAUpper + 10, lx);
        } else if (cByte == aUnderscore && (j == lx->inpLength - 1 || isHexDigit(inp[j + 1]))) {            
            throwExc(errorNumericEndUnderscore, lx);            
        } else {
            break;
        }
        VALIDATE(lx->numericNextInd <= 16, errorNumericBinWidthExceeded)
        j++;
    }
    int64_t resultValue = calcHexNumber(lx);
    add((Token){ .tp = tokInt, .payload1 = resultValue >> 32, .payload2 = resultValue & LOWER32BITS, 
                .startByte = lx->i, .lenBytes = j - lx->i }, lx);
    lx->numericNextInd = 0;
    lx->i = j; // CONSUME the hex number
}

/**
 * Parses the floating-pointt numbers using just the "fast path" of David Gay's "strtod" function,
 * extended to 16 digits.
 * I.e. it handles only numbers with 15 digits or 16 digits with the first digit not 9,
 * and decimal powers within [-22; 22].
 * Parsing the rest of numbers exactly is a huge and pretty useless effort. Nobody needs these
 * floating literals in text form: they are better input in binary, or at least text-hex or text-binary.
 * Input: array of bytes that are digits (without leading zeroes), and the negative power of ten.
 * So for '0.5' the input would be (5 -1), and for '1.23000' (123000 -5).
 * Example, for input text '1.23' this function would get the args: ([1 2 3] 1)
 * Output: a 64-bit floating-pointt number, encoded as a long (same bits)
 */
private Int calcFloating(double* result, Int powerOfTen, Lexer* lx, Arr(byte) inp){
    Int indTrailingZeroes = lx->numericNextInd - 1;
    Int ind = lx->numericNextInd;
    while (indTrailingZeroes > -1 && lx->numeric[indTrailingZeroes] == 0) { 
        indTrailingZeroes--;
    }

    // how many powers of 10 need to be knocked off the significand to make it fit
    Int significandNeeds = ind - 16 >= 0 ? ind - 16 : 0;
    // how many power of 10 significand can knock off (these are just trailing zeroes)
    Int significantCan = ind - indTrailingZeroes - 1;
    // how many powers of 10 need to be added to the exponent to make it fit
    Int exponentNeeds = -22 - powerOfTen;
    // how many power of 10 at maximum can be added to the exponent
    Int exponentCanAccept = 22 - powerOfTen;

    if (significantCan < significandNeeds || significantCan < exponentNeeds || significandNeeds > exponentCanAccept) {
        return -1;
    }

    // Transfer of decimal powers from significand to exponent to make them both fit within their respective limits
    // (10000 -6) -> (1 -2); (10000 -3) -> (10 0)
    Int transfer = (significandNeeds >= exponentNeeds) ? (
             (ind - significandNeeds == 16 && significandNeeds < significantCan && significandNeeds + 1 <= exponentCanAccept) ? 
                (significandNeeds + 1) : significandNeeds
        ) : exponentNeeds;
    lx->numericNextInd -= transfer;
    Int finalPowerTen = powerOfTen + transfer;

    if (!integerWithinDigits(maximumPreciselyRepresentedFloatingInt, sizeof(maximumPreciselyRepresentedFloatingInt), lx)) {
        return -1;
    }

    int64_t significandInt = calcIntegerWithinLimits(lx);
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
private void decNumber(bool isNegative, Lexer* lx, Arr(byte) inp) {
    Int j = (isNegative) ? (lx->i + 1) : lx->i;
    Int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
    bool metDot = false;
    bool metNonzero = false;
    Int maximumInd = (lx->i + 40 > lx->inpLength) ? (lx->i + 40) : lx->inpLength;
    while (j < maximumInd) {
        byte cByte = inp[j];

        if (isDigit(cByte)) {
            if (metNonzero) {
                addNumeric(cByte - aDigit0, lx);
            } else if (cByte != aDigit0) {
                metNonzero = true;
                addNumeric(cByte - aDigit0, lx);
            }
            if (metDot) digitsAfterDot++;
        } else if (cByte == aUnderscore) {
            VALIDATE(j != (lx->inpLength - 1) && isDigit(inp[j + 1]), errorNumericEndUnderscore)
        } else if (cByte == aDot) {
            if (j + 1 < maximumInd && !isDigit(inp[j + 1])) { // this dot is not decimal - it's a statement closer
                break;
            }
            
            VALIDATE(!metDot, errorNumericMultipleDots)
            metDot = true;
        } else {
            break;
        }
        j++;
    }

    VALIDATE(j >= lx->inpLength || !isDigit(inp[j]), errorNumericWidthExceeded)

    if (metDot) {
        double resultValue = 0;
        Int errorCode = calcFloating(&resultValue, -digitsAfterDot, lx, inp);
        VALIDATE(errorCode == 0, errorNumericFloatWidthExceeded)

        int64_t bitsOfFloat = longOfDoubleBits((isNegative) ? (-resultValue) : resultValue);
        add((Token){ .tp = tokFloat, .payload1 = (bitsOfFloat >> 32), .payload2 = (bitsOfFloat & LOWER32BITS),
                    .startByte = lx->i, .lenBytes = j - lx->i}, lx);
    } else {
        int64_t resultValue = 0;
        Int errorCode = calcInteger(&resultValue, lx);
        VALIDATE(errorCode == 0, errorNumericIntWidthExceeded)

        if (isNegative) resultValue = -resultValue;
        add((Token){ .tp = tokInt, .payload1 = resultValue >> 32, .payload2 = resultValue & LOWER32BITS, 
                .startByte = lx->i, .lenBytes = j - lx->i }, lx);
    }
    lx->i = j; // CONSUME the decimal number
}

private void lexNumber(Lexer* lx, Arr(byte) inp) {
    wrapInAStatement(lx, inp);
    byte cByte = CURR_BT;
    if (lx->i == lx->inpLength - 1 && isDigit(cByte)) {
        add((Token){ .tp = tokInt, .payload2 = cByte - aDigit0, .startByte = lx->i, .lenBytes = 1 }, lx);
        lx->i++; // CONSUME the single-digit number
        return;
    }
    
    byte nByte = NEXT_BT;
    if (nByte == aXLower) {
        hexNumber(lx, inp);
    } else {
        decNumber(false, lx, inp);
    }
    lx->numericNextInd = 0;
}

/**
 * Adds a token which serves punctuation purposes, i.e. either a ( or  a [
 * These tokens are used to define the structure, that is, nesting within the AST.
 * Upon addition, they are saved to the backtracking stack to be updated with their length
 * once it is known.
 * Consumes no bytes.
 */
private void openPunctuation(untt tType, untt spanLevel, Int startByte, Lexer* lx) {
    push( ((BtToken){ .tp = tType, .tokenInd = lx->nextInd, .spanLevel = spanLevel}), lx->backtrack);
    add((Token) {.tp = tType, .payload1 = (tType < firstCoreFormTokenType) ? 0 : spanLevel, .startByte = startByte }, lx);
}

/**
 * Lexer action for a paren-type reserved word. It turns parentheses into an slParenMulti core form.
 * If necessary (parens inside statement) it also deletes last token and removes the top frame
 */
private void lexReservedWord(untt reservedWordType, Int startByte, Lexer* lx, Arr(byte) inp) {    
    StackBtToken* bt = lx->backtrack;
    
    Int expectations = (*lx->langDef->reservedParensOrNot)[reservedWordType - firstCoreFormTokenType];
    if (expectations == 0 || expectations == 2) { // the reserved words that live at the start of a statement
        VALIDATE(!hasValues(bt) || peek(bt).spanLevel == slScope, errorCoreNotInsideStmt)
        addStatement(reservedWordType, startByte, lx);
    } else if (expectations == 1) { // the "(core" case
        VALIDATE(hasValues(bt), errorCoreMissingParen)
        
        BtToken top = peek(bt);
        VALIDATE(top.tokenInd == lx->nextInd - 1, errorCoreNotAtSpanStart) // if this isn't the first token inside the parens
        
        if (bt->length > 1 && (*bt->content)[bt->length - 2].tp == tokStmt) {
            // Parens are wrapped in statements because we didn't know that the following token is a reserved word.
            // So when meeting a reserved word at start of parens, we need to delete the paren token & lex frame.
            // The new reserved token type will be written over the tokStmt that precedes the tokParen.
            pop(bt);
            lx->nextInd--;
            top = peek(bt);
        }
        
        // update the token type and the corresponding frame type
        lx->tokens[top.tokenInd].tp = reservedWordType;
        lx->tokens[top.tokenInd].payload1 = slParenMulti;
        (*bt->content)[bt->length - 1].tp = reservedWordType;
        (*bt->content)[bt->length - 1].spanLevel = top.tp == tokScope ? slScope : slParenMulti;
    }
}

/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was capitalized
 */
private bool wordChunk(Lexer* lx, Arr(byte) inp) {
    bool result = false;
    if (CURR_BT == aUnderscore) {
        checkPrematureEnd(2, lx);
        lx->i++; // CONSUME the underscore
    } else {
        checkPrematureEnd(1, lx);
    }

    if (isCapitalLetter(CURR_BT)) {
        result = true;
    } else VALIDATE(isLowercaseLetter(CURR_BT), errorWordChunkStart)
    lx->i++; // CONSUME the first letter of the word

    while (lx->i < lx->inpLength && isAlphanumeric(CURR_BT)) {
        lx->i++; // CONSUME alphanumeric characters
    }
    VALIDATE(lx->i >= lx->inpLength || CURR_BT != aUnderscore, errorWordUnderscoresOnlyAtStart)
    return result;
}

/** Closes the current statement. Consumes no tokens */
private void closeStatement(Lexer* lx) {
    BtToken top = peek(lx->backtrack);
    VALIDATE(top.spanLevel != slSubexpr, errorPunctuationOnlyInMultiline)
    if (top.spanLevel == slStmt) {
        setStmtSpanLength(top.tokenInd, lx);
        pop(lx->backtrack);
    }    
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Precondition: we are pointing at the first letter character of the word (i.e. past the possible "@" or ":")
 * Examples of acceptable expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of unacceptable expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private void wordInternal(untt wordType, Lexer* lx, Arr(byte) inp) {
    Int startByte = lx->i;

    bool wasCapitalized = wordChunk(lx, inp);

    while (lx->i < (lx->inpLength - 1)) {
        byte currBt = CURR_BT;
        if (currBt == aMinus) {
            byte nextBt = NEXT_BT;
            if (isLetter(nextBt) || nextBt == aUnderscore) {
                lx->i++; // CONSUME the letter or underscore
                bool isCurrCapitalized = wordChunk(lx, inp);
                VALIDATE(!wasCapitalized || !isCurrCapitalized, errorWordCapitalizationOrder)
                wasCapitalized = isCurrCapitalized;
            } else {
                break;
            }
        } else {
            break;
        }        
    }

    Int realStartByte = (wordType == tokWord) ? startByte : (startByte - 1); // accounting for the . or @ at the start
    bool paylCapitalized = wasCapitalized ? 1 : 0;

    byte firstByte = lx->inp->content[startByte];
    Int lenBytes = lx->i - realStartByte;
    Int lenString = lx->i - startByte;
        
    if (firstByte < aALower || firstByte > aYLower) {
        wrapInAStatementStarting(startByte, lx, inp);
        Int uniqueStringInd = addStringStore(inp, startByte, lenBytes, lx->stringTable, lx->stringStore);
        add((Token){ .tp=wordType, .payload1=paylCapitalized, .payload2 = uniqueStringInd,
                     .startByte=realStartByte, .lenBytes=lenBytes }, lx);
        return;
    }
    Int mbReservedWord = (*lx->possiblyReservedDispatch)[firstByte - aALower](startByte, lenString, lx);

    if (mbReservedWord <= 0) {
        wrapInAStatementStarting(startByte, lx, inp);
        Int uniqueStringInd = addStringStore(inp, startByte, lenString, lx->stringTable, lx->stringStore);
        add((Token){ .tp=wordType, .payload1=paylCapitalized, .payload2 = uniqueStringInd,
                     .startByte=realStartByte, .lenBytes=lenBytes }, lx);
        return;
    }

    VALIDATE(wordType != tokDotWord, errorWordReservedWithDot)

    if (mbReservedWord == tokElse){
        closeStatement(lx);
        add((Token){.tp = tokElse, .startByte = realStartByte, .lenBytes = 4}, lx);
    } else if (mbReservedWord < firstCoreFormTokenType) {
        if (mbReservedWord == reservedAnd) {
            wrapInAStatementStarting(startByte, lx, inp);
            add((Token){.tp=tokOperator, .payload1 = opTAnd, .startByte=realStartByte, .lenBytes=3}, lx);
        } else if (mbReservedWord == reservedOr) {
            wrapInAStatementStarting(startByte, lx, inp);
            add((Token){.tp=tokOperator, .payload1 = opTOr, .startByte=realStartByte, .lenBytes=2}, lx);
        } else if (mbReservedWord == reservedTrue) {
            wrapInAStatementStarting(startByte, lx, inp);
            add((Token){.tp=tokBool, .payload2=1, .startByte=realStartByte, .lenBytes=4}, lx);
        } else if (mbReservedWord == reservedFalse) {
            wrapInAStatementStarting(startByte, lx, inp);
            add((Token){.tp=tokBool, .payload2=0, .startByte=realStartByte, .lenBytes=5}, lx);
        } else if (mbReservedWord == tokDispose) {
            wrapInAStatementStarting(startByte, lx, inp);
            add((Token){.tp=tokDispose, .payload2=0, .startByte=realStartByte, .lenBytes=7}, lx);
        }
    } else {
        lexReservedWord(mbReservedWord, realStartByte, lx, inp);
    }
}


private void lexWord(Lexer* lx, Arr(byte) inp) {
    wordInternal(tokWord, lx, inp);
}

/** 
 * The dot is a statement separator
 */
private void lexDot(Lexer* lx, Arr(byte) inp) {
    if (lx->i < lx->inpLength - 1 && isLetter(NEXT_BT)) {        
        lx->i++; // CONSUME the dot
        wordInternal(tokDotWord, lx, inp);
        return;        
    }
    if (!hasValues(lx->backtrack) || peek(lx->backtrack).spanLevel == slScope) {
        // if we're at top level or directly inside a scope, do nothing since there're no stmts to close
    } else {
        closeStatement(lx);
        lx->i++;  // CONSUME the dot
    }
}

/**
 * Handles the "=", ":=" and "+=" tokens. 
 * Changes existing tokens and parent span to accout for the fact that we met an assignment operator 
 * mutType = 0 if it's immutable assignment, 1 if it's ":=", 2 if it's a regular operator mut "+="
 */
private void processAssignment(Int mutType, untt opType, Lexer* lx) {
    BtToken currSpan = peek(lx->backtrack);

    if (currSpan.tp == tokAssignment || currSpan.tp == tokReassign || currSpan.tp == tokMutation) {
        throwExc(errorOperatorMultipleAssignment, lx);
    } else if (currSpan.spanLevel != slStmt) {
        throwExc(errorOperatorAssignmentPunct, lx);
    } 
    Int tokenInd = currSpan.tokenInd;
    Token* tok = (lx->tokens + tokenInd);
    untt tp;
    
    if (mutType == 0) {
        tok->tp = tokAssignment;
    } else if (mutType == 1) {
        tok->tp = tokReassign;
    } else {
        tok->tp = tokMutation;
        tok->payload1 = opType;
    } 
    (*lx->backtrack->content)[lx->backtrack->length - 1] = (BtToken){
        .tp = tok->tp, .spanLevel = slStmt, .tokenInd = tokenInd }; 
}

/**
 * A single colon means "parentheses until the next closing paren or end of statement"
 */
private void lexColon(Lexer* lx, Arr(byte) inp) {           
    if (lx->i < lx->inpLength && NEXT_BT == aEqual) { // mutation assignment, :=
        lx->i += 2; // CONSUME the ":="
        processAssignment(1, 0, lx);
    } else {
        push(((BtToken){ .tp = tokParens, .tokenInd = lx->nextInd, .spanLevel = slSubexpr, .wasOrigColon = true}),
             lx->backtrack);
        add((Token) {.tp = tokParens, .startByte = lx->i }, lx);
        lx->i++; // CONSUME the ":"
    }    
}


private void lexOperator(Lexer* lx, Arr(byte) inp) {
    wrapInAStatement(lx, inp);    
    
    byte firstSymbol = CURR_BT;
    byte secondSymbol = (lx->inpLength > lx->i + 1) ? inp[lx->i + 1] : 0;
    byte thirdSymbol = (lx->inpLength > lx->i + 2) ? inp[lx->i + 2] : 0;
    byte fourthSymbol = (lx->inpLength > lx->i + 3) ? inp[lx->i + 3] : 0;
    Int k = 0;
    Int opType = -1; // corresponds to the opT... operator types
    OpDef (*operators)[countOperators] = lx->langDef->operators;
    while (k < countOperators && (*operators)[k].bytes[0] < firstSymbol) {
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
    VALIDATE (opType > -1, errorOperatorUnknown)
    
    OpDef opDef = (*operators)[opType];
    bool isAssignment = false;
    
    Int lengthOfBaseOper = (opDef.bytes[1] == 0) ? 1 : (opDef.bytes[2] == 0 ? 2 : (opDef.bytes[3] == 0 ? 3 : 4));
    Int j = lx->i + lengthOfBaseOper;
    if (opDef.assignable && j < lx->inpLength && inp[j] == aEqual) {
        isAssignment = true;
        j++;
    }
    if (isAssignment) { // mutation operators like "*=" or "*.="
        processAssignment(2, opType, lx);
    } else {
        add((Token){ .tp = tokOperator, .payload1 = opType, .startByte = lx->i, .lenBytes = j - lx->i}, lx);
    }
    lx->i = j; // CONSUME the operator
}


private void lexEqual(Lexer* lx, Arr(byte) inp) {
    checkPrematureEnd(2, lx);
    byte nextBt = NEXT_BT;
    if (nextBt == aEqual) {
        lexOperator(lx, inp); // ==        
    } else if (nextBt == aGT) { // => is a statement terminator inside if-like scopes        
        // arrows are only allowed inside "if"s and the like
        if (lx->backtrack->length < 2) {
            throwExc(errorCoreMisplacedArrow, lx);
        }
        BtToken grandparent = (*lx->backtrack->content)[lx->backtrack->length - 2];
        VALIDATE(grandparent.tp == tokIf, errorCoreMisplacedArrow)
        closeStatement(lx);
        add((Token){ .tp = tokArrow, .startByte = lx->i, .lenBytes = 2 }, lx);
        lx->i += 2;  // CONSUME the arrow "=>"
    } else {
        processAssignment(0, 0, lx);
        lx->i++; // CONSUME the =
    }
}

/**
 *  Doc comments, syntax is (* Doc comment ).
 *  Precondition: we are past the opening "(*"
 */
private void docComment(Lexer* lx, Arr(byte) inp) {
    Int startByte = lx->i;
    
    Int parenLevel = 1;
    Int j = lx->i;
    for (; j < lx->inpLength; j++) {
        byte cByte = inp[j];
        // Doc comments may contain arbitrary UTF-8 symbols, but we care only about newlines and parentheses
        if (cByte == aNewline) {
            addNewLine(j, lx);
        } else if (cByte == aParenLeft) {
            parenLevel++;
        } else if (cByte == aParenRight) {
            parenLevel--;
            if (parenLevel == 0) {
                j++; // CONSUME the right parenthesis
                break;
            }
        }
    }
    VALIDATE(parenLevel == 0, errorPunctuationExtraOpening)
    if (j > lx->i) {
        add((Token){.tp = tokDocComment, .startByte = lx->i - 2, .lenBytes = j - lx->i + 2}, lx);
    }
    lx->i = j; // CONSUME the doc comment
}

/** Handles the binary operator as well as the unary negation operator */
private void lexMinus(Lexer* lx, Arr(byte) inp) {
    if (lx->i == lx->inpLength - 1) {        
        lexOperator(lx, inp);
    } else {
        byte nextBt = NEXT_BT;
        if (isDigit(nextBt)) {
            wrapInAStatement(lx, inp);
            decNumber(true, lx, inp);
            lx->numericNextInd = 0;
        } else if (isLowercaseLetter(nextBt) || nextBt == aUnderscore) {
            add((Token){.tp = tokOperator, .payload1 = opTNegation, .startByte = lx->i, .lenBytes = 1}, lx);
            lx->i++; // CONSUME the minus symbol
        } else {
            lexOperator(lx, inp);
        }    
    }
}

/** An ordinary until-line-end comment. It doesn't get included in the AST, just discarded by the lexer.
 * Just like a newline, this needs to test if we're in a breakable statement because a comment goes until the line end.
 */
private void lexComment(Lexer* lx, Arr(byte) inp) {    
    if (lx->i >= lx->inpLength) return;
    
    maybeBreakStatement(lx);
        
    Int j = lx->i;
    while (j < lx->inpLength) {
        byte cByte = inp[j];
        if (cByte == aNewline) {
            lx->i = j + 1; // CONSUME the comment
            return;
        } else {
            j++;
        }
    }
    lx->i = j;  // CONSUME the comment
}

/** If we are inside a compound (=2) core form, we need to increment the clause count */ 
private void mbIncrementClauseCount(Lexer* lx) {
    if (hasValues(lx->backtrack)) {
        BtToken top = peek(lx->backtrack);
        if (top.tp >= firstCoreFormTokenType && (*lx->langDef->reservedParensOrNot)[top.tp - firstCoreFormTokenType] == 2) {
            (*lx->backtrack->content)[lx->backtrack->length - 1].countClauses++;
        }
    }
}

/** If we're inside a compound (=2) core form, we need to check if its clause count is saturated */ 
private void mbCloseCompoundCoreForm(Lexer* lx) {
    BtToken top = peek(lx->backtrack);
    if (top.tp >= firstCoreFormTokenType && (*lx->langDef->reservedParensOrNot)[top.tp - firstCoreFormTokenType] == 2) {
        if (top.countClauses == 2) {
            setSpanLength(top.tokenInd, lx);
            pop(lx->backtrack);
        }
    }
}

/** An opener for a scope or a scopeful core form. Precondition: we are past the "(-".
 * Consumes zero or 1 byte
 */
private void openScope(Lexer* lx, Arr(byte) inp) {
    Int startByte = lx->i;
    lx->i += 2; // CONSUME the "(."
    VALIDATE(!hasValues(lx->backtrack) || peek(lx->backtrack).spanLevel == slScope, errorPunctuationScope)
    VALIDATE(lx->i < lx->inpLength, errorPrematureEndOfInput)
    byte currBt = CURR_BT;
    if (currBt == aLLower && testForWord(lx->inp, lx->i, reservedBytesLoop, 4)) {
            openPunctuation(tokLoop, slScope, startByte, lx);
            lx->i += 4; // CONSUME the "loop"
            return; 
    } else if (lx->i < lx->inpLength - 2 && isSpace(inp[lx->i + 1])) {        
        if (currBt == aFLower) {
            openPunctuation(tokFnDef, slScope, startByte, lx);
            lx->i += 2; // CONSUME the "f "
            return;
        } else if (currBt == aILower) {
            openPunctuation(tokIf, slScope, startByte, lx);
            lx->i += 2; // CONSUME the "i "
            return;
        }  
    }        
    openPunctuation(tokScope, slScope, startByte, lx);    
}

/** Handles the "(*" case (doc-comment), the "(:" case (data initializer) as well as the common subexpression case */
private void lexParenLeft(Lexer* lx, Arr(byte) inp) {
    mbIncrementClauseCount(lx);
    Int j = lx->i + 1;
    VALIDATE(j < lx->inpLength, errorPunctuationUnmatched)
    if (inp[j] == aColon) { // "(:" starts a new data initializer        
        openPunctuation(tokData, slSubexpr, lx->i, lx);
        lx->i += 2; // CONSUME the "(:"
    } else if (inp[j] == aTimes) {
        lx->i += 2; // CONSUME the "(*"
        docComment(lx, inp);
    } else if (inp[j] == aDot) {
        openScope(lx, inp);
    } else {
        wrapInAStatement(lx, inp);
        openPunctuation(tokParens, slSubexpr, lx->i, lx);
        lx->i++; // CONSUME the left parenthesis
    }
}


private void lexParenRight(Lexer* lx, Arr(byte) inp) {
    // TODO handle syntax like "(foo 5).field" and "arr[5].field"
    Int startInd = lx->i;
    closeRegularPunctuation(tokParens, lx);
    
    if (!hasValues(lx->backtrack)) return;    
    mbCloseCompoundCoreForm(lx);
    
    lx->lastClosingPunctInd = startInd;    
}


void lexSpace(Lexer* lx, Arr(byte) inp) {
    lx->i++; // CONSUME the space
    while (lx->i < lx->inpLength && CURR_BT == aSpace) {
        lx->i++; // CONSUME a space
    }
}

/** 
 * Tl is not indentation-sensitive, but it is newline-sensitive. Thus, a newline charactor closes the current
 * statement unless it's inside an inline span (i.e. parens or accessor parens)
 */
private void lexNewline(Lexer* lx, Arr(byte) inp) {
    addNewLine(lx->i, lx);    
    maybeBreakStatement(lx);
    
    lx->i++;     // CONSUME the LF
    while (lx->i < lx->inpLength) {
        if (CURR_BT != aSpace && CURR_BT != aTab && CURR_BT != aCarrReturn) {
            break;
        }
        lx->i++; // CONSUME a space or tab
    }
}


private void lexStringLiteral(Lexer* lx, Arr(byte) inp) {
    wrapInAStatement(lx, inp);
    Int j = lx->i + 1;
    for (; j < lx->inpLength && inp[j] != aQuote; j++);
    VALIDATE(j != lx->inpLength, errorPrematureEndOfInput)
    add((Token){.tp=tokString, .startByte=(lx->i), .lenBytes=(j - lx->i + 1)}, lx);
    lx->i = j + 1; // CONSUME the string literal, including the closing quote character
}


void lexUnexpectedSymbol(Lexer* lx, Arr(byte) inp) {
    throwExc(errorUnrecognizedByte, lx);
}

void lexNonAsciiError(Lexer* lx, Arr(byte) inp) {
    throwExc(errorNonAscii, lx);
}

/** Must agree in order with token types in LexerConstants.h */
const char* tokNames[] = {
    "Int", "Float", "Bool", "String", "_", "DocComment", 
    "word", ".word", "operator", "dispose", 
    ":", "(.", "stmt", "(", "(:", "=", ":=", "mutation", "=>", "else",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue", 
    "defer", "each", "embed", "export", "exposePriv", "fn", "interface", 
    "lambda", "meta", "package", "return", "struct", "try", "yield",
    "if", "ifPr", "match", "impl", "loop"
};

void printLexer(Lexer* a) {
    if (a->wasError) {
        printf("Error: ");
        printString(a->errMsg);
    }
    for (int i = 0; i < a->totalTokens; i++) {
        Token tok = a->tokens[i];
        if (tok.payload1 != 0 || tok.payload2 != 0) {
            printf("%d: %s %d %d [%d; %d]\n", i, tokNames[tok.tp], tok.payload1, tok.payload2, tok.startByte, tok.lenBytes);
        } else {
            printf("%d: %s [%d; %d]\n", i, tokNames[tok.tp], tok.startByte, tok.lenBytes);
        }        
    }
}

/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
int equalityLexer(Lexer a, Lexer b) {
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.totalTokens < b.totalTokens ? a.totalTokens : b.totalTokens;
    int i = 0;
    for (; i < commonLength; i++) {
        Token tokA = a.tokens[i];
        Token tokB = b.tokens[i];
        if (tokA.tp != tokB.tp || tokA.lenBytes != tokB.lenBytes || tokA.startByte != tokB.startByte 
            || tokA.payload1 != tokB.payload1 || tokA.payload2 != tokB.payload2) {
            printf("\n\nUNEQUAL RESULTS\n", i);
            if (tokA.tp != tokB.tp) {
                printf("Diff in tp, %s but was expected %s\n", tokNames[tokA.tp], tokNames[tokB.tp]);
            }
            if (tokA.lenBytes != tokB.lenBytes) {
                printf("Diff in lenBytes, %d but was expected %d\n", tokA.lenBytes, tokB.lenBytes);
            }
            if (tokA.startByte != tokB.startByte) {
                printf("Diff in startByte, %d but was expected %d\n", tokA.startByte, tokB.startByte);
            }
            if (tokA.payload1 != tokB.payload1) {
                printf("Diff in payload1, %d but was expected %d\n", tokA.payload1, tokB.payload1);
            }
            if (tokA.payload2 != tokB.payload2) {
                printf("Diff in payload2, %d but was expected %d\n", tokA.payload2, tokB.payload2);
            }            
            return i;
        }
    }
    return (a.totalTokens == b.totalTokens) ? -2 : i;        
}


private LexerFunc (*tabulateDispatch(Arena* a))[256] {
    LexerFunc (*result)[256] = allocateOnArena(256*sizeof(LexerFunc), a);
    LexerFunc* p = *result;
    for (Int i = 0; i < 128; i++) {
        p[i] = &lexUnexpectedSymbol;
    }
    for (Int i = 128; i < 256; i++) {
        p[i] = &lexNonAsciiError;
    }
    for (Int i = aDigit0; i <= aDigit9; i++) {
        p[i] = &lexNumber;
    }

    for (Int i = aALower; i <= aZLower; i++) {
        p[i] = &lexWord;
    }
    for (Int i = aAUpper; i <= aZUpper; i++) {
        p[i] = &lexWord;
    }
    p[aUnderscore] = lexWord;
    p[aDot] = &lexDot;
    p[aColon] = &lexColon;
    p[aEqual] = &lexEqual;

    for (Int i = 0; i < countOperatorStartSymbols; i++) {
        p[operatorStartSymbols[i]] = &lexOperator;
    }
    p[aMinus] = &lexMinus;
    p[aParenLeft] = &lexParenLeft;
    p[aParenRight] = &lexParenRight;
    p[aSpace] = &lexSpace;
    p[aCarrReturn] = &lexSpace;
    p[aNewline] = &lexNewline;
    p[aQuote] = &lexStringLiteral;
    p[aSemicolon] = &lexComment;
    return result;
}

/**
 * Table for dispatch on the first letter of a word in case it might be a reserved word.
 * It's indexed on the diff between first byte and the letter "a" (the earliest letter a reserved word may start with)
 */
private ReservedProbe (*tabulateReservedBytes(Arena* a))[countReservedLetters] {
    ReservedProbe (*result)[countReservedLetters] = allocateOnArena(countReservedLetters*sizeof(ReservedProbe), a);
    ReservedProbe* p = *result;
    for (Int i = 1; i < 25; i++) {
        p[i] = determineUnreserved;
    }
    p[0] = determineReservedA;
    p[1] = determineReservedB;
    p[2] = determineReservedC;
    p[4] = determineReservedE;
    p[5] = determineReservedF;
    p[8] = determineReservedI;
    p[11] = determineReservedL;
    p[12] = determineReservedM;
    p[14] = determineReservedO;
    p[17] = determineReservedR;
    p[18] = determineReservedS;
    p[19] = determineReservedT;
    p[24] = determineReservedY;
    return result;
}

/**
 * Table for properties of core syntax forms, organized by reserved word.
 * It's indexed on the diff between token id and firstCoreFormTokenType.
 * It contains 1 for all parenthesized core forms and 2 for more complex
 * forms like "fn"
 */
private Int (*tabulateReserved(Arena* a))[countCoreForms] {
    Int (*result)[countCoreForms] = allocateOnArena(countCoreForms*sizeof(int), a);
    int* p = *result;        
    p[tokIf - firstCoreFormTokenType] = 1;
    p[tokFnDef - firstCoreFormTokenType] = 1;
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
    p[ 0] = (OpDef){ .name=s("!="),   .arity=2, .bytes={aExclamation, aEqual, 0, 0 } };
    p[ 1] = (OpDef){ .name=s("!"),    .arity=1, .bytes={aExclamation, 0, 0, 0 } };
    p[ 2] = (OpDef){ .name=s("#"),    .arity=1, .bytes={aSharp, 0, 0, 0 }, .overloadable=true, };    
    p[ 3] = (OpDef){ .name=s("$"),    .arity=1, .bytes={aDollar, 0, 0, 0 }, .overloadable=true,  };
    p[ 4] = (OpDef){ .name=s("%."),   .arity=2, .bytes={aPercent, aDot, 0, 0 } };
    p[ 5] = (OpDef){ .name=s("%"),    .arity=2, .bytes={aPercent, 0, 0, 0 } };
    p[ 6] = (OpDef){ .name=s("&&"),   .arity=2, .bytes={aAmp, aAmp, 0, 0 }, .assignable=true, };
    p[ 7] = (OpDef){ .name=s("&"),    .arity=1, .bytes={aAmp, 0, 0, 0 } };
    p[ 8] = (OpDef){ .name=s("'"),    .arity=1, .bytes={aApostrophe, 0, 0, 0 } };
    p[ 9] = (OpDef){ .name=s("*."),   .arity=2, .bytes={aTimes, aDot, 0, 0 }, .assignable = true, .overloadable = true };    
    p[10] = (OpDef){ .name=s("*"),    .arity=2, .bytes={aTimes, 0, 0, 0 }, .assignable = true, .overloadable = true };
    p[11] = (OpDef){ .name=s("++"),   .arity=1, .bytes={aPlus, aPlus, 0, 0 }, .overloadable=true };
    p[12] = (OpDef){ .name=s("+."),   .arity=2, .bytes={aPlus, aDot, 0, 0}, .assignable = true, .overloadable = true };
    p[13] = (OpDef){ .name=s("+"),    .arity=2, .bytes={aPlus, 0, 0, 0 }, .assignable = true, .overloadable = true };
    p[14] = (OpDef){ .name=s(","),    .arity=3, .bytes={aComma, 0, 0, 0 } };    
    p[15] = (OpDef){ .name=s("--"),   .arity=1, .bytes={aMinus, aMinus, 0, 0 }, .overloadable=true, };
    p[16] = (OpDef){ .name=s("-."),   .arity=2, .bytes={aMinus, aDot, 0, 0 }, .assignable = true, .overloadable = true };    
    p[17] = (OpDef){ .name=s("-"),    .arity=2, .bytes={aMinus, 0, 0, 0 }, .assignable = true, .overloadable = true };
    p[18] = (OpDef){ .name=s("/."),   .arity=2, .bytes={aDivBy, aDot, 0, 0 }, .assignable = true, .overloadable = true };    
    p[19] = (OpDef){ .name=s("/"),    .arity=2, .bytes={aDivBy, 0, 0, 0 }, .assignable = true, .overloadable = true };
    p[20] = (OpDef){ .name=s("<<."),  .arity=2, .bytes={aLT, aLT, aDot, 0 }, .assignable = true, .overloadable = true };        
    p[21] = (OpDef){ .name=s("<<"),   .arity=2, .bytes={aLT, aLT, 0, 0 }, .assignable = true, .overloadable = true };    
    p[22] = (OpDef){ .name=s("<="),   .arity=2, .bytes={aLT, aEqual, 0, 0 } };    
    p[23] = (OpDef){ .name=s("<>"),   .arity=2, .bytes={aLT, aGT, 0, 0 } };    
    p[24] = (OpDef){ .name=s("<"),    .arity=2, .bytes={aLT, 0, 0, 0 } };
    p[25] = (OpDef){ .name=s("=="),   .arity=2, .bytes={aEqual, aEqual, 0, 0 } };
    p[26] = (OpDef){ .name=s(">=<="), .arity=3, .bytes={aGT, aEqual, aLT, aEqual } };
    p[27] = (OpDef){ .name=s(">=<"),  .arity=3, .bytes={aGT, aEqual, aLT, 0 } };
    p[28] = (OpDef){ .name=s("><="),  .arity=3, .bytes={aGT, aLT, aEqual, 0 } };
    p[29] = (OpDef){ .name=s("><"),   .arity=3, .bytes={aGT, aLT, 0, 0 } };
    p[30] = (OpDef){ .name=s(">="),   .arity=2, .bytes={aGT, aEqual, 0, 0 } };
    p[31] = (OpDef){ .name=s(">>."),  .arity=2, .bytes={aGT, aGT, aDot, 0 }, .assignable = true, .overloadable = true };
    p[32] = (OpDef){ .name=s(">>"),   .arity=2, .bytes={aGT, aGT, 0, 0 }, .assignable = true, .overloadable = true };
    p[33] = (OpDef){ .name=s(">"),    .arity=2, .bytes={aGT, 0, 0, 0 } };
    p[34] = (OpDef){ .name=s("?:"),   .arity=2, .bytes={aQuestion, aColon, 0, 0 } };
    p[35] = (OpDef){ .name=s("?"),    .arity=1, .bytes={aQuestion, 0, 0, 0 } };
    p[36] = (OpDef){ .name=s("@"),    .arity=1, .bytes={aAt, 0, 0, 0 } };
    p[37] = (OpDef){ .name=s("^."),   .arity=2, .bytes={aCaret, aDot, 0, 0 }, .assignable = true, .overloadable = true };
    p[38] = (OpDef){ .name=s("^"),    .arity=2, .bytes={aCaret, 0, 0, 0 }, .assignable = true, .overloadable = true };
    p[39] = (OpDef){ .name=s("||"),   .arity=2, .bytes={aPipe, aPipe, 0, 0 }, .assignable=true, };
    p[40] = (OpDef){ .name=s("|"),    .arity=2, .bytes={aPipe, 0, 0, 0 } };
    p[41] = (OpDef){ .name=s("and"),  .arity=2, .bytes={0, 0, 0, 0 } };
    p[42] = (OpDef){ .name=s("or"),   .arity=2, .bytes={0, 0, 0, 0 }, .assignable=true, };
    p[43] = (OpDef){ .name=s("neg"),  .arity=2, .bytes={0, 0, 0, 0 } };
    return result;
}



/*
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
LanguageDefinition* buildLanguageDefinitions(Arena* a) {
    LanguageDefinition* result = allocateOnArena(sizeof(LanguageDefinition), a);
    result->possiblyReservedDispatch = tabulateReservedBytes(a);
    result->dispatchTable = tabulateDispatch(a);
    result->operators = tabulateOperators(a);
    result->reservedParensOrNot = tabulateReserved(a);
    return result;
}


Lexer* createLexer(String* inp, LanguageDefinition* langDef, Arena* a) {
    Lexer* lx = allocateOnArena(sizeof(Lexer), a);
    Arena* aTemp = mkArena();
    (*lx) = (Lexer){
        .i = 0, .langDef = langDef, .inp = inp, .nextInd = 0, .inpLength = inp->length,
        .tokens = allocateOnArena(LEXER_INIT_SIZE*sizeof(Token), a), .capacity = LEXER_INIT_SIZE,
        .newlines = allocateOnArena(1000*sizeof(int), a), .newlinesCapacity = 500,
        .numeric = allocateOnArena(50*sizeof(int), aTemp), .numericCapacity = 50,
        .backtrack = createStackBtToken(16, aTemp),
        .stringTable = createStackint32_t(16, a), .stringStore = createStringStore(100, aTemp),
        .wasError = false, .errMsg = &empty,
        .arena = a, .aTemp = aTemp
    };
    return lx;
}

/**
 * Finalizes the lexing of a single input: checks for unclosed scopes, and closes semicolons and an open statement, if any.
 */
private void finalize(Lexer* lx) {
    if (!hasValues(lx->backtrack)) return;
    closeColons(lx);
    BtToken top = pop(lx->backtrack);
    VALIDATE(top.spanLevel != slScope && !hasValues(lx->backtrack), errorPunctuationExtraOpening)

    setStmtSpanLength(top.tokenInd, lx);    
    deleteArena(lx->aTemp);
}


Lexer* lexicallyAnalyze(String* input, LanguageDefinition* langDef, Arena* a) {
    Lexer* lx = createLexer(input, langDef, a);

    Int inpLength = input->length;
    Arr(byte) inp = input->content;
    
    if (inpLength == 0) {
        throwExc("Empty input", lx);
    }

    // Check for UTF-8 BOM at start of file
    if (inpLength >= 3
        && (unsigned char)inp[0] == 0xEF
        && (unsigned char)inp[1] == 0xBB
        && (unsigned char)inp[2] == 0xBF) {
        lx->i = 3;
    }
    LexerFunc (*dispatch)[256] = langDef->dispatchTable;
    lx->possiblyReservedDispatch = langDef->possiblyReservedDispatch;

    // Main loop over the input
    if (setjmp(excBuf) == 0) {
        while (lx->i < inpLength) {
            ((*dispatch)[inp[lx->i]])(lx, inp);
        }
        finalize(lx);
    }
    lx->totalTokens = lx->nextInd;
    return lx;
}
