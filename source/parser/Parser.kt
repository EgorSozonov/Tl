package parser
import lexer.*
import java.lang.StringBuilder
import java.util.*
import kotlin.collections.ArrayList
import language.*
import lexer.RegularToken.*
import parser.RegularAST.*
import utils.IntPair
import utils.LOWER24BITS
import utils.LOWER27BITS
import utils.LOWER32BITS

class Parser(private val inp: Lexer) {


val ast = AST()

var wasError: Boolean
    private set
var errMsg: String
    private set

private val fnDefBacktrack = Stack<FunctionDef>()   // The stack of function definitions
private var currFnDef: FunctionDef
private val importedScope: LexicalScope

/** Map for interning/deduplication of identifier names */
private var allStrings = HashMap<String, Int>(50)

/** Index of first non-builtin in 'functions' of AST. For executable files, this is the index of the Entrypoint function */
var indFirstFunction: Int = 0
    private set

/** Index of first non-builtin in 'identifiers' of AST */
var indFirstName: Int = 0
    private set

private val parseTable: ArrayList<ArrayList<Parser.(Int, ParseFrame, Int, Int) -> Unit>>

/**
 * Main parser method
 */
fun parse(imports: ArrayList<Import>) {
    inp.currChunk = inp.firstChunk
    inp.currInd = 0

    try {
        insertImports(imports, inp.fileType)

        if (this.inp.fileType == FileType.executable) {
            currFnDef.funcId = indFirstFunction
            fnDefBacktrack.push(currFnDef)
            scopeInitEntryPoint()
        }

        while (inp.currTokInd < inp.totalTokens) {
            if (currFnDef.spanStack.isEmpty()) exitWithError(errorUnexpectedToken)

            val currFrame = currFnDef.spanStack.peek()
            val tokType = inp.currTokenType()

            val lenTokens = inp.currLenTokens()
            val startByte = inp.currStartByte()
            val lenBytes = inp.currLenBytes()

            val tableColumn = currFrame.spanType.internalVal.toInt() - firstSpanASTType
            if (tokType >= firstPunctuationTokenType) {
                inp.nextToken()
                parseTable[tokType - firstPunctuationTokenType + 1 ][tableColumn](lenTokens, currFrame, startByte, lenBytes)
            } else {
                parseTable[0][tableColumn](lenTokens, currFrame, startByte, lenBytes)
            }
            maybeCloseSpans()
        }
    } catch (e: Exception) {
        if (!wasError) {
            wasError = true
            errMsg = e.message ?: ""
        }

        println(e.message ?: "")
    }
}

//region ParserSpans

/**
 * Init of the implicit scope in a .tlx (executable) file.
 */
private fun scopeInitEntryPoint() {
    val totalTokens = inp.totalTokens
    val newScope = LexicalScope()

    detectNestedFunctions(newScope, totalTokens)
    currFnDef.appendSpan(SpanAST.functionDef, totalTokens, 0, inp.inp.size)
    currFnDef.openScopeFromExisting(1, totalTokens, newScope, 0, inp.inp.size) // 1, because node 0 will be functionDef
}

/**
 * Walks the current scope breadth-first without recursing into inner scopes.
 * The purpose is to find all names of functions defined (neseted) in this scope.
 * They are stored inside the LexicalScope in the reverse order they were found in (for fast deletion during codegen).
 * Does not consume any tokens.
 */
private fun detectNestedFunctions(newScope: LexicalScope, lenTokensScope: Int) {
    val originalPosition = inp.currentPosition()
    var j = 0

    while (j < lenTokensScope) {
        val currToken = inp.lookAhead(0)
        if (currToken.tType < firstPunctuationTokenType) {
            exitWithError(errorScope)
        }
        val lenTokensSpan = (currToken.payload and LOWER32BITS).toInt()
        if (currToken.tType == PunctuationToken.stmtFn.internalVal.toInt()) {
            inp.nextToken() // fn token
            var consumedTokens = 0

            val newFnDef = detectNestedFn(j, newScope, lenTokensScope, lenTokensSpan)
            newScope.funDefs.add(newFnDef.fst)
            consumedTokens += newFnDef.snd

            inp.skipTokens(lenTokensSpan - consumedTokens)
        } else {
            inp.skipTokens(lenTokensSpan + 1) // + 1 for the span token itself
        }

        j += lenTokensSpan + 1
    }

    newScope.funDefs.reverse()
    inp.seek(originalPosition)
}

/**
 * Adds the function from a single "fn" definition to current scope. Validates function name and that of its params.
 * Returns the pair [funcId, number of consumed tokens]
 */
private fun detectNestedFn(i: Int, newScope: LexicalScope, lenTokensScope: Int, lenTokensStatement: Int): IntPair {
    var arity = 0
    var j = i
    if (inp.currTokenType() != word.internalVal.toInt()) exitWithError(errorFnNameAndParams)

    val fnName = readString(word)
    inp.nextToken() // function name
    j += 2

    val nameSet = HashSet<String>(4)
    nameSet.add(fnName)

    while (j < lenTokensScope && inp.currTokenType() == wordType) {
        val paramName = readString(word)
        val existingBinding = lookupBinding(paramName)
        if (existingBinding != null || nameSet.contains(paramName)) {
            exitWithError(errorFnNameAndParams)
        }
        nameSet.add(paramName)
        addString(paramName)
        inp.nextToken() // the current param name
        j++
        arity++
    }
    val currTokType = inp.currTokenType()
    if (arity == 0) {
        exitWithError(errorFnNameAndParams)
    } else if (arity == lenTokensStatement || (currTokType != PunctuationToken.lexScope.internalVal.toInt() && currTokType != PunctuationToken.parens.internalVal.toInt())) {
        exitWithError(errorFnMissingBody)
    }

    val nameId = addString(fnName)
    val funcId = ast.functions.totalNodes
    ast.functions.appendThree(nameId, arity and LOWER24BITS, 0)

    if (lookupFunction(fnName, arity) != null) {
        exitWithError(errorDuplicateFunction)
    }
    if (newScope.functions.containsKey(fnName)) {
        val lstExisting = newScope.functions[fnName]!!
        lstExisting.add(IntPair(funcId, arity))
    } else {
        newScope.functions[fnName] = arrayListOf(IntPair(funcId, arity))
    }
    return IntPair(funcId, arity + 2) // +2 for the "fn" and the function name
}

/**
 * First it walks the current scope breadth-first without recursing into inner scopes.
 * The purpose is to find all names of functions defined in this scope.
 * Then it walks from the start again, this time depth-first, and writes instructions into the
 * AST scratch space
 */
private fun scopeInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    createScope(null, 0, lenTokens, startByte, lenBytes)
}

/**
 * Precondition: we've skipped the lexScope token and are pointing at the first token of content.
 */
private fun createScope(mbNewScope: LexicalScope?, extraTokenLength: Int,
                        lenTokens: Int, startByte: Int, lenBytes: Int) {
    // if we are in an expression, then this scope will be an operand for some operator
    val sentinelToken = inp.currTokInd + lenTokens
    if (currFnDef.subexprs.isNotEmpty()) {
        val funStack = currFnDef.subexprs.last()
        exprIncrementArity(funStack[funStack.size - 1])
    }

    val newScope = mbNewScope ?: LexicalScope()
    detectNestedFunctions(newScope, lenTokens)
    currFnDef.openScopeFromExisting(currFnDef.totalNodes, sentinelToken, newScope, startByte, lenBytes)
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
private fun maybeCloseSpans() {
    while (fnDefBacktrack.isNotEmpty()) { // loop over FunctionDefs
        while (currFnDef.spanStack.isNotEmpty()) { // loop over subscopes and expressions inside FunctionDef
            val frame = currFnDef.spanStack.peek()
            if (inp.currTokInd < frame.sentinelToken) return
            if (inp.currTokInd > frame.sentinelToken) {
                exitWithError(errorInconsistentSpan)
            }
            if (frame.spanType == SpanAST.scope) {
                currFnDef.subscopes.removeLast()
            } else if (frame.spanType == SpanAST.expression) {
                currFnDef.subexprs.removeLast()
            }
            currFnDef.spanStack.pop()
            currFnDef.setSpanLength(frame.indStartNode)

        }

        val freshlyMintedFn = fnDefBacktrack.pop()
        ast.storeFreshFunction(freshlyMintedFn)

        if (fnDefBacktrack.isNotEmpty()) {
            currFnDef = fnDefBacktrack.peek()
        }
    }
}

//endregion

//region CoreForms

/** Inserts the "break" statement. Validates that we are inside at least one loop. */
private fun coreBreakInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    validateInsideLoop()
    currFnDef.appendSpan(SpanAST.breakSpan, 0, startByte, lenBytes)
}


private fun validateInsideLoop() {
    for (i in (currFnDef.spanStack.size - 1) downTo 0) {
        if (currFnDef.spanStack[i].spanType == SpanAST.loop) return
    }
    exitWithError(errorLoopBreakOutside)
}


private fun coreCatchInit(lenTokens: Int, startByte: Int, lenBytes: Int) {
    //validateCoreForm(stmtType, lenTokens)
}


private fun coreCatch(f: ParseFrame) {
    //validateCoreForm(stmtType, lenTokens)
}

/**
 * Parses a core form: function definition, like "fn foo x y { x + y }"
 * or "fn bar x y { print x; x + y }"
 * This function doesn't do the validation that has already been done in scopeInitAddNestedFn()
 */
private fun coreFnInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    val currentScope = currFnDef.subscopes.last()
    val newFunScope = LexicalScope()
    val funcId = currentScope.funDefs.removeLast()
    val funcSignature = ast.getFunc(funcId)

    inp.nextToken() // the function name

    val newFnDef = FunctionDef(funcId, funcSignature.arity, functionPrec)
    fnDefBacktrack.push(newFnDef)

    newFnDef.appendSpan(SpanAST.functionDef, 0, startByte, lenBytes)

    var j = 0
    while (j < funcSignature.arity) {
        val paramName = readString(word)

        newFunScope.bindings[paramName] = allStrings[paramName]!!
        newFnDef.appendName(allStrings[paramName]!!, inp.currStartByte(), paramName.length)

        inp.nextToken() // function parameter
        j++
    }

    val currTokenType = inp.currTokenType()
    if (predicateFunctionBody(currTokenType)) exitWithError(errorFnMissingBody)

    currFnDef.appendSpanWithPayload(SpanAST.fnDefPlaceholder, newFnDef.funcId, 0, 0, 0)

    val newScopeLenTokens = inp.currLenTokens()
    val newScopelenBytes = inp.currLenBytes()
    val newScopeStartByte = inp.currStartByte()

    if (newScopeLenTokens + newFnDef.arity + 2 != lenTokens) exitWithError(errorInconsistentSpan) // + 2 = fnName + coreForm token
    inp.nextToken() // the scope token for the function body
    currFnDef = newFnDef
    createScope(newFunScope, newFnDef.arity + 2, newScopeLenTokens, newScopeStartByte, newScopelenBytes)
}

private fun predicateFunctionBody(tokenType: Int): Boolean {
    return tokenType != PunctuationToken.lexScope.internalVal.toInt()
}

/** The start of an if statement set. Looks ahead to validate: 1) the number of clauses is even;
 * 2) the odd clauses are all plain expressions
 * 3) the "_" clause, if any, is penultimate
 *
 * Precondition: we are standing past the "if" reserved word
 *
 * Example syntax:
 * (?  (x > 5) -> (print "> 5")
 *     (x > 0) -> (print "(0; 5]")
 *              : (print "< 0")
 */
private fun coreIfInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    if (lenTokens < 2) exitWithError(errorCoreFormTooShort)

    currFnDef.openContainerSpan(SpanAST.ifSpan, inp.currTokInd + lenTokens, startByte, lenBytes)
    //coreIf(lenTokens, currFnDef.spanStack.last(), startByte, lenBytes)
    currFnDef.openContainerSpan(SpanAST.ifClauseGroup, f.sentinelToken, inp.currStartByte(), 0)
    val clauseFrame = currFnDef.spanStack.peek()

    // we accept boolean literals, identifiers and expressions; things like "!a" are allowed to be without parens
    val currTokType = inp.currTokenType()
    if (currTokType == wordType) {
        parseIdent()
    } else if (currTokType == boolTok.internalVal.toInt()) {
        currFnDef.appendNode(litBool, 0, inp.currChunk.tokens[inp.currInd + 3], inp.currStartByte(), inp.currLenBytes())
        inp.nextToken() // the literal token
    } else if (currTokType == operatorTok.internalVal.toInt()) {
        if (OperatorToken.fromInt(inp.currChunk.tokens[inp.currInd + 3]).opType == OperatorType.elseBranch) {
            currFnDef.appendNode(underscore, 0, 0, inp.currStartByte(), 1)
        } else {
            parsePrefixFollowedByAtom(f.sentinelToken)
        }
    } else if (currTokType == PunctuationToken.parens.internalVal.toInt()) {
        val parenLenTokens = inp.currLenTokens()
        val parenLenBytes = inp.currLenBytes()
        inp.nextToken()
        expressionInit(parenLenTokens, f, inp.currStartByte(), parenLenBytes)
    } else {
        exitWithError(errorIfLeft)
    }
}


private fun coreIfAtom(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    currFnDef.openContainerSpan(SpanAST.ifClauseGroup, f.sentinelToken, inp.currStartByte(), 0)

    // we accept boolean literals, identifiers and expressions; things like "!a" are allowed to be without parens
    val currTokType = inp.currTokenType()
    if (currTokType == wordType) {
        parseIdent()
    } else if (currTokType == boolTok.internalVal.toInt()) {
        currFnDef.appendNode(litBool, 0, inp.currChunk.tokens[inp.currInd + 3], inp.currStartByte(), inp.currLenBytes())
        inp.nextToken() // the literal token
    } else if (currTokType == operatorTok.internalVal.toInt()) {
        if (OperatorToken.fromInt(inp.currChunk.tokens[inp.currInd + 3]).opType == OperatorType.elseBranch) {
            currFnDef.appendNode(underscore, 0, 0, inp.currStartByte(), 1)
        } else {
            parsePrefixFollowedByAtom(f.sentinelToken)
        }
    } else {
        exitWithError(errorIfLeft)
    }
}


private fun coreIfParens(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    currFnDef.openContainerSpan(SpanAST.ifClauseGroup, f.sentinelToken, startByte, 0)

    // we accept boolean literals, identifiers and expressions; things like "!a" are allowed to be without parens
    expressionInit(lenTokens, f, startByte, lenBytes)
}


private fun coreIfClAtom(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    // we accept anything that can be evaluated (expressions, scopes, expression-like core forms etc)
    var currTokType = inp.currTokenType()
    if (currTokType != operatorTok.internalVal.toInt()) {
        exitWithError(errorIfMalformed)
    }
    val opType = OperatorToken.fromInt(inp.currChunk.tokens[inp.currInd + 3]).opType
    if (opType != OperatorType.arrowRight && opType != OperatorType.elseBranch) exitWithError(errorIfMalformed)
    inp.nextToken() // the "->" or the ":" token

    currTokType = inp.currTokenType()
    val sentinelByteRight = inp.currStartByte() + inp.currLenBytes()
    val parsedLiteralOrIdent = parseLiteralOrIdentifier(currTokType)
    if (!parsedLiteralOrIdent) {
        if (currTokType == operatorTok.internalVal.toInt()) {
            parsePrefixFollowedByAtom(f.sentinelToken)
        } else {
            val rightLenTokens = inp.currLenTokens()
            val rightStartByte = inp.currStartByte()
            val rightLenBytes = inp.currLenBytes()
            if (currTokType == PunctuationToken.parens.internalVal.toInt()) {
                expressionInit(rightLenTokens, f, rightStartByte, rightLenBytes)
            } else if (currTokType == PunctuationToken.lexScope.internalVal.toInt()) {
                scopeInit(rightLenTokens, f, rightStartByte, rightLenBytes)
            } else {
                exitWithError(errorIfRight)
            }
        }
    }
    currFnDef.setSpanLenBytes(f.indStartNode, sentinelByteRight)
    currFnDef.spanStack.pop()
}

/** Parses sequences like "!!true" or "'b" into an expression. This is necessary for parentheses reduction */
private fun parsePrefixFollowedByAtom(sentinelToken: Int) {
    val startByte = inp.currStartByte()
    val opers = ArrayList<IntPair>() // {id of function, startByte}
    while (inp.currTokInd < sentinelToken) {
        val tokType = inp.currTokenType()
        if (tokType == wordType) {
            break
        } else if (tokType == operatorTok.internalVal.toInt()) {
            val operInfo = OperatorToken.fromInt(inp.currChunk.tokens[inp.currInd + 3])
            if (operatorDefinitions[operInfo.opType.internalVal].precedence == prefixPrec) {
                opers.add(IntPair(operInfo.opType.internalVal, inp.currStartByte()))
            } else {
                exitWithError(errorIncorrectPrefixSequence)
            }
        } else {
            exitWithError(errorIncorrectPrefixSequence)
        }
        inp.nextToken() // the prefix operator
    }
    val wordStartByte = inp.currStartByte()
    val wordLenBytes = inp.currLenBytes()
    currFnDef.appendSpan(SpanAST.expression, opers.size + 1, startByte, wordStartByte - startByte + wordLenBytes)
    parseIdent()

    for (i in (opers.size - 1) downTo 0) {
        currFnDef.appendNode(idFunc, -1, opers[i].fst, opers[i].snd, 1)
    }
    return
}

/** Consumes 1 token */
private fun parseIdent() {
    val identName = readString(word)
    val mbBinding = lookupBinding(identName) ?: exitWithError(errorUnknownBinding)
    currFnDef.appendNode(ident, 0, mbBinding, inp.currStartByte(), identName.length)
    inp.nextToken() // the identifier token
}

/** Parses 0 or 1 token. Returns false if didn't parse anything. */
private fun parseLiteralOrIdentifier(currTokType: Int): Boolean {
    if (currTokType <= topVerbatimTokenVariant) {
        currFnDef.currChunk.nodes[currFnDef.nextInd    ] = inp.currChunk.tokens[inp.currInd    ]
        currFnDef.currChunk.nodes[currFnDef.nextInd + 1] = inp.currChunk.tokens[inp.currInd + 1]
        currFnDef.currChunk.nodes[currFnDef.nextInd + 2] = inp.currChunk.tokens[inp.currInd + 2]
        currFnDef.currChunk.nodes[currFnDef.nextInd + 3] = inp.currChunk.tokens[inp.currInd + 3]
        inp.nextToken()
        currFnDef.bump()
        currFnDef.ensureSpaceForNode()
        return true
    }
    when (currTokType) {
        wordType -> {
            parseIdent()
            return true
        }
        litString.internalVal.toInt() -> {
            // TODO interpolation, escapes
            currFnDef.appendNode(litString, 0, inp.currChunk.tokens[inp.currInd + 3], inp.currStartByte(), inp.currLenBytes())
        }
        else -> {
            return false
        }
    }
    inp.nextToken() // the literal or ident token
    return true
}


private fun coreIfEqInit(lenTokens: Int, startByte: Int, lenBytes: Int) {

}

private fun coreIfEq(f: ParseFrame) {

}

private fun coreIfPredInit(lenTokens: Int, startByte: Int, lenBytes: Int) {

}

private fun coreIfPred(f: ParseFrame) {

}


private fun coreLoopInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    if (lenTokens == 0) exitWithError(errorCoreFormTooShort)

    val sentinelToken = inp.currTokInd + lenTokens
    currFnDef.openContainerSpan(SpanAST.loop, sentinelToken, startByte, lenBytes )
    currFnDef.openScope(currFnDef.totalNodes, sentinelToken, startByte + 5, lenBytes - 5) // 5 for the "loop " bytes
}

private fun coreMatch(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreReturnInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    createExpr(SpanAST.returnExpression, lenTokens, startByte, lenBytes)
}

private fun coreTry(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreWhile(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreUnexpectedInit(lenTokens: Int, startByte: Int, lenBytes: Int) {
    exitWithError(errorCoreFormUnexpected)
}

private fun spErr(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    exitWithError(errorCoreFormUnexpected)
}


private fun validateCoreForm(stmtType: Int, lenTokens: Int) {
    if (lenTokens < 3) {
        exitWithError(errorCoreFormTooShort)
    }
}

//endregion

//region DataInitializersAccessors

private fun dataInitializerInit(payload1: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    // TODO
}
private fun dataInitializer(f: ParseFrame) {
    // TODO
}


private fun dataAccessorInit(payload1: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    // TODO
}


private fun dataAccessor(f: ParseFrame) {
    // TODO
}

//endregion

//region TypeDeclarations

private fun typeDeclarationInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    if (lenTokens == 1) {
        currFnDef.appendSpan(SpanAST.typeDecl, 1, startByte, lenBytes)
        typeExpressionWord()
        inp.nextToken() // the word
        return
    }

    currFnDef.openTypeDecl(currFnDef.totalNodes, inp.currTokInd + lenTokens, startByte, lenBytes)
    typeSubExprInit(lenTokens)
    typeDeclaration(currFnDef.spanStack.last())
}


private fun typeDeclaration(f: ParseFrame) {
    val functionStack = currFnDef.subexprs.last()
    while (inp.currTokInd < f.sentinelToken) {
        val currTokType = inp.currTokenType()
        typeCloseSubEx(functionStack)
        val topSubexpr = functionStack.last()
        if (currTokType == word.internalVal.toInt()) {
            typeExpressionWord()
            typeExOperand(topSubexpr)
        } else if (currTokType == dotWord.internalVal.toInt()) {
            val fnName = readString(dotWord)
            if (!allStrings.containsKey(fnName)) exitWithError(errorUnknownFunction)
            val payload2 = inp.currChunk.tokens[inp.currInd + 3]
            typeExFuncall(allStrings[fnName]!!, functionPrec, 0, topSubexpr, payload2)
        } else if (currTokType == operatorTok.internalVal.toInt()) {
            typeExOperator(topSubexpr)
        } else if (currTokType == PunctuationToken.parens.internalVal.toInt()) {
            val subExprLenTokens = inp.currChunk.tokens[inp.currInd + 3]
            typeExIncrementArity(topSubexpr)
            inp.nextToken() // the parens token
            typeSubExprInit(subExprLenTokens)
            continue
        } else {
            exitWithError(errorTypeDeclCannotContain)
        }
        inp.nextToken() // any leaf token
    }
    typeCloseSubEx(functionStack)
}


private fun typeExpressionWord() {
    val payload2 = inp.currChunk.tokens[inp.currInd + 3]
    val startByte = inp.currStartByte()
    val lenBytes = inp.currLenBytes()
    val theWord = readString(word)
    if (payload2 > 0) {
        val indType = lookupType(theWord) ?: exitWithError(errorUnknownType)
        currFnDef.appendNode(idType, payload2, indType, startByte, lenBytes)
    } else {
        // type parameter
        val stringId = addString(theWord)
        currFnDef.appendNode(idType, 0, stringId, startByte, lenBytes)
    }
}

/**
 * For functions, the first param is the stringId of the name.
 * For operators, it's the negative index of operator from its payload
 */
private fun typeExFuncall(nameStringId: Int, precedence: Int, maxArity: Int, topSubexpr: Subexpr, payload2: Int) {
    val startByte = inp.currChunk.tokens[inp.currInd + 1]

    if (topSubexpr.isStillPrefix) {
        if (topSubexpr.operators.size != 1) exitWithError(errorTypeDeclError)
        val prefixFunction = topSubexpr.operators.removeLast()
        typeAppendFunc(prefixFunction, true)

        val newFun = FunctionCall(nameStringId, precedence, 1, maxArity, payload2 > 0, startByte)
        topSubexpr.isStillPrefix = false
        topSubexpr.operators.add(newFun)
    } else {
        if (nameStringId < -1 && precedence != prefixPrec) {
            // infix operator, need to check if it's the first in this subexpr
            if (topSubexpr.operators.size == 1 && topSubexpr.operators[0].nameStringId == -1) {
                if (topSubexpr.operators[0].arity != 1) exitWithError(errorExpressionInfixNotSecond)

                val newFun = FunctionCall(nameStringId, precedence, 1, maxArity, false, startByte)
                topSubexpr.operators[0] = (newFun)
                return
            }
        }
        while (topSubexpr.operators.isNotEmpty() && topSubexpr.operators.last().precedence >= precedence) {
            if (topSubexpr.operators.last().precedence == prefixPrec) {
                exitWithError(errorOperatorUsedInappropriately)
            }
            typeAppendFunc(topSubexpr.operators.last(), false)
            topSubexpr.operators.removeLast()
        }
        topSubexpr.operators.add(FunctionCall(nameStringId, precedence, 1, maxArity, payload2 > 0, startByte))
    }
}

/**
 * State modifier that must be called whenever an operand is encountered in an expression parse
 */
private fun typeExOperand(topSubExpr: Subexpr) {
    if (topSubExpr.operators.isEmpty()) return

    // flush all unaries
    val opers = topSubExpr.operators
    while (opers.last().precedence == prefixPrec) {
        typeAppendFunc(opers.last(), false)
        opers.removeLast()
    }
    typeExIncrementArity(topSubExpr)
}


private fun typeExOperator(topSubexpr: Subexpr) {
    val operTypeIndex = inp.currChunk.tokens[inp.currInd + 3].toLong()
    val operInfo = functionalityOfOper(operTypeIndex)
    if (operInfo.first < 0) exitWithError(errorOperatorUsedInappropriately)

    if (operInfo.second == prefixPrec) {
        val startByte = inp.currChunk.tokens[inp.currInd + 1]
        topSubexpr.operators.add(FunctionCall(-operInfo.first - 2, prefixPrec, 1, 1, false, startByte))
    } else {
        typeExFuncall(-operInfo.first - 2, operInfo.second, operInfo.third, topSubexpr, 0)
    }
}

/**
 * Adds a frame for a subexpression.
 * Precondition: the current token must be past the opening paren / statement token
 * Examples: "Map String Int"
 *           "Foo + Bar"
 */
private fun typeSubExprInit(lenTokens: Int) {
    val firstTok = inp.lookAhead(0)
    val subexprs = currFnDef.subexprs.last()
    if (lenTokens == 1) {
        if (firstTok.tType != wordType) exitWithError(errorUnexpectedToken)
        typeExpressionWord()
        typeExOperand(subexprs.last())
    } else {
        // Need to determine if this subexpr is in prefix form.
        // It is prefix if it starts with a word and the second token is not an infixOperator
        var isPrefix = true
        if (firstTok.tType == wordType) {
            val secondToken = inp.lookAhead(1)
            if (secondToken.tType == operatorTok.internalVal.toInt()) {
                val operToken = OperatorToken.fromInt(secondToken.payload.toInt())
                if (operatorDefinitions[operToken.opType.internalVal].precedence != prefixPrec) {
                    isPrefix = false
                }
            }
            if (isPrefix) {
                typeOpenPrefixFunction(subexprs, lenTokens)
                inp.nextToken() // the first token of this subexpression, which is a word
                return
            }
        } else {
            isPrefix = false
        }

        subexprs.add(Subexpr(arrayListOf(FunctionCall(-1, functionPrec, 0, 0, false, 0)),
                                  inp.currTokInd + lenTokens, isStillPrefix = isPrefix, false))
    }
}


private fun typeOpenPrefixFunction(functionStack: ArrayList<Subexpr>, lenTokens: Int) {
    val funcName = readString(word)
    val payload2 = inp.currChunk.tokens[inp.currInd + 3]
    val startByte = inp.currStartByte()
    if (payload2 > 0) {
        if (!allStrings.containsKey(funcName)) exitWithError(errorUnknownFunction)
        val funcNameId = allStrings[funcName]!!

        functionStack.add(Subexpr(arrayListOf(FunctionCall(funcNameId, functionPrec, 0, 0, payload2 > 0, startByte)),
                                  inp.currTokInd + lenTokens, isStillPrefix = true, false))
    } else {
        val stringId = addString(funcName)
        functionStack.add(Subexpr(arrayListOf(FunctionCall(stringId, functionPrec, 0, 0, false, startByte)),
                                  inp.currTokInd + lenTokens, isStillPrefix = true, false))
    }
}

/** Close subexpressions in a type expression */
private fun typeCloseSubEx(funCallStack: ArrayList<Subexpr>) {
    for (k in (funCallStack.size - 1) downTo 0) {
        val funInSt = funCallStack[k]
        if (inp.currTokInd != funInSt.sentinelToken) return
        if (funInSt.operators.size == 1) {
            if (funInSt.isStillPrefix) {
                typeAppendFunc(funInSt.operators[0], true)
            } else if (funInSt.operators[0].nameStringId == -1) {
                if (funInSt.operators[0].arity != 1) {
                    // this is cases like "(1 2 3)" or "(!x y)"
                    exitWithError(errorExpressionFunctionless)
                }
            } else {
                for (m in funInSt.operators.size - 1 downTo 0) {
                    typeAppendFunc(funInSt.operators[m], false)
                }
            }
        } else {
            for (m in funInSt.operators.size - 1 downTo 0) {
                typeAppendFunc(funInSt.operators[m], false)
            }
        }

        funCallStack.removeLast()

        // flush parent's prefix opers, if any, because this subexp was their operand
        if (funCallStack.isNotEmpty()) {
            val opersPrev = funCallStack[k - 1].operators
            for (n in (opersPrev.size - 1) downTo 0) {
                if (opersPrev[n].precedence == prefixPrec) {
                    typeAppendFunc(opersPrev[n], false)
                    opersPrev.removeLast()
                } else {
                    break
                }
            }
        }
    }
}

/**
 * Appends a node that represents a function name in a function call.
 * For functions, fnParse.nameStringId is the id of the function name.
 * For operators, it's (-indOper - 1), where indOper is the index inside 'operatorDefinitions' array.
 */
private fun typeAppendFunc(fnParse: FunctionCall, isPrefixWord: Boolean) {
    if (fnParse.nameStringId > -1) {
        // functions
        val fnName = ast.identifiers[fnParse.nameStringId]
        val lenBytes = if (isPrefixWord) { fnName.length } else { fnName.length + 1 } // +1 because of the dot if infix
        if (fnParse.isCapitalized) {
            val fnId = lookupTypeFunction(fnName, fnParse.arity) ?: exitWithError(errorUnknownTypeFunction)
            val payload1 = (1 shl 30) + (fnParse.arity and LOWER27BITS)
            currFnDef.appendNode(typeFunc, payload1, fnId, fnParse.startByte, lenBytes)
        } else {
            currFnDef.appendNode(typeFunc, (fnParse.arity and LOWER27BITS), fnParse.nameStringId, fnParse.startByte, lenBytes)
        }
    } else {
        // operators
        val operInfo = operatorDefinitions[-fnParse.nameStringId - 2]
        val payload1 = (1 shl 31) + (fnParse.arity and LOWER27BITS)
        currFnDef.appendNode(typeFunc, payload1, -fnParse.nameStringId - 2, fnParse.startByte,
            operInfo.name.length)
    }
}

/**
 * Increments the arity of the top non-prefix operator. The prefix ones are ignored because there is no
 * need to track their arity: they are flushed on first operand or on closing of the first subexpr after them.
 */
private fun typeExIncrementArity(topFrame: Subexpr) {
    var n = topFrame.operators.size - 1
    while (n > -1 && topFrame.operators[n].precedence == prefixPrec) {
        n--
    }
    if (n < 0) exitWithError(errorExpressionError)

    val oper = topFrame.operators[n]
    oper.arity++
    if (oper.maxArity > 0 && oper.arity > oper.maxArity) exitWithError(errorOperatorWrongArity)
}


private fun lookupType(name: String): Int? {
    for (f in (fnDefBacktrack.size - 1) downTo 0) {
        val fnDef = fnDefBacktrack[f]
        val subscopes = fnDef.subscopes
        for (j in (subscopes.size - 1) downTo 0) {
            if (subscopes[j].types.containsKey(name)) {
                return subscopes[j].types[name]
            }
        }
    }

    return importedScope.types.getOrDefault(name, null)
}


private fun lookupTypeFunction(name: String, arity: Int): Int? {
    val subscopes = currFnDef.subscopes
    for (j in (subscopes.size - 1) downTo 0) {
        if (subscopes[j].functions.containsKey(name)) {
            val lst = subscopes[j].functions[name]!!
            for (funcId in lst) {
                if (funcId.snd == arity) return funcId.fst
            }
        }
    }
    if (importedScope.functions.containsKey(name)) {
        val lst = importedScope.functions[name]!!
        for (funcId in lst) {
            if (funcId.snd == arity) return funcId.fst
        }
    }
    return null
}

//endregion

//region Expressions

/**
 * An expression, i.e. a series of funcalls and/or operator calls.
 * Precondition: the expression token has already been skipped
 * 'additionalPrefixToken' - set to 1 if
 */
private fun expressionInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    createExpr(SpanAST.expression, lenTokens, startByte, lenBytes)
}


private fun createExpr(exprType: SpanAST, lenTokens: Int, startByte: Int, lenBytes: Int)  {
    if (lenTokens == 1) {
        val theToken = inp.lookAhead(0)
        exprSingleItem(theToken)
        return
    }

    if (exprType == SpanAST.expression) {
        currFnDef.openExpression(currFnDef.totalNodes, inp.currTokInd + lenTokens, startByte, lenBytes)
    } else {
        currFnDef.openRetExpression(currFnDef.totalNodes, inp.currTokInd + lenTokens, startByte, lenBytes)
    }

    subexprInit(lenTokens)

    expression(currFnDef.spanStack.last())
}

/**
 * Parses (a part of) an expression. Uses an extended Shunting Yard algorithm from Dijkstra to
 * flatten all internal parens into a single "Reverse Polish Notation" stream.
 * I.e. into a post-order traversal of a function call tree. In the resulting AST nodes, the function names are
 * annotated with arities.
 * TODO also flatten dataInits and dataIndexers (i.e. {} and [])
 */
private fun expression(parseFrame: ParseFrame) {
    val functionStack = currFnDef.subexprs.last()
    while (inp.currTokInd < parseFrame.sentinelToken) {
        val tokType = inp.currTokenType()
        subexprClose(functionStack)
        val topSubexpr = functionStack.last()
        if (tokType == PunctuationToken.parens.internalVal.toInt()) {
            val subExprLenTokens = inp.currChunk.tokens[inp.currInd + 3]
            exprIncrementArity(functionStack.last())
            inp.nextToken() // the parens token
            subexprInit(subExprLenTokens)
            continue
        } else if (tokType >= firstPunctuationTokenType) {
            exitWithError(errorExpressionCannotContain)
        } else {
            when (tokType) {
                word.internalVal.toInt() -> {
                    exprWord(topSubexpr)
                }
                intTok.internalVal.toInt() -> {
                    exprOperand(topSubexpr)
                    currFnDef.appendNode(
                        litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
                        inp.currStartByte(), inp.currLenBytes()
                    )
                }
                stringTok.internalVal.toInt() -> {
                    exprOperand(topSubexpr)
                    currFnDef.appendNode(
                        litString, 0, 0,
                        inp.currStartByte(), inp.currLenBytes()
                    )
                }
                dotWord.internalVal.toInt() -> {
                    val fnName = readString(dotWord)
                    if (!allStrings.containsKey(fnName)) exitWithError(errorUnknownFunction)
                    exprFuncall(allStrings[fnName]!!, functionPrec, 0, topSubexpr)
                }
                operatorTok.internalVal.toInt() -> {
                    exprOperator(topSubexpr)
                }
            }
            inp.nextToken() // any leaf token
        }
    }
    subexprClose(functionStack)
}

/**
 * A single-item subexpression, like "(5)" or "x". Cannot be a function call.
 */
private fun exprSingleItem(theTok: TokenLite) {
    if (parseLiteralOrIdentifier(theTok.tType)) {
    } else if (theTok.tType == reservedWord.internalVal.toInt()) {
        exitWithError(errorCoreFormInappropriate)
    } else {
        exitWithError(errorUnexpectedToken)
    }
}


private fun exprWord(topSubexpr: Subexpr) {
    val theWord = readString(word)

    val binding = lookupBinding(theWord)
    if (binding != null) {
        currFnDef.appendNode(ident, 0, binding,
            inp.currStartByte(), inp.currLenBytes())
        exprOperand(topSubexpr)
    } else {
        exitWithError(errorUnknownBinding)
    }
}

/**
 * For functions, the first param is the stringId of the name.
 * For operators, it's the negative index of operator from its payload
 */
private fun exprFuncall(nameStringId: Int, precedence: Int, maxArity: Int, topSubexpr: Subexpr) {
    val startByte = inp.currChunk.tokens[inp.currInd + 1]

    if (topSubexpr.isStillPrefix) {
        if (topSubexpr.operators.size != 1) exitWithError(errorExpressionError)
        val prefixFunction = topSubexpr.operators.removeLast()
        appendFnName(prefixFunction, true)

        val newFun = FunctionCall(nameStringId, precedence, 1, maxArity, false, startByte)
        topSubexpr.isStillPrefix = false
        topSubexpr.operators.add(newFun)
    } else {
        if (nameStringId < -1 && precedence != prefixPrec) {
            // infix operator, need to check if it's the first in this subexpr
            if (topSubexpr.operators.size == 1 && topSubexpr.operators[0].nameStringId == -1) {
                if (topSubexpr.operators[0].arity != 0) exitWithError(errorExpressionInfixNotSecond)

                val newFun = FunctionCall(nameStringId, precedence, 1, maxArity, false, startByte)
                topSubexpr.operators[0] = newFun
                return
            }
        }
        while (topSubexpr.operators.isNotEmpty() && topSubexpr.operators.last().precedence >= precedence) {
            if (topSubexpr.operators.last().precedence == prefixPrec) {
                exitWithError(errorOperatorUsedInappropriately)
            }
            appendFnName(topSubexpr.operators.last(), false)
            topSubexpr.operators.removeLast()
        }
        topSubexpr.operators.add(FunctionCall(nameStringId, precedence, 1, maxArity, false, startByte))
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
    val operTypeIndex = inp.currChunk.tokens[inp.currInd + 3].toLong()
    val operInfo = functionalityOfOper(operTypeIndex)
    if (operInfo.first < 0) exitWithError(errorOperatorUsedInappropriately)

    if (operInfo.second == prefixPrec) {
        val startByte = inp.currStartByte()
        topSubexpr.operators.add(FunctionCall(-operInfo.first - 2, prefixPrec, 1, 1, false, startByte))
    } else {
        exprFuncall(-operInfo.first - 2, operInfo.second, operInfo.third, topSubexpr)
    }
}

/**
 * Adds a frame for a subexpression.
 * Precondition: the current token must be 1 past the opening paren / statement token
 * Examples: "foo 5 a"
 *           "5 + !a"
 * TODO: allow for core forms (but not scopes!)
 */
private fun subexprInit(lenTokens: Int) {
    val firstTok = inp.lookAhead(0)
    val subexprs = currFnDef.subexprs.last()
    val isHeadPosition = if (subexprs.isEmpty()) {
        false
    } else {
        val currSubexpr = subexprs.last()
        currSubexpr.isStillPrefix && currSubexpr.operators.size == 1
                && currSubexpr.operators[0].arity == 0
    }
    if (lenTokens == 1) {
        exprSingleItem(firstTok)
    } else {
        // Need to determine if this subexpr is in prefix form.
        // It is prefix if it starts with a word and the second token is not an infixOperator
        var isPrefix = true
        if (firstTok.tType == wordType) {
            val secondToken = inp.lookAhead(1)
            if (secondToken.tType == operatorTok.internalVal.toInt()) {
                val operToken = OperatorToken.fromInt(secondToken.payload.toInt())
                if (operatorDefinitions[operToken.opType.internalVal].precedence != prefixPrec) {
                    isPrefix = false
                }
            }
            if (isPrefix) {
                exprOpenPrefixFunction(subexprs, lenTokens, isHeadPosition)
                inp.nextToken() // the first token of this subexpression, which is a word
                return
            }
        } else if (firstTok.tType != PunctuationToken.parens.internalVal.toInt()) {
            isPrefix = false
        }
        currFnDef.subexprs.last().add(Subexpr(arrayListOf(FunctionCall(-1, 0, 0, 0, false, 0)),
                                              inp.currTokInd + lenTokens, isPrefix, isHeadPosition))
    }
}

private fun exprOpenPrefixFunction(functionStack: ArrayList<Subexpr>, lenTokens: Int, isHeadPosition: Boolean) {
    val funcName = readString(word)
    val startByte = inp.currStartByte()

    if (!allStrings.containsKey(funcName)) exitWithError(errorUnknownFunction)
    val funcNameId = allStrings[funcName]!!

    functionStack.add(Subexpr(arrayListOf(FunctionCall(funcNameId, functionPrec, 0, 0, false, startByte)),
                              inp.currTokInd + lenTokens, isStillPrefix = true, isInHeadPosition = isHeadPosition))
}

/**
 * Flushes the finished subexpr frames from the top of the funcall stack.
 * A subexpr frame is finished when it has no tokens left.
 * Flushing includes appending its operators, clearing the operator stack, and appending
 * prefix unaries from the previous subexpr frame, if any.
 */
private fun subexprClose(funCallStack: ArrayList<Subexpr>) {
    for (k in (funCallStack.size - 1) downTo 0) {
        val currSubexpr = funCallStack[k]
        if (inp.currTokInd != currSubexpr.sentinelToken) return

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
                exitWithError(errorExpressionFunctionless)
            } else {
                for (m in currSubexpr.operators.size - 1 downTo 0) {
                    appendFnName(currSubexpr.operators[m], false)
                }
            }
        } else if (currSubexpr.isInHeadPosition) {
            exitWithError(errorExpressionHeadFormOperators)
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
private fun exprIncrementArity(topFrame: Subexpr) {
    var n = topFrame.operators.size - 1
    while (n > -1 && topFrame.operators[n].precedence == prefixPrec) {
        n--
    }
    if (n < 0) {
        exitWithError(errorExpressionError)
    }
    val oper = topFrame.operators[n]
    if (oper.nameStringId == -1) return
    oper.arity++
    if (oper.maxArity > 0 && oper.arity > oper.maxArity) {
        exitWithError(errorOperatorWrongArity)
    }
}

/**
 * Appends a node that represents a function name in a function call.
 * For functions, fnParse.nameStringId is the id of the function name.
 * For operators, it's (-indOper - 1), where indOper is the index inside 'operatorDefinitions' array.
 */
private fun appendFnName(fnParse: FunctionCall, isPrefixWord: Boolean) {
    if (fnParse.nameStringId > -1) {
        // functions
        val fnName = ast.identifiers[fnParse.nameStringId]
        val fnId = lookupFunction(fnName, fnParse.arity) ?: exitWithError(errorUnknownFunction)
        val lenBytes = if (isPrefixWord) { fnName.length } else { fnName.length + 1 } // +1 because of the dot if infix
        currFnDef.appendNode(idFunc, fnParse.arity, fnParse.nameStringId, fnParse.startByte, lenBytes)
    } else {
        // operators
        val operInfo = operatorDefinitions[-fnParse.nameStringId - 2]
        if (operInfo.arity != fnParse.arity) exitWithError(errorOperatorWrongArity)
        currFnDef.appendNode(idFunc, -fnParse.arity, -fnParse.nameStringId - 2, fnParse.startByte,
            operInfo.name.length)
    }
}

//endregion

//region Assignments

/**
 * Starts parsing an assignment statement like "x = y + 5".
 */
private fun assignmentInit(lenTokens: Int, f: ParseFrame, startByte: Int, lenBytes: Int) {
    if (lenTokens < 2) exitWithError(errorAssignment)

    val sentinelToken = inp.currTokInd + lenTokens
    val fstTokenType = inp.currTokenType()
    val numPrefixTokens = inp.currChunk.tokens[inp.currInd + 2] // Equals 1 for now
    if (fstTokenType != word.internalVal.toInt()) exitWithError(errorAssignment)

    val bindingName = readString(word)
    inp.nextToken() // the name of the binding

    val mbBinding = lookupBinding(bindingName)
    if (mbBinding != null) exitWithError(errorAssignmentShadowing)

    val strId = addString(bindingName)
    currFnDef.subscopes.last().bindings[bindingName] = strId

    currFnDef.openAssignment(currFnDef.totalNodes, sentinelToken, startByte, lenBytes)
    currFnDef.appendNode(binding, 0, strId, startByte, bindingName.length)
    val startOfExpr = inp.currChunk.tokens[inp.currInd + 1]
    val remainingLen = lenTokens - 1

    val tokType = inp.currTokenType()
    if (tokType == PunctuationToken.lexScope.internalVal.toInt()) {
        createScope(null, 0, remainingLen - 1, startOfExpr, lenBytes - startOfExpr + startByte)
    } else {
        if (remainingLen == 1) {
            parseLiteralOrIdentifier(tokType)
        } else if (tokType == operatorTok.internalVal.toInt()) {
            parsePrefixFollowedByAtom(sentinelToken)
        } else {
            createExpr(SpanAST.expression, lenTokens - 1, startOfExpr, lenBytes - startOfExpr + startByte)
        }
    }
}

private fun assignment(f: ParseFrame) {
    // TODO
}

//endregion

//region ProgrammaticBuilding

fun buildInsertString(s: String): Parser {
    addString(s)
    return this
}


fun buildNode(nType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int): Parser {
    ast.functionBodies.appendNode(nType, payload1, payload2, startByte, lenBytes)
    return this
}


fun buildFunction(nameId: Int, arity: Int, bodyId: Int): Parser {
    ast.functions.build(nameId, arity and LOWER24BITS, 0, bodyId)
    return this
}

fun buildFnDefPlaceholder(funcId: Int): Parser {
    ast.appendFnDefPlaceholder(funcId)
    return this
}


fun buildIntNode(payload: Long, startByte: Int, lenBytes: Int): Parser {
    ast.functionBodies.appendNode(litInt, (payload ushr 32).toInt(), (payload and LOWER32BITS).toInt(), startByte, lenBytes)
    return this
}


fun buildFloatNode(payload: Double, startByte: Int, lenBytes: Int): Parser {
    val payloadAsLong = payload.toBits()
    ast.functionBodies.appendNode(litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(), startByte, lenBytes)
    return this
}


fun buildSpan(nType: SpanAST, lenTokens: Int, startByte: Int, lenBytes: Int): Parser {
    ast.appendSpan(nType, lenTokens, startByte, lenBytes)
    return this
}

/**
 * For programmatic LexResult construction (builder pattern)
 */
fun buildError(msg: String): Parser {
    wasError = true
    errMsg = msg
    return this
}

//endregion

//region ParserUtilities

/**
 * Reads the string from current word, dotWord or atWord. Does NOT consume tokens.
 */
private fun readString(tType: RegularToken): String {
    return if (tType == word) {
        String(inp.inp, inp.currChunk.tokens[inp.currInd + 1],
            inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
    } else {
        String(inp.inp, inp.currChunk.tokens[inp.currInd + 1] + 1,
            (inp.currChunk.tokens[inp.currInd] and LOWER27BITS) - 1)
    }
}


private fun lookupBinding(name: String): Int? {
    for (f in (fnDefBacktrack.size - 1) downTo 0) {
        val fnDef = fnDefBacktrack[f]
        val subscopes = fnDef.subscopes
        for (j in (subscopes.size - 1) downTo 0) {
            if (subscopes[j].bindings.containsKey(name)) {
                return subscopes[j].bindings[name]
            }
        }
    }

    return importedScope.bindings.getOrDefault(name, null)
}

/**
 * Returns {operId precedence arity} of an operator from the payload in the token
 * TODO sort out Int vs Long silliness
 */
private fun functionalityOfOper(operPayload: Long): Triple<Int, Int, Int> {
    val index = OperatorToken.fromInt(operPayload.toInt()).opType.internalVal
    val operDefinition = operatorDefinitions[index]
    return Triple(index, operDefinition.precedence, operDefinition.arity)
}


private fun lookupFunction(name: String, arity: Int): Int? {
    val subscopes = currFnDef.subscopes
    for (j in (subscopes.size - 1) downTo 0) {
        if (subscopes[j].functions.containsKey(name)) {
            val lst = subscopes[j].functions[name]!!
            for (funcId in lst) {
                if (funcId.snd == arity) return funcId.fst
            }
        }
    }
    if (importedScope.functions.containsKey(name)) {
        val lst = importedScope.functions[name]!!
        for (funcId in lst) {
            if (funcId.snd == arity) return funcId.fst
        }
    }
    return null
}


private fun addString(s: String): Int {
    if (this.allStrings.containsKey(s)) {
        return allStrings[s]!!
    }
    val id = ast.identifiers.size
    ast.identifiers.add(s)
    this.allStrings[s] = id
    return id
}


private fun exitWithError(msg: String): Nothing {
    val startByte = inp.currChunk.tokens[inp.currInd + 1]
    val endByte = startByte + inp.currChunk.tokens[inp.currInd] and LOWER27BITS
    wasError = true
    errMsg = "[$startByte $endByte] $msg"
    throw Exception(errMsg)
}

fun getStringLiteral(startByte: Int, lenBytes: Int): String {
    return String(inp.inp, startByte, lenBytes)
}

/** The programmatic/builder method for inserting all non-builtin function bindings into top scope */
fun insertImports(imports: ArrayList<Import>, fileType: FileType): Parser {
    val uniqueNames = HashSet<String>()
    val builtins = builtInOperators()
    for (bui in builtins) {
        val funcId = ast.functions.totalNodes
        ast.functions.appendThree(-1, bui.arity and LOWER24BITS, 0)
        importedScope.functions[bui.name] = arrayListOf(IntPair(funcId, bui.arity))
    }
    this.indFirstFunction = builtins.size
    if (fileType == FileType.executable) {
        ast.functions.appendThree(-1, 0, 0) // the entrypoint
    }
    for (imp in imports) {
        if (uniqueNames.contains(imp.name)) {
            exitWithError(errorImportsNonUnique)
        }
        uniqueNames.add(imp.name)
        val strId = addString(imp.name)
        if (imp.arity > -1) {
            val funcId = ast.functions.totalNodes
            val nativeNameId = addString(imp.nativeName)
            ast.functions.appendThree(strId, imp.arity and LOWER24BITS, nativeNameId)
            importedScope.functions[imp.name] = arrayListOf(IntPair(funcId, imp.arity))
        } else {
            importedScope.bindings[imp.name] = strId
        }
    }

    addBuiltInTypes()
    return this
}


fun getFileType(): FileType {
    return inp.fileType
}

/**
 * Pretty printer function for debugging purposes
 */
fun toDebugString(): String {
    val result = StringBuilder()
    result.appendLine("Parse result")
    result.appendLine(if (wasError) {errMsg} else { "OK" })
    result.appendLine("astType [startByte lenBytes] (payload/lenTokens)")
    AST.print(ast, result)
    return result.toString()
}


private fun addBuiltInTypes() {
    addBuiltInType("Int")
    addBuiltInType("Float")
    addBuiltInType("Bool")
    addBuiltInType("String")
    indFirstName = ast.identifiers.size
}

private fun addBuiltInType(name: String) {
    val nameInd = addString(name)
    ast.types.appendFirst(nameInd)
    importedScope.types[name] = ast.types.totalNodes - 1
}

//endregion

init {
    wasError = false
    errMsg = ""

    currFnDef = FunctionDef(indFirstFunction, 0, functionPrec)

    importedScope = LexicalScope()
    parseTable = createParseTable()
}

companion object {


    /** Rows:
     * 1) first row for RegularTokens
     * 2) a row for every PunctuationToken except stmtCore
     * 3) a row for every core form in the order of the "reserved*" constants
     *
     * Columns: a column for every SpanAST value
     *
     * Values: 0 if action is not permitted,
     * -1 if it is context-free (row-driven),
     * positive number if it is context-dependent (custom function)
     */
    private fun structureOfDispatch(): Array<Array<Int>> {
                 // scope  expr  funDef funDPtr assig ret tyDecl  if  ifCl loop break
        return arrayOf(
            // RegularToken row
            arrayOf(  0,     -1,     0,      0,   -1,   -1,    1,  3,   5,    0,    1),

            arrayOf(  0,     -1,     0,      0,   -1,   -1,    0,  0,   0,    0,    0), // curlyBrace
            arrayOf(  0,      0,     0,      0,    0,    0,    0,  0,   0,    0,    0), // brackets
            arrayOf(  -1,     0,     0,      0,   -1,   -1,    2,  4,   0,   -1,    0), // parens
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,    0,    0), // stmtAssignment
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,    0,    0), // stmtTypeDecl
            arrayOf(  -1,     0,     0,      0,   -1,    0,    0,  0,   0,   -1,    0), // lexScope

            // core spans
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,    0,    0), // fn
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,    0,    0), // return
            arrayOf(  -1,     0,     0,      0,   -1,    0,    0,  0,   0,    0,    0), // if
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,   -1,    0), // loop
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,   -1,    0), // break
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,    0,    0), // ifEq
            arrayOf(  -1,     0,     0,      0,    0,    0,    0,  0,   0,    0,    0), // ifPred
        )

    }

    private fun createParseTable(): ArrayList<ArrayList<Parser.(Int, ParseFrame, Int, Int) -> Unit>> {
        val structure = structureOfDispatch()

        val rowFunctions = arrayOf(
         // leafNode                curlyBrace                   brackets           parens
            Parser::spErr,          Parser::spErr,               Parser::spErr,     Parser::expressionInit,
         // stmtAssig               typeDecl                     lexScope           fn
            Parser::assignmentInit, Parser::typeDeclarationInit, Parser::scopeInit, Parser::coreFnInit,
         // return                  if                           loop                     break
            Parser::coreReturnInit, Parser::coreIfInit,          Parser::coreLoopInit,    Parser::coreBreakInit,
         // ifEq                    ifPred
            Parser::spErr,          Parser::spErr,
        )

        val columnFunctions = arrayOf(
         // 0               typeDecl - atom  typeDecl - parens         if - parens
            Parser::spErr,   Parser::spErr, Parser::spErr, Parser::coreIfAtom,
         // ifCl - atom
            Parser::coreIfParens, Parser::coreIfClAtom,
        )
        val result: ArrayList<ArrayList<Parser.(Int, ParseFrame, Int, Int) -> Unit>> = ArrayList(structure.size)
        for (i in structure.indices) {
            val rw = structure[i]
            result.add(ArrayList(rw.size))
            for (j in rw.indices) {
                result[i].add(when (rw[j]) {
                    -1 -> rowFunctions[i]
                    else -> columnFunctions[rw[j]]
                })
            }
        }
        return result
    }

    //region CompanionUtilities

    fun builtInOperators(): ArrayList<BuiltinOperator> {
        return ArrayList(operatorDefinitions.filter { it.name != "" } .map{BuiltinOperator(it.name, it.arity)})
    }


    fun toAllowedSpans(areCoresValid: Boolean, areAssignmentsValid: Boolean, areTypeDeclsValid: Boolean,
                       areScopesValid: Boolean, areExpressionsValid: Boolean, areRegularASTValid: Boolean): Int {
        var result = if (areCoresValid) { 1 } else { 0 }
        if (areAssignmentsValid) result += 2
        if (areTypeDeclsValid) result += 4
        if (areScopesValid) result += 8
        if (areExpressionsValid) result += 16
        if (areRegularASTValid) result += 32
        return result
    }


    private fun isTokenAllowed(tType: Int, payload2: Int, allowedSpans: Int): Boolean {
        if (tType == RegularToken.reservedWord.internalVal.toInt()) return allowedSpans % 2 > 0
        if (tType == PunctuationToken.curlyBraces.internalVal.toInt()) return allowedSpans and 8 > 0
        if (tType == RegularToken.operatorTok.internalVal.toInt() && payload2 == 5 ) return allowedSpans and 4 > 0

        return true
    }

    private fun areCoreFunctionsAllowed(allowedSpans: Int): Boolean {
        return allowedSpans % 2 > 0
    }

    private fun areAssignmentsAllowed(allowedSpans: Int): Boolean {
        return allowedSpans and 2 > 0
    }

    private fun areTypeDeclsAllowed(allowedSpans: Int): Boolean {
        return allowedSpans and 4 > 0
    }

    private fun areScopesAllowed(allowedSpans: Int): Boolean {
        return allowedSpans and 8 > 0
    }

    private fun areExpressionsAllowed(allowedSpans: Int): Boolean {
        return allowedSpans and 16 > 0
    }

    /**
     * Equality comparison for parsers. Returns null if they are equal
     */
    fun diffInd(a: Parser, b: Parser): Int? {
        if (a.wasError != b.wasError || !a.errMsg.endsWith(b.errMsg)) {
            return 0
        }
        return AST.diffInd(a.ast, b.ast)
    }

    /**
     * Pretty printer function for debugging purposes
     */
    fun printSideBySide(a: Parser, b: Parser, indDiff: Int): String {
        val result = StringBuilder()
        result.appendLine("Result | Expected")
        result.appendLine("${if (a.wasError) {a.errMsg} else { "OK" }} | ${if (b.wasError) {b.errMsg} else { "OK" }}")
        result.appendLine("astType [startByte lenBytes] (payload/lenTokens)")
        AST.printSideBySide(a.ast, b.ast, indDiff, result)
        return result.toString()
    }

    //endregion
}


}