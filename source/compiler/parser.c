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
    if (frame.tp == nodScope) {
        popScopeFrame(pr->activeBindings, pr->scopeStack);
    }
    setSpanLength(frame.startNodeInd, pr);
    return frame;
}

/** Calculates the sentinel token for a token at a specific index */
private Int calcSentinel(Token tok, Int tokInd) {
    return (tok.tp >= firstPunctuationTokenType ? (tokInd + tok.payload2 + 1) : (tokInd + 1));
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
        Int operBindingId = theTok.payload1;
        OpDef operDefinition = (*pr->parDef->operators)[operBindingId];
        if (operDefinition.arity == 1) {
            addNode((Node){ .tp = nodId, .payload1 = operBindingId,
                .startByte = theTok.startByte, .lenBytes = theTok.lenBytes}, pr);
        } else {
            throwExc(errorUnexpectedToken);
        }
    } else if (theTok.tp <= topVerbatimTokenVariant) {
        addNode((Node){.tp = theTok.tp, .payload1 = theTok.payload1, .payload2 = theTok.payload2,
                        .startByte = theTok.startByte, .lenBytes = theTok.lenBytes }, pr);
    } else if (theTok.tp >= firstCoreFormTokenType) {
        throwExc(errorCoreFormInappropriate);
    } else {
        throwExc(errorUnexpectedToken);
    }
}

/** Counts the arity of the call, including skipping unary operators. Consumes no tokens. */
void exprCountArity(Int* arity, Int sentinelToken, Arr(Token) tokens, Parser* pr) {
    Int j = pr->i;
    Token firstTok = tokens[j];

    j = calcSentinel(firstTok, j);
    while (j < sentinelToken) {
        Token tok = tokens[j];
        if (tok.tp < firstPunctuationTokenType) {
            j++;
        } else {
            j += tok.payload2 + 1;
        }
        if (tok.tp != tokOperator || (*pr->parDef->operators)[tok.payload1].arity > 1) {            
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
private void exprSubexpr(Token parenTok, Int* arity, Arr(Token) tokens, Parser* pr) {
    Token firstTok = tokens[pr->i];    
    
    if (parenTok.payload2 == 1) {
        exprSingleItem(tokens[pr->i], pr);
        *arity = 0;
        pr->i++; // CONSUME the single item within parens
    } else {
        exprCountArity(arity, pr->i + parenTok.payload2, tokens, pr);
        if (firstTok.tp == tokWord || firstTok.tp == tokOperator) {
            Int mbBindingId = -1;
            if (firstTok.tp == tokWord) {
                mbBindingId = pr->activeBindings[firstTok.payload2];
            } else if (firstTok.tp == tokOperator) {
                mbBindingId = firstTok.payload1;
            }
            
            VALIDATE(mbBindingId > -1, errorUnknownFunction)            

            addNode((Node){.tp = nodCall, .payload1 = mbBindingId, .payload2 = *arity, // todo overload
                           .startByte = firstTok.startByte, .lenBytes = firstTok.lenBytes}, pr);
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
private void subexprClose(Arr(Token) tokens, Parser* pr) {
    while (pr->scopeStack->length > 0 && pr->i == peek(pr->backtrack).sentinelToken) {
        popFrame(pr);        
    }
}

/** An operator token in non-initial position is either a funcall (if unary) or an operand. Consumes no tokens. */
private void exprOperator(Token tok, ScopeStackFrame* topSubexpr, Arr(Token) tokens, Parser* pr) {
    Int bindingId = tok.payload1;
    OpDef operDefinition = (*pr->parDef->operators)[bindingId];

    if (operDefinition.arity == 1) {
        addNode((Node){ .tp = nodCall, .payload1 = bindingId, .payload2 = 1,
                        .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);
    } else {
        addNode((Node){ .tp = nodId, .payload1 = bindingId, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);
    }
}

/** Parses an expression. Precondition: we are 1 past the span token */
private void parseExpr(Token exprTok, Arr(Token) tokens, Parser* pr) {
    Int sentinelToken = pr->i + exprTok.payload2;
    Int arity = 0;
    if (exprTok.payload2 == 1) {
        exprSingleItem(tokens[pr->i], pr);
        pr->i++; // CONSUME the single item within parens
        return;
    }

    push(((ParseFrame){.tp = nodExpr, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + exprTok.payload2 }), 
         pr->backtrack);        
    addNode((Node){ .tp = nodExpr, .startByte = exprTok.startByte, .lenBytes = exprTok.lenBytes }, pr);

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
            addNode((Node){ .tp = currTok.tp, .payload1 = currTok.payload1, .payload2 = currTok.payload2,
                            .startByte = currTok.startByte, .lenBytes = currTok.lenBytes }, pr);
            pr->i++; // CONSUME the verbatim token
        } else {
            if (tokType == tokWord) {
                Int mbBindingId = pr->activeBindings[currTok.payload2];
                print("mbBind currTok.payload2 %d %d", currTok.payload2, mbBindingId)
                VALIDATE(mbBindingId > -1, errorUnknownBinding)
                addNode((Node){ .tp = nodId, .payload1 = mbBindingId, .payload2 = currTok.payload2, 
                            .startByte = currTok.startByte, .lenBytes = currTok.lenBytes}, pr);                
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
        popFrame(pr);
    }
}

/** Parses top-level function bodies */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Parser* pr) {
    while (pr->i < sentinelToken) {
        Token currTok = tokens[pr->i];
        untt contextType = peek(pr->backtrack).tp;
        
        // pre-parse hooks that let contextful syntax forms (e.g. "if") detect parsing errors and maintain their state
        if (contextType >= firstResumableForm) {
            ((*pr->parDef->resumableTable)[contextType - firstResumableForm])(&currTok, tokens, pr);
        } else {
            pr->i++; // CONSUME any span token
        }
        print("main i %d", pr->i)
        ((*pr->parDef->nonResumableTable)[currTok.tp])(currTok, tokens, pr);
        
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

private void openAssignment(Int j, Token assignmentToken, Arr(Token) tokens, Parser* pr) {
    Int sentinelToken = j + assignmentToken.payload2;
    Token bindingToken = tokens[j];
    VALIDATE(bindingToken.tp == tokWord, errorAssignment)

    Int mbBinding = pr->activeBindings[bindingToken.payload2];
    VALIDATE(mbBinding == -1, errorAssignmentShadowing)

    Int newBindingId = createBinding(bindingToken.payload2, (Binding){.flavor = bndMut }, pr);
                                         
    push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
         pr->backtrack);
    addNode((Node){.tp = nodAssignment, .startByte = assignmentToken.startByte, .lenBytes = assignmentToken.lenBytes}, pr);
    
    addNode((Node){.tp = nodBinding, .payload1 = newBindingId, 
            .startByte = bindingToken.startByte, .lenBytes = bindingToken.lenBytes}, pr);
}


private void parseAssignment(Token tok, Arr(Token) tokens, Parser* pr) {
    const Int rightSideLen = tok.payload2 - 1;
    if (rightSideLen < 1) {
        throwExc(errorAssignment);
    }
    Int sentinelToken = pr->i + tok.payload2;

    openAssignment(pr->i, tok, tokens, pr);

    //~ Token bindingToken = tokens[pr->i];
    //~ VALIDATE(bindingToken.tp == tokWord, errorAssignment)

    //~ Int mbBinding = pr->activeBindings[bindingToken.payload2];
    //~ VALIDATE(mbBinding == -1, errorAssignmentShadowing)

    //~ Int newBindingId = createBinding(
     //~ bindingToken.payload2, (Binding){.flavor = bndImmut, .defStart = pr->i - 1, .defSentinel = sentinelToken}, pr
    //~ );
                                         
    //~ push(((ParseFrame){ .tp = nodAssignment, .startNodeInd = pr->nextInd, .sentinelToken = sentinelToken }), 
         //~ pr->backtrack);    
    //~ addNode((Node){.tp = nodAssignment, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);
    
    //~ addNode((Node){.tp = nodBinding, .payload1 = newBindingId, 
            //~ .startByte = bindingToken.startByte, .lenBytes = bindingToken.lenBytes}, pr);
    
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


private void parseSkip(Token tok, Arr(Token) tokens, Parser* pr) {
    
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

/** To be called at the start of an "if" clause. It validates the grammar and emits nodes. Consumes no tokens.
 * Precondition: we are pointing at the init token of left side of "if" (i.e. at a tokStmt or the like)
 */
private void ifAddClause(Token tok, Arr(Token) tokens, Parser* pr) {
    VALIDATE(tok.tp == tokStmt || tok.tp == tokWord || tok.tp == tokBool, errorIfLeft)
    Int leftTokSkip = (tok.tp >= firstPunctuationTokenType) ? (tok.payload2 + 1) : 1;
    Int j = pr->i + leftTokSkip;
    VALIDATE(j + 1 < pr->inpLength, errorPrematureEndOfTokens)
    VALIDATE(tokens[j].tp == tokArrow, errorIfMalformed)
    
    j++; // the arrow
    
    Token rightToken = tokens[j];
    Int rightTokLength = (rightToken.tp >= firstPunctuationTokenType) ? (rightToken.payload2 + 1) : 1;    
    Int clauseSentinelByte = rightToken.startByte + rightToken.lenBytes;
    
    ParseFrame clauseFrame = (ParseFrame){ .tp = nodIfClause, .startNodeInd = pr->nextInd, .sentinelToken = j + rightTokLength };
    push(clauseFrame, pr->backtrack);
    addNode((Node){.tp = nodIfClause, .payload1 = leftTokSkip,
         .startByte = tok.startByte, .lenBytes = clauseSentinelByte - tok.startByte }, pr);
}


private void parseIf(Token tok, Arr(Token) tokens, Parser* pr) {
    ParseFrame newParseFrame = (ParseFrame){ .tp = nodIf, .startNodeInd = pr->nextInd, 
        .sentinelToken = pr->i + tok.payload2 };
    push(newParseFrame, pr->backtrack);
    addNode((Node){.tp = nodIf, .payload1 = tok.payload1, .startByte = tok.startByte, .lenBytes = tok.lenBytes}, pr);

    ifAddClause(tokens[pr->i], tokens, pr);
}


private void parseLoop(Token loopTok, Arr(Token) tokens, Parser* pr) {
    // first thing must be a statement with one or two items
    // first item is either empty or an expr
    // second item, if present, must have an even number of tokens ordered like this: word expr word expr ...

    Token tokenStmt = tokens[pr->i];
    Int sentinelStmt = pr->i + tokenStmt.payload2 + 1;
    VALIDATE(tokenStmt.tp == tokStmt, errorLoopSyntaxError)    
    VALIDATE(sentinelStmt < pr->i + loopTok.payload2, errorLoopEmptyBody)
    
    Int indLeftSide = pr->i + 1;
    Token tokLeftSide = tokens[indLeftSide]; // + 1 because pr->i points at the stmt so far

    VALIDATE(tokLeftSide.tp == tokWord || tokLeftSide.tp == tokBool || tokLeftSide.tp == tokParens, errorLoopHeader)
    Int sentinelLeftSide = calcSentinel(tokLeftSide, indLeftSide); 

    Int startOfScope = sentinelStmt;
    Int startByteScope = -1;
    Int indRightSide = -1;
    if (sentinelLeftSide < sentinelStmt) {
        indRightSide = sentinelLeftSide;
        startOfScope = indRightSide;
        Token tokRightSide = tokens[indRightSide];
        startByteScope = tokRightSide.startByte;
        VALIDATE(calcSentinel(tokRightSide, indRightSide) == sentinelStmt, errorLoopHeader)
    }
    
    Int sentToken = startOfScope - pr->i + loopTok.payload2;
        
    push(((ParseFrame){ .tp = nodLoop, .startNodeInd = pr->nextInd, .sentinelToken = sentToken }), pr->backtrack);
    addNode((Node){.tp = nodLoop, .payload1 = slScope, .startByte = loopTok.startByte, .lenBytes = loopTok.lenBytes}, pr);
    Int j;

    print("scope sentinel %d", pr->i + loopTok.payload2)
    push(((ParseFrame){ .tp = nodScope, .startNodeInd = pr->nextInd, .sentinelToken = pr->i + loopTok.payload2 }), pr->backtrack);
    addNode((Node){.tp = nodScope, .payload1 = slScope, .startByte = startByteScope,
            .lenBytes = loopTok.lenBytes - startByteScope + loopTok.startByte}, pr);

    // variable initializations, if any
    if (indRightSide > -1) {
        j = indRightSide + 1;
        while (j < sentinelStmt) {
            Token tokBinding = tokens[j];
            VALIDATE(tokBinding.tp = tokWord, errorLoopSyntaxError)
            
            Token tokExpr = tokens[j + 1];
            VALIDATE(tokExpr.tp < firstPunctuationTokenType || tokExpr.tp == tokStmt, errorLoopSyntaxError)
            
            openAssignment(j, ((Token){.payload2 = tokExpr.payload2 + 2,
                                       .startByte = tokBinding.startByte,
                                       .lenBytes = tokExpr.startByte + tokExpr.lenBytes - tokBinding.startByte }), tokens, pr);
            j += 2;
        }
    }

    // loop body
    
    Token tokBody = tokens[sentinelStmt];
    if (startByteScope < 0) {
        startByteScope = tokBody.startByte;
    }
    push(((ParseFrame){ .tp = nodLoopCond, .startNodeInd = pr->nextInd, .sentinelToken = sentToken }), pr->backtrack);
    addNode((Node){.tp = nodLoopCond, .payload1 = slScope, .startByte = startByteScope,
            .lenBytes = loopTok.lenBytes - startByteScope + loopTok.startByte
        }, pr);
    pr->i = indLeftSide + 1;
    parseExpr(tokens[indLeftSide], tokens, pr);
    
    
    pr->i = sentinelStmt; // CONSUME the loop token and its first statement
}


private void resumeIf(Token* tok, Arr(Token) tokens, Parser* pr) {
    if (tok->tp == tokElse) {        
        VALIDATE(pr->i < pr->inpLength, errorPrematureEndOfTokens)
        pr->i++; // CONSUME the "else"
        *tok = tokens[pr->i];        

        push(((ParseFrame){ .tp = nodElse, .startNodeInd = pr->nextInd,
                           .sentinelToken = calcSentinel(*tok, pr->i)}), pr->backtrack);
        addNode((Node){.tp = nodElse, .startByte = tok->startByte, .lenBytes = tok->lenBytes }, pr);       
        pr->i++; // CONSUME the token after the "else"
    } else {
        ifAddClause(*tok, tokens, pr);
        pr->i++; // CONSUME the init token of the span
    }
}

private void resumeIfPr(Token* tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeImpl(Token* tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}

private void resumeMatch(Token* tok, Arr(Token) tokens, Parser* pr) {
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
    p[tokArrow]      = &parseSkip;
    
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
        .text = lx->inp, .inp = lx, .inpLength = lx->totalTokens, .parDef = buildParserDefinitions(lx->langDef, a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aBt), .i = 0,
        .stringStore = lx->stringStore,
        .stringTable = lx->stringTable, .strLength = stringTableLength,
        .bindings = allocateOnArena(sizeof(Binding)*64, a), .bindNext = 0, .bindCap = 64,
        .overloads = allocateOnArena(sizeof(Int)*64, a), .overlNext = 0, .overlCap = 64,
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
