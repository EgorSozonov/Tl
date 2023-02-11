#include "Lexer.h"
#include "../utils/Arena.h"
#include "../utils/String.h"
#include "../utils/Stack.h"
#include <string.h>



Lexer* createLexer(Arena* ar) {
    Lexer* result = allocateOnArena(ar, sizeof(Lexer));
    result->capacity = LEX_CHUNK_SIZE;
    result->tokens = allocateOnArena(ar, LEX_CHUNK_SIZE*sizeof(Token)); // 1-element array    
    result->arena = ar;

    return result;
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
static void handleFullChunk(Lexer* lexer) {
    Token* newStorage = allocateOnArena(lexer->arena, lexer->capacity*2*sizeof(Token));
    memcpy(newStorage, lexer->tokens, lexer->capacity*(sizeof(Token)));
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





//~ typedef struct {
    //~ Token* tokens;
    //~ int totalTokens;
    //~ bool wasError;
    //~ String* errMsg;
    
    //~ int currInd; // for reading a previously filled token storage. This is the absolute index (i.e. not within a single array)
    
    //~ int chunkCapacity;
    //~ int chunkCount;
    //~ Token** storage; // chunkCapacity-length array of pointers to LEX_CHUNK_SIZE-sized arrays of Token
    
    //~ Token* currChunk; // the current token array being added to
    //~ int nextInd; // the relative (i.e. withing a single token array, currChunk) index for the next token to be added
//~ } Lexer;




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


//~ void errorOut(char* errMsg, LexResult* res) {
    //~ res->wasError = true;
    //~ res->errMsg = allocateLiteral(res->arena, errMsg);
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
