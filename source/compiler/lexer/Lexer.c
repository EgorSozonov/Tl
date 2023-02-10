#include "Lexer.h"
#include "../utils/Arena.h"
#include "../utils/String.h"
#include "../utils/Stack.h"
#include <string.h>

/**
 * The ASCII notation for the lowest 64-bit integer, -9_223_372_036_854_775_808
 */
static char minInt[] = {
        57,
        50, 50, 51,
        51, 55, 50,
        48, 51, 54,
        56, 53, 52,
        55, 55, 53,
        56, 48, 56,
    };

/**
 * The ASCII notation for the highest 64-bit integer, 9_223_372_036_854_775_807
 */
static char maxInt[] = {
        57,
        50, 50, 51,
        51, 55, 50,
        48, 51, 54,
        56, 53, 52,
        55, 55, 53,
        56, 48, 55,
    };

DEFINE_STACK(RememberedToken)

LexResult* mkLexResult(Arena* ar) {
    LexResult* result = arenaAllocate(ar, sizeof(LexResult));
    result->arena = ar;
    LexChunk* firstChunk = arenaAllocate(ar, sizeof(LexChunk));
    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->currInd = 0;
    result->totalNumber = 0;
    return result;
}

void addToken(LexResult* lr, Token tok) {
    if (lr->currInd < (LEX_CHUNK_SIZE - 1)) {
        lr->currInd++;
        lr->currChunk->tokens[lr->currInd] = tok;
    } else {
        LexChunk* nextChunk = arenaAllocate(lr->arena, sizeof(LexChunk));
        nextChunk->tokens[0] = tok;
        lr->currChunk->next = nextChunk;
        lr->currChunk = nextChunk;
        lr->currInd = 1;
    }
    lr->totalNumber++;
}






// lexNumber
// lexInt
// lexFloat
bool _isLexicographicallyLE(char* byteStart, char digits[19]) {
    for (int i = 0; i < 19; i++) {
        if (*(byteStart + i) < digits[i]) { return true; }
        if (*(byteStart + i) < digits[i]) { return false; }
    }
    return true;
}

bool checkIntRange(char* byteStart, int byteLength, bool isNegative) {
    if (byteLength != 19) return byteLength < 19;
    return isNegative ? _isLexicographicallyLE(byteStart, minInt) : _isLexicographicallyLE(byteStart, maxInt);
}

uint64_t intOfDigits(char* byteStart, int byteLength) {
    uint64_t result = 0;
    uint64_t powerOfTen = 1;
    for (char* ind = byteStart + byteLength - 1; ind >= byteStart; --byteStart) {
        uint64_t digitValue = (*ind - ASCII.digit0)*powerOfTen;
        result += digitValue;
        powerOfTen *= 10;
    }
    return result;
}


// lexWord
// stringOfASCII
// lexWordSection
// lexDotWord
// lexOperator
// isOperatorSymb
// lexStringLiteral
// lexComment
// filterBytes

void errorOut(char* errMsg, LexResult* res) {
    res->wasError = true;
    res->errMsg = allocateLiteral(res->arena, errMsg);
}

LexResult* lexicallyAnalyze(String* inp, Arena* ar) {
    LexResult* result = mkLexResult(ar);
    StackRememberedToken* backtrack = mkStackRememberedToken(ar, 100);
    const int len = inp->length;

    // scratch space to write out digits excluding underscores from number literals
    String* digitsScratchPad = allocateScratchSpace(ar, 16);

    for (int i = 0; i < len; i++) {
        unsigned char cByte = inp->content[i];
        unsigned char nextByte = inp->content[i + 1];
        if (cByte > 127) {
            errorOut("Non-ASCII characters are only allowed in comments and string litersl!", result);
            return result;
        }
        if (cByte == ASCII.space || cByte == ASCII.emptyCR) {
            i++;
        } else if (cByte == ASCII.emptyLF) {
            if (hasValuesRememberedToken(backtrack)) {
                RememberedToken* a = backtrack->content[backtrack->length - 1];
                if (a->token->tokType == TokType.curlyBraces && true) {
                    // we are in a CurlyBraces context, so a newline means closing current statement and opening a new one


                }
                i++;
            }
        } else if (cByte == ASCII.colonSemi) {

        } else if (cByte == ASCII.curlyOpen) {

        } else if (cByte == ASCII.curlyClose) {

        } else if (cByte == ASCII.parenthesisOpen) {

        } else if (cByte == ASCII.parenthesisClose) {

        } else if (cByte == ASCII.bracketOpen) {

        } else if (cByte == ASCII.bracketClose)

    }

    return result;
}
