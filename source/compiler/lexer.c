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


#define CURR_BT inp[lr->i]
#define NEXT_BT inp[lr->i + 1]
	
 
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

/** Sets i to beyond input's length to communicate to callers that lexing is over */
_Noreturn private void throwExc(const char errMsg[], Lexer* lr) {   
	lr->wasError = true;
#ifdef TRACE	
	printf("Error on i = %d\n", lr->i);
#endif	
	lr->errMsg = str(errMsg, lr->arena);
	longjmp(excBuf, 1);
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private void checkPrematureEnd(Int requiredSymbols, Lexer* lr) {
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
private void closeColons(Lexer* lr) {
	while (hasValues(lr->backtrack) && peek(lr->backtrack).wasOriginallyColon) {
		RememberedToken top = pop(lr->backtrack);
		Int j = top.tokenInd;		
		lr->tokens[j].lenBytes = lr->i - lr->tokens[j].startByte;
		lr->tokens[j].payload2 = lr->nextInd - j - 1;
	}
}

/**
 * Finds the top-level punctuation opener by its index, and sets its lengths.
 * Called when the matching closer is lexed.
 */
private void setSpanLength(Int tokenInd, Lexer* lr) {
	lr->tokens[tokenInd].lenBytes = lr->i - lr->tokens[tokenInd].startByte;
	lr->tokens[tokenInd].payload2 = lr->nextInd - tokenInd - 1;
}

/**
 * Correctly calculates the lenBytes for a single-line, statement-type span.
 */
private void setStmtSpanLength(Int topTokenInd, Lexer* lr) {
	Token lastToken = lr->tokens[lr->nextInd - 1];
	Int byteAfterLastToken = lastToken.startByte + lastToken.lenBytes;

	Int byteAfterLastPunct = lr->lastClosingPunctInd + 1;
	Int lenBytes = (byteAfterLastPunct > byteAfterLastToken ? byteAfterLastPunct : byteAfterLastToken) 
					- lr->tokens[topTokenInd].startByte;	

	lr->tokens[topTokenInd].lenBytes = lenBytes;
	lr->tokens[topTokenInd].payload2 = lr->nextInd - topTokenInd - 1;
}

/**
 * Validation to catch unmatched closing punctuation
 */
private void validateClosingPunct(untt closingType, untt openType, Lexer* lr) {
	if ((closingType == tokParens && openType == tokBrackets) 
		|| (closingType == tokBrackets && openType != tokBrackets && openType != tokAccessor)) {
			throwExc(errorPunctuationUnmatched, lr);
	}
}


#define PROBERESERVED(reservedBytesName, returnVarName)	\
	lenReser = sizeof(reservedBytesName); \
	if (lenBytes == lenReser && testByteSequence(lr->inp, startByte, reservedBytesName, lenReser)) \
		return returnVarName;


private Int determineReservedA(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesAlias, tokAlias)
	PROBERESERVED(reservedBytesAnd, tokAnd)
	PROBERESERVED(reservedBytesAwait, tokAwait)
	PROBERESERVED(reservedBytesAssert, tokAssert)
	PROBERESERVED(reservedBytesAssertDbg, tokAssertDbg)
	return 0;
}

private Int determineReservedB(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesBreak, tokBreak)
	return 0;
}

private Int determineReservedC(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesCatch, tokCatch)
	PROBERESERVED(reservedBytesContinue, tokContinue)
	return 0;
}


private Int determineReservedE(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesEmbed, tokEmbed)
	return 0;
}


private Int determineReservedF(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesFalse, reservedFalse)
	PROBERESERVED(reservedBytesFn, tokFnDef)
	return 0;
}


private Int determineReservedI(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesIf, tokIf)
	PROBERESERVED(reservedBytesIfEq, tokIfEq)
	PROBERESERVED(reservedBytesIfPr, tokIfPr)
	PROBERESERVED(reservedBytesImpl, tokImpl)
	PROBERESERVED(reservedBytesInterface, tokIface)
	return 0;
}


private Int determineReservedL(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesLambda, tokLambda)
	PROBERESERVED(reservedBytesLambda1, tokLambda1)
	PROBERESERVED(reservedBytesLambda2, tokLambda2)
	PROBERESERVED(reservedBytesLambda3, tokLambda3)
	PROBERESERVED(reservedBytesLoop, tokLoop)
	return 0;
}


private Int determineReservedM(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesMatch, tokMatch)
	PROBERESERVED(reservedBytesMut, tokMut)
	return 0;
}


private Int determineReservedO(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesOr, tokOr)
	return 0;
}


private Int determineReservedR(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesReturn, tokReturn)
	return 0;
}


private Int determineReservedS(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesStruct, tokStruct)
	return 0;
}


private Int determineReservedT(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesTrue, reservedTrue)
	PROBERESERVED(reservedBytesTry, tokTry)
	return 0;
}


private Int determineReservedY(Int startByte, Int lenBytes, Lexer* lr) {
	Int lenReser;
	PROBERESERVED(reservedBytesYield, tokYield)
	return 0;
}

private Int determineUnreserved(Int startByte, Int lenBytes, Lexer* lr) {
	return -1;
}


private void addNewLine(Int j, Lexer* lr) {
	lr->newlines[lr->newlinesNextInd] = j;
	lr->newlinesNextInd++;
	if (lr->newlinesNextInd == lr->newlinesCapacity) {
		Arr(int) newNewlines = allocateOnArena(lr->newlinesCapacity*2*sizeof(int), lr->arena);
		memcpy(newNewlines, lr->newlines, lr->newlinesCapacity);
		lr->newlines = newNewlines;
		lr->newlinesCapacity *= 2;
	}
}

private void addStatement(untt stmtType, Int startByte, Lexer* lr) {
	push(((RememberedToken){ .tp = stmtType, .tokenInd = lr->nextInd}), lr->backtrack);
	add((Token){ .tp = stmtType, .startByte = startByte}, lr);
}


private void wrapInAStatementStarting(Int startByte, Lexer* lr, Arr(byte) inp) {	
	if (hasValues(lr->backtrack)) {
		if (peek(lr->backtrack).isMultiline) {			
			push(((RememberedToken){ .tp = tokStmt, .tokenInd = lr->nextInd }), lr->backtrack);
			add((Token){ .tp = tokStmt, .startByte = startByte},  lr);
		}
	} else {
		addStatement(tokStmt, startByte, lr);
	}
}


private void wrapInAStatement(Lexer* lr, Arr(byte) inp) {
	if (hasValues(lr->backtrack)) {
		if (peek(lr->backtrack).isMultiline) {
			push(((RememberedToken){ .tp = tokStmt, .tokenInd = lr->nextInd }), lr->backtrack);
			add((Token){ .tp = tokStmt, .startByte = lr->i},  lr);
		}
	} else {
		addStatement(tokStmt, lr->i, lr);
	}
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ) or a ] .
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private void closeRegularPunctuation(Int closingType, Lexer* lr) {
	StackRememberedToken* bt = lr->backtrack;
	closeColons(lr);
	if (!hasValues(bt)) {
		throwExc(errorPunctuationExtraClosing, lr);
	}
	RememberedToken top = pop(bt);
	if (bt->length > 0 && closingType == tokParens && !top.isMultiline && ((*bt->content)[bt->length - 1].isMultiline)) {
		// since a closing parenthesis might be closing something with statements inside it, like a lex scope
		// or a core syntax form, we need to close the last statement before closing its parent
		setStmtSpanLength(top.tokenInd, lr);
		top = pop(bt);
	}
	validateClosingPunct(closingType, top.tp, lr);
	setSpanLength(top.tokenInd, lr);
	lr->i++;
}

/** 
 * The dot which is preceded by a space is a statement separator.
 * It is allowed only in multiline spans or in single-line spans directly inside multiline ones.
 */
private void lexDot(Lexer* lr, Arr(byte) inp) {
	Int multiLineMode = 0;
	if (!hasValues(lr->backtrack) || peek(lr->backtrack).isMultiline) {
		// if we're at top level or directly inside a scope, do nothing since there're no stmts to close
		lr->i++;
		return;
	} else {
		// We must be in a single-line span which is directly inside a multiline, so we close it
		// Otherwise, error				
		Int len = lr->backtrack->length;
		if (len == 1 || len >= 2 && (*lr->backtrack->content)[len - 2].isMultiline) {
			RememberedToken top = peek(lr->backtrack);
			setStmtSpanLength(top.tokenInd, lr);
			pop(lr->backtrack);
			lr->i++;
			return;
		}
	}
	throwExc(errorPunctuationOnlyInMultiline, lr);
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
	Int j = lr->numericNextInd - 1;

	Int loopLimit = -1;
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
private bool integerWithinDigits(const byte* b, Int bLength, Lexer* lr){
	if (lr->numericNextInd != bLength) return (lr->numericNextInd < bLength);
	for (Int j = 0; j < lr->numericNextInd; j++) {
		if (lr->numeric[j] < b[j]) return true;
		if (lr->numeric[j] > b[j]) return false;
	}
	return true;
}


private Int calcInteger(int64_t* result, Lexer* lr) {
	if (lr->numericNextInd > 19 || !integerWithinDigits(maxInt, sizeof(maxInt), lr)) return -1;
	*result = calcIntegerWithinLimits(lr);
	return 0;
}


private int64_t calcHexNumber(Lexer* lr) {
	int64_t result = 0;
	int64_t powerOfSixteen = 1;
	Int j = lr->numericNextInd - 1;

	// If the literal is full 16 bits long, then its upper sign contains the sign bit
	Int loopLimit = -1; //if (byteBuf.ind == 16) { 0 } else { -1 }
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
 * TODO add floating-pointt literals like 0x12FA.
 */
private void hexNumber(Lexer* lr, Arr(byte) inp) {
	checkPrematureEnd(2, lr);
	lr->numericNextInd = 0;
	Int j = lr->i + 2;
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
private Int calcFloating(double* result, Int powerOfTen, Lexer* lr, Arr(byte) inp){
	Int indTrailingZeroes = lr->numericNextInd - 1;
	Int ind = lr->numericNextInd;
	while (indTrailingZeroes > -1 && lr->numeric[indTrailingZeroes] == 0) { 
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
	lr->numericNextInd -= transfer;
	Int finalPowerTen = powerOfTen + transfer;

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
	Int j = (isNegative) ? (lr->i + 1) : lr->i;
	Int digitsAfterDot = 0; // this is relative to first digit, so it includes the leading zeroes
	bool metDot = false;
	bool metNonzero = false;
	Int maximumInd = (lr->i + 40 > lr->inpLength) ? (lr->i + 40) : lr->inpLength;
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
		Int errorCode = calcFloating(&resultValue, -digitsAfterDot, lr, inp);
		if (errorCode != 0) {
			throwExc(errorNumericFloatWidthExceeded, lr);
		}

		int64_t bitsOfFloat = longOfDoubleBits((isNegative) ? (-resultValue) : resultValue);
		add((Token){ .tp = tokFloat, .payload1 = (bitsOfFloat >> 32), .payload2 = (bitsOfFloat & LOWER32BITS),
					.startByte = lr->i, .lenBytes = j - lr->i}, lr);
	} else {
		int64_t resultValue = 0;
		Int errorCode = calcInteger(&resultValue, lr);
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
private void openPunctuation(untt tType, Lexer* lr) {
	lr->i++;
	push( ((RememberedToken){ 
		.tp = tType, .tokenInd = lr->nextInd, .isMultiline = tType == tokScope
	}), lr->backtrack);
	add((Token) {.tp = tType, .startByte = lr->i }, lr);	
}

/**
 * Lexer action for a reserved word. It mutates the type of current span and.
 * If necessary (parens inside statement) it also deletes last token and removes the top frame
 */
private void lexReservedWord(untt reservedWordType, Int startByte, Lexer* lr, Arr(byte) inp) {	
	StackRememberedToken* bt = lr->backtrack;
	
	if (reservedWordType == tokMut && (!hasValues(bt) || peek(bt).isMultiline)) {
		addStatement(tokMutTemp, startByte, lr);
		return;
	}
	
	Int expectations = (*lr->langDef->reservedParensOrNot)[reservedWordType - firstCoreFormTokenType];
	if (expectations == 0 || expectations == 2) { // the reserved words that live at the start of a statement
		if (!hasValues(bt) || peek(bt).isMultiline) {
			addStatement(reservedWordType, startByte, lr);
		} else {			
			throwExc(errorCoreNotInsideStmt, lr);
		}
	} else if (expectations == 1) { // the "(reserved" case
		if (!hasValues(bt)) {
			throwExc(errorCoreMissingParen, lr);
		}
		RememberedToken top = peek(bt);
		if (top.tokenInd + 1 != lr->nextInd) { // if this isn't the first token inside the parens
			throwExc(errorCoreNotAtSpanStart, lr);
		}
		
		if (bt->length > 1 && (*bt->content)[bt->length - 2].tp == tokStmt) {
			// Parens are wrapped in statements because we didn't know that the following token is a reserved word.
			// So when meeting a reserved word at start of parens, we need to delete the paren token & lex frame.
			// The new reserved token type will be written over the tokStmt that precedes the tokParen.
			pop(bt);
			lr->nextInd--;
			top = peek(bt);
			lr->tokens[top.tokenInd].startByte = startByte;
		}
		
		// update the token type and the corresponding frame type
		lr->tokens[top.tokenInd].tp = reservedWordType;
		(*bt->content)[bt->length - 1].tp = reservedWordType;
		(*bt->content)[bt->length - 1].isMultiline = true; // all parenthetical core forms are considered multiline
	}
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
 * Precondition: we are pointing at the first letter character of the word (i.e. past the possible "@" or ":")
 * Examples of acceptable expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of unacceptable expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private void wordInternal(untt wordType, Lexer* lr, Arr(byte) inp) {
	Int startByte = lr->i;

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

	Int realStartByte = (wordType == tokWord) ? startByte : (startByte - 1); // accounting for the . or @ at the start
	bool paylCapitalized = wasCapitalized ? 1 : 0;

	byte firstByte = lr->inp->content[startByte];
	Int lenBytes = lr->i - realStartByte;
	Int lenString = lr->i - startByte;
	if (wordType != tokWord && isAlsoAccessor) {
		throwExc(errorWordWrongAccessor, lr);
	}
		
	if (wordType == tokAtWord || firstByte < aALower || firstByte > aYLower) {
		wrapInAStatementStarting(startByte, lr, inp);
		Int uniqueStringInd = addStringStore(inp, startByte, lenBytes, lr->stringTable, lr->stringStore);
		add((Token){ .tp=wordType, .payload1=paylCapitalized, .payload2 = uniqueStringInd,
					 .startByte=realStartByte, .lenBytes=lenBytes }, lr);
		if (isAlsoAccessor) {
			openPunctuation(tokAccessor, lr);
		}
	} else {
		Int mbReservedWord = (*lr->possiblyReservedDispatch)[firstByte - aALower](startByte, lenString, lr);
		if (mbReservedWord > 0) {
			if (wordType == tokDotWord) {
				throwExc(errorWordReservedWithDot, lr);
			} else if (mbReservedWord < firstCoreFormTokenType) {			   
				if (mbReservedWord == tokAnd) {
					add((Token){.tp=tokAnd, .startByte=realStartByte, .lenBytes=3}, lr);
				} else if (mbReservedWord == tokOr) {
					add((Token){.tp=tokOr, .startByte=realStartByte, .lenBytes=2}, lr);
				} else if (mbReservedWord == reservedTrue) {
					wrapInAStatementStarting(startByte, lr, inp);
					add((Token){.tp=tokBool, .payload2=1, .startByte=realStartByte, .lenBytes=lenBytes}, lr);
				} else if (mbReservedWord == reservedFalse) {
					wrapInAStatementStarting(startByte, lr, inp);
					add((Token){.tp=tokBool, .payload2=0, .startByte=realStartByte, .lenBytes=lenBytes}, lr);
				} else if (mbReservedWord == tokDispose) {
					wrapInAStatementStarting(startByte, lr, inp);
					add((Token){.tp=tokDispose, .payload2=0, .startByte=realStartByte, .lenBytes=7}, lr);
				}
			} else {
				lexReservedWord(mbReservedWord, realStartByte, lr, inp);
			}
		} else  {			
			wrapInAStatementStarting(startByte, lr, inp);
			Int uniqueStringInd = addStringStore(inp, startByte, lenString, lr->stringTable, lr->stringStore);
			add((Token){ .tp=wordType, .payload1=paylCapitalized, .payload2 = uniqueStringInd,
						 .startByte=realStartByte, .lenBytes=lenBytes }, lr);
			if (isAlsoAccessor) {
				openPunctuation(tokAccessor, lr);
			}
		}
	}
}


private void lexWord(Lexer* lr, Arr(byte) inp) {	
	wordInternal(tokWord, lr, inp);
}


private void lexAtWord(Lexer* lr, Arr(byte) inp) {
	wrapInAStatement(lr, inp);
	checkPrematureEnd(2, lr);
	lr->i++; // CONSUME the "@" symbol
	wordInternal(tokAtWord, lr, inp);
}


private void setSpanAssignment(Int tokenInd, Int mutType, untt opType, Lexer* lr) {
	Token* tok = (lr->tokens + tokenInd);
	untt tp;
	if (mutType == 0) {
		if (tok->tp == tokMutTemp) {
			tok->payload1 = 1;
		}
		tok->tp = tokAssignment;		

		(*lr->backtrack->content)[lr->backtrack->length - 1] = (RememberedToken){
			.tp = tokAssignment, .tokenInd = tokenInd
		};  
	} else if (mutType == 1) {
		tok->tp = tokReassign;
		(*lr->backtrack->content)[lr->backtrack->length - 1] = (RememberedToken){
			.tp = tokReassign, .tokenInd = tokenInd
		};
	} else {
		tok->tp = tokMutation;
		tok->payload1 = (mutType == 3 ? 1 : 0)  + (opType << 1);
		(*lr->backtrack->content)[lr->backtrack->length - 1] = (RememberedToken){
			.tp = tokMutation, .tokenInd = tokenInd
		}; 
	}
}

/**
 * Handles the "=", ":=" and "+=" forms. 
 * Changes existing tokens and parent span to accout for the fact that we met an assignment operator 
 * mutType = 0 if it's immutable assignment, 1 if it's ":=", 2 if it's a regular operator mut ("+="),
 * 3 if it's an extended operator mut ("+.=")
 */
private void processAssignment(Int mutType, untt opType, Lexer* lr) {
	RememberedToken currSpan = peek(lr->backtrack);

	if (currSpan.tp == tokAssignment || currSpan.tp == tokReassign || currSpan.tp == tokMutation) {
		throwExc(errorOperatorMultipleAssignment, lr);
	} else if (currSpan.tp == tokMutTemp) { // the "mut x = ..." definition
		if (mutType != 0) throwExc(errorOperatorMutableDef, lr);	
	} else if (currSpan.tp != tokStmt) {
		throwExc(errorOperatorAssignmentPunct, lr);
	}
	setSpanAssignment(currSpan.tokenInd, mutType, opType, lr);	
}


private void lexComma(Lexer* lr, Arr(byte) inp) {	   
	checkPrematureEnd(2, lr);
	lr->i++; // CONSUME the ","
	byte nextBt = CURR_BT;
	if (nextBt == aParenLeft) {
		// TODO tokFuncExpr
	} else {
		wordInternal(tokFuncWord, lr, inp);
	}	
}

private void lexSemicolon(Lexer* lr, Arr(byte) inp) {
	lr->i++; // CONSUME the ";"
	push(((RememberedToken){ .tp = tokParens, .tokenInd = lr->nextInd, .wasOriginallyColon = true}),
		lr->backtrack
	);
	add((Token) {.tp = tokParens, .startByte = lr->i }, lr);
}

/**
 * A single colon is the "parens until end" symbol,
 * a double colon :: is the "else clause" symbol.
 */
private void lexColon(Lexer* lr, Arr(byte) inp) {   
	lr->i++; // the ":"
	byte nextBt = CURR_BT;
	if (nextBt == aEqual) { // mutation assignment, :=
		lr->i++; // the "="
		processAssignment(1, 0, lr);
	} else if (nextBt == aColon) {
		lr->i++; // the second ":"
		
		Int len = lr->backtrack->length;
		if (len >= 2 && (*lr->backtrack->content)[len - 2].tp) {
			untt parentType = (*lr->backtrack->content)[len - 2].tp;
			if (parentType == tokIf || parentType == tokIfEq || parentType == tokIfPr || parentType == tokMatch) {
				RememberedToken top = peek(lr->backtrack);
				setStmtSpanLength(top.tokenInd, lr);
				pop(lr->backtrack);
			} else {
				throwExc(errorCoreMisplacedElse, lr);
			}		
		} else {
			throwExc(errorCoreMisplacedElse, lr);
		}				
		push(((RememberedToken){ .tp = tokElse, .tokenInd = lr->nextInd }),
			lr->backtrack
		);
		add((Token) {.tp = tokElse, .startByte = lr->i }, lr);
	} else {
		peek(lr->backtrack).indentation = lr->indentation;
		lr->indentation++;
	}	
}


private void addOperator(Int opType, Int isExtension, Int startByte, Int lenBytes, Lexer* lr) {
	add((Token){ .tp = tokOperator, .payload1 = (isExtension & 1) + (opType << 1), 
				.startByte = startByte, .lenBytes = lenBytes }, lr);
}


private void lexOperator(Lexer* lr, Arr(byte) inp) {
	wrapInAStatement(lr, inp);
	
	byte firstSymbol = CURR_BT;
	byte secondSymbol = (lr->inpLength > lr->i + 1) ? inp[lr->i + 1] : 0;
	byte thirdSymbol = (lr->inpLength > lr->i + 2) ? inp[lr->i + 2] : 0;
	byte fourthSymbol = (lr->inpLength > lr->i + 3) ? inp[lr->i + 3] : 0;
	Int k = 0;
	Int opType = -1; // corresponds to the opT... operator types
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
	Int isExtensionAssignment = 0;
	
	Int lengthOfBaseOper = (opDef.bytes[1] == 0) ? 1 : (opDef.bytes[2] == 0 ? 2 : (opDef.bytes[3] == 0 ? 3 : 4));
	Int j = lr->i + lengthOfBaseOper;
	if (isExtensible && j < lr->inpLength && inp[j] == aDot) {
		isExtensionAssignment += 1;
		j++;		
	}
	if ((isExtensible || opDef.assignable) && j < lr->inpLength && inp[j] == aEqual) {
		isExtensionAssignment += 2;
		j++;
	}
	if ((isExtensionAssignment & 2) > 0) { // mutation operators like "*=" or "*.="
		processAssignment(isExtensionAssignment & 1 > 0 ? 3 : 2, opType, lr);
	} else {
		addOperator(opType, isExtensionAssignment, lr->i, j - lr->i, lr);
	}
	lr->i = j;
}


private void lexEqual(Lexer* lr, Arr(byte) inp) {	   
	checkPrematureEnd(2, lr);
	byte nextBt = NEXT_BT;
	if (nextBt == aEqual) {
		lexOperator(lr, inp); // ==		
	} else {
		processAssignment(0, 0, lr);
		lr->i++; // =
	}
}

/**
 * Appends a doc comment token if it's the first one, or elongates the existing token if there was already
 * a doc comment on the previous line.
 * This logic handles multiple consecutive lines of doc comments.
 * NOTE: this is the only function in the whole lexer that needs the endByte instead of lenBytes!
 */
private void appendDocComment(Int startByte, Int endByte, Lexer* lr) {
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
	Int startByte = lr->i + 1; // +1 for the third dash in "---"
	for (Int j = startByte; j < lr->inpLength; j++) {
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
 * Doc comments, syntax is -- The comment or ---Doc comment
 * Precondition: we are past the "--"
 */
private void comment(Lexer* lr, Arr(byte) inp) {
	if (lr->i >= lr->inpLength) return;
	
	if (CURR_BT == aMinus) {
		docComment(lr, inp);
	} else {
		Int j = lr->i;
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
}

/** Handles the binary operator, the unary operator and the comments */
private void lexMinus(Lexer* lr, Arr(byte) inp) {	
	Int j = lr->i + 1;
	if (j >= lr->inpLength) {
		throwExc(errorPrematureEndOfInput, lr);
	}
	byte nextBt = NEXT_BT;
	if (nextBt == aMinus) {
		lr->i += 2;
		comment(lr, inp);
	} else if (isDigit(nextBt)) {
		wrapInAStatement(lr, inp);
		decNumber(true, lr, inp);
		lr->numericNextInd = 0;
	} else {
		lexOperator(lr, inp);
	}	
}

/** If we are inside a compound (=2) core form, we need to increment the clause count */ 
private void mbIncrementClauseCount(Lexer* lr) {
	if (hasValues(lr->backtrack)) {
		RememberedToken top = peek(lr->backtrack);
		if (top.tp >= firstCoreFormTokenType && (*lr->langDef->reservedParensOrNot)[top.tp - firstCoreFormTokenType] == 2) {
			(*lr->backtrack->content)[lr->backtrack->length - 1].countClauses++;
		}
	}
}

/** If we're inside a compound (=2) core form, we need to check if its clause count is saturated */ 
private void mbCloseCompoundCoreForm(Lexer* lr) {
	RememberedToken top = peek(lr->backtrack);
	if (top.tp >= firstCoreFormTokenType && (*lr->langDef->reservedParensOrNot)[top.tp - firstCoreFormTokenType] == 2) {
		if (top.countClauses == 2) {
			setSpanLength(top.tokenInd, lr);
			pop(lr->backtrack);
		}
	}
}


private void lexParenLeft(Lexer* lr, Arr(byte) inp) {
	mbIncrementClauseCount(lr);
	Int j = lr->i + 1;
	if (j < lr->inpLength && inp[j] == aColon) { // "(:" starts a new lexical scope
		lr->i++; // the ":"
		openPunctuation(tokScope, lr);		
	} else {
		wrapInAStatement(lr, inp);
		openPunctuation(tokParens, lr);
	}	
}


private void lexParenRight(Lexer* lr, Arr(byte) inp) {
	// TODO handle syntax like "(foo 5).field"
	Int startInd = lr->i;
	closeRegularPunctuation(tokParens, lr);
	
	if (!hasValues(lr->backtrack)) return;	
	mbCloseCompoundCoreForm(lr);
	
	lr->lastClosingPunctInd = startInd;	
}


void lexBracketLeft(Lexer* lr, Arr(byte) inp) {
	wrapInAStatement(lr, inp);
	openPunctuation(tokBrackets, lr);
}


void lexBracketRight(Lexer* lr, Arr(byte) inp) {
	// TODO handle syntax like "foo[5].field"	
	Int startInd = lr->i;
	closeRegularPunctuation(tokBrackets, lr);
	mbCloseCompoundCoreForm(lr);
	
	lr->lastClosingPunctInd = startInd;
}


void lexSpace(Lexer* lr, Arr(byte) inp) {
	lr->i++;
	while (lr->i < lr->inpLength && CURR_BT == aSpace) {
		if (CURR_BT != aSpace) return;
		lr->i++;
	}
}

/** Tl is indentation-sensitive. */
private void lexNewline(Lexer* lr, Arr(byte) inp) {
	addNewLine(lr->i, lr);
	lr->i++;	  // CONSUME the LF
	Int j = 0;
	while (lr->i < lr->inpLength && CURR_BT == aTab) {
		lr->i++;
		j += 4;
	}
	while (lr->i < lr->inpLength && CURR_BT == aSpace) {
		lr->i++;
		j++;
	}
	if (j % 4 == 0) {
		lr->indentation = j/4;
		// todo close span
	} else if (j % 4 == 1) {
		VALIDATE((j - 1)/4 == lr->indentation, errorWrongIndentation)
	} else {
		throwExc(errorWrongIndentation);
	}
}


private void lexStringLiteral(Lexer* lr, Arr(byte) inp) {
	wrapInAStatement(lr, inp);
	Int j = lr->i + 1;
	for (; j < lr->inpLength && inp[j] != aQuote; j++);
	
	if (j == lr->inpLength) {
		throwExc(errorPrematureEndOfInput, lr);
	}	
	add((Token){.tp=tokString, .startByte=(lr->i), .lenBytes=(j - lr->i + 1)}, lr);
	lr->i = j + 1;
}


void lexUnexpectedSymbol(Lexer* lr, Arr(byte) inp) {
	throwExc(errorUnrecognizedByte, lr);
}

void lexNonAsciiError(Lexer* lr, Arr(byte) inp) {
	throwExc(errorNonAscii, lr);
}

/** Must agree in order with token types in LexerConstants.h */
const char* tokNames[] = {
	"Int", "Float", "Bool", "String", "_", "DocComment", 
	"word", ".word", "@word", ",func", "operator", "and", "or", "dispose", ":",  "mutTemp",
	"(:", ".", "()", "[]", "accessor[]", "funcExpr", "assign", ":=", "mutation", "else", ";",
	"alias", "assert", "assertDbg", "await", "break", "catch", "continue", 
	"defer", "embed", "export", "exposePriv", "fn", "interface", 
	"lambda", "lam1", "lam2", "lam3", "package", "return", "struct", "try", "yield",
	"if", "ifEq", "ifPr", "impl", "match", "loop", "mut"
};


void printLexer(Lexer* a) {
	if (a->wasError) {
		printf("Error: ");
		printString(a->errMsg);
	}
	for (int i = 0; i < a->totalTokens; i++) {
		Token tok = a->tokens[i];
		if (tok.payload1 != 0 || tok.payload2 != 0) {
			printf("%s %d %d [%d; %d]\n", tokNames[tok.tp], tok.payload1, tok.payload2, tok.startByte, tok.lenBytes);
		} else {
			printf("%s [%d; %d]\n", tokNames[tok.tp], tok.startByte, tok.lenBytes);
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
	p[aComma] = &lexComma;
	p[aDot] = &lexDot;
	p[aAt] = &lexAtWord;
	p[aColon] = &lexColon;
	p[aEqual] = &lexEqual;

	for (Int i = 0; i < countOperatorStartSymbols; i++) {
		p[operatorStartSymbols[i]] = &lexOperator;
	}
	p[aMinus] = &lexMinus;
	p[aParenLeft] = &lexParenLeft;
	p[aParenRight] = &lexParenRight;
	p[aBracketLeft] = &lexBracketLeft;
	p[aBracketRight] = &lexBracketRight;
	p[aSpace] = &lexSpace;
	p[aCarrReturn] = &lexSpace;
	p[aNewline] = &lexNewline;
	p[aQuote] = &lexStringLiteral;
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
	p[tokFnDef - firstCoreFormTokenType] = 2;
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
	p[ 0] = (OpDef){ .name=s("!="), .precedence=11,		 .arity=2, .bytes={aExclamation, aEqual, 0, 0 } };
	p[ 1] = (OpDef){ .name=s("!"),  .precedence=prefixPrec, .arity=1, .bytes={aExclamation, 0, 0, 0 } };
	p[ 2] = (OpDef){ .name=s("#"),  .precedence=prefixPrec, .arity=1, .overloadable = true, .bytes={aSharp, 0, 0, 0 } };	
	p[ 3] = (OpDef){ .name=s("$"),  .precedence=prefixPrec, .arity=1, .overloadable = true, .bytes={aDollar, 0, 0, 0 } };	
	p[ 4] = (OpDef){ .name=s("%"),  .precedence=20,		 .arity=2, .extensible=true, .bytes={aPercent, 0, 0, 0 } };
	p[ 5] = (OpDef){ .name=s("&&"), .precedence=9,		  .arity=2, .bytes={aAmp, aAmp, 0, 0 }, .assignable=true };
	p[ 6] = (OpDef){ .name=s("&"),  .precedence=prefixPrec, .arity=1, .bytes={aAmp, 0, 0, 0 } };
	p[ 7] = (OpDef){ .name=s("'"),  .precedence=prefixPrec, .arity=1, .bytes={aApostrophe, 0, 0, 0 } };
	p[ 8] = (OpDef){ .name=s("*"),  .precedence=20,		 .arity=2, .extensible=true, .bytes={aTimes, 0, 0, 0 } };
	p[ 9] = (OpDef){ .name=s("+"),  .precedence=17,		 .arity=2, .extensible=true, .bytes={aPlus, 0, 0, 0 } };
	p[10] = (OpDef){ .name=s("-"),  .precedence=17,		 .arity=2, .extensible=true, .bytes={aMinus, 0, 0, 0 } };
	p[11] = (OpDef){ .name=s("/"),  .precedence=20,		 .arity=2, .extensible=true, .bytes={aDivBy, 0, 0, 0 } };
	p[12] = (OpDef){ .name=s("<-"), .precedence=1,		  .arity=2, .bytes={aLT, aMinus, 0, 0 } };
	p[13] = (OpDef){ .name=s("<<"), .precedence=14,		 .arity=2, .extensible=true, .bytes={aLT, aLT, 0, 0 } };	
	p[14] = (OpDef){ .name=s("<="), .precedence=12,		 .arity=2, .bytes={aLT, aEqual, 0, 0 } };	
	p[15] = (OpDef){ .name=s("<>"), .precedence=12,		 .arity=2, .bytes={aLT, aGT, 0, 0 } };	
	p[16] = (OpDef){ .name=s("<"),  .precedence=12,		 .arity=2, .bytes={aLT, 0, 0, 0 } };
	p[17] = (OpDef){ .name=s("=="), .precedence=11,		 .arity=2, .bytes={aEqual, aEqual, 0, 0 } };
	p[18] = (OpDef){ .name=s(">=<="), .precedence=12,	   .arity=3, .bytes={aGT, aEqual, aLT, aEqual } };
	p[19] = (OpDef){ .name=s(">=<"), .precedence=12,		.arity=3, .bytes={aGT, aEqual, aLT, 0 } };
	p[20] = (OpDef){ .name=s("><="), .precedence=12,		.arity=3, .bytes={aGT, aLT, aEqual, 0 } };
	p[21] = (OpDef){ .name=s("><"), .precedence=12,		 .arity=3, .bytes={aGT, aLT, 0, 0 } };
	p[22] = (OpDef){ .name=s(">="), .precedence=12,		 .arity=2, .bytes={aGT, aEqual, 0, 0 } };
	p[23] = (OpDef){ .name=s(">>"), .precedence=14,		 .arity=2, .extensible=true, .bytes={aGT, aGT, 0, 0 } };
	p[24] = (OpDef){ .name=s(">"),  .precedence=12,		 .arity=2, .bytes={aGT, 0, 0, 0 } };
	p[25] = (OpDef){ .name=s("?:"), .precedence=1,		  .arity=2, .bytes={aQuestion, aColon, 0, 0 } };
	p[26] = (OpDef){ .name=s("?"),  .precedence=prefixPrec, .arity=1, .bytes={aQuestion, 0, 0, 0 } };
	p[27] = (OpDef){ .name=s("^"),  .precedence=21,		 .arity=2, .extensible=true, .bytes={aCaret, 0, 0, 0 } };
	p[28] = (OpDef){ .name=s("||"), .precedence=3,		  .arity=2, .bytes={aPipe, aPipe, 0, 0 }, .assignable=true };
	p[29] = (OpDef){ .name=s("|"),  .precedence=9,		  .arity=2, .bytes={aPipe, 0, 0, 0 } };	
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
	Lexer* result = allocateOnArena(sizeof(Lexer), a);	

	result->langDef = langDef;
	result->arena = a;
	result->aTemp = mkArena();
	
	result->inp = inp;
	result->inpLength = inp->length;	
	
	result->tokens = allocateOnArena(LEXER_INIT_SIZE*sizeof(Token), a);
	result->capacity = LEXER_INIT_SIZE;
		
	result->newlines = allocateOnArena(1000*sizeof(int), a);
	result->newlinesCapacity = 500;
	
	result->numeric = allocateOnArena(50*sizeof(int), result->aTemp);
	result->numericCapacity = 50;
	
	result->backtrack = createStackRememberedToken(16, result->aTemp);
	
	result->stringTable = createStackint32_t(16, a);
	result->stringStore = createStringStore(100, result->aTemp);

	result->errMsg = &empty;

	return result;
}

/**
 * Finalizes the lexing of a single input: checks for unclosed scopes, and closes semicolons and an open statement, if any.
 */
private void finalize(Lexer* lr) {
	if (!hasValues(lr->backtrack)) return;
	closeColons(lr);
	RememberedToken top = pop(lr->backtrack);
	if (top.isMultiline || hasValues(lr->backtrack)) {
		throwExc(errorPunctuationExtraOpening, lr);
	}
	setStmtSpanLength(top.tokenInd, lr);	
	deleteArena(lr->aTemp);
}


Lexer* lexicallyAnalyze(String* input, LanguageDefinition* langDef, Arena* a) {
	Lexer* result = createLexer(input, langDef, a);

	Int inpLength = input->length;
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
	LexerFunc (*dispatch)[256] = langDef->dispatchTable;
	result->possiblyReservedDispatch = langDef->possiblyReservedDispatch;
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
