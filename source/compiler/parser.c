#include "parser.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stringMap.h"
#include "../utils/structures/scopeStack.h"
#include <stdbool.h>


jmp_buf excBuf;

typedef struct ScopeStack ScopeStack;

private void parseVerbatim(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseErrorBareAtom(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseExpr(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseAssignment(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseReassignment(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseMutation(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseAlias(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseAssert(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseAssertDbg(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseAwait(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseBreak(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseCatch(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseContinue(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseDefer(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseDispose(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseExport(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseExposePrivates(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseFnDef(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseInterface(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseImpl(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseLambda(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseLambda1(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseLambda2(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseLambda3(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parsePackage(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseReturn(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseStruct(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseTry(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void parseYield(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}



private void parseIf(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void ifResume(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseLoop(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void loopResume(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


private void resumeIf(uint tokType, Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void resumeIfEq(uint tokType, Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void resumeIfPr(uint tokType, Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void resumeImpl(uint tokType, Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void resumeMatch(uint tokType, Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void resumeLoop(uint tokType, Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void resumeMut(uint tokType, Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}


_Noreturn private void throwExc(const char errMsg[], Parser* pr) {   
    pr->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d\n", pr->i);
#endif    
    pr->errMsg = allocLit(errMsg, pr->arena);
    longjmp(excBuf, 1);
}

private ParserFunc (*tabulateNonresumableDispatch(Arena* a))[countNonresumableForms] {
    ParserFunc (*result)[countNonresumableForms] = allocateOnArena(countNonresumableForms*sizeof(ParserFunc), a);
    ParserFunc* p = *result;
    int i = 0;
    while (i <= firstPunctuationTokenType) {
        p[i] = &parseErrorBareAtom;
        i++;
    }
    p[tokScope] = &parseScope;
    p[tokStmt] = &parseExpr;
    p[tokParens] = &parseErrorBareAtom;
    p[tokBrackets] = &parseErrorBareAtom;
    p[tokAccessor] = &parseErrorBareAtom;
    p[tokFuncExpr] = &parseFuncExpr;
    p[tokAssignment] = &parseAssignment;
    p[tokReassign] = &parseReassign;
    p[tokMutation] = &parseMutation;
    p[tokElse] = &parseElse;
    p[tokSemicolon] = &parseSemicolon;
    
    p[tokAlias] = &parseAlias;
    p[tokAssert] = &parseAssert;
    p[tokAssertDbg] = &tokAssertDbg;
    p[tokAwait] = &parseAlias;
    p[tokBreak] = &parseAlias;
    p[tokCatch] = &parseAlias;
    p[tokContinue] = &parseAlias;
    p[tokDefer] = &parseAlias;
    p[tokEmbed] = &parseAlias;
    p[tokExport] = &parseAlias;
    p[tokExposePriv] = &parseAlias;
    p[tokFnDef] = &parseAlias;
    p[tokIface] = &parseAlias;
    p[tokLambda] = &parseAlias;
    p[tokLambda1] = &parseAlias;
    p[tokLambda2] = &parseAlias;
    p[tokLambda3] = &parseAlias;
    p[tokPackage] = &parseAlias;
    p[tokReturn] = &parseAlias;
    p[tokStruct] = &parseAlias;
    p[tokTry] = &parseAlias;
    p[tokYield] = &parseAlias; 
    return result;
}

private ResumeFunc (*tabulateResumableDispatch(Arena* a))[countResumableForms] {
    ResumeFunc (*result)[countResumableForms] = allocateOnArena(countResumableForms*sizeof(ResumeFunc), a);
    ResumeFunc* p = *result;
    int i = 0;
    p[tokIf - firstResumableForm] = &resumeIf;
    p[tokIfEq - firstResumableForm] = &resumeIfEq;
    p[tokIfPr - firstResumableForm] = &resumeIfPr;
    p[tokImpl - firstResumableForm] = &resumeImpl;
    p[tokMatch - firstResumableForm] = &resumeMatch;
    p[tokLoop - firstResumableForm] = &resumeLoop;
    p[tokMut - firstResumableForm] = &resumeMut;
    
    return result;
}

/*
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
ParserDefinition* buildParserDefinitions(Arena* a) {
    ParserDefinition* result = allocateOnArena(sizeof(ParserDefinition), a);
    result->resumableTable = tabulateResumableDispatch(a);
    result->nonresumableTable = tabulateNonresumableDispatch(a);
    return result;
}

Parser* createParser(Lexer* inp, Arena* a) {
    Parser* result = allocateOnArena(sizeof(Parser), a);    

    result->parDef = buildParserDefinitions(a);    
    result->arena = a;
    result->inp = inp;
    result->inpLength = inp->length;    
    
    result->tokens = allocateOnArena(LEXER_INIT_SIZE*sizeof(Token), a);
    result->capacity = LEXER_INIT_SIZE;
        
    result->newlines = allocateOnArena(1000*sizeof(int), a);
    result->newlinesCapacity = 500;
    
    result->numeric = allocateOnArena(50*sizeof(int), a);
    result->numericCapacity = 50;

    
    result->backtrack = createStackRememberedToken(16, a);

    result->errMsg = &empty;

    return result;
}


Parser* parse(Lexer* lr, Arr(byte) text, ParserDefinition* pDef, Arena* a) {
    Parser* pr = createParser(lr, a);
    int inpLength = lr->totalTokens;
    int i = 0;
    ParserFunc (*dispatch)[countResumableForms] = pDef->nonresumableTable;
    ResumeFunc (*dispatchResumable)[countNonresumableForms] = pDef->resumableTable;
    if (setjmp(excBuf) == 0) {
        while (i < inpLength) {
            currTok = (*lr->tokens)[i].tp;
            contextTok = peek(pr->backtrack).tp;
            if (contextTok >= firstResumableForm) {                
                ((*dispatchResumable)[contextTok - firstResumableForm])(currTok, text, inp, pr);
            } else {
                ((*dispatch)[currTok])(text, inp, pr);
            }
        }
        finalize(result);
    }
    result->totalTokens = result->nextInd;
    return result;
}




/** Returns the non-negative binding id if this name is in scope, -1 otherwise.
 * If the name is found in the current function, the boolean param is set to false, otherwise to true.
 */
int searchForBinding(String* name, bool* foundInOuterFunction, ScopeStack* scSt) {
    
}
