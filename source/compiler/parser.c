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
private void addFnCall(FunctionCall fnCall, Arr(Token) tokens, Parser* pr);


/** Creates a new binding and adds it to the current scope */
Int createBinding(Int nameId, Binding b, Parser* pr) {
    pr->bindings[pr->bindNext] = b;
    pr->bindNext++;
    if (pr->bindNext == pr->bindCap) {
        Arr(Binding) newBindings = allocateOnArena(2*pr->bindCap*sizeof(Binding), pr->a);
        memcpy(newBindings, pr->bindings, pr->bindCap*sizeof(Binding));
        pr->bindings = newBindings;
        pr->bindNext++;
        pr->bindCap *= 2;
    }
    Int newBindingId = pr->bindNext - 1;
    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (pr->scopeStack->length > 0) {
            addBinding(nameId, newBindingId, pr->activeBindings, pr->scopeStack); // adds it to the ScopeStack
        }
        pr->activeBindings[nameId] = newBindingId; // makes it active
    }
    
    return newBindingId;
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


private ParseFrame popFrame(Parser* pr) {    
    ParseFrame frame = pop(pr->backtrack);
    if (frame.tp == nodScope || frame.tp == nodExpr) {
        popScopeFrame(pr->activeBindings, pr->scopeStack);    
    }
    setSpanLength(frame.startNodeInd, pr);
    return frame;
}

/**
 * A single-item subexpression, like "(foo)" or "(!)". Consumes no tokens.
 */
private void exprSingleItem(Token theTok, Parser* pr) {
    if (theTok.tp == tokWord) {
        Int mbBinding = pr->activeBindings[theTok.payload2];
        VALIDATE(mbBinding > -1, errorUnknownFunction)
        addNode((Node){.tp = nodCall, .payload1 = mbBinding, .payload2 = 0,
                       .startByte = theTok.startByte, .lenBytes = theTok.lenBytes}, pr);        
    } else if (theTok.tp == tokOperator) {
        Int operBindingId = theTok.payload1 >> 1; // TODO extended operators
        OpDef operDefinition = (*pr->parDef->operators)[operBindingId];
        if (operDefinition.arity == 1) {
            addNode((Node){ .tp = nodId, .payload1 = operBindingId,
                .startByte = theTok.startByte, .lenBytes = theTok.lenBytes}, pr);
        } else {
            throwExc(errorUnexpectedToken);
        }
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
    while (n > -1 && topSubexpr->fnCalls[n].isUnary) {
        n--;
    }    
    if (n < 0) {
        return;
    }
    
    FunctionCall* fnCall = topSubexpr->fnCalls + n;
    fnCall->arity++;
    
    if (fnCall->bindingId <= countOperators) {
        OpDef operDefinition = (*pr->parDef->operators)[fnCall->bindingId];
        VALIDATE((fnCall->arity & LOWER16BITS) <= operDefinition.arity, errorOperatorWrongArity)
    }
}

/**
 * Parses a subexpression within an expression.
 * Precondition: the current token must be 1 past the opening paren / statement token
 * Examples: "foo 5 a"
 *           "+ 5 !a"
 * 
 * Consumes 1 or 2 tokens
 * TODO: allow for core forms (but not scopes!)
 */
private void exprSubexpr(Token parenTok, Arr(Token) tokens, Parser* pr) {
    exprIncrementArity(pr->scopeStack->topScope, pr);
    Token firstTok = tokens[pr->i];    
    
    if (parenTok.payload2 == 1) {
        exprSingleItem(tokens[pr->i], pr);
        pr->i++; // CONSUME the single item within parens
    } else {
        push(((ParseFrame){.tp = nodExpr, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + parenTok.payload2 }), 
             pr->backtrack);        
        pushSubexpr(pr->scopeStack);
        addNode((Node){ .tp = nodExpr, .startByte = parenTok.startByte, .lenBytes = parenTok.lenBytes }, pr);
        if (firstTok.tp == tokWord || firstTok.tp == tokOperator || firstTok.tp == tokAnd || firstTok.tp == tokOr) {
            Int mbBindingId = -1;
            if (firstTok.tp == tokWord) {
                mbBindingId = pr->activeBindings[firstTok.payload2];
            } else if (firstTok.tp == tokOperator) {
                mbBindingId = firstTok.payload1 >> 1;
            }
            VALIDATE(mbBindingId > -1, errorUnknownFunction)

            pushFnCall((FunctionCall){.bindingId = mbBindingId, .arity = 0, .tokId = pr->i}, pr->scopeStack);
            pr->i++; // CONSUME the function or operator call token
        }
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
        }// else VALIDATE(currSubexpr.length > 0, errorExpressionFunctionless)
        for (int m = currSubexpr.length - 1; m > -1; m--) {
            addFnCall(currSubexpr.fnCalls[m], tokens, pr);
        }
        popFrame(pr);        

        // flush parent's prefix opers, if any, because this subexp was their operand
        if (scStack->topScope->isSubexpr) {
            ScopeStackFrame parentFrame = *scStack->topScope;
            Int n = parentFrame.length - 1;
            while (n > -1 && parentFrame.fnCalls[n].isUnary) {
                addFnCall(parentFrame.fnCalls[n], tokens, pr);
                n--;
            }
            parentFrame.length = n + 1;
        }
    }
}

/**
 * Appends a function call from the stack to AST.
 */
private void addFnCall(FunctionCall fnCall, Arr(Token) tokens, Parser* pr) {
    // operators
    if (fnCall.bindingId < countOperators) {        
        OpDef operDefinition = (*pr->parDef->operators)[fnCall.bindingId];
        VALIDATE(operDefinition.arity == fnCall.arity, errorOperatorWrongArity)
    }
    Token tok = tokens[fnCall.tokId];       
    addNode((Node){.tp = nodCall, .payload1 = fnCall.bindingId, .payload2 = fnCall.arity,
                    .startByte = tok.startByte, .lenBytes = tok.lenBytes }, pr);
}

/**
 * State modifier that must be called whenever an operand is encountered in an expression parse. Flueshes unaries.
 * Consumes no tokens.
 */
private void exprOperand(ScopeStackFrame* topSubexpr, Arr(Token) tokens, Parser* pr) {
    if (topSubexpr->length == 0) return;

    Int i = topSubexpr->length - 1;
    // write all the prefix unaries
    while (i > -1 && topSubexpr->fnCalls[i].isUnary) {
        addFnCall(topSubexpr->fnCalls[i], tokens, pr);
        i--;
    }
    
    topSubexpr->length = i + 1;    
    exprIncrementArity(topSubexpr, pr);
}

/** An operator token in non-initial position is either a funcall (if unary) or an operand. Consumes no tokens. */
private void exprOperator(Token tok, ScopeStackFrame* topSubexpr, Arr(Token) tokens, Parser* pr) {
    Int bindingId = tok.payload1 >> 1; // TODO extended operators
    OpDef operDefinition = (*pr->parDef->operators)[bindingId];

    if (operDefinition.arity == 1) {
        pushFnCall((FunctionCall){.bindingId = bindingId, .arity = 1, .tokId = pr->i, .isUnary = true}, pr->scopeStack);
    } else {
        addNode((Node){ .tp = nodId, .payload1 = bindingId, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);
        exprOperand(topSubexpr, tokens, pr);
    }
}

/** Parses an expression. Precondition: we are 1 past the span token */
private void parseExpr(Token exprTok, Arr(Token) tokens, Parser* pr) {
    Int sentinelToken = pr->i + exprTok.payload2;
    exprSubexpr(exprTok, tokens, pr);
        
    while (pr->i < sentinelToken) {
        subexprClose(tokens, pr);
        Token currTok = tokens[pr->i];
        untt tokType = currTok.tp;
        
        ScopeStackFrame* topSubexpr = pr->scopeStack->topScope;
        if (tokType == tokParens) {
            pr->i++; // CONSUME the parens token
            exprSubexpr(currTok, tokens, pr);
        } else VALIDATE(tokType < firstPunctuationTokenType, errorExpressionCannotContain)
        else if (tokType <= topVerbatimTokenVariant) {
            addNode((Node){ .tp = currTok.tp, .payload1 = currTok.payload1, .payload2 = currTok.payload2,
                            .startByte = currTok.startByte, .lenBytes = currTok.lenBytes }, pr);
            exprOperand(topSubexpr, tokens, pr);
            pr->i++; // CONSUME the verbatim token
        } else {
            if (tokType == tokWord) {
                Int mbBindingId = pr->activeBindings[currTok.payload2];

                if (peek(pr->backtrack).startNodeInd == pr->nextInd - 1) {
                    VALIDATE(mbBindingId > -1, errorUnknownFunction)
                    pushFnCall((FunctionCall){.bindingId = mbBindingId, .tokId = pr->i}, pr->scopeStack);
                } else {
                    VALIDATE(mbBindingId > -1, errorUnknownBinding)
                    addNode((Node){ .tp = nodId, .payload1 = mbBindingId, .payload2 = currTok.payload2, 
                                .startByte = currTok.startByte, .lenBytes = currTok.lenBytes}, pr);
                    exprOperand(topSubexpr, tokens, pr);
                }
            } else if (tokType == tokOperator) {
                exprOperator(currTok, topSubexpr, tokens, pr);
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
        } else VALIDATE(pr->i == frame.sentinelToken, errorInconsistentSpan)
        print("poppin frame i = %d tp = %d frame.sentinelToken %d", pr->i, frame.tp, frame.sentinelToken)
        popFrame(pr);
    }
}

/** Parses top-level function bodies */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Parser* pr) {
    while (pr->i < sentinelToken) {
        Token currTok = tokens[pr->i];
        untt contextType = peek(pr->backtrack).tp;
        pr->i++; // CONSUME any span token
        // pre-parse hooks that let contextful syntax forms (e.g. "if") detect parsing errors and keep track of their state
        if (contextType >= firstResumableForm) {
            print("before hook i = %d currTok %d contextType %d", pr->i, currTok.tp, contextType)
            ((*pr->parDef->resumableTable)[contextType - firstResumableForm])(
                contextType, &currTok, tokens, pr
            );
        }
        print("i = %d currTok %d", pr->i, currTok.tp)
        ((*pr->parDef->nonResumableTable)[currTok.tp])(
            currTok, tokens, pr
        );
        
        maybeCloseSpans(pr);
    }    
}

/** Consumes 0 tokens */
private void parseIdent(Token tok, Parser* pr) {
    Int nameId = tok.payload2;
    Int mbBinding = pr->activeBindings[nameId];
    VALIDATE(mbBinding > -1, errorUnknownBinding)    
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
    VALIDATE(bindingToken.tp == tokWord, errorAssignment)

    Int mbBinding = pr->activeBindings[bindingToken.payload2];
    VALIDATE(mbBinding == -1, errorAssignmentShadowing)

    Int newBindingId = createBinding(
     bindingToken.payload2, (Binding){.flavor = bndImmut, .defStart = pr->i - 1, .defSentinel = sentinelToken}, pr
    );
                                         
    push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
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
            if (parseLiteralOrIdentifier(rightSideToken, pr) == false) {
                throwExc(errorAssignment);
            }
        } else if (rightSideToken.tp == tokIf) {
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
    Int lenTokens = tok.payload2;
    Int sentinelToken = pr->i + lenTokens;        
    
    push(((ParseFrame){ .tp = nodReturn, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
            pr->backtrack);
    addNode((Node){.tp = nodReturn, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);
    
    Token rightSideToken = tokens[pr->i];
    if (rightSideToken.tp == tokScope) {
        print("scope %d", rightSideToken.tp);
        //openScope(pr);
    } else {        
        if (lenTokens == 1) {
            if (parseLiteralOrIdentifier(rightSideToken, pr) == false) {               
                throwExc(errorReturn);
            }
        } else {
            parseExpr((Token){ .payload2 = lenTokens, .startByte = rightSideToken.startByte, 
                               .lenBytes = tok.lenBytes - rightSideToken.startByte + tok.startByte
                       }, 
                       tokens, pr);
        }
    }
}


private void parseScope(Token tok, Arr(Token) tokens, Parser* pr) {
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
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = pr->nextInd, 
        .sentinelToken = pr->i + tok.payload2 };
    push(newParseFrame, pr->backtrack);
    addNode((Node){.tp = nodIf, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);

    print("clause sentinel %d", pr->i + tok.payload2)
    ParseFrame clauseFrame = (ParseFrame){ .tp = nodIfClause, .startNodeInd = pr->nextInd, 
        .sentinelToken = pr->i + tok.payload2 };
    push(clauseFrame, pr->backtrack);
    addNode((Node){.tp = nodIfClause, .startByte = tok.startByte }, pr);
}


private void parseLoop(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void loopResume(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private void resumeIf(untt tokType, Token* tok, Arr(Token) tokens, Parser* pr) {
    Int clauseInd = (*pr->backtrack->content)[pr->backtrack->length - 1].clauseInd;
    print("resume clauseInd %d pr->i %d", clauseInd, pr->i)
    if (clauseInd > 0 && clauseInd % 2 == 0) {
        if (peek(pr->backtrack).tp == nodIfClause) {
            ParseFrame oldFrame = popFrame(pr);
            print("oldFrame %d sentinel %d", oldFrame.tp, oldFrame.sentinelToken)
            ParseFrame clauseFrame = (ParseFrame){ .tp = nodIfClause, .startNodeInd = pr->nextInd, 
                .sentinelToken = pr->i + tok->payload2 };
            push(clauseFrame, pr->backtrack);
            addNode((Node){.tp = nodIfClause, .startByte = tok->startByte }, pr);
        }
        
    } else {
        *tok = tokens[pr->i]; // get the token after the arrow
        pr->i++; // CONSUME whichever token is after the arrow
    }
    (*pr->backtrack->content)[pr->backtrack->length - 1].clauseInd = clauseInd + 1;
}

private void resumeIfPr(untt tokType, Token* tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeImpl(untt tokType, Token* tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeMatch(untt tokType, Token* tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeLoop(untt tokType, Token* tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


private ParserFunc (*tabulateNonresumableDispatch(Arena* a))[countSyntaxForms] {
    ParserFunc (*result)[countSyntaxForms] = allocateOnArena(countSyntaxForms*sizeof(ParserFunc), a);
    ParserFunc* p = *result;
    int i = 0;
    while (i <= firstPunctuationTokenType) {
        p[i] = &parseErrorBareAtom;
        i++;
    }
    p[tokScope]      = &parseScope;
    p[tokStmt]       = &parseExpr;
    p[tokParens]     = &parseErrorBareAtom;
    p[tokAccessor]   = &parseErrorBareAtom;
    p[tokAssignment] = &parseAssignment;
    p[tokReassign]   = &parseReassignment;
    p[tokMutation]   = &parseMutation;
    p[tokElse]       = &parseElse;
    
    p[tokAlias]      = &parseAlias;
    p[tokAssert]     = &parseAssert;
    p[tokAssertDbg]  = &parseAssertDbg;
    p[tokAwait]      = &parseAlias;
    p[tokBreak]      = &parseAlias;
    p[tokCatch]      = &parseAlias;
    p[tokContinue]   = &parseAlias;
    p[tokDefer]      = &parseAlias;
    p[tokEmbed]      = &parseAlias;
    p[tokExport]     = &parseAlias;
    p[tokExposePriv] = &parseAlias;
    p[tokFnDef]      = &parseAlias;
    p[tokIface]      = &parseAlias;
    p[tokLambda]     = &parseAlias;
    p[tokPackage]    = &parseAlias;
    p[tokReturn]     = &parseReturn;
    p[tokStruct]     = &parseAlias;
    p[tokTry]        = &parseAlias;
    p[tokYield]      = &parseAlias;

    p[tokIf]         = &parseIf;
    p[tokLoop]       = &parseLoop;
    return result;
}
 

private ResumeFunc (*tabulateResumableDispatch(Arena* a))[countResumableForms] {
    ResumeFunc (*result)[countResumableForms] = allocateOnArena(countResumableForms*sizeof(ResumeFunc), a);
    ResumeFunc* p = *result;
    int i = 0;
    p[nodIfClause - firstResumableForm] = &resumeIf;
    p[nodIfPr  - firstResumableForm] = &resumeIfPr;
    p[nodImpl  - firstResumableForm] = &resumeImpl;
    p[nodMatch - firstResumableForm] = &resumeMatch;
    p[nodLoop  - firstResumableForm] = &resumeLoop;
    
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


void importBindings(Arr(BindingImport) bindings, Int countBindings, Parser* pr) {   
    for (int i = 0; i < countBindings; i++) {
        Int mbNameId = getStringStore(pr->text->content, bindings[i].name, pr->stringTable, pr->stringStore);
        if (mbNameId > -1) {           
            Int newBindingId = createBinding(mbNameId, bindings[i].binding, pr);
        }
    }
}

/* Bindings for the built-in operators, types, functions. */
// TODO extensible operators
void insertBuiltinBindings(LanguageDefinition* langDef, Parser* pr) {
    for (int i = 0; i < countOperators; i++) {
        createBinding(-1, (Binding){ .flavor = bndCallable}, pr);
    }    
    
    BindingImport builtins[4] =  {
        (BindingImport) { .name = str("Int", pr->a),    .binding = (Binding){ .flavor = bndType} },
        (BindingImport) { .name = str("Float", pr->a),    .binding = (Binding){ .flavor = bndType} },
        (BindingImport) { .name = str("String", pr->a), .binding = (Binding){ .flavor = bndType} },
        (BindingImport) { .name = str("Bool", pr->a),   .binding = (Binding){ .flavor = bndType} }
    };
    importBindings(builtins, sizeof(builtins)/sizeof(BindingImport), pr);
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
    (*result) = (Parser) {
        .text = lx->inp, .inp = lx, .parDef = buildParserDefinitions(lx->langDef, a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aBt), .i = 0,
        .stringStore = lx->stringStore,
        .stringTable = lx->stringTable, .strLength = stringTableLength,
        .bindings = allocateOnArena(sizeof(Binding)*64, a), .bindNext = 0, .bindCap = 64, // 0th binding is reserved
        .activeBindings = allocateOnArena(sizeof(Int)*stringTableLength, a),
        .nodes = allocateOnArena(sizeof(Node)*64, a), .nextInd = 0, .capacity = 64,
        .wasError = false, .errMsg = &empty, .a = a, .aBt = aBt
    };
    if (stringTableLength > 0) {
        memset(result->activeBindings, 0xFF, stringTableLength*sizeof(Int)); // activeBindings is filled with -1
    }
    
    insertBuiltinBindings(lx->langDef, result);

    return result;    
}


/** Parses top-level types but not functions and adds their bindings to the scope */
private void parseToplevelTypes(Lexer* lr, Parser* pr) {
}

/** Parses top-level constants but not functions, and adds their bindings to the scope */
private void parseToplevelConstants(Lexer* lx, Parser* pr) {
    pr->i = 0;
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
private void parseToplevelFunctionNames(Lexer* lx, Parser* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.payload2;
            if (lenTokens < 3) {
                throwExc(errorFnNameAndParams);
            }
            Token fnName = lx->tokens[(pr->i) + 2]; // + 2 because we skip over the "fn" and "stmt" span tokens
            if (fnName.tp != tokWord || fnName.payload1 > 0) { // function name must be a lowercase word
                throwExc(errorFnNameAndParams);
            }
            Int newBinding = createBinding(fnName.payload2, (Binding){ .flavor = bndCallable }, pr);
        } 
        pr->i += (tok.payload2 + 1);        
    } 
}

/** 
 * Parses a top-level function signature.
 * The result is [FnDef BindingName Scope BindingParam1 BindingParam2 ... ]
 */
private void parseFnSignature(Token fnDef, Lexer* lx, Parser* pr) {
    Int fnSentinel = pr->i + fnDef.payload2 - 1;
    Int byteSentinel = fnDef.startByte + fnDef.lenBytes;
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodFnDef, .startNodeInd = pr->nextInd, 
        .sentinelToken = fnSentinel };
    Token fnName = lx->tokens[pr->i];
    pr->i++; // CONSUME the function name token

    // the function's return type, it's optional
    Int fnReturnTypeId = 0;
    if (lx->tokens[pr->i].tp == tokWord) {
        Token fnReturnType = lx->tokens[pr->i];

        VALIDATE(fnReturnType.payload1 > 0, errorFnNameAndParams) 
            else VALIDATE(pr->activeBindings[fnReturnType.payload2] > -1, errorUnknownType)
        
        fnReturnTypeId = pr->activeBindings[fnReturnType.payload2];
        
        pr->i++; // CONSUME the function return type token
    }

    // the fnDef scope & node
    push(newParseFrame, pr->backtrack);
    addNode((Node){.tp = nodFnDef, .payload1 = pr->activeBindings[fnName.payload2],
                        .startByte = fnDef.startByte, .lenBytes = fnDef.lenBytes} , pr);
    addNode((Node){ .tp = nodBinding, .payload1 = pr->activeBindings[fnName.payload2], 
            .startByte = fnName.startByte, .lenBytes = fnName.lenBytes}, pr);

    // the scope for the function body
    VALIDATE(lx->tokens[pr->i].tp == tokParens, errorFnNameAndParams)
    push(((ParseFrame){ .tp = nodScope, .startNodeInd = pr->nextInd, 
        .sentinelToken = fnSentinel }), pr->backtrack);        
    pushScope(pr->scopeStack);
    Token parens = lx->tokens[pr->i];
    addNode((Node){ .tp = nodScope, 
                    .startByte = parens.startByte, .lenBytes = byteSentinel - parens.startByte }, pr);           
    
    Int paramsSentinel = pr->i + parens.payload2 + 1;
    pr->i++; // CONSUME the parens token for the param list            
    
    while (pr->i < paramsSentinel) {
        Token paramName = lx->tokens[pr->i];
        VALIDATE(paramName.tp == tokWord && paramName.payload1 == 0, errorFnNameAndParams)
        Int newBindingId = createBinding(paramName.payload2, (Binding){.flavor = bndImmut}, pr);
        Node paramNode = (Node){.tp = nodBinding, .payload1 = newBindingId, 
                                .startByte = paramName.startByte, .lenBytes = paramName.lenBytes };
        pr->i++; // CONSUME a param name
        
        VALIDATE(pr->i < paramsSentinel, errorFnNameAndParams)
        
        Token paramType = lx->tokens[pr->i];
        if (paramType.payload1 < 1) {
            throwExc(errorFnNameAndParams);
        }
        Int typeBindingId = pr->activeBindings[paramType.payload2]; // the binding of this parameter's type
        VALIDATE(typeBindingId > -1, errorUnknownType)        
        pr->bindings[newBindingId].typeId = typeBindingId;
        pr->i++; // CONSUME the param's type name
        
        addNode(paramNode, pr);        
    }
    
    VALIDATE(pr->i < fnSentinel && lx->tokens[pr->i].tp >= firstPunctuationTokenType, errorFnMissingBody)
}


private void parseFnBody(Int sentinelToken, Arr(Token) inp, Parser* pr) {
    print("parse fn body")
    ParserFunc (*dispatch)[countSyntaxForms] = pr->parDef->nonResumableTable;
    ResumeFunc (*dispatchResumable)[countResumableForms] = pr->parDef->resumableTable;
    if (setjmp(excBuf) == 0) {
        while (pr->i < sentinelToken) {
            Token currTok = inp[pr->i];
            Int contextTokTp = peek(pr->backtrack).tp;
            if (contextTokTp >= firstResumableForm) {      
                ((*dispatchResumable)[contextTokTp - firstResumableForm])(contextTokTp, &currTok, inp, pr);
            } else {
                ((*dispatch)[currTok.tp])(currTok, inp, pr);
            }
            maybeCloseSpans(pr);
        }
        //finalize(result);
    }
}

/** Parses top-level function params and bodies */
private void parseFunctionBodies(Lexer* lx, Parser* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];        
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.payload2;
            Int sentinelToken = pr->i + lenTokens + 1;
            if (lenTokens < 2) {
                throwExc(errorFnNameAndParams);
            }
            pr->i += 2; // CONSUME the function def token and the stmt token
            
            parseFnSignature(tok, lx, pr);
            parseUpTo(sentinelToken, lx->tokens, pr);
            //parseFnBody(sentinelToken, lx->tokens, pr);
        }
        pr->i += (tok.payload2 + 1);    // CONSUME the whole function definition    
    }
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

    ParserFunc (*dispatch)[countSyntaxForms] = pDef->nonResumableTable;
    ResumeFunc (*dispatchResumable)[countResumableForms] = pDef->resumableTable;
    if (setjmp(excBuf) == 0) {
        parseToplevelTypes(lx, pr);    
        parseToplevelConstants(lx, pr);
        parseToplevelFunctionNames(lx, pr);
        parseFunctionBodies(lx, pr);
    }
    return pr;
}
