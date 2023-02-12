#include "Lexer.h"
#include "../../utils/Arena.h"
#include "../../utils/String.h"
#include "../../utils/Stack.h"
#include <string.h>
#include <stdarg.h>

#define private static

    /*     for (i in aDigit0..aDigit9) { */
    /*         dispatchTable[i] = Lexer::lexNumber */
    /*     } */

    /*     for (i in aALower..aZLower) { */
    /*         dispatchTable[i] = Lexer::lexWord */
    /*     } */
    /*     for (i in aAUpper..aZUpper) { */
    /*         dispatchTable[i] = Lexer::lexWord */
    /*     } */
    /*     dispatchTable[aUnderscore.toInt()] = Lexer::lexWord */
    /*     dispatchTable[aDot.toInt()] = Lexer::lexDotSomething */
    /*     dispatchTable[aAt.toInt()] = Lexer::lexAtWord */

    /*     for (i in operatorStartSymbols) { */
    /*         dispatchTable[i.toInt()] = Lexer::lexOperator */
    /*     } */
    /*     dispatchTable[aMinus.toInt()] = Lexer::lexMinus */
    /*     dispatchTable[aParenLeft.toInt()] = Lexer::lexParenLeft */
    /*     dispatchTable[aParenRight.toInt()] = Lexer::lexParenRight */
    /*     dispatchTable[aCurlyLeft.toInt()] = Lexer::lexCurlyLeft */
    /*     dispatchTable[aCurlyRight.toInt()] = Lexer::lexCurlyRight */
    /*     dispatchTable[aBracketLeft.toInt()] = Lexer::lexBracketLeft */
    /*     dispatchTable[aBracketRight.toInt()] = Lexer::lexBracketRight */

    /*     dispatchTable[aQuestion.toInt()] = Lexer::lexQuestionMark */
    /*     dispatchTable[aSpace.toInt()] = Lexer::lexSpace */
    /*     dispatchTable[aCarriageReturn.toInt()] = Lexer::lexSpace */
    /*     dispatchTable[aNewline.toInt()] = Lexer::lexNewline */

    /*     dispatchTable[aQuote.toInt()] = Lexer::lexStringLiteral */
    /*     dispatchTable[aSemicolon.toInt()] = Lexer::lexComment */
    /*     dispatchTable[aColon.toInt()] = Lexer::lexColon */
    /* } */

DEFINE_STACK(RememberedToken)

Lexer* createLexer(Arena* ar) {
    Lexer* result = allocateOnArena(ar, sizeof(Lexer));
    result->capacity = LEX_CHUNK_SIZE;
    result->tokens = allocateOnArena(ar, LEX_CHUNK_SIZE*sizeof(Token)); // 1-element array
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

private Lexer* buildLexer(int totalTokens, Arena *ar, /* Tokens */ ...) {
    Lexer* result = createLexer(ar);
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


private void exitWithError(const char errMsg[], Lexer* res) {
    res->wasError = true;
    res->errMsg = allocateLiteral(res->arena, errMsg);
}


private void finalize(Lexer* this) {
    if (!this->backtrack.empty()) {
        exitWithError(errorPunctuationExtraOpening, this)
    }
}


Lexer* lexicallyAnalyze(String* inp, Arena* ar) {
    Lexer* result = createLexer(ar);
    if (inp->length == 0) {
        exitWithError("Empty input", result);
    }

	// Check for UTF-8 BOM at start of file
	int i = 0;
	if (inp->length >= 3 && (unsigned char)inp->content[0] == 0xEF && inp->content[1] == 0xBB && inp->content[2] == 0xBF) {
		i = 3;
	}

	// Main loop over the input
	while (i < inp->length && !result->wasError) {
		unsigned byte cByte = inp[i];
		if (cByte >= 0) {
			dispatchTable[cByte.toInt()]()
		} else {
			exitWithError(errorNona)
		}
	}

	finalize(result);

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
