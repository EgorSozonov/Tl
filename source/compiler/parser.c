#include <stdbool.h>
#include "parser.h"
#include "parser.internal.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stringMap.h"
#include "../utils/structures/scopeStack.h"



jmp_buf excBuf;

_Noreturn private void throwExc0(const char errMsg[], Parser* pr) {   
    pr->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d\n", pr->i);
#endif    
    pr->errMsg = str(errMsg, pr->arena);
    longjmp(excBuf, 1);
}

#define throwExc(msg) throwExc0(msg, pr)


//typedef struct ScopeStack ScopeStack;

private void parseVerbatim(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseErrorBareAtom(Lexer* lr, Arr(byte) text, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseExpr(Lexer* lr, Parser* pr) {
    throwExc(errorTemp, pr);
}

private void parseAssignment(Lexer* lr, Parser* pr) {
    if (lenTokens < 2) {
        throwExc(errorAssignment);
    }
    Int sentinelToken = lr->currInd + lenTokens;
    untt fstTokenType = lr->tokens[lr->currInd].tp;
    if (fstTokenType != tokWord) {
        throwExc(errorAssignment);        
    }
    
    String bindingName = readString(tokWord);
    nextToken(lr);
    
    Int mbBinding = lookupBinding(bindingName, pr->stringMap);
    if (mbBinding > -1) {
        throwExc(errorAssignmentShadowing);
    }
    Int strId = addString(bindingName, pr);
    pr->bindings[newBinding] = strId;
    
    openAssignment();
    appendNode((Node){});
    
    untt tokType = currTokenType(lr);
    if (tokType == tokScope) {
        createScope();
    } else {
        if (remainingLen == 1) {
            parseLiteralOrIdentifier(tokType);
        } else if (tokType == tokOperator) {
            parsePrefixFollowedByAtom(sentinelToken);
        } else {
            createExpr(nodExpr, lenTokens - 1, startOfExpr, lenBytes - startOfExpr + startByte);
        }
    }
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

Parser* createParser(Lexer* lr, Arena* a) {
    Parser* result = allocateOnArena(sizeof(Parser), a);    
    Arena* aBt = mkArena();
    
    (*result) = (Parser) {.text = lr->inp, .inp = lr, .parDef = buildParserDefinitions(a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(aBt), .i = 0,
        .stringTable = (Arr(Int))lr->stringTable->content, .strLength = lr->stringTable->length,
        .bindings = allocateOnArena(sizeof(Binding)*64, a), .bindNext = 0, .bindCap = 64,
        .nodes = allocateOnArena(sizeof(Node)*64, a), .nextInd = 0, .capacity = 64,
        .wasError = false, .errMsg = &empty, .a = a, .aBt = aBt
        };    

    return result;    
}

private void maybeCloseSpans(Parser* pr) {
    while(hasValues(pr->currFnDef->backtrack)) {
    }
}

/** Parses top-level types but not functions and adds their bindings to the scope */
private void parseToplevelTypes(Lexer* lr, Parser* pr) {
}

/** Parses top-level constants but not functions and adds their bindings to the scope */
private void parseToplevelConstants(Lexer* lr, Parser* pr) {
    // walk over all tokens
    // when seeing something that is neither an immutable assignment nor a func definition, error out
    // skip the func definitions but parse the assignments
    
}

/** Parses top-level function names and adds their bindings to the scope */
private void parseToplevelFunctionNames(Lexer* lr, Parser* pr) {
}

/** Parses top-level function bodies */
private void parseFunctionBodies(Lexer* lr, Parser* pr) {
    if (setjmp(excBuf) == 0) {
        while (i < inpLength) {
            currTok = (*lr->tokens)[i].tp;
            contextTok = peek(pr->backtrack).tp;
            if (contextTok >= firstResumableForm) {                
                ((*dispatchResumable)[contextTok - firstResumableForm])(currTok, inp, pr);
            } else {
                ((*dispatch)[currTok])(text, inp, pr);
            }
            maybeCloseSpans(pr);
        }
        finalize(result);
    }
    pr->totalTokens = result->nextInd;
}

/** Parses a single file in 4 passes, see docs/parser.txt */
Parser* parse(Lexer* lr, ParserDefinition* pDef, Arena* a) {
    Parser* pr = createParser(lr, a);
    int inpLength = lr->totalTokens;
    int i = 0;
    
    ParserFunc (*dispatch)[countResumableForms] = pDef->nonresumableTable;
    ResumeFunc (*dispatchResumable)[countNonresumableForms] = pDef->resumableTable;
    
    parseToplevelTypes(lr, pr);    
    parseToplevelConstants(lr, pr);
    parseToplevelFunctionNames(lr, pr);
    parseFunctionBodies(lr, pr);
            
    return pr;
}
