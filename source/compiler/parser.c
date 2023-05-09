#include <stdbool.h>
#include <string.h>
#include "parser.h"
#include "parser.internal.h"

// The Parser. Consumes tokens and the string table from the Lexer. 
// Produces nodes and bindings.
// All lines that consume a token are marked with "// CONSUME"


extern jmp_buf excBuf; // defined in lexer.c

_Noreturn private void throwExc0(const char errMsg[], Parser* pr) {   
    
    pr->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d\n", pr->i);
#endif    
    pr->errMsg = str(errMsg, pr->a);
    longjmp(excBuf, 1);
}

#define throwExc(msg) throwExc0(msg, pr)

// Forward declarations
private bool parseLiteralOrIdentifier(Token tok, Parser* pr);
private void appendFnNode(FunctionCall fnCall, Arr(Token) tokens, Parser* pr);


Int createBinding(Binding b, Parser* pr) {
    pr->bindings[pr->bindNext] = b;
    pr->bindNext++;
    if (pr->bindNext == pr->bindCap) {
        Arr(Binding) newBindings = allocateOnArena(2*pr->bindCap*sizeof(Binding), pr->a);
        memcpy(newBindings, pr->bindings, pr->bindCap*sizeof(Binding));
        pr->bindings = newBindings;
        pr->bindNext++;
        pr->bindCap *= 2;
    }
    return pr->bindNext - 1;
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunk(Parser* pr) {
    Node* newStorage = allocateOnArena(pr->capacity*2*sizeof(Node), pr->a);
    memcpy(newStorage, pr->nodes, (pr->capacity)*(sizeof(Node)));
    pr->nodes = newStorage;
    pr->capacity *= 2;
}


void addNode(Node n, Parser* pr) {
    pr->nodes[pr->nextInd] = n;
    pr->nextInd++;
    if (pr->nextInd == pr->capacity) {
        handleFullChunk(pr);
    }
}

/**
 * Finds the top-level punctuation opener by its index, and sets its node length.
 * Called when the parsing of a span is finished.
 */
private void setSpanLength(Int nodeInd, Parser* pr) {
    pr->nodes[nodeInd].payload2 = pr->nextInd - nodeInd - 1;
}


private void parseVerbatim(Token tok, Parser* pr) {
    addNode((Node){.tp = tok.tp, .startByte = tok.startByte, .lenBytes = tok.lenBytes, 
        .payload1 = tok.payload1, .payload2 = tok.payload2}, pr);
}

private void parseErrorBareAtom(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void popFrame(Parser* pr) {    
    ParseFrame frame = pop(pr->backtrack);
    if (frame.tp == nodScope || frame.tp == nodExpr) {
        popScopeFrame(pr->activeBindings, pr->scopeStack);    
    }
    setSpanLength(frame.startNodeInd, pr);
}


/**
 * A single-item subexpression, like "(5)" or "x". Consumes no nodes.
 */
private void exprSingleItem(Token theTok, Parser* pr) {
    if (parseLiteralOrIdentifier(theTok, pr)) {
    } else if (theTok.tp == tokFuncWord) {
        Int mbBinding = pr->activeBindings[theTok.payload2];
        if (mbBinding < 0) {
            throwExc(errorUnknownFunction);
        }
        addNode((Node){.tp = nodFunc, .payload1 = mbBinding, .payload2 = 0,
                       .startByte = theTok.startByte, .lenBytes = theTok.lenBytes}, pr);        
    } else if (theTok.tp >= firstCoreFormTokenType) {
        throwExc(errorCoreFormInappropriate);
    } else {
        throwExc(errorUnexpectedToken);
    }
}

/**
 * Increments the arity of the top non-prefix operator. The prefix ones are ignored because there is no
 * need to track their arity: they are flushed on first operand or upon closing the subExpr following them.
 */
private void exprIncrementArity(ScopeStackFrame* topSubexpr, Parser* pr) {
    Int n = topSubexpr->length - 1;
    while (n > -1 && (topSubexpr->fnCalls[n].precedenceArity >> 16) == prefixPrec) {
        n--;
    }
    
    if (n < 0) {
        return;
    }
    FunctionCall* fnCall = topSubexpr->fnCalls + n;
    
    
    // The first bindings are always for the operators, and they have known arities, so no need to increment
    if (fnCall->bindingId < countOperators) {
        return;
    }

    fnCall->precedenceArity++;
    if (fnCall->bindingId <= countOperators) {
        OpDef operDefinition = (*pr->parDef->operators)[fnCall->bindingId];
        if ((fnCall->precedenceArity & LOWER16BITS) > operDefinition.arity) {            
            throwExc(errorOperatorWrongArity);
        }
    }
}

/**
 * Parses a subexpression within an expression.
 * Precondition: the current token must be 1 past the opening paren / statement token
 * Examples: "foo 5 a"
 *           "5 + !a"
 * 
 * Consumes 1 or 2 tokens
 * TODO: allow for core forms (but not scopes!)
 */
private void exprSubexpr(Int lenTokens, Arr(Token) tokens, Parser* pr) {
    exprIncrementArity(pr->scopeStack->topScope, pr);
    Token initTok = tokens[pr->i];
    pr->i++; // CONSUME the parens token      
    Token firstTok = tokens[pr->i];    
    
    if (lenTokens == 1) {
        exprSingleItem(tokens[pr->i], pr);
        pr->i++; // CONSUME the single item within parens
    } else {        
        push(((ParseFrame){.tp = nodExpr, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + lenTokens }), 
             pr->backtrack);        
        pushSubexpr(pr->scopeStack);
        addNode((Node){ .tp = nodExpr, .startByte = initTok.startByte, .lenBytes = initTok.lenBytes }, pr);
    }
}

/**
 * Flushes the finished subexpr frames from the top of the funcall stack.
 * A subexpr frame is finished iff current token equals its sentinel.
 * Flushing includes appending its operators, clearing the operator stack, and appending
 * prefix unaries from the previous subexpr frame, if any.
 */
private void subexprClose(Arr(Token) tokens, Parser* pr) {    
    ScopeStack* scStack = pr->scopeStack;
    while (scStack->length > 0) {
        ScopeStackFrame currSubexpr = *scStack->topScope;
        ParseFrame pFrame = peek(pr->backtrack);               

        
        if (pr->i != pFrame.sentinelToken || !currSubexpr.isSubexpr) {
            return;
        } else if (currSubexpr.length == 0) {
            throwExc(errorExpressionFunctionless);
        }
                
        for (int m = currSubexpr.length - 1; m > -1; m--) {
            appendFnNode(currSubexpr.fnCalls[m], tokens, pr);
        }
        popFrame(pr);        

        // flush parent's prefix opers, if any, because this subexp was their operand
        if (scStack->topScope->isSubexpr) {
            ScopeStackFrame parentFrame = *scStack->topScope;
            Int n = parentFrame.length - 1;
            while (n > -1 && (parentFrame.fnCalls[n].precedenceArity >> 16) == prefixPrec) {                
                appendFnNode(parentFrame.fnCalls[n], tokens, pr);
                n--;
            }
            parentFrame.length = n + 1;
        }
    }
}

/**
 * Appends a node that represents a function call in an expression.
 */
private void appendFnNode(FunctionCall fnCall, Arr(Token) tokens, Parser* pr) {
    Int arity = fnCall.precedenceArity & LOWER16BITS;
    if (fnCall.bindingId < countOperators) {
        // operators
        OpDef operDefinition = (*pr->parDef->operators)[fnCall.bindingId];
        if (operDefinition.arity != arity) {
            throwExc(errorOperatorWrongArity);
        }   
    }
    Token tok = tokens[fnCall.tokId];       
    addNode((Node){.tp = nodFunc, .payload1 = fnCall.bindingId, .payload2 = arity,
                    .startByte = tok.startByte, .lenBytes = tok.lenBytes }, pr);
}

/**
 * State modifier that must be called whenever an operand is encountered in an expression parse. Flueshes unaries.
 * Consumes no tokens.
 */
private void exprOperand(ScopeStackFrame* topSubexpr, Arr(Token) tokens, Parser* pr) {
    if (topSubexpr->length == 0) return;

    Int i = topSubexpr->length - 1;
    while (i > -1 && topSubexpr->fnCalls[i].precedenceArity >> 16 == prefixPrec) {
        
        appendFnNode(topSubexpr->fnCalls[i], tokens, pr);
        i--;
    }
    
    topSubexpr->length = i + 1;
    exprIncrementArity(topSubexpr, pr);
}

/** 
 * Handles function calls and infix operators in expressions.
 * Performs flushing of higher-precedence operators in accordance with the Shunting Yard algorithm.
 * Consumes no tokens.
 */
private void exprFnCall(Int bindingId, Token tok, ScopeStackFrame* topSubexpr, 
                        Arr(Token) tokens, Parser* pr) {
    Int n = topSubexpr->length - 1;
    while (n > 0) {
        FunctionCall fnCall = topSubexpr->fnCalls[n];
        Int precedence = fnCall.precedenceArity >> 16;
        if (precedence >= precedence) {
            break;
        } else if (precedence == prefixPrec) {
            throwExc(errorOperatorUsedInappropriately);
        }
        Token origTok = tokens[fnCall.tokId];
        addNode((Node){ .tp = nodFunc, .payload1 = fnCall.bindingId, .payload2 = fnCall.precedenceArity & LOWER16BITS,
                        .startByte = origTok.startByte, .lenBytes = origTok.lenBytes }, pr);
        n--;
    }
    topSubexpr->length = n + 1;
       
    Int precedence = (bindingId < countOperators) ? (*pr->parDef->operators)[bindingId].precedence : functionPrec;
    
    ParseFrame currFrame = peek(pr->backtrack);
    // Since function calls are infix, we need to check if the present subexpression has had its first operand already)
    Int initArity = ((peek(pr->backtrack)).startNodeInd + 1 == pr->nextInd) ? 0 : 1;
    
    addFnCall((FunctionCall){.bindingId = bindingId, .precedenceArity = (precedence << 16) + initArity, .tokId = pr->i}, 
                pr->scopeStack);
}


private void exprOperator(Token tok, ScopeStackFrame* topSubexpr, Arr(Token) tokens, Parser* pr) {
    Int bindingId = tok.payload1 >> 1; // TODO extended operators
    OpDef operDefinition = (*pr->parDef->operators)[bindingId];

    if (operDefinition.precedence == prefixPrec) {
        addFnCall((FunctionCall){.bindingId = bindingId, .precedenceArity = (prefixPrec << 16) + 1,
            .tokId = pr->i}, pr->scopeStack);        
    } else {
        exprFnCall(bindingId, tok, topSubexpr, tokens, pr);        
    }
}


private void parseExpr(Token tok, Arr(Token) tokens, Parser* pr) {
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodExpr, .startNodeInd = pr->nextInd, 
        .sentinelToken = pr->i + tok.payload2 };
    addNode((Node){ .tp = nodExpr, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);    
        
    push(newParseFrame, pr->backtrack);
    pushSubexpr(pr->scopeStack);
        
    while (pr->i < newParseFrame.sentinelToken) {
        subexprClose(tokens, pr);
        Token currTok = tokens[pr->i];
        untt tokType = currTok.tp;
        
        ScopeStackFrame* topSubexpr = pr->scopeStack->topScope;
        if (tokType == tokParens) {                
            exprSubexpr(tokens[pr->i].payload2, tokens, pr);
        } else if (tokType >= firstPunctuationTokenType) {
            throwExc(errorExpressionCannotContain);
        } else if (tokType <= topVerbatimTokenVariant) {
            addNode((Node){ .tp = currTok.tp, .payload1 = currTok.payload1, .payload2 = currTok.payload2,
                            .startByte = currTok.startByte, .lenBytes = currTok.lenBytes }, pr);
            exprOperand(topSubexpr, tokens, pr);
            pr->i++; // CONSUME the verbatim token
        } else {            
            if (tokType == tokWord) {
                Int mbBindingId = pr->activeBindings[currTok.payload2];
                if (mbBindingId < 0) {
                    throwExc(errorUnknownBinding);
                }
                addNode((Node){ .tp = nodId, .payload1 = mbBindingId, .payload2 = currTok.payload2, 
                                .startByte = currTok.startByte, .lenBytes = currTok.lenBytes}, pr);
                exprOperand(topSubexpr, tokens, pr);
            } else if (tokType == tokFuncWord) {
                Int mbBindingId = pr->activeBindings[currTok.payload2];
                if (mbBindingId < 0) {
                    throwExc(errorUnknownFunction);
                }
                exprFnCall(mbBindingId, tok, topSubexpr, tokens, pr);
            } else if (tokType == tokOperator) {
                exprOperator(tok, topSubexpr, tokens, pr);
            }
            pr->i++; // CONSUME any leaf token
        }
    }
    subexprClose(tokens, pr);    
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
private void maybeCloseSpans(Parser* pr) {    
    while (hasValues(pr->backtrack)) { // loop over subscopes and expressions inside FunctionDef
        ParseFrame frame = peek(pr->backtrack);

        if (pr->i < frame.sentinelToken) {
            return;
        } else if (pr->i > frame.sentinelToken) {
            throwExc(errorInconsistentSpan);
        }
        popFrame(pr);
    }
}

/** Parses top-level function bodies */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Parser* pr) {     
    while (pr->i < sentinelToken) {
        Token currTok = tokens[pr->i];
        untt contextType = peek(pr->backtrack).tp;
        pr->i++; // CONSUME any span token
        if (contextType >= firstResumableForm) {                
            ((*pr->parDef->resumableTable)[contextType - firstResumableForm])(
                contextType, currTok, tokens, pr
            );
        } else {
            ((*pr->parDef->nonResumableTable)[currTok.tp])(
                currTok, tokens, pr
            );
        }
        maybeCloseSpans(pr);
    }    
}

/** Consumes 0 tokens */
private void parseIdent(Token tok, Parser* pr) {
    Int nameId = tok.payload2;
    Int mbBinding = pr->activeBindings[nameId];
    if (mbBinding == 0) {
        throwExc(errorUnknownBinding);
    }
    addNode((Node){.tp = nodId, .payload1 = mbBinding, .payload2 = nameId,
                   .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);
}

/** Consumes 0 or 1 tokens. Returns false if didn't parse anything. */
private bool parseLiteralOrIdentifier(Token tok, Parser* pr) {
    if (tok.tp <= topVerbatimTokenVariant) {
        parseVerbatim(tok, pr);
    } else if (tok.tp == tokWord) {        
        parseIdent(tok, pr);
    } else {
        return false;        
    }
    pr->i++; // CONSUME the literal or ident token
    return true;
}


private void parseAssignment(Token tok, Arr(Token) tokens, Parser* pr) {
    const Int rightSideLen = tok.payload2 - 1;
    if (rightSideLen < 1) {
        throwExc(errorAssignment);
    }
    Int sentinelToken = pr->i + tok.payload2;   
        
    Token bindingToken = tokens[pr->i];
    if (bindingToken.tp != tokWord) {
        throwExc(errorAssignment);
    }
    Int mbBinding = pr->activeBindings[bindingToken.payload2];
    if (mbBinding > 0) {
        throwExc(errorAssignmentShadowing);
    }
    Int newBindingId = createBinding((Binding){.flavor = 0, .defStart = pr->i - 1, .defSentinel = sentinelToken}, pr);
    pr->activeBindings[bindingToken.payload2] = newBindingId;
    
    push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = pr->i - 1, .sentinelToken = sentinelToken }), 
            pr->backtrack);
    
    addNode((Node){.tp = nodAssignment, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);
    addNode((Node){.tp = nodBinding, .payload1 = newBindingId, 
            .startByte = bindingToken.startByte, .lenBytes = bindingToken.lenBytes}, pr);
    
    pr->i++; // CONSUME the word token before the assignment sign
    Token rightSideToken = tokens[pr->i];
    if (rightSideToken.tp == tokScope) {
        print("scope %d", rightSideToken.tp);
        //openScope(pr);
    } else {        
        if (rightSideLen == 1) {
            if (!parseLiteralOrIdentifier(rightSideToken, pr)) {               
                throwExc(errorAssignment);
            }
        } else if (rightSideToken.tp == tokOperator) {
            //parsePrefixFollowedByAtom(sentinelToken);
        } else {
            parseExpr((Token){ .payload2 = rightSideLen, .startByte = rightSideToken.startByte, 
                               .lenBytes = tok.lenBytes - rightSideToken.startByte + tok.startByte
                       }, 
                       tokens, pr);
        }
    }
}

private void parseReassignment(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void parseMutation(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void parseAlias(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void parseAssert(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseAssertDbg(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseAwait(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseBreak(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseCatch(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseContinue(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseDefer(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseDispose(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseElse(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseExposePrivates(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseFnDef(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void parseFuncExpr(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void parseInterface(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseImpl(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseLambda(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseLambda1(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseLambda2(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseLambda3(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parsePackage(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void parseReturn(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseStruct(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseTry(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void parseYield(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}



private void parseIf(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void ifResume(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void parseLoop(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void loopResume(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void resumeIf(untt tokType, Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeIfEq(untt tokType, Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeIfPr(untt tokType, Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeImpl(untt tokType, Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeMatch(untt tokType, Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeLoop(untt tokType, Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeMut(untt tokType, Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private ParserFunc (*tabulateNonresumableDispatch(Arena* a))[countNonresumableForms] {
    ParserFunc (*result)[countNonresumableForms] = allocateOnArena(countNonresumableForms*sizeof(ParserFunc), a);
    ParserFunc* p = *result;
    int i = 0;
    while (i <= firstPunctuationTokenType) {
        p[i] = &parseErrorBareAtom;
        i++;
    }
    p[tokScope] = &parseAlias;
    p[tokStmt] = &parseExpr;
    p[tokParens] = &parseErrorBareAtom;
    p[tokBrackets] = &parseErrorBareAtom;
    p[tokAccessor] = &parseErrorBareAtom;
    p[tokFuncExpr] = &parseFuncExpr;
    p[tokAssignment] = &parseAssignment;
    p[tokReassign] = &parseReassignment;
    p[tokMutation] = &parseMutation;
    p[tokElse] = &parseElse;
    p[tokSemicolon] = &parseAlias;
    
    p[tokAlias] = &parseAlias;
    p[tokAssert] = &parseAssert;
    p[tokAssertDbg] = &parseAssertDbg;
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
    p[tokIf    - firstResumableForm] = &resumeIf;
    p[tokIfEq  - firstResumableForm] = &resumeIfEq;
    p[tokIfPr  - firstResumableForm] = &resumeIfPr;
    p[tokImpl  - firstResumableForm] = &resumeImpl;
    p[tokMatch - firstResumableForm] = &resumeMatch;
    p[tokLoop  - firstResumableForm] = &resumeLoop;
    p[tokMut   - firstResumableForm] = &resumeMut;
    
    return result;
}

/*
* Definition of the operators, reserved words and lexer dispatch for the lexer.
*/
ParserDefinition* buildParserDefinitions(LanguageDefinition* langDef, Arena* a) {
    ParserDefinition* result = allocateOnArena(sizeof(ParserDefinition), a);
    result->resumableTable = tabulateResumableDispatch(a);    
    result->nonResumableTable = tabulateNonresumableDispatch(a);
    result->operators = langDef->operators;
    return result;
}


Parser* createParser(Lexer* lx, Arena* a) {
    if (lx->wasError) {
        Parser* result = allocateOnArena(sizeof(Parser), a);
        result->wasError = true;
        return result;
    }
    
    Parser* result = allocateOnArena(sizeof(Parser), a);
    Arena* aBt = mkArena();
    Int stringTableLength = lx->stringTable->length;
    (*result) = (Parser) {.text = lx->inp, .inp = lx, .parDef = buildParserDefinitions(lx->langDef, a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aBt), .i = 0,
        .stringTable = (Arr(Int))lx->stringTable->content, .strLength = stringTableLength,
        .bindings = allocateOnArena(sizeof(Binding)*64, a), .bindNext = 0, .bindCap = 64, // 0th binding is reserved
        .activeBindings = allocateOnArena(sizeof(Int)*stringTableLength, a),
        .nodes = allocateOnArena(sizeof(Node)*64, a), .nextInd = 0, .capacity = 64,
        .wasError = false, .errMsg = &empty, .a = a, .aBt = aBt
        };
    if (stringTableLength > 0) {
        memset(result->activeBindings, 0xFF, stringTableLength*sizeof(Int)); // activeBindings is filled with -1
    }
    // Bindings for the built-in operators. // TODO extensible operators
    for (int i = 0; i < countOperators; i++) {
        createBinding((Binding){ .flavor = bndCallable }, result);
    }

    return result;    
}


/** Parses top-level types but not functions and adds their bindings to the scope */
private void parseToplevelTypes(Lexer* lr, Parser* pr) {
}

/** Parses top-level constants but not functions, and adds their bindings to the scope */
private void parseToplevelConstants(Lexer* lx, Parser* pr) {
    lx->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokAssignment) {
            parseUpTo(pr->i + tok.payload2, lx->tokens, pr);
        } else {
            pr->i += (tok.payload2 + 1);
        }
    }    
}

/** Parses top-level function names and adds their bindings to the scope */
private void parseToplevelFunctionNames(Lexer* lr, Parser* pr) {
}

/** Parses top-level function bodies */
private void parseFunctionBodies(Lexer* lr, Parser* pr) {
    //~ if (setjmp(excBuf) == 0) {
        //~ while (pr->i < lr->totalTokens) {
            //~ currTok = (*lr->tokens)[i].tp;
            //~ contextTok = peek(pr->backtrack).tp;
            //~ if (contextTok >= firstResumableForm) {                
                //~ ((*dispatchResumable)[contextTok - firstResumableForm])(currTok, inp, pr);
            //~ } else {
                //~ ((*dispatch)[currTok])(text, inp, pr);
            //~ }
            //~ maybeCloseSpans(pr);
        //~ }
        //~ finalize(result);
    //~ }
    //~ pr->totalTokens = result->nextInd;
}

/** Parses a single file in 4 passes, see docs/parser.txt */
Parser* parse(Lexer* lx, Arena* a) {
    Parser* pr = createParser(lx, a);
    return parseWithParser(lx, pr, a);
}

Parser* parseWithParser(Lexer* lx, Parser* pr, Arena* a) {
    ParserDefinition* pDef = pr->parDef;
    int inpLength = lx->totalTokens;
    int i = 0;
    
    ParserFunc (*dispatch)[countNonresumableForms] = pDef->nonResumableTable;
    ResumeFunc (*dispatchResumable)[countResumableForms] = pDef->resumableTable;
    if (setjmp(excBuf) == 0) {
        parseToplevelTypes(lx, pr);    
        parseToplevelConstants(lx, pr);
        parseToplevelFunctionNames(lx, pr);
        parseFunctionBodies(lx, pr);
    }
    return pr;
}
