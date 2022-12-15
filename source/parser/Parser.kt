package parser
import lexer.*
import java.lang.StringBuilder
import java.util.*
import kotlin.collections.ArrayList
import lexer.OperatorToken
import lexer.RegularToken.*
import parser.RegularAST.*
import utils.IntPair
import utils.LOWER27BITS
import utils.LOWER32BITS

class Parser(private val inp: Lexer) {


val ast = AST()

var wasError: Boolean
    private set
var errMsg: String
    private set

// The backtracks and mutable state to support nested data structures
private val fnDefBacktrack = Stack<FunctionDef>()   // The stack of function definitions
private var currFnDef: FunctionDef
private val importedScope: LexicalScope

/** Map for interning/deduplication of identifier names */
private var allStrings = HashMap<String, Int>(50)

/** Index of first non-builtin in 'functions' of AST */
var indFirstFunction: Int = 0
    private set


/**
 * Main parser method
 */
fun parse(imports: ArrayList<ImportOrBuiltin>) {
    val lastChunk = inp.currChunk
    val sentinelInd = inp.nextInd
    inp.currChunk = inp.firstChunk
    inp.currInd = 0

    insertImports(imports)

    if (this.inp.fileType == FileType.executable) {
        ast.funcNode(-1, 0, -1, 0) // entrypoint function

        fnDefBacktrack.push(currFnDef)
        scopeInitEntryPoint()
    }

    try {
        while ((inp.currChunk != lastChunk || inp.currInd < sentinelInd)) {
            val tokType = inp.currTokenType()
            if (Lexer.isStatement(tokType)) {
                parseTopLevelStatement()
            } else if (tokType == PunctuationToken.curlyBraces.internalVal.toInt()) {
                val lenTokens = inp.currLenTokens()
                val startByte = inp.currStartByte()
                val lenBytes = inp.currLenBytes()

                scopeInit(null, lenTokens, startByte, lenBytes)
            } else if (currFnDef.backtrack.isNotEmpty()) {
                val currFrame = currFnDef.backtrack.peek()
                dispatchTable[currFrame.extentType.internalVal.toInt() - 10](currFrame)
            } else {
                errorOut(errorUnexpectedToken)
                return
            }

            maybeCloseFrames()
        }
    } catch (e: Exception) {
        errorOut(e.message ?: "")
    }
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
private fun maybeCloseFrames() {
    if (fnDefBacktrack.isEmpty()) return

    val topFrame = currFnDef.backtrack.peek()
    if (topFrame.tokensRead < topFrame.lenTokens) return

    var isFunClose = false

    var tokensToAdd = topFrame.lenTokens + topFrame.additionalPrefixToken

    if (topFrame.extentType == FrameAST.scope) {
        if (currFnDef.backtrack.size == 1) isFunClose = true
        closeScope(tokensToAdd)
    } else if (topFrame.extentType == FrameAST.expression) {
        closeExpr()
    }
    currFnDef.setExtentLength(topFrame.indNode)
    currFnDef.backtrack.pop()

    var i = currFnDef.backtrack.size - 1
    while (i > -1) {
        val frame = currFnDef.backtrack[i]
        frame.tokensRead += tokensToAdd
        if (frame.tokensRead < frame.lenTokens) {
            break
        } else if (frame.tokensRead == frame.lenTokens) {
            if (frame.extentType == FrameAST.scope) {
                closeScope(tokensToAdd)
                if (currFnDef.backtrack.size == 1) isFunClose = true
            } else if (frame.extentType == FrameAST.expression) {
                closeExpr()
            }
            currFnDef.backtrack.pop()
            tokensToAdd = frame.lenTokens + frame.additionalPrefixToken
            currFnDef.setExtentLength(frame.indNode)
        } else {
            throw Exception(errorInconsistentExtent)
        }
        i--
    }

    if (isFunClose) {
        val freshlyMintedFn = fnDefBacktrack.pop()
        val freshBodyId = ast.totalNodes
        ast.storeFreshFunction(freshlyMintedFn)
        ast.setBodyId(freshlyMintedFn.funcId, freshBodyId)

        if (fnDefBacktrack.isNotEmpty()) currFnDef = fnDefBacktrack.peek()
    }
}


/** Closes a frame that is a scope. Since we're not just tracking consumed tokens in the
 * frames, but also in the FunctionStack (i.e. in the subexpressions), we need to duly update
 * those when a scope is closed.
 */
private fun closeScope(tokensToAdd: Int) {
    if (currFnDef.subexprs.isNotEmpty()) {
        val functionStack = currFnDef.subexprs.peek()
        if (functionStack.isNotEmpty()) {
            functionStack.last().tokensRead += tokensToAdd
        }
    }
    currFnDef.subscopes.pop()
}


private fun parseTopLevelStatement(): Int {
    val lenTokens = inp.currLenTokens()
    val stmtType = inp.currTokenType()
    val startByte = inp.currStartByte()
    val lenBytes = inp.currLenBytes()
    inp.nextToken()
    val nextTokType = inp.currTokenType()
    if (nextTokType == word.internalVal.toInt()) {
        val firstLetter = inp.inp[startByte]
        if (firstLetter < aCLower || firstLetter > aWLower) {
            parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
        } else {
            possiblyCoreDispatch[firstLetter - aCLower](stmtType, lenTokens, startByte, lenBytes)
        }
    } else {
        parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
    }
    return lenTokens + 1 // plus 1 is for the statement token itself
}


/**
 * Parses any kind of statement (fun, assignment, typedecl) except core forms
 * The @startByte and @lenBytes may both be 0, this means that the first token is not a word.
 * Otherwise they are both nonzero.
 */
private fun parseNoncoreStatement(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    if (stmtType == PunctuationToken.statementFun.internalVal.toInt()) {
        exprInit(lenTokens, startByte, lenBytes, 1)
    } else if (stmtType == PunctuationToken.statementAssignment.internalVal.toInt()) {
        assignmentInit(lenTokens, startByte, lenBytes)
    } else if (stmtType == PunctuationToken.statementTypeDecl.internalVal.toInt()) {
        parseStatementTypeDecl(lenTokens, startByte, lenBytes)
    }
}

private fun coreCatch(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

/**
 * Parses a core form: function definition, like "fn foo x y { x + y }"
 * or "fn bar x y { print x; x + y }"
 */
private fun coreFnDefinition(lenTokens: Int, startByte: Int, lenBytes: Int) {
    val currentScope = currFnDef.subscopes.peek()
    val newFunScope = LexicalScope()

    inp.nextToken() // the "fn" keyword
    if (inp.currTokenType() != wordType) {
        throw Exception(errorFnNameAndParams)
    }

    val functionName = readString(word)
    inp.nextToken() // the function name

    if (!currentScope.functions.containsKey(functionName)) {
        throw Exception(errorUnknownFunction)
    }

    val newFnDef = FunctionDef(0, 0, funcPrecedence)
    fnDefBacktrack.push(newFnDef)
    newFnDef.appendExtent(FrameAST.functionDef, 0, startByte, lenBytes)

    var arity = 0
    while (arity < lenTokens && inp.currTokenType() == wordType) {
        val paramName = readString(word)
        if (newFunScope.bindings.containsKey(paramName) || paramName == functionName) {
            throw Exception(errorFnNameAndParams)
        }

        newFunScope.bindings[paramName] = arity
        val paramNameId = addString(functionName)
        newFnDef.appendName(paramNameId, inp.currStartByte(), paramName.length)

        inp.nextToken()
        arity++
    }
    if (arity == 0) {
        throw Exception(errorFnNameAndParams)
    } else if (arity == lenTokens || inp.currTokenType() != PunctuationToken.curlyBraces.internalVal.toInt()) {
        throw Exception(errorFnMissingBody)
    }

    val funcList = currentScope.functions[functionName]!!
    val func = funcList.firstOrNull { it.snd == arity } ?: throw Exception(errorUnknownFunction)
    newFnDef.arity = arity
    newFnDef.funcId = func.fst

    val currTokenType = inp.currTokenType()
    if (currTokenType != PunctuationToken.curlyBraces.internalVal.toInt()) {
        throw Exception(errorFnMissingBody)
    }

    val scopeLenTokens = inp.currLenTokens()
    val scopelenBytes = inp.currLenBytes()
    val scopeStartByte = inp.currStartByte()

    if (scopeLenTokens + arity + 3 != lenTokens) {
        throw Exception(errorInconsistentExtent)
    }

    scopeInit(newFunScope, scopeLenTokens, scopeStartByte, scopelenBytes)
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


private fun coreFnValidateNames() {

}


private fun coreIf(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}


private fun coreIfEq(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreIfPred(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreFor(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreForRaw(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreMatch(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreReturn(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreTry(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}

private fun coreWhile(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
}


private fun validateCoreForm(stmtType: Int, lenTokens: Int) {
    if (lenTokens < 3) {
        throw Exception(errorCoreFormTooShort)
    }
    if (stmtType != PunctuationToken.statementFun.internalVal.toInt()) {
        throw Exception(errorCoreFormAssignment)
    }
}

/**
 * First it walks the current scope breadth-first without recursing into inner scopes.
 * The purpose is to find all names of functions defined in this scope.
 * Then it walks from the start again, this time depth-first, and writes instructions into the
 * AST scratch space
 */
private fun scopeInit(mbLexicalScope: LexicalScope?, lenTokens: Int, startByte: Int, lenBytes: Int) {
    // if we are in an expression, then this scope will be an operand for some operator
    if (currFnDef.subexprs.isNotEmpty()) {
        val funStack = currFnDef.subexprs.peek()
        exprIncrementArity(funStack[funStack.size - 1])
    }

    currFnDef.appendExtent(FrameAST.scope, lenTokens, startByte, lenBytes)
    val newScope = mbLexicalScope ?: LexicalScope()
    this.currFnDef.subscopes.push(newScope)

    inp.nextToken() // the curlyBraces token

    scopeInitAddNestedFunctions(newScope, lenTokens)

    currFnDef.backtrack.push(ParseFrame(FrameAST.scope, currFnDef.totalNodes - 1, lenTokens, 1))
}

/**
 * Init of the implicit scope in a .tlx (executable) file.
 */
private fun scopeInitEntryPoint() {
    val lenTokens = inp.totalTokens

    val newScope = LexicalScope()
    currFnDef.subscopes.push(newScope)
    currFnDef.appendExtent(FrameAST.scope, lenTokens, 0, inp.inp.size)

    scopeInitAddNestedFunctions(newScope, lenTokens)

    currFnDef.backtrack.push(ParseFrame(FrameAST.scope, currFnDef.totalNodes - 1, lenTokens, 1))
}

/**
 * Walks the current scope breadth-first without recursing into inner scopes.
 * The purpose is to find all names of functions defined (neseted) in this scope.
 * Does not consume any tokens.
 */
private fun scopeInitAddNestedFunctions(newScope: LexicalScope, lenTokensScope: Int) {
    val originalPosition = inp.currentPosition()
    var j = 0

    while (j < lenTokensScope) {
        val currToken = inp.nextToken(0)
        if (currToken.tType < firstPunctuationTokenType) {
            throw Exception(errorScope)
        }
        val lenTokensExtent = (currToken.payload and LOWER32BITS).toInt()
        if (currToken.tType == PunctuationToken.statementAssignment.internalVal.toInt()) {
            inp.nextToken()
            val nextTokenType = inp.currTokenType()
            var consumedTokens = 0
            if (nextTokenType == wordType
                && inp.currChunk.tokens[inp.currInd + 1] == 2) {
                val startByte = inp.currChunk.tokens[inp.currInd] and LOWER27BITS
                if (inp.inp[startByte] == aFLower && inp.inp[startByte + 1] == aNLower) {
                    consumedTokens = scopeInitAddNestedFn(j, newScope, lenTokensScope)
                }
            }
            inp.skipTokens(lenTokensExtent - consumedTokens)
        } else {
            inp.skipTokens(lenTokensExtent + 1) // + 1 for the extent token itself
        }

        j += lenTokensExtent + 1
    }

    inp.seek(originalPosition)
}

/**
 * Adds the function from a single "fn" definition. Returns the number of consumed tokens
 */
private fun scopeInitAddNestedFn(i: Int, newScope: LexicalScope, lenTokensScope: Int): Int {
    var arity = 0
    var j = i
    inp.nextToken() // "fn"
    if (inp.currTokenType() != word.internalVal.toInt()) throw Exception(errorFnNameAndParams)

    val fnName = readString(word)

    inp.nextToken() // function name
    j += 2

    while (j < lenTokensScope && inp.currTokenType() == wordType) {
        inp.nextToken()
        j++
        arity++
    }
    if (arity == 0) throw Exception(errorFnNameAndParams)

    val nameId = addString(fnName)
    val funcId = ast.funcTotalNodes
    val parentFuncId = if (!fnDefBacktrack.isEmpty()) { currFnDef.funcId } else { -1 }
    ast.funcNode(nameId, arity, parentFuncId, 0)

    if (newScope.functions.containsKey(fnName)) {
        val lstExisting = newScope.functions[fnName]!!
        if (lstExisting.any { it.snd == arity }) {
            throw Exception(errorDuplicateFunction)
        }
        lstExisting.add(IntPair(funcId, arity))
    } else {
        newScope.functions[fnName] = arrayListOf(IntPair(funcId, arity))
    }
    return arity + 2 // +2 for the "fn" and the function name
}

/**
 * Parses a scope (curly braces)
 * This function does NOT emit the scope node - that's the responsibility of the caller.
 */
private fun scope(parseFrame: ParseFrame) {
    var j = parseFrame.tokensRead
    var tokensConsumed = 0
    while (j < parseFrame.lenTokens) {
        val tokType = inp.currTokenType()
        if (Lexer.isStatement(tokType)) {
            tokensConsumed = parseTopLevelStatement()
        } else {
            throw Exception(errorScope)
        }
        j += tokensConsumed
    }
    parseFrame.tokensRead = j
}

/**
 * An expression, i.e. a series of funcalls and/or operator calls.
 */
private fun exprInit(lenTokens: Int, startByte: Int, lenBytes: Int, additionalPrefixToken: Int) {
    if (lenTokens == 1) {
        val theToken = inp.nextToken(0)
        exprSingleItem(theToken)
        return
    }

    val funCallStack = ArrayList<Subexpr>()
    currFnDef.appendExtent(FrameAST.expression, 0, startByte, lenBytes)

    currFnDef.subexprs.push(funCallStack)
    val newFrame = ParseFrame(FrameAST.expression, currFnDef.totalNodes - 1, lenTokens, additionalPrefixToken)
    currFnDef.backtrack.push(newFrame)

    val initConsumedTokens = subexprInit(newFrame.lenTokens, false, funCallStack)
    newFrame.tokensRead += initConsumedTokens

    expr(newFrame)
}

/**
 * Parses (a part of) an expression. Uses an extended Shunting Yard algo from Dijkstra to
 * flatten all internal parens into a single "Reverse Polish Notation" stream.
 * I.e. into a post-order traversal of a function call tree. In the resulting AST nodes, the function names are
 * annotated with arities.
 * TODO also flatten dataInits and dataIndexers (i.e. [] and .[])
 */
private fun expr(parseFrame: ParseFrame) {
    var stackInd = 0
    val functionStack = currFnDef.subexprs.peek()

    var j = parseFrame.tokensRead
    while (j < parseFrame.lenTokens) {
        val tokType = inp.currTokenType()
        closeSubexpr(functionStack, stackInd)
        stackInd = functionStack.size - 1
        var consumedTokens = 1
        if (tokType == PunctuationToken.parens.internalVal.toInt()) {
            val subExprLenTokens = inp.currChunk.tokens[inp.currInd + 3]
            exprIncrementArity(functionStack[stackInd])
            consumedTokens = subexprInit(subExprLenTokens, true, functionStack)
            stackInd = functionStack.size - 1
        } else if (tokType >= 10) {
            throw Exception(errorExpressionCannotContain)
        } else {
            when (tokType) {
                word.internalVal.toInt() -> {
                    exprWord(functionStack, stackInd)
                }
                intTok.internalVal.toInt() -> {
                    exprOperand(functionStack, stackInd)
                    currFnDef.appendNode(
                        litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
                        inp.currStartByte(), inp.currLenBytes()
                    )
                }
                dotWord.internalVal.toInt() -> {
                    val fnName = readString(dotWord)
                    if (!allStrings.containsKey(fnName)) throw Exception(errorUnknownFunction)
                    exprFuncall(allStrings[fnName]!!, funcPrecedence, 0, functionStack, stackInd)
                }
                operatorTok.internalVal.toInt() -> {
                    exprOperator(functionStack, stackInd)
                }
            }
            inp.nextToken()
        }

        for (i in 0..stackInd) {
            functionStack[i].tokensRead += consumedTokens
        }
        j += consumedTokens
    }
    parseFrame.tokensRead = j
    closeSubexpr(functionStack, stackInd)
}

/**
 * Adds a frame for a subexpression.
 * Handles the unary negation operator "-" (since it can only ever be at the start of a subexpression).
 * Precondition: the current token must be past the opening paren / statement token
 * Returns: number of consumed tokens
 * Examples: "5 .foo a"
 *           "5 + !a"
 * TODO: allow for core forms (but not scopes!)
 */
private fun subexprInit(lenTokens: Int, isBeforeFirstToken: Boolean, functionStack: ArrayList<Subexpr>): Int{
    var consumedTokens = 0
    if (isBeforeFirstToken) {
        inp.nextToken()
        consumedTokens++
    }
    val firstTok = inp.nextToken(0)
    return if (lenTokens == 1) {
        exprSingleItem(firstTok)
        consumedTokens + 1
    } else if (firstTok.tType == operatorTok.internalVal.toInt()
        && functionalityOfOper(firstTok.payload).first == minusOperatorIndex){
        val sndTok = inp.nextToken(1)
        exprNegationOper(sndTok, lenTokens, functionStack)
    } else {
        val startIndToken = if (isBeforeFirstToken) { -1 } else { 0 }
        functionStack.add(Subexpr(arrayListOf(FunctionCall(-1, 0, 0, 0, 0)), startIndToken, lenTokens))
        consumedTokens
    }
}

/**
 * A single-item subexpression, like "(5)" or "x". Cannot be a function call.
 */
private fun exprSingleItem(theTok: TokenLite) {
    val startByte = inp.currStartByte()
    if (theTok.tType == intTok.internalVal.toInt()) {
        currFnDef.appendNode(litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            startByte, inp.currLenBytes())
    } else if (theTok.tType == litFloat.internalVal.toInt()) {
        currFnDef.appendNode(litFloat, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            startByte, inp.currLenBytes())
    } else if (theTok.tType == litString.internalVal.toInt()) {
        currFnDef.appendNode(litString, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            startByte, inp.currLenBytes())
    } else if (theTok.tType == word.internalVal.toInt()) {
        val theWord = readString(word)
        if (theWord == "true") {
            currFnDef.appendNode(litBool, 0, 1, startByte, 4)
        } else if (theWord == "false") {
            currFnDef.appendNode(litString, 0, 0, startByte, 5)
        } else {
            val mbBinding = lookupBinding(theWord)
            if (mbBinding != null) {
                currFnDef.appendNode(ident, 0, mbBinding,
                    startByte, inp.currLenBytes())
            } else {
                throw Exception(errorUnknownBinding)
            }
        }
    } else {
        throw Exception(errorUnexpectedToken)
    }
    currFnDef.backtrack.peek().tokensRead++
    inp.nextToken()
}

/**
 * A subexpression that starts with "-". Can be either a negated numeric literal "(-5)", identifier "(-x)"
 * or parentheses "(-(...))".
 * Returns the number of consumed tokens.
 */
private fun exprNegationOper(sndTok: TokenLite, lenTokens: Int, functionStack: ArrayList<Subexpr>): Int {
    val startByteMinus = inp.currChunk.tokens[inp.currInd + 1]
    inp.nextToken()
    if (sndTok.tType == intTok.internalVal.toInt() || sndTok.tType == litFloat.internalVal.toInt()) {
        val startByteLiteral = inp.currChunk.tokens[inp.currInd + 1]
        val lenLiteral = inp.currLenBytes()
        val totalBytes = (startByteLiteral - startByteMinus + lenLiteral)
        val payload =
            (inp.currChunk.tokens[inp.currInd + 2].toLong() shl 32) + inp.currChunk.tokens[inp.currInd + 3].toLong() * (-1)
        if (sndTok.tType == intTok.internalVal.toInt()) {
            currFnDef.appendNode(litInt, (payload ushr 32).toInt(), (payload and LOWER32BITS).toInt(),
                startByteMinus, totalBytes)
        } else {
            val payloadActual: Double = Double.fromBits(payload) * (-1.0)
            val payloadAsLong = payloadActual.toBits()
            currFnDef.appendNode(litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(),
                        startByteMinus,totalBytes)
        }

        inp.nextToken()
        if (lenTokens == 2) {
            val opers = functionStack.last().operators
            while (opers.last().precedence == prefixPrecedence) {
                appendFnName(opers.last())
                opers.removeLast()
            }
            return 3 // the parens, the "-" sign, and the literal
        }
    } else if (sndTok.tType == word.internalVal.toInt()) {
        functionStack.last().operators.add(FunctionCall(-minusOperatorIndex, prefixPrecedence, 0, 1, inp.currChunk.tokens[inp.currInd + 1]))
    } else if (sndTok.tType == PunctuationToken.parens.internalVal.toInt()) {
        functionStack.last().operators.add(FunctionCall(-minusOperatorIndex, prefixPrecedence, 0, 1, inp.currChunk.tokens[inp.currInd + 1]))
    } else {
        errorOut(errorUnexpectedToken)
        return 0
    }
    functionStack.add(Subexpr(arrayListOf(FunctionCall(-1000, 0, 0, 0, 0)), 1, lenTokens, false))
    return 2 // the parens and the "-" sign
}


private fun exprWord(functionStack: ArrayList<Subexpr>, stackInd: Int) {
    val theWord = readString(word)

    val binding = lookupBinding(theWord)
    if (binding != null) {
        currFnDef.appendNode(ident, 0, binding,
            inp.currStartByte(), inp.currLenBytes())
        exprOperand(functionStack, stackInd)
    } else {
        throw Exception(errorUnknownBinding)
    }
}

/**
 * For functions, the first param is the stringId of the name.
 * For operators, it's the negative index of operator from its payload
 */
private fun exprFuncall(nameStringId: Int, precedence: Int, maxArity: Int, functionStack: ArrayList<Subexpr>, stackInd: Int) {
    val startByte = inp.currChunk.tokens[inp.currInd + 1]
    val topParen = functionStack[stackInd]

    if (topParen.firstFun) {
        val newFun = FunctionCall(
            nameStringId, precedence, functionStack[stackInd].operators[0].arity, maxArity,
            inp.currChunk.tokens[inp.currInd + 1]
        )
        topParen.firstFun = false
        functionStack[stackInd].operators[0] = newFun
    } else {
        while (topParen.operators.isNotEmpty() && topParen.operators.last().precedence >= precedence) {
            if (topParen.operators.last().precedence == prefixPrecedence) {
                throw Exception(errorOperatorUsedInappropriately)
            }
            appendFnName(topParen.operators.last())
            topParen.operators.removeLast()
        }
        topParen.operators.add(FunctionCall(nameStringId, precedence, 1, maxArity, startByte))
    }
}


/**
 * State modifier that must be called whenever an operand is encountered in an expression parse
 */
private fun exprOperand(functionStack: ArrayList<Subexpr>, stackInd: Int) {
    if (functionStack[stackInd].operators.isEmpty()) return

    // flush all unaries
    val opers = functionStack[stackInd].operators
    while (opers.last().precedence == prefixPrecedence) {
        appendFnName(opers.last())
        opers.removeLast()
    }
    exprIncrementArity(functionStack[stackInd])
}

/**
 * Increments the arity of the top non-prefix operator. The prefix ones are ignored because there is no
 * need to track their arity: they are flushed on first operand or on closing of the first subexpr after them.
 */
private fun exprIncrementArity(topFrame: Subexpr) {
    var n = topFrame.operators.size - 1
    while (n > -1 && topFrame.operators[n].precedence == prefixPrecedence) {
        n--
    }
    if (n < 0) {
        throw Exception(errorExpressionError)
    }
    val oper = topFrame.operators[n]
    oper.arity++
    if (oper.maxArity > 0 && oper.arity > oper.maxArity) {
        throw Exception(errorOperatorWrongArity)
    }
}


private fun exprOperator(functionStack: ArrayList<Subexpr>, stackInd: Int) {
    val operTypeIndex = inp.currChunk.tokens[inp.currInd + 3].toLong()
    val operInfo = functionalityOfOper(operTypeIndex)
    if (operInfo.first < 0) {
        throw Exception(errorOperatorUsedInappropriately)
    }
    if (operInfo.second == prefixPrecedence) {
        val startByte = inp.currChunk.tokens[inp.currInd + 1]
        functionStack[stackInd].operators.add(FunctionCall(-operInfo.first - 1, prefixPrecedence, 1, 1, startByte))
    } else {
        exprFuncall(-operInfo.first - 1, operInfo.second, operInfo.third, functionStack, stackInd)
    }
}

/**
 * Flushes the finished subexpr frames from the top of the funcall stack.
 * A subexpr frame is finished when it has no tokens left.
 * Flushing includes appending its operators, clearing the operator stack, and appending
 * prefix unaries from the previous subexpr frame, if any.
 */
private fun closeSubexpr(funCallStack: ArrayList<Subexpr>, stackInd: Int) {
    for (k in stackInd downTo 0) {
        val funInSt = funCallStack[k]
        if (funInSt.tokensRead != funInSt.lenTokens) break
        for (m in funInSt.operators.size - 1 downTo 0) {
            appendFnName(funInSt.operators[m])
        }
        funCallStack.removeLast()

        // flush parent's prefix opers, if any, because this subexp was their operand
        if (funCallStack.isNotEmpty()) {
            val opersPrev = funCallStack[k - 1].operators
            for (n in (opersPrev.size - 1) downTo 0) {
                if (opersPrev[n].precedence == prefixPrecedence) {
                    appendFnName(opersPrev[n])
                    opersPrev.removeLast()
                } else {
                    break
                }
            }
        }
    }
}

/**
 * This function is similar to closeSubexpr, but it operates at the end of a whole expression.
 * As such, it requires that all the functions in stack be saturated.
 */
private fun closeExpr() {
    val functionStack = currFnDef.subexprs.pop()
    for (k in (functionStack.size - 1) downTo 0) {
        val funInSt = functionStack[k]
        for (o in funInSt.operators.size - 1 downTo 0) {
            val theFunction = funInSt.operators[o]
            if (theFunction.maxArity > 0 && theFunction.arity != theFunction.maxArity) {
                throw Exception(errorOperatorWrongArity)
            }
            appendFnName(theFunction)
        }
    }
}

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
    val subscopes = currFnDef.subscopes
    for (j in (subscopes.size - 1) downTo 0) {
        if (subscopes[j].bindings.containsKey(name)) {
            return subscopes[j].bindings[name]
        }
    }
    if (importedScope.bindings.containsKey(name)) {
        return importedScope.bindings[name]
    }
    return null
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

/**
 * Starts parsing an assignment statement like "x = y + 5".
 */
private fun assignmentInit(lenTokens: Int, startByte: Int, lenBytes: Int) {
    if (lenTokens < 3) {
        throw Exception(errorAssignment)
    }
    val fstTokenType = inp.currTokenType()
    val sndTokenType = inp.nextTokenType(1)
    if (fstTokenType != word.internalVal.toInt() || sndTokenType != operatorTok.internalVal.toInt()) {
        throw Exception(errorAssignment)
    }

    val bindingName = readString(word)
    inp.nextToken() // the name of the binding

    val sndOper = OperatorToken.fromInt(inp.currChunk.tokens[inp.currInd + 3])
    if (sndOper.opType != OperatorType.immDefinition) {
        throw Exception(errorAssignment)
    }

    val mbBinding = lookupBinding(bindingName)
    if (mbBinding != null) {
        throw Exception(errorAssignmentShadowing)
    }

    val strId = addString(bindingName)
    currFnDef.subscopes.peek().bindings[bindingName] = strId

    inp.nextToken() // equals sign

    currFnDef.appendExtent(FrameAST.statementAssignment, 0, startByte, lenBytes)
    currFnDef.appendNode(binding, 0, strId, startByte, bindingName.length)
    val startOfExpr = inp.currChunk.tokens[inp.currInd + 1]

    // we've already consumed 2 tokens in this func
    val newFrame = ParseFrame(FrameAST.statementAssignment, currFnDef.totalNodes - 2, lenTokens, 1, 2)
    currFnDef.backtrack.push(newFrame)

    // TODO check if we have a scope or a core form here
    val tokType = inp.currTokenType()
    if (tokType == PunctuationToken.curlyBraces.internalVal.toInt()) {
        scopeInit(null, lenTokens - 2, startOfExpr, lenBytes - startOfExpr + startByte)
    } else {
        exprInit(lenTokens - 2, startOfExpr, lenBytes - startOfExpr + startByte, 0)
    }
}

/**
 * Parses an assignment statement like "x = y + 5" and enriches the environment with new bindings.
 */
private fun assignment(parseFrame: ParseFrame) {


}


private fun parseStatementTypeDecl(lenTokens: Int, startByte: Int, lenBytes: Int) {

}


private fun parseUnexpectedToken() {
    throw Exception(errorUnexpectedToken)
}

/**
 * Returns the index of the "catch" parser if the word starting with "c" under cursor is indeed "catch"
 */
fun parseFromC(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
}

/**
 * Parse a statement where the first word is possibly "fn", "for" or "forRaw"
 */
fun parseFromF(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    if (lenBytes == 2 && inp.inp[startByte + 1] == aNLower) {
        coreFnDefinition(lenTokens, startByte, lenBytes)
    }
    parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
}

/**
 * Parse a statement where the first word is possibly "if", "ifEq" or "ifPred"
 */
fun parseFromI(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
}

/**
 * Parse a statement where the first word is possibly "match"
 */
fun parseFromM(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
}


/**
 * Parse a statement where the first word is possibly "return"
 */
fun parseFromR(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
}

/**
 * Parse a funcall statement where the first word is possibly "try"
 */
fun parseFromT(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
}

/**
 * Parse a funcall statement where the first word is possibly "while"
 */
fun parseFromW(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
}

/**
 * Appends a node that represents a function name in a function call
 */
private fun appendFnName(fnParse: FunctionCall) {
    if (fnParse.nameStringId > -1) {
        // functions
        val fnName = ast.identifiers[fnParse.nameStringId]
        val fnId = lookupFunction(fnName, fnParse.arity) ?: throw Exception(errorUnknownFunction)
        currFnDef.appendNode(idFunc, 0, fnId, fnParse.startByte, fnName.length)
    } else {
        // operators
        val operInfo = operatorFunctionality[-fnParse.nameStringId - 1]
        currFnDef.appendNode(idFunc, 0, operatorBindingIndices[-fnParse.nameStringId - 1], fnParse.startByte, operInfo.first.length)
    }

}

/**
 * Returns [operId precedence arity] of an operator from the payload in the token
 * TODO sort out Int vs Long silliness
 */
private fun functionalityOfOper(operPayload: Long): Triple<Int, Int, Int> {
    val index = OperatorToken.fromInt(operPayload.toInt()).opType.internalVal
    val precArity = operatorFunctionality[index]
    return Triple(index, precArity.second, precArity.third)
}


private fun errorOut(msg: String) {
    this.wasError = true
    this.errMsg = msg
}


fun buildInsertString(s: String): Parser {
    addString(s)
    return this
}


fun buildNode(nType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int): Parser {
    ast.appendNode(nType, payload1, payload2, startByte, lenBytes)
    return this
}


fun buildFloatNode(payload: Double, startByte: Int, lenBytes: Int): Parser {
    val payloadAsLong = payload.toBits()
    ast.appendNode(litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(), startByte, lenBytes)
    return this
}


fun buildExtent(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int): Parser {
    ast.appendExtent(nType, lenTokens, startByte, lenBytes)
    return this
}

/**
 * For programmatic LexResult construction (builder pattern)
 */
fun buildError(msg: String): Parser {
    errorOut(msg)
    return this
}

/** The programmatic/builder method for inserting all non-builtin function bindings into top scope */
fun insertImports(imports: ArrayList<ImportOrBuiltin>): Parser {
    val uniqueNames = HashSet<String>()
    val builtins = builtInOperators()
    for (bui in builtins) {
        val funcId = ast.funcTotalNodes
        ast.funcNode(-1, bui.arity, -1, 0)
        importedScope.functions[bui.name] = arrayListOf(IntPair(funcId, bui.arity))
    }
    for (imp in imports) {
        if (uniqueNames.contains(imp.name)) {
            throw Exception(errorImportsNonUnique)
        }
        uniqueNames.add(imp.name)
        val strId = addString(imp.name)
        if (imp.arity > 0) {
            val funcId = ast.funcTotalNodes
            ast.funcNode(strId, imp.arity, -1, 0)
            importedScope.functions[imp.name] = arrayListOf(IntPair(funcId, imp.arity))
        } else {
            importedScope.bindings[imp.name] = strId
        }
    }
    this.indFirstFunction = builtins.size

    return this
}


fun getFileType(): FileType {
    return inp.fileType
}


init {
    wasError = false
    errMsg = ""

    currFnDef = FunctionDef(indFirstFunction, 0, funcPrecedence)

    importedScope = LexicalScope()
}

companion object {
    /** The numeric values must correspond to the values of FrameAST enum */
    private val dispatchTable: Array<Parser.(ParseFrame) -> Unit> = Array(3) {
        i -> when(i) {
            0 -> Parser::scope
            1 -> Parser::expr
            else -> Parser::assignment
        }
    }

    /**
     * Table for dispatch on the first letter of the first word of a statement in case
     * it might be a core syntax form.
     */
    private val possiblyCoreDispatch: Array<Parser.(Int, Int, Int, Int) -> Unit> = Array(21) {
        i -> when(i) {
            0 -> Parser::parseFromC
            3 -> Parser::parseFromF
            6 -> Parser::parseFromI
            10 -> Parser::parseFromM
            15 -> Parser::parseFromR
            17 -> Parser::parseFromT
            20 -> Parser::parseFromW
            else -> Parser::parseNoncoreStatement
        }
    }

    fun builtInOperators(): ArrayList<ImportOrBuiltin> {
        val result = ArrayList<ImportOrBuiltin>(20)
        for (of in operatorFunctionality.filter { x -> x.first != "" }) {
            result.add(ImportOrBuiltin(of.first, of.second, of.third))
        }

        return result
    }


    /**
     * Equality comparison for parsers.
     */
    fun equality(a: Parser, b: Parser): Boolean {
        if (a.wasError != b.wasError || a.errMsg != b.errMsg) {
            return false
        }
        return AST.equality(a.ast, b.ast)
    }

    /**
     * Pretty printer function for debugging purposes
     */
    fun printSideBySide(a: Parser, b: Parser): String {
        val result = StringBuilder()
        result.appendLine("Result | Expected")
        result.appendLine("${if (a.wasError) {a.errMsg} else { "OK" }} | ${if (b.wasError) {b.errMsg} else { "OK" }}")
        result.appendLine("astType [startByte lenBytes] (payload/lenTokens)")
        AST.printSideBySide(a.ast, b.ast, result)
        return result.toString()
    }
}
}