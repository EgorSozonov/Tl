#include <stdbool.h>
#include <string.h>
#include "compiler.h"
#include "compiler.internal.h"

// The Compiler. Consumes tokens and the string table from the Lexer. 
// Produces nodes and bindings.
// All lines that consume a token are marked with "// CONSUME"


extern jmp_buf excBuf; // defined in lexer.c

_Noreturn private void throwExc0(const char errMsg[], Compiler* pr) {   
    
    pr->wasError = true;
#ifdef TRACE    
    printf("Error on i = %d\n", pr->i);
#endif    
    pr->errMsg = str(errMsg, pr->a);
    longjmp(excBuf, 1);
}

#define throwExc(msg) throwExc0(msg, pr)

// Forward declarations
private bool parseLiteralOrIdentifier(Token tok, Compiler* pr);


/** Validates a new binding (that it is unique), creates an entity for it, and adds it to the current scope */
Int createBinding(Token bindingToken, Entity b, Compiler* pr) {
    VALIDATE(bindingToken.tp == tokWord, errorAssignment)

    Int nameId = bindingToken.pl2;
    Int mbBinding = pr->activeBindings[nameId];
    VALIDATE(mbBinding == -1, errorAssignmentShadowing)
    
    pr->entities[pr->entNext] = b;
    Int newBindingId = pr->entNext;
    pr->entNext++;
    if (pr->entNext == pr->entCap) {
        Arr(Entity) newBindings = allocateOnArena(2*pr->entCap*sizeof(Entity), pr->a);
        memcpy(newBindings, pr->entities, pr->entCap*sizeof(Entity));
        pr->entities = newBindings;
        pr->entCap *= 2;
    }    
    
    if (nameId > -1) { // nameId == -1 only for the built-in operators
        if (pr->scopeStack->length > 0) {
            addBinding(nameId, newBindingId, pr->activeBindings, pr->scopeStack); // adds it to the ScopeStack
        }
        pr->activeBindings[nameId] = newBindingId; // makes it active
    }
    
    return newBindingId;
}

/** Processes the name of a defined function. Creates an overload counter, or increments it if it exists. Consumes no tokens. */
void encounterFnDefinition(Int nameId, Compiler* pr) {    
    Int activeValue = (nameId > -1) ? pr->activeBindings[nameId] : -1;
    VALIDATE(activeValue < 0, errorAssignmentShadowing);
    if (activeValue == -1) {
        pr->overloadCounts[pr->overlCNext] = 1;
        pr->activeBindings[nameId] = -pr->overlCNext - 2;
        pr->overlCNext++;
        if (pr->overlCNext == pr->overlCCap) {
            Arr(Int) newOverloads = allocateOnArena(2*pr->overlCCap*4, pr->a);
            memcpy(newOverloads, pr->overloadCounts, pr->overlCCap*4);
            pr->overloadCounts = newOverloads;
            pr->overlCCap *= 2;
        }
    } else {
        pr->overloadCounts[-activeValue - 2]++;
    }
}


Int importEntity(Int nameId, Entity b, Compiler* pr) {
    if (nameId > -1) {
        Int mbBinding = pr->activeBindings[nameId];
        VALIDATE(mbBinding == -1, errorAssignmentShadowing)
    }

    pr->entities[pr->entNext] = b;
    Int newBindingId = pr->entNext;
    pr->entNext++;
    if (pr->entNext == pr->entCap) {
        Arr(Entity) newBindings = allocateOnArena(2*pr->entCap*sizeof(Entity), pr->a);
        memcpy(newBindings, pr->entities, pr->entCap*sizeof(Entity));
        pr->entities = newBindings;
        pr->entCap *= 2;
    }    
    
    if (pr->scopeStack->length > 0) {
        // adds it to the ScopeStack so it will be cleaned up when the scope is over
        addBinding(nameId, newBindingId, pr->activeBindings, pr->scopeStack); 
    }
    pr->activeBindings[nameId] = newBindingId; // makes it active
           
    return newBindingId;
}

/** The current chunk is full, so we move to the next one and, if needed, reallocate to increase the capacity for the next one */
private void handleFullChunk(Compiler* pr) {
    Node* newStorage = allocateOnArena(pr->capacity*2*sizeof(Node), pr->a);
    memcpy(newStorage, pr->nodes, (pr->capacity)*(sizeof(Node)));
    pr->nodes = newStorage;
    pr->capacity *= 2;
}


void addNode(Node n, Compiler* pr) {
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
private void setSpanLength(Int nodeInd, Compiler* pr) {
    pr->nodes[nodeInd].pl2 = pr->nextInd - nodeInd - 1;
}


private void parseVerbatim(Token tok, Compiler* pr) {
    addNode((Node){.tp = tok.tp, .startBt = tok.startBt, .lenBts = tok.lenBts, 
        .pl1 = tok.pl1, .pl2 = tok.pl2}, pr);
}

private void parseErrorBareAtom(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private ParseFrame popFrame(Compiler* pr) {    
    ParseFrame frame = pop(pr->backtrack);
    if (frame.tp == nodScope) {
        popScopeFrame(pr->activeBindings, pr->scopeStack);
    }
    setSpanLength(frame.startNodeInd, pr);
    return frame;
}

/** Calculates the sentinel token for a token at a specific index */
private Int calcSentinel(Token tok, Int tokInd) {
    return (tok.tp >= firstPunctuationTokenType ? (tokInd + tok.pl2 + 1) : (tokInd + 1));
}

/**
 * A single-item subexpression, like "(foo)" or "(!)". Consumes no tokens.
 */
private void exprSingleItem(Token theTok, Compiler* pr) {
    if (theTok.tp == tokWord) {
        Int mbOverload = pr->activeBindings[theTok.pl2];
        VALIDATE(mbOverload != -1, errorUnknownFunction)
        addNode((Node){.tp = nodCall, .pl1 = mbOverload, .pl2 = 0,
                       .startBt = theTok.startBt, .lenBts = theTok.lenBts}, pr);        
    } else if (theTok.tp == tokOperator) {
        Int operBindingId = theTok.pl1;
        OpDef operDefinition = (*pr->parDef->operators)[operBindingId];
        if (operDefinition.arity == 1) {
            addNode((Node){ .tp = nodId, .pl1 = operBindingId,
                .startBt = theTok.startBt, .lenBts = theTok.lenBts}, pr);
        } else {
            throwExc(errorUnexpectedToken);
        }
    } else if (theTok.tp <= topVerbatimTokenVariant) {
        addNode((Node){.tp = theTok.tp, .pl1 = theTok.pl1, .pl2 = theTok.pl2,
                        .startBt = theTok.startBt, .lenBts = theTok.lenBts }, pr);
    } else if (theTok.tp >= firstCoreFormTokenType) {
        throwExc(errorCoreFormInappropriate);
    } else {
        throwExc(errorUnexpectedToken);
    }
}

/** Counts the arity of the call, including skipping unary operators. Consumes no tokens. */
void exprCountArity(Int* arity, Int sentinelToken, Arr(Token) tokens, Compiler* pr) {
    Int j = pr->i;
    Token firstTok = tokens[j];

    j = calcSentinel(firstTok, j);
    while (j < sentinelToken) {
        Token tok = tokens[j];
        if (tok.tp < firstPunctuationTokenType) {
            j++;
        } else {
            j += tok.pl2 + 1;
        }
        if (tok.tp != tokOperator || (*pr->parDef->operators)[tok.pl1].arity > 1) {            
            (*arity)++;
        }
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
private void exprSubexpr(Token parenTok, Int* arity, Arr(Token) tokens, Compiler* pr) {
    Token firstTok = tokens[pr->i];    
    
    if (parenTok.pl2 == 1) {
        exprSingleItem(tokens[pr->i], pr);
        *arity = 0;
        pr->i++; // CONSUME the single item within parens
    } else {
        exprCountArity(arity, pr->i + parenTok.pl2, tokens, pr);
        
        if (firstTok.tp == tokWord || firstTok.tp == tokOperator) {
            Int mbBindingId = -1;
            if (firstTok.tp == tokWord) {
                mbBindingId = pr->activeBindings[firstTok.pl2];
            } else if (firstTok.tp == tokOperator) {
                VALIDATE(*arity == (*pr->parDef->operators)[firstTok.pl1].arity, errorOperatorWrongArity)
                mbBindingId = -firstTok.pl1 - 2;
            }
            
            VALIDATE(mbBindingId < -1, errorUnknownFunction)            

            addNode((Node){.tp = nodCall, .pl1 = mbBindingId, .pl2 = *arity, // todo overload
                           .startBt = firstTok.startBt, .lenBts = firstTok.lenBts}, pr);
            *arity = 0;
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
private void subexprClose(Arr(Token) tokens, Compiler* pr) {
    while (pr->scopeStack->length > 0 && pr->i == peek(pr->backtrack).sentinelToken) {
        popFrame(pr);        
    }
}

/** An operator token in non-initial position is either a funcall (if unary) or an operand. Consumes no tokens. */
private void exprOperator(Token tok, ScopeStackFrame* topSubexpr, Arr(Token) tokens, Compiler* pr) {
    Int bindingId = tok.pl1;
    OpDef operDefinition = (*pr->parDef->operators)[bindingId];

    if (operDefinition.arity == 1) {
        addNode((Node){ .tp = nodCall, .pl1 = -bindingId - 2, .pl2 = 1,
                        .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    } else {
        addNode((Node){ .tp = nodId, .pl1 = -bindingId - 2, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    }
}

/** Parses an expression. Precondition: we are 1 past the span token */
private void parseExpr(Token exprTok, Arr(Token) tokens, Compiler* pr) {
    Int sentinelToken = pr->i + exprTok.pl2;
    Int arity = 0;
    if (exprTok.pl2 == 1) {
        exprSingleItem(tokens[pr->i], pr);
        pr->i++; // CONSUME the single item within parens
        return;
    }

    push(((ParseFrame){.tp = nodExpr, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + exprTok.pl2 }), 
         pr->backtrack);        
    addNode((Node){ .tp = nodExpr, .startBt = exprTok.startBt, .lenBts = exprTok.lenBts }, pr);

    exprSubexpr(exprTok, &arity, tokens, pr);
    while (pr->i < sentinelToken) {
        subexprClose(tokens, pr);
        Token currTok = tokens[pr->i];
        untt tokType = currTok.tp;
        
        ScopeStackFrame* topSubexpr = pr->scopeStack->topScope;
        if (tokType == tokParens) {
            pr->i++; // CONSUME the parens token
            exprSubexpr(currTok, &arity, tokens, pr);
        } else VALIDATE(tokType < firstPunctuationTokenType, errorExpressionCannotContain)
        else if (tokType <= topVerbatimTokenVariant) {
            addNode((Node){ .tp = currTok.tp, .pl1 = currTok.pl1, .pl2 = currTok.pl2,
                            .startBt = currTok.startBt, .lenBts = currTok.lenBts }, pr);
            pr->i++; // CONSUME the verbatim token
        } else {
            if (tokType == tokWord) {
                Int mbBindingId = pr->activeBindings[currTok.pl2];
                VALIDATE(mbBindingId != -1, errorUnknownBinding)
                addNode((Node){ .tp = nodId, .pl1 = mbBindingId, .pl2 = currTok.pl2, 
                            .startBt = currTok.startBt, .lenBts = currTok.lenBts}, pr);                
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
private void maybeCloseSpans(Compiler* pr) {
    while (hasValues(pr->backtrack)) { // loop over subscopes and expressions inside FunctionDef
        ParseFrame frame = peek(pr->backtrack);
        if (pr->i < frame.sentinelToken) {
            return;
        } else VALIDATE(pr->i == frame.sentinelToken, errorInconsistentSpan)
        popFrame(pr);
    }
}

/** Parses top-level function bodies */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Compiler* pr) {
    while (pr->i < sentinelToken) {
        Token currTok = tokens[pr->i];
        untt contextType = peek(pr->backtrack).tp;
        
        // pre-parse hooks that let contextful syntax forms (e.g. "if") detect parsing errors and maintain their state
        if (contextType >= firstResumableForm) {
            ((*pr->parDef->resumableTable)[contextType - firstResumableForm])(&currTok, tokens, pr);
        } else {
            pr->i++; // CONSUME any span token
        }
        ((*pr->parDef->nonResumableTable)[currTok.tp])(currTok, tokens, pr);
        
        maybeCloseSpans(pr);
    }    
}

/** Consumes 0 or 1 tokens. Returns false if didn't parse anything. */
private bool parseLiteralOrIdentifier(Token tok, Compiler* pr) {
    if (tok.tp <= topVerbatimTokenVariant) {
        parseVerbatim(tok, pr);
    } else if (tok.tp == tokWord) {        
        Int nameId = tok.pl2;
        Int mbBinding = pr->activeBindings[nameId];
        VALIDATE(mbBinding != -1, errorUnknownBinding)    
        addNode((Node){.tp = nodId, .pl1 = mbBinding, .pl2 = nameId,
                       .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    } else {
        return false;        
    }
    pr->i++; // CONSUME the literal or ident token
    return true;
}


private void parseAssignment(Token tok, Arr(Token) tokens, Compiler* pr) {
    const Int rightSideLen = tok.pl2 - 1;
    if (rightSideLen < 1) {
        throwExc(errorAssignment);
    }
    Int sentinelToken = pr->i + tok.pl2;

    Token bindingToken = tokens[pr->i];
    Int newBindingId = createBinding(bindingToken, (Entity){ }, pr);

    push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
         pr->backtrack);
    addNode((Node){.tp = nodAssignment, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    
    addNode((Node){.tp = nodBinding, .pl1 = newBindingId, 
            .startBt = bindingToken.startBt, .lenBts = bindingToken.lenBts}, pr);
    
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
            parseExpr((Token){ .pl2 = rightSideLen, .startBt = rightSideToken.startBt, 
                               .lenBts = tok.lenBts - rightSideToken.startBt + tok.startBt
                       }, 
                       tokens, pr);
        }
    }
}

private void parseReassignment(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

private void parseMutation(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

private void parseAlias(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

private void parseAssert(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseAssertDbg(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseAwait(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

// TODO validate we are inside at least as many loops as we are breaking out of
private void parseBreak(Token tok, Arr(Token) tokens, Compiler* pr) {
    VALIDATE(tok.pl2 <= 1, errorBreakContinueTooComplex);
    if (tok.pl2 == 1) {
        Token nextTok = tokens[pr->i];
        VALIDATE(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errorBreakContinueInvalidDepth)
        addNode((Node){.tp = nodBreak, .pl1 = nextTok.pl2, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
        pr->i++; // CONSUME the Int after the "break"
    } else {
        addNode((Node){.tp = nodBreak, .pl1 = 1, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    }    
}


private void parseCatch(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseContinue(Token tok, Arr(Token) tokens, Compiler* pr) {
    VALIDATE(tok.pl2 <= 1, errorBreakContinueTooComplex);
    if (tok.pl2 == 1) {
        Token nextTok = tokens[pr->i];
        VALIDATE(nextTok.tp == tokInt && nextTok.pl1 == 0 && nextTok.pl2 > 0, errorBreakContinueInvalidDepth)
        addNode((Node){.tp = nodContinue, .pl1 = nextTok.pl2, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
        pr->i++; // CONSUME the Int after the "continue"
    } else {
        addNode((Node){.tp = nodContinue, .pl1 = 1, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    }    
}


private void parseDefer(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseDispose(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseExposePrivates(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseFnDef(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseInterface(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseImpl(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseLambda(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseLambda1(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseLambda2(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseLambda3(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parsePackage(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseReturn(Token tok, Arr(Token) tokens, Compiler* pr) {
    Int lenTokens = tok.pl2;
    Int sentinelToken = pr->i + lenTokens;        
    
    push(((ParseFrame){ .tp = nodReturn, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
            pr->backtrack);
    addNode((Node){.tp = nodReturn, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);
    
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
            parseExpr((Token){ .pl2 = lenTokens, .startBt = rightSideToken.startBt, 
                               .lenBts = tok.lenBts - rightSideToken.startBt + tok.startBt
                       }, 
                       tokens, pr);
        }
    }
}


private void parseSkip(Token tok, Arr(Token) tokens, Compiler* pr) {
    
}


private void parseScope(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

private void parseStruct(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseTry(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


private void parseYield(Token tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

/** To be called at the start of an "if" clause. It validates the grammar and emits nodes. Consumes no tokens.
 * Precondition: we are pointing at the init token of left side of "if" (i.e. at a tokStmt or the like)
 */
private void ifAddClause(Token tok, Arr(Token) tokens, Compiler* pr) {
    VALIDATE(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errorIfLeft)
    Int leftTokSkip = (tok.tp >= firstPunctuationTokenType) ? (tok.pl2 + 1) : 1;
    Int j = pr->i + leftTokSkip;
    VALIDATE(j + 1 < pr->inpLength, errorPrematureEndOfTokens)
    VALIDATE(tokens[j].tp == tokArrow, errorIfMalformed)
    
    j++; // the arrow
    
    Token rightToken = tokens[j];
    Int rightTokLength = (rightToken.tp >= firstPunctuationTokenType) ? (rightToken.pl2 + 1) : 1;    
    Int clauseSentinelByte = rightToken.startBt + rightToken.lenBts;
    
    ParseFrame clauseFrame = (ParseFrame){ .tp = nodIfClause, .startNodeInd = pr->nextInd, .sentinelToken = j + rightTokLength };
    push(clauseFrame, pr->backtrack);
    addNode((Node){.tp = nodIfClause, .pl1 = leftTokSkip,
         .startBt = tok.startBt, .lenBts = clauseSentinelByte - tok.startBt }, pr);
}


private void parseIf(Token tok, Arr(Token) tokens, Compiler* pr) {
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = pr->nextInd, 
        .sentinelToken = pr->i + tok.pl2 };
    push(newParseFrame, pr->backtrack);
    addNode((Node){.tp = nodIf, .pl1 = tok.pl1, .startBt = tok.startBt, .lenBts = tok.lenBts}, pr);

    ifAddClause(tokens[pr->i], tokens, pr);
}


private void parseLoop(Token loopTok, Arr(Token) tokens, Compiler* pr) {
    Token tokenStmt = tokens[pr->i];
    Int sentinelStmt = pr->i + tokenStmt.pl2 + 1;
    VALIDATE(tokenStmt.tp == tokStmt, errorLoopSyntaxError)    
    VALIDATE(sentinelStmt < pr->i + loopTok.pl2, errorLoopEmptyBody)
    
    Int indLeftSide = pr->i + 1;
    Token tokLeftSide = tokens[indLeftSide]; // + 1 because pr->i points at the stmt so far

    VALIDATE(tokLeftSide.tp == tokWord || tokLeftSide.tp == tokBool || tokLeftSide.tp == tokParens, errorLoopHeader)
    Int sentinelLeftSide = calcSentinel(tokLeftSide, indLeftSide); 

    Int startOfScope = sentinelStmt;
    Int startBtScope = tokens[startOfScope].startBt;
    Int indRightSide = -1;
    if (sentinelLeftSide < sentinelStmt) {
        indRightSide = sentinelLeftSide;
        startOfScope = indRightSide;
        Token tokRightSide = tokens[indRightSide];
        startBtScope = tokRightSide.startBt;
        VALIDATE(calcSentinel(tokRightSide, indRightSide) == sentinelStmt, errorLoopHeader)
    }
    
    Int sentToken = startOfScope - pr->i + loopTok.pl2;
        
    push(((ParseFrame){ .tp = nodLoop, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + loopTok.pl2 }), pr->backtrack);
    addNode((Node){.tp = nodLoop, .pl1 = slScope, .startBt = loopTok.startBt, .lenBts = loopTok.lenBts}, pr);

    push(((ParseFrame){ .tp = nodScope, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + loopTok.pl2 }), pr->backtrack);
    addNode((Node){.tp = nodScope, .startBt = startBtScope,
            .lenBts = loopTok.lenBts - startBtScope + loopTok.startBt}, pr);

    // variable initializations, if any
    if (indRightSide > -1) {
        pr->i = indRightSide + 1;
        while (pr->i < sentinelStmt) {
            Token binding = tokens[pr->i];
            VALIDATE(binding.tp = tokWord, errorLoopSyntaxError)
            
            Token expr = tokens[pr->i + 1];
            
            VALIDATE(expr.tp < firstPunctuationTokenType || expr.tp == tokParens, errorLoopSyntaxError)
            
            Int initializationSentinel = calcSentinel(expr, pr->i + 1);
            Int bindingId = createBinding(binding, ((Entity){}), pr);
            Int indBindingSpan = pr->nextInd;
            addNode((Node){.tp = nodAssignment, .pl2 = initializationSentinel - pr->i,
                           .startBt = binding.startBt,
                           .lenBts = expr.lenBts + expr.startBt - binding.startBt}, pr);
            addNode((Node){.tp = nodBinding, .pl1 = bindingId,
                           .startBt = binding.startBt, .lenBts = binding.lenBts}, pr);
                        
            if (expr.tp == tokParens) {
                pr->i += 2;
                parseExpr(expr, tokens, pr);
            } else {
                exprSingleItem(expr, pr);
            }
            setSpanLength(indBindingSpan, pr);
            
            pr->i = initializationSentinel;
        }
    }

    // loop body
    
    Token tokBody = tokens[sentinelStmt];
    if (startBtScope < 0) {
        startBtScope = tokBody.startBt;
    }    
    addNode((Node){.tp = nodLoopCond, .pl1 = slStmt, .pl2 = sentinelLeftSide - indLeftSide,
         .startBt = tokLeftSide.startBt, .lenBts = tokLeftSide.lenBts}, pr);
    pr->i = indLeftSide + 1;
    parseExpr(tokens[indLeftSide], tokens, pr);
    
    
    pr->i = sentinelStmt; // CONSUME the loop token and its first statement
}


private void resumeIf(Token* tok, Arr(Token) tokens, Compiler* pr) {
    if (tok->tp == tokElse) {        
        VALIDATE(pr->i < pr->inpLength, errorPrematureEndOfTokens)
        pr->i++; // CONSUME the "else"
        *tok = tokens[pr->i];        

        push(((ParseFrame){ .tp = nodElse, .startNodeInd = pr->nextInd,
                           .sentinelToken = calcSentinel(*tok, pr->i)}), pr->backtrack);
        addNode((Node){.tp = nodElse, .startBt = tok->startBt, .lenBts = tok->lenBts }, pr);       
        pr->i++; // CONSUME the token after the "else"
    } else {
        ifAddClause(*tok, tokens, pr);
        pr->i++; // CONSUME the init token of the span
    }
}

private void resumeIfPr(Token* tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

private void resumeImpl(Token* tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}

private void resumeMatch(Token* tok, Arr(Token) tokens, Compiler* pr) {
    throwExc(errorTemp);
}


void addFunctionType(Int arity, Arr(Int) paramsAndReturn, Compiler* pr) {
    Int neededLen = pr->typeNext + arity + 2;
    while (pr->typeCap < neededLen) {
        StackInt* newTypes = createStackint32_t(pr->typeCap*2*4, pr->a);
        memcpy(newTypes, pr->types, pr->typeNext);
        pr->types = newTypes;
        pr->typeCap *= 2;
    }
    (*pr->types->content)[pr->typeNext] = arity + 1;
    memcpy(pr->types + pr->typeNext + 1, paramsAndReturn, arity + 1);
    pr->typeNext += (arity + 2);
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
    p[tokAssignment] = &parseAssignment;
    p[tokReassign]   = &parseReassignment;
    p[tokMutation]   = &parseMutation;
    p[tokArrow]      = &parseSkip;
    
    p[tokAlias]      = &parseAlias;
    p[tokAssert]     = &parseAssert;
    p[tokAssertDbg]  = &parseAssertDbg;
    p[tokAwait]      = &parseAlias;
    p[tokBreak]      = &parseBreak;
    p[tokCatch]      = &parseAlias;
    p[tokContinue]   = &parseContinue;
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
    p[tokElse]       = &parseSkip;
    p[tokLoop]       = &parseLoop;
    return result;
}
 

private ResumeFunc (*tabulateResumableDispatch(Arena* a))[countResumableForms] {
    ResumeFunc (*result)[countResumableForms] = allocateOnArena(countResumableForms*sizeof(ResumeFunc), a);
    ResumeFunc* p = *result;
    int i = 0;
    p[nodIf    - firstResumableForm] = &resumeIf;
    p[nodIfPr  - firstResumableForm] = &resumeIfPr;
    p[nodImpl  - firstResumableForm] = &resumeImpl;
    p[nodMatch - firstResumableForm] = &resumeMatch;    
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


void importEntities(Arr(EntityImport) impts, Int countBindings, Compiler* pr) {
    for (int i = 0; i < countBindings; i++) {
        Int mbNameId = getStringStore(pr->text->content, impts[i].name, pr->stringTable, pr->stringStore);
        if (mbNameId > -1) {           
            Int newBindingId = importEntity(mbNameId, impts[i].entity, pr);
        }
    }    
}


void importOverloads(Arr(OverloadImport) impts, Int countImports, Compiler* pr) {
    for (int i = 0; i < countImports; i++) {        
        Int mbNameId = getStringStore(pr->text->content, impts[i].name, pr->stringTable, pr->stringStore);
        if (mbNameId > -1) {
            encounterFnDefinition(mbNameId, pr);
        }
    }
}

/* Entities and overloads for the built-in operators, types and functions. */
void importBuiltins(LanguageDefinition* langDef, Compiler* pr) {
    Arr(String*) baseTypes = allocateOnArena(5*sizeof(String*), pr->aTmp);
    baseTypes[0] = str("Int", pr->aTmp);
    baseTypes[1] = str("Long", pr->aTmp);
    baseTypes[2] = str("Float", pr->aTmp);
    baseTypes[3] = str("String", pr->aTmp);
    baseTypes[4] = str("Bool", pr->aTmp);
    for (int i = 0; i < 5; i++) {
        push(0, pr->types);
        Int typeNameId = getStringStore(pr->text->content, baseTypes[i], pr->stringTable, pr->stringStore);
        if (typeNameId > -1) {
            pr->activeBindings[typeNameId] = i;
        }
    }
    
    EntityImport builtins[2] =  {
        (EntityImport) { .name = str("math-pi", pr->a), .entity = (Entity){.typeId = typFloat} },
        (EntityImport) { .name = str("math-e", pr->a),  .entity = (Entity){.typeId = typFloat} }
    };    
    for (int i = 0; i < countOperators; i++) {
        pr->overloadCounts[i] = (*langDef->operators)[i].overs;
    }
    pr->overlCNext = countOperators;
    importEntities(builtins, sizeof(builtins)/sizeof(EntityImport), pr);
}


Compiler* createCompiler(Lexer* lx, Arena* a) {
    if (lx->wasError) {
        Compiler* result = allocateOnArena(sizeof(Compiler), a);
        result->wasError = true;
        return result;
    }
    
    Compiler* result = allocateOnArena(sizeof(Compiler), a);
    Arena* aTmp = mkArena();
    Int stringTableLength = lx->stringTable->length;
    Int initNodeCap = lx->totalTokens > 64 ? lx->totalTokens : 64;
    (*result) = (Compiler) {
        .text = lx->inp, .inp = lx, .inpLength = lx->totalTokens, .parDef = buildParserDefinitions(lx->langDef, a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aTmp), .i = 0,
        
        .nodes = allocateOnArena(sizeof(Node)*initNodeCap, a), .nextInd = 0, .capacity = initNodeCap,
        
        .entities = allocateOnArena(sizeof(Entity)*64, a), .entNext = 0, .entCap = 64,        
        .overloadCounts = allocateOnArena(4*(countOperators + 10), aTmp),
        .overlCNext = 0, .overlCCap = countOperators + 10,        
        .activeBindings = allocateOnArena(4*stringTableLength, a),
        
        .types = createStackint32_t(64, a), .typeNext = 0, .typeCap = 64,
        .expStack = createStackint32_t(16, aTmp),
        
        .stringStore = lx->stringStore, .stringTable = lx->stringTable, .strLength = stringTableLength,
        .wasError = false, .errMsg = &empty, .a = a, .aTmp = aTmp
    };
    if (stringTableLength > 0) {
        memset(result->activeBindings, 0xFF, stringTableLength*sizeof(Int)); // activeBindings is filled with -1
    }
    importBuiltins(lx->langDef, result);
    return result;    
}

/** Parses top-level types but not functions and adds their bindings to the scope */
private void parseToplevelTypes(Lexer* lr, Compiler* pr) {
}

/** Parses top-level constants but not functions, and adds their bindings to the scope */
private void parseToplevelConstants(Lexer* lx, Compiler* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokAssignment) {
            parseUpTo(pr->i + tok.pl2, lx->tokens, pr);
        } else {
            pr->i += (tok.pl2 + 1);
        }
    }    
}

/** Parses top-level function names and adds their bindings to the scope */
private void parseToplevelFunctionNames(Lexer* lx, Compiler* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.pl2;
            if (lenTokens < 3) {
                throwExc(errorFnNameAndParams);
            }
            Token fnName = lx->tokens[(pr->i) + 2]; // + 2 because we skip over the "fn" and "stmt" span tokens
            if (fnName.tp != tokWord || fnName.pl1 > 0) { // function name must be a lowercase word
                throwExc(errorFnNameAndParams);
            }
            encounterFnDefinition(fnName.pl2, pr);
        }
        pr->i += (tok.pl2 + 1);        
    } 
}

/** 
 * Parses a top-level function signature.
 * The result is [FnDef EntityName Scope EntityParam1 EntityParam2 ... ]
 */
private void parseFnSignature(Token fnDef, Lexer* lx, Compiler* pr) {
    Int fnSentinel = pr->i + fnDef.pl2 - 1;
    Int byteSentinel = fnDef.startBt + fnDef.lenBts;
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodFnDef, .startNodeInd = pr->nextInd, 
        .sentinelToken = fnSentinel };
    Token fnName = lx->tokens[pr->i];
    pr->i++; // CONSUME the function name token

    // the function's return type, it's optional
    if (lx->tokens[pr->i].tp == tokTypeName) {
        Token fnReturnType = lx->tokens[pr->i];

        VALIDATE(pr->activeBindings[fnReturnType.pl2] > -1, errorUnknownType)
        
        pr->i++; // CONSUME the function return type token
    }

    // the fnDef scope & node
    push(newParseFrame, pr->backtrack);
    encounterFnDefinition(fnName.pl2, pr);
    addNode((Node){.tp = nodFnDef, .pl1 = pr->activeBindings[fnName.pl2],
                        .startBt = fnDef.startBt, .lenBts = fnDef.lenBts} , pr);

    // the scope for the function body
    VALIDATE(lx->tokens[pr->i].tp == tokParens, errorFnNameAndParams)
    push(((ParseFrame){ .tp = nodScope, .startNodeInd = pr->nextInd, 
        .sentinelToken = fnSentinel }), pr->backtrack);        
    pushScope(pr->scopeStack);
    Token parens = lx->tokens[pr->i];
    addNode((Node){ .tp = nodScope, 
                    .startBt = parens.startBt, .lenBts = byteSentinel - parens.startBt }, pr);           
    
    Int paramsSentinel = pr->i + parens.pl2 + 1;
    pr->i++; // CONSUME the parens token for the param list            
    
    while (pr->i < paramsSentinel) {
        Token paramName = lx->tokens[pr->i];
        VALIDATE(paramName.tp == tokWord, errorFnNameAndParams)
        Int newBindingId = createBinding(paramName, (Entity){.nameId = paramName.pl2}, pr);
        Node paramNode = (Node){.tp = nodBinding, .pl1 = newBindingId, 
                                .startBt = paramName.startBt, .lenBts = paramName.lenBts };
        pr->i++; // CONSUME a param name
        
        VALIDATE(pr->i < paramsSentinel, errorFnNameAndParams)
        Token paramType = lx->tokens[pr->i];
        VALIDATE(paramType.tp == tokTypeName, errorFnNameAndParams)
        
        Int typeBindingId = pr->activeBindings[paramType.pl2]; // the binding of this parameter's type
        VALIDATE(typeBindingId > -1, errorUnknownType)
        
        pr->entities[newBindingId].typeId = typeBindingId;
        pr->i++; // CONSUME the param's type name
        
        addNode(paramNode, pr);        
    }
    
    VALIDATE(pr->i < fnSentinel && lx->tokens[pr->i].tp >= firstPunctuationTokenType, errorFnMissingBody)
}

/** Parses top-level function params and bodies */
private void parseFunctionBodies(Lexer* lx, Compiler* pr) {
    pr->i = 0;
    const Int len = lx->totalTokens;
    while (pr->i < len) {
        Token tok = lx->tokens[pr->i];
        if (tok.tp == tokFnDef) {
            Int lenTokens = tok.pl2;
            Int sentinelToken = pr->i + lenTokens + 1;
            if (lenTokens < 2) {
                throwExc(errorFnNameAndParams);
            }
            pr->i += 2; // CONSUME the function def token and the stmt token
            parseFnSignature(tok, lx, pr);
            parseUpTo(sentinelToken, lx->tokens, pr);
            pr->i = sentinelToken;
        } else {            
            pr->i += (tok.pl2 + 1);    // CONSUME the whole non-function span
        }
    }
}

/** Parses a single file in 4 passes, see docs/parser.txt */
Compiler* parse(Lexer* lx, Arena* a) {
    Compiler* pr = createCompiler(lx, a);
    return parseWithCompiler(lx, pr, a);
}


Compiler* parseWithCompiler(Lexer* lx, Compiler* pr, Arena* a) {
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


Arr(Int) createOverloads(Compiler* pr) {
    Int neededCount = 0;
    for (Int i = 0; i < pr->overlCNext; i++) {
        neededCount += (2*pr->overloadCounts[i] + 1); // a typeId and an entityId for each overload, plus a length field for the list
    }
    print("total number of overloads needed: %d", neededCount);
    Arr(Int) overloads = allocateOnArena(neededCount*4, pr->a);

    Int j = 0;
    for (Int i = 0; i < pr->overlCNext; i++) {
        overloads[j] = pr->overloadCounts[i]; // length of the overload list (which will be filled during type check/resolution)
        j += (2*pr->overloadCounts[i] + 1);
    }
    return overloads;
}


Int typeResolveExpr(Int indExpr, Compiler* pr) {
    Node expr = pr->nodes[indExpr];
    Int sentinelNode = indExpr + expr.pl2 + 1;
    for (int i = indExpr + 1; i < sentinelNode; ++i) {
        Node nd = pr->nodes[i];
        if (nd.tp <= nodString) {
            push((Int)nd.tp, pr->expStack);
            push(-1, pr->expStack); // -1 arity means it's not a call
        } else {
            push(nd.pl1, pr->expStack); // bindingId or overloadId            
            push(nd.tp == nodCall ? nd.pl2 : -1, pr->expStack);            
        }
    }
    Int j = 2*expr.pl2 - 1;
    Arr(Int) st = *pr->expStack->content;
    while (j > -1) {        
        if (st[j + 1] == -1) {
            j -= 2;
        } else { // a function call
            // resolve overload
            // check & resolve arg types
            // replace the arg types on the stack with the return type
            // shift the remaining stuff on the right so it directly follows the return
        }
    }
}
