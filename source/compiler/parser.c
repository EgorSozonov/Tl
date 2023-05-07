#include <stdbool.h>
#include <string.h>
#include "parser.h"
#include "parser.internal.h"



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


private void parseVerbatim(Token tok, Parser* pr) {
    addNode((Node){.tp = tok.tp, .startByte = tok.startByte, .lenBytes = tok.lenBytes, 
        .payload1 = tok.payload1, .payload2 = tok.payload2}, pr);
}

private void parseErrorBareAtom(Token tok, Arr(Token) tokens, Parser* pr) {
    throwExc(errorTemp);
}


/**
 * A single-item subexpression, like "(5)" or "x". Cannot be a function call.
 */
private void exprSingleItem(Token theTok, Parser* pr) {
    if (parseLiteralOrIdentifier(theTok.tp)) {
    } else if (theTok.tp >= firstCoreFormTokenType) {
        throwExc(errorCoreFormInappropriate)
    } else {
        throwExc(errorUnexpectedToken)
    }
}

/**
 * Adds a frame for a subexpression.
 * Precondition: the current token must be 1 past the opening paren / statement token
 * Examples: "foo 5 a"
 *           "5 + !a"
 * TODO: allow for core forms (but not scopes!)
 */
private void subexprInit(Int lenTokens, Arr(Token) tokens, Parser* pr) {
    Token firstTok = tokens[pr->i];    
    
    if (lenTokens == 1) {
        exprSingleItem(firstTok, pr);
        return;
    }
    pushSubexpr(pr->scopeStack);    
}


/**
 * Flushes the finished subexpr frames from the top of the funcall stack.
 * A subexpr frame is finished iff current token equals its sentinel.
 * Flushing includes appending its operators, clearing the operator stack, and appending
 * prefix unaries from the previous subexpr frame, if any.
 */
private void subexprClose(Parser* pr) {
    while (pr->scopeStack->length > 0) {
        ScopeStackFrame currSubexpr = pr->scopeStack->topScope;
        if (pr->i != currSubexpr.sentinelToken || !(currSubexpr->isSubexpr)) {
            return;
        }

        if (currSubexpr.operators.size == 1) {
            if (currSubexpr.isInHeadPosition) {
                // this is cases like "((f 5) 1.2)" - the inner head-position operator gets moved out into the outer subexpr
                val parentSubexpr = funCallStack[k - 1]
                parentSubexpr.operators[0] = currSubexpr.operators[0]
            } else if (currSubexpr.isStillPrefix) {
                appendFnName(currSubexpr.operators[0], true)
            } else if (currSubexpr.operators[0].nameStringId == -1
                        && currSubexpr.operators[0].arity != 1) {
                // this is cases like "(1 2 3)" or "(!x y)"
                throwExc(errorExpressionFunctionless)
            } else {
                for (m in currSubexpr.operators.size - 1 downTo 0) {
                    appendFnName(currSubexpr.operators[m], false)
                }
            }
        } else if (currSubexpr.isInHeadPosition) {
            throwExc(errorExpressionHeadFormOperators)
        } else {
            for (m in currSubexpr.operators.size - 1 downTo 0) {
                appendFnName(currSubexpr.operators[m], false)
            }
        }

        funCallStack.removeLast()

        // flush parent's prefix opers, if any, because this subexp was their operand
        if (funCallStack.isNotEmpty()) {
            val opersPrev = funCallStack[k - 1].operators
            for (n in (opersPrev.size - 1) downTo 0) {
                if (opersPrev[n].precedence == prefixPrec) {
                    appendFnName(opersPrev[n], false)
                    opersPrev.removeLast()
                } else {
                    break
                }
            }
        }
    }
}


/**
 * Increments the arity of the top non-prefix operator. The prefix ones are ignored because there is no
 * need to track their arity: they are flushed on first operand or on closing of the first subexpr after them.
 */
private void exprIncrementArity(topFrame: Subexpr, Parser* pr) {
    var n = topFrame.operators.size - 1
    while (n > -1 && topFrame.operators[n].precedence == prefixPrec) {
        n--;
    }
    if (n < 0) {
        throwExc(errorExpressionError)
    }
    val oper = topFrame.operators[n]
    if (oper.nameStringId == -1) return
    oper.arity++
    if (oper.maxArity > 0 && oper.arity > oper.maxArity) {
        throwExc(errorOperatorWrongArity)
    }
}

/**
 * State modifier that must be called whenever an operand is encountered in an expression parse
 */
private fun exprOperand(topSubexpr: Subexpr) {
    if (topSubexpr.operators.isEmpty()) return

    // flush all unaries
    val opers = topSubexpr.operators
    while (opers.last().precedence == prefixPrec) {
        appendFnName(opers.last(), false)
        opers.removeLast()
    }
    exprIncrementArity(topSubexpr)
}


private fun exprOperator(topSubexpr: Subexpr) {
    val operTypeIndex = lx.currPayload2()
    val operInfo = operatorDefinitions[operTypeIndex]

    if (operInfo.precedence == prefixPrec) {
        val startByte = lx.currStartByte()
        topSubexpr.operators.add(FunctionCall(-operInfo.bindingIndex - 2, prefixPrec, 1, 1, false, startByte))
    } else {
        exprFuncall(-operInfo.bindingIndex - 2, operInfo.precedence, operInfo.arity, topSubexpr)
    }
}



/**
 * For functions, the first param is the stringId of the name.
 * For operators, it's the negative index of operator from its payload
 */
private void exprFuncall(nameStringId: Int, precedence: Int, maxArity: Int, topSubexpr: Subexpr,
                         Parser* pr) {
    val startByte = lx.currStartByte()

    if (topSubexpr.isStillPrefix) {
        if (topSubexpr.operators.size != 1) throwExc(errorExpressionError)
        val prefixFunction = topSubexpr.operators.removeLast()
        appendFnName(prefixFunction, true)

        val newFun = FunctionCall(nameStringId, precedence, 1, maxArity, false, startByte)
        topSubexpr.isStillPrefix = false
        topSubexpr.operators.add(newFun)
    } else {
        if (nameStringId < -1 && precedence != prefixPrec) {
            // infix operator, need to check if it's the first in this subexpr
            if (topSubexpr.operators.size == 1 && topSubexpr.operators[0].nameStringId == -1) {
                if (topSubexpr.operators[0].arity != 0) throwExc(errorExpressionInfixNotSecond)

                val newFun = FunctionCall(nameStringId, precedence, 1, maxArity, false, startByte)
                topSubexpr.operators[0] = newFun
                return
            }
        }
        while (topSubexpr.operators.isNotEmpty() && topSubexpr.operators.last().precedence >= precedence) {
            if (topSubexpr.operators.last().precedence == prefixPrec) {
                throwExc(errorOperatorUsedInappropriately)
            }
            appendFnName(topSubexpr.operators.last(), false)
            topSubexpr.operators.removeLast()
        }
        topSubexpr.operators.add(FunctionCall(nameStringId, precedence, 1, maxArity, false, startByte))
    }
}


private void parseExpr(Token tok, Arr(Token) tokens, Parser* pr) {
    appendSpan(nodExpr, 0, startByte, lenBytes)
    val newFrame = ParseFrame(nodExpr, startNodeInd, sentinelToken)
    spanStack.push(newFrame)
    subexprs.add(ArrayList<Subexpr>())
    
    
    val functionStack = currFnDef.subexprs.last()
    while (lx.currTokInd < parseFrame.sentinelToken) {
        val tokType = lx.currTokenType()
        subexprClose(functionStack)
        val topSubexpr = functionStack.last()
        if (tokType == tokParens) {
            val subExprLenTokens = lx.currPayload2()
            exprIncrementArity(functionStack.last())
            lx.nextToken() // the parens token
            subexprInit(subExprLenTokens)
            continue
        } else if (tokType >= firstPunctTok) {
            throwExc(errorExpressionCannotContain)
        } else {
            when (tokType) {
                tokWord -> {
                    exprWord(topSubexpr)
                }
                tokInt -> {
                    exprOperand(topSubexpr)
                    currFnDef.appendNode(
                        nodInt, lx.currPayload1(), lx.currPayload2(),
                        lx.currStartByte(), lx.currLenBytes()
                    )
                }
                tokString -> {
                    exprOperand(topSubexpr)
                    currFnDef.appendNode(
                        nodString, 0, 0,
                        lx.currStartByte(), lx.currLenBytes()
                    )
                }
                tokOperator -> {
                    exprOperator(topSubexpr)
                }
            }
            lx.nextToken() // any leaf token
        }
    }
    subexprClose(functionStack)
    
}

/**
 * Finds the top-level punctuation opener by its index, and sets its node length.
 * Called when the parsing of a span is finished.
 */
private void setSpanLength(Int nodeInd, Parser* pr) {
    pr->nodes[nodeInd].payload2 = pr->nextInd - nodeInd - 1;
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
        if (pr->i < frame.sentinelToken) return;
        if (pr->i > frame.sentinelToken) {
            print("i = %d sentinel = %d", pr->i, frame.sentinelToken);
            throwExc(errorInconsistentSpan);
        }
        if (frame.tp == nodScope || frame.tp == nodExpr) {
            popScopeFrame(pr->activeBindings, pr->scopeStack);
        }
        pop(pr->backtrack);
        setSpanLength(frame.startNodeInd, pr);
    }
}

/** Parses top-level function bodies */
private void parseUpTo(Int sentinelToken, Arr(Token) tokens, Parser* pr) {     
    while (pr->i < sentinelToken) {
        Token currTok = tokens[pr->i];
        untt contextType = peek(pr->backtrack).tp;
        pr->i++;
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
    pr->i++; // the literal or ident token
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
    addNode((Node){.tp = nodBinding, .payload2 = newBindingId, 
            .startByte = bindingToken.startByte, .lenBytes = bindingToken.lenBytes}, pr);
    
    pr->i++; // the word token before the assignment sign
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
            //createExpr(nodExpr, lenTokens - 1, startOfExpr, lenBytes - startOfExpr + startByte);
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
ParserDefinition* buildParserDefinitions(Arena* a) {
    ParserDefinition* result = allocateOnArena(sizeof(ParserDefinition), a);
    result->resumableTable = tabulateResumableDispatch(a);
    result->nonResumableTable = tabulateNonresumableDispatch(a);
    return result;
}


Parser* createParser(Lexer* lr, Arena* a) {
    if (lr->wasError) {
        Parser* result = allocateOnArena(sizeof(Parser), a);
        result->wasError = true;
        return result;
    }
    
    Parser* result = allocateOnArena(sizeof(Parser), a);
    Arena* aBt = mkArena();
    Int stringTableLength = lr->stringTable->length;
    (*result) = (Parser) {.text = lr->inp, .inp = lr, .parDef = buildParserDefinitions(a),
        .scopeStack = createScopeStack(),
        .backtrack = createStackParseFrame(16, aBt), .i = 0,
        .stringTable = (Arr(Int))lr->stringTable->content, .strLength = stringTableLength,
        .bindings = allocateOnArena(sizeof(Binding)*64, a), .bindNext = 1, .bindCap = 64, // 0th binding is reserved
        .activeBindings = allocateOnArena(sizeof(Int)*stringTableLength, a),
        .nodes = allocateOnArena(sizeof(Node)*64, a), .nextInd = 0, .capacity = 64,
        .wasError = false, .errMsg = &empty, .a = a, .aBt = aBt
        };
    if (stringTableLength > 0) {
        memset(result->activeBindings, 0, stringTableLength*sizeof(Int));
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
Parser* parse(Lexer* lr, ParserDefinition* pDef, Arena* a) {
    Parser* pr = createParser(lr, a);
    int inpLength = lr->totalTokens;
    int i = 0;
    
    ParserFunc (*dispatch)[countNonresumableForms] = pDef->nonResumableTable;
    ResumeFunc (*dispatchResumable)[countResumableForms] = pDef->resumableTable;
    if (setjmp(excBuf) == 0) {
        parseToplevelTypes(lr, pr);    
        parseToplevelConstants(lr, pr);
        parseToplevelFunctionNames(lr, pr);
        parseFunctionBodies(lr, pr);
    }
    return pr;
}
