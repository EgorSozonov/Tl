package parser
import lexer.*
import java.lang.StringBuilder
import java.util.*
import kotlin.collections.ArrayList
import kotlin.collections.HashMap
import lexer.OperatorToken
import lexer.RegularToken.*
import parser.RegularAST.*

class Parser {


private var inp: Lexer
var firstChunk: ParseChunk = ParseChunk()                    // First array of tokens
    private set
var currChunk: ParseChunk                                    // Last array of tokens
    private set
var nextInd: Int                                             // Next ind inside the current token array
    private set
var wasError: Boolean                                        // The lexer's error flagerrorScope
    private set
var errMsg: String
    private set
private var i: Int                                           // current index inside input byte array
var totalNodes: Int
    private set
var fileType: FileType = FileType.library
    private set

// The backtracks and mutable state to support nested data structures
private val fnDefBacktrack = Stack<FunctionDef>()   // The stack of function definitions
private var currFnDef: FunctionDef

private val globalScope: LexicalScope

// The storage for parsed bindings etc
private var bindings: MutableList<Binding>
private var functionBindings: MutableList<FunctionBinding>
private var scopes: MutableList<LexicalScope>
private var strings: ArrayList<String>
private val unknownFunctions: HashMap<String, ArrayList<UnknownFunLocation>>
val indFirstFunction: Int


/**
 * Main parser method
 */
fun parse(lexer: Lexer, fileType: FileType) {
    this.inp = lexer
    this.fileType = fileType
    val lastChunk = inp.currChunk
    val sentinelInd = inp.nextInd
    inp.currChunk = inp.firstChunk
    inp.currInd = 0

    if (fileType == FileType.executable) {
        fnDefBacktrack.push(currFnDef)

        val firstScope = LexicalScope()
        this.scopes.add(firstScope)
        currFnDef.subscopes.push(firstScope)
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
    setPunctuationLength(topFrame.indNode)
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
            setPunctuationLength(frame.indNode)
        } else {
            throw Exception(errorInconsistentExtent)
        }
        i--
    }

    if (isFunClose) {
        fnDefBacktrack.pop()
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
    return lenTokens + 1 // plus 1 for the statement token itself
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
    val newFunScope = LexicalScope()

    inp.nextToken() // skipping the "fn" keyword
    if (inp.currTokenType() != wordType) {
        throw Exception(errorFnNameAndParams)
    }

    appendNode(PunctuationAST.functionDef, lenTokens, startByte, lenBytes)

    val functionName = readString(word)
    appendNode(fnDefName, 0, this.strings.size, inp.currStartByte(), functionName.length)
    this.strings.add(functionName)
    inp.nextToken()

    var j = 0
    while (j < lenTokens && inp.currTokenType() == wordType) {
        val paramName = readString(word)
        if (newFunScope.bindings.containsKey(paramName) || paramName == functionName) {
            throw Exception(errorFnNameAndParams)
        }

        newFunScope.bindings[paramName] = j
        appendNode(fnDefParam, 0, this.strings.size, inp.currStartByte(), paramName.length)
        this.strings.add(paramName)
        inp.nextToken()

        j++
    }
    if (j == 0) {
        throw Exception(errorFnNameAndParams)
    } else if (j == lenTokens || inp.currTokenType() != PunctuationToken.curlyBraces.internalVal.toInt()) {
        throw Exception(errorFnMissingBody)
    }

    // Actually create the function and push it to stack. But the AST nodes have already been emitted, see above
    val newFnDef = FunctionDef(ind = functionBindings.size, indNode = totalNodes)
    val newFnBinding = FunctionBinding(functionName, funcPrecedence, j, j, 0)
    this.functionBindings.add(newFnBinding)
    fnDefBacktrack.push(newFnDef)

    val scopeLenTokens = inp.currLenTokens()
    val scopelenBytes = inp.currLenBytes()
    val scopeStartByte = inp.currStartByte()

    if (scopeLenTokens + j + 3 != lenTokens) {
        throw Exception(errorInconsistentExtent)
    }

    scopeInit(newFunScope, scopeLenTokens, scopeStartByte, scopelenBytes)
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


private fun scopeInit(mbLexicalScope: LexicalScope?, lenTokens: Int, startByte: Int, lenBytes: Int) {
    // if we are in an expression, then this scope will be an operand for some operator
    if (currFnDef.subexprs.isNotEmpty()) {
        val funStack = currFnDef.subexprs.peek()
        exprIncrementArity(funStack[funStack.size - 1])
    }

    appendNode(FrameAST.scope, lenTokens, startByte, lenBytes)

    if (mbLexicalScope != null) {
        this.scopes.add(mbLexicalScope)
        this.currFnDef.subscopes.push(mbLexicalScope)
    } else {
        val newScope = LexicalScope()
        this.scopes.add(newScope)
        this.currFnDef.subscopes.push(newScope)
    }


    inp.nextToken() // the curlyBraces token
    val newFrame = ParseFrame(FrameAST.scope, this.totalNodes - 1, lenTokens, 1)
    currFnDef.backtrack.push(newFrame)
    //scope(newFrame)
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


private fun exprInit(lenTokens: Int, startByte: Int, lenBytes: Int, additionalPrefixToken: Int) {
    if (lenTokens == 1) {
        val theToken = inp.nextToken(0)
        exprSingleItem(theToken)
        return
    }

    val funCallStack = ArrayList<Subexpr>()
    appendNode(FrameAST.expression, 0, startByte, lenBytes)

    currFnDef.subexprs.push(funCallStack)
    val newFrame = ParseFrame(FrameAST.expression, totalNodes - 1, lenTokens, additionalPrefixToken)
    currFnDef.backtrack.push(newFrame)

    val initConsumedTokens = exprStartSubexpr(newFrame.lenTokens, false, funCallStack)
    newFrame.tokensRead += initConsumedTokens

    expr(newFrame)
}

/**
 * Parses a statement that is a fun call. Uses an extended Shunting Yard algo from Dijkstra to
 * flatten all internal parens into a single "Reverse Polish Notation" stream.
 * I.e. into basically a post-order traversal of a function call tree. In the resulting AST nodes, function names are
 * annotated with arities.
 * This function does NOT emit the expression node - that's the responsibility of the caller.
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
            consumedTokens = exprStartSubexpr(subExprLenTokens, true, functionStack)
            stackInd = functionStack.size - 1
        } else if (tokType >= 10) {
            break
        } else {
            if (tokType == word.internalVal.toInt()) {
                exprWord(functionStack, stackInd)
            } else if (tokType == intTok.internalVal.toInt()) {
                exprOperand(functionStack, stackInd)
                appendNode(
                    litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
                    inp.currStartByte(), inp.currLenBytes()
                )
            } else if (tokType == dotWord.internalVal.toInt()) {
                exprInfix(readString(dotWord), funcPrecedence, 0, functionStack, stackInd)
            } else if (tokType == operatorTok.internalVal.toInt()) {
                exprOperator(functionStack, stackInd)
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
 * Adds a frame for the expression or subexpression by determining its structure: prefix or infix.
 * Also handles the negation operator "-" (since it can only ever be at the start of a subexpression).
 * Precondition: the current token must be past the opening paren / statement token
 * Returns: number of consumed tokens
 * Examples: "foo 5 a" is prefix
 *           "5 .foo a" is infix
 *           "5 + a" is infix
 */
private fun exprStartSubexpr(lenTokens: Int, isBeforeFirstToken: Boolean, functionStack: ArrayList<Subexpr>): Int{
    var consumedTokens = 0
    if (isBeforeFirstToken) {
        inp.nextToken()
        consumedTokens++
    }
    val firstTok = inp.nextToken(0)
    if (lenTokens == 1) {
        exprSingleItem(firstTok)
        return consumedTokens + 1
    } else if (firstTok.tType == operatorTok.internalVal.toInt()
               && functionalityOfOper(firstTok.payload).first == "-"){
        val sndTok = inp.nextToken(1)
        return exprNegationOper(sndTok, lenTokens, functionStack)
    } else {
        val sndTok = inp.nextToken(1)
        val startIndToken = if (isBeforeFirstToken) { -1 } else { 0 }
        val isPrefix = firstTok.tType == word.internalVal.toInt()
                && sndTok.tType != dotWord.internalVal.toInt()
                && (sndTok.tType != operatorTok.internalVal.toInt()
                || functionalityOfOper(sndTok.payload).second == prefixPrecedence)
        functionStack.add(Subexpr(arrayListOf(FunctionCall("", 0, 0, 0, 0)), startIndToken, lenTokens, isPrefix))
        return consumedTokens
    }
}

/**
 * A single-item subexpression, like "(5)" or "x". Cannot be a function call.
 */
private fun exprSingleItem(theTok: TokenLite) {
    val startByte = inp.currStartByte()
    if (theTok.tType == intTok.internalVal.toInt()) {
        appendNode(litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            startByte, inp.currLenBytes())
    } else if (theTok.tType == litFloat.internalVal.toInt()) {
        appendNode(litFloat, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            startByte, inp.currLenBytes())
    } else if (theTok.tType == litString.internalVal.toInt()) {
        appendNode(litString, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            startByte, inp.currLenBytes())
    } else if (theTok.tType == word.internalVal.toInt()) {
        val theWord = readString(word)
        if (theWord == "true") {
            appendNode(litBool, 0, 1, startByte, 4)
        } else if (theWord == "false") {
            appendNode(litString, 0, 0, startByte, 5)
        } else {
            val mbBinding = lookupBinding(theWord)
            if (mbBinding != null) {
                appendNode(ident, 0, mbBinding,
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
            appendNode(litInt, (payload ushr 32).toInt(), (payload and LOWER32BITS).toInt(),
                startByteMinus, totalBytes)
        } else {
            val payloadActual: Double = Double.fromBits(payload) * (-1.0)
            val payloadAsLong = payloadActual.toBits()
            appendNode(litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(),
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
        functionStack.last().operators.add(FunctionCall("-", prefixPrecedence, 0, 1, inp.currChunk.tokens[inp.currInd + 1]))
    } else if (sndTok.tType == PunctuationToken.parens.internalVal.toInt()) {
        functionStack.last().operators.add(FunctionCall("-", prefixPrecedence, 0, 1, inp.currChunk.tokens[inp.currInd + 1]))
    } else {
        errorOut(errorUnexpectedToken)
        return 0
    }
    functionStack.add(Subexpr(arrayListOf(FunctionCall("", 0, 0, 0, 0)), 1, lenTokens, false))
    return 2 // the parens and the "-" sign
}


private fun exprWord(functionStack: ArrayList<Subexpr>, stackInd: Int) {
    val theWord = readString(word)
    if (functionStack[stackInd].prefixMode && functionStack[stackInd].tokensRead == 0) {
        functionStack[stackInd].operators[0] = FunctionCall(theWord, funcPrecedence, 0, 0,
                                                            inp.currStartByte())
    } else {
        val binding = lookupBinding(theWord)
        if (binding != null) {
            appendNode(ident, 0, binding,
                inp.currStartByte(), inp.currLenBytes())
            exprOperand(functionStack, stackInd)
        } else {
            throw Exception(errorUnknownBinding)
        }
    }
}


private fun exprInfix(fnName: String, precedence: Int, maxArity: Int, functionStack: ArrayList<Subexpr>, stackInd: Int) {
    val startByte = inp.currChunk.tokens[inp.currInd + 1]
    val topParen = functionStack[stackInd]

    if (topParen.prefixMode) {
        throw Exception(errorExpressionError)
    } else if (topParen.firstFun) {
        val newFun = FunctionCall(
            fnName, precedence, functionStack[stackInd].operators[0].arity, maxArity,
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
        topParen.operators.add(FunctionCall(fnName, precedence, 1, maxArity, startByte))
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
    val operInfo = functionalityOfOper(inp.currChunk.tokens[inp.currInd + 3].toLong())
    if (operInfo.first == "") {
        throw Exception(errorOperatorUsedInappropriately)
    }
    if (operInfo.second == prefixPrecedence) {
        val startByte = inp.currChunk.tokens[inp.currInd + 1]
        functionStack[stackInd].operators.add(FunctionCall(operInfo.first, prefixPrecedence, 1, 1, startByte))
    } else {
        exprInfix(operInfo.first, operInfo.second, operInfo.third, functionStack, stackInd)
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

/**
 * Appends a node that either points to a function binding, or is pointed to by an unknown function
  */
private fun appendFnName(fnParse: FunctionCall) {
    val fnName = fnParse.name
    val mbId = lookupFunction(fnName, fnParse.arity)
    if (mbId != null) {
        appendNode(idFunc, 0, mbId, fnParse.startByte, fnParse.name.length)
    } else {
        if (unknownFunctions.containsKey(fnName)) {
            val lst: ArrayList<UnknownFunLocation> = unknownFunctions[fnName]!!
            lst.add(UnknownFunLocation(totalNodes, fnParse.arity))
        } else {
            unknownFunctions[fnName] = arrayListOf(UnknownFunLocation(totalNodes, fnParse.arity))
        }
        appendNode(idFunc, 0, -1, fnParse.startByte, fnParse.name.length)
    }
}


private fun lookupBinding(name: String): Int? {
    val subscopes = currFnDef.subscopes
    for (j in (subscopes.size - 1) downTo 0) {
        if (subscopes[j].bindings.containsKey(name)) {
            return subscopes[j].bindings[name]
        }
    }
    return null
}


private fun lookupFunction(name: String, arity: Int): Int? {
    val subscopes = currFnDef.subscopes
    for (j in (subscopes.size - 1) downTo  0) {
        if (subscopes[j].functions.containsKey(name)) {
            val lst = subscopes[j].functions[name]!!
            for (funcId in lst) {
                if (this.functionBindings[funcId].arity == arity) {
                    return funcId
                }
            }
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
    val theName = readString(word)
    inp.nextToken()
    val sndOper = OperatorToken.fromInt(inp.currChunk.tokens[inp.currInd + 3])
    if (sndOper.opType != OperatorType.immDefinition) {
        throw Exception(errorAssignment)
    }

    val mbBinding = lookupBinding(theName)
    if (mbBinding != null) {
        throw Exception(errorAssignmentShadowing)
    }

    inp.nextToken()

    bindings.add(Binding(theName))
    val newBindingId = bindings.size - 1
    currFnDef.subscopes.peek().bindings[theName] = newBindingId

    appendNode(FrameAST.statementAssignment, 0, startByte, lenBytes)
    appendNode(binding, 0, newBindingId, startByte, theName.length)
    val startOfExpr = inp.currChunk.tokens[inp.currInd + 1]

    // we've already consumed 2 tokens in this func
    val newFrame = ParseFrame(FrameAST.statementAssignment, totalNodes - 2, lenTokens, 1, 2)
    currFnDef.backtrack.push(newFrame)
    exprInit(lenTokens - 2, startOfExpr, lenBytes - startOfExpr + startByte, 0)

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
 * Returns [name precedence arity] of an operator from the payload in the token
 * TODO sort out Int vs Long silliness
 */
private fun functionalityOfOper(operPayload: Long): Triple<String, Int, Int> {
    return operatorFunctionality[OperatorToken.fromInt(operPayload.toInt()).opType.internalVal]
}

private fun errorOut(msg: String) {
    this.wasError = true
    this.errMsg = msg
}

/**
 * Finds the top-level punctuation opener by its index, and sets its node length.
 * Called when the parsing of punctuation is finished.
 */
private fun setPunctuationLength(nodeInd: Int) {
    var curr = firstChunk
    var j = nodeInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }

    curr.nodes[j + 3] = totalNodes - nodeInd - 1
}

/**
 * For programmatic LexResult construction (builder pattern)
 */
fun buildError(msg: String): Parser {
    errorOut(msg)
    return this
}


fun setInput(inp: Lexer) {
    this.inp = inp
}

/** Append a regular AST node */
private fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    currChunk.nodes[nextInd + 2] = payload1
    currChunk.nodes[nextInd + 3] = payload2
    bump()
}


private fun appendNode(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    currChunk.nodes[nextInd + 2] = 0
    currChunk.nodes[nextInd + 3] = lenTokens
    bump()
}


private fun appendNode(nType: PunctuationAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    currChunk.nodes[nextInd + 2] = 0
    currChunk.nodes[nextInd + 3] = lenTokens
    bump()
}

/** Append a punctuation AST node */
private fun appendNode(startByte: Int, lenBytes: Int, tType: PunctuationAST) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    bump()
}

/** The programmatic/builder method for allocating a function binding */
fun buildBinding(binding: Binding): Parser {
    this.bindings.add(binding)
    return this
}

/** The programmatic/builder method for allocating a function binding */
fun buildFBinding(fBinding: FunctionBinding): Parser {
    this.functionBindings.add(fBinding)
    return this
}

/** The programmatic/builder method for inserting all non-builtin function bindings into top scope */
fun buildInsertBindingsIntoScope(): Parser {
    if (currFnDef.subscopes.isEmpty()) {
        currFnDef.subscopes.add(LexicalScope())
    }
    val topScope = currFnDef.subscopes.peek()
    for (id in 0 until this.bindings.size) {
        val name = this.bindings[id].name
        if (!topScope.bindings.containsKey(name)) {
            topScope.bindings[name]  = id
        }
    }
    for (id in 0 until this.functionBindings.size) {
        val name = this.functionBindings[id].name
        if (topScope.functions.containsKey(name)) {
            topScope.functions[name]!!.add(id)
        } else {
            topScope.functions[name] = arrayListOf(id)
        }
    }
    return this
}


fun buildNode(nType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int): Parser {
    appendNode(nType, payload1, payload2, startByte, lenBytes)
    return this
}


fun buildNode(payload: Double, startByte: Int, lenBytes: Int): Parser {
    val payloadAsLong = payload.toBits()
    appendNode(litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(), startByte, lenBytes)
    return this
}


fun buildNode(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int): Parser {
    appendNode(nType, lenTokens, startByte, lenBytes)
    return this
}


fun buildNode(nType: PunctuationAST, lenTokens: Int, startByte: Int, lenBytes: Int): Parser {
    appendNode(nType, lenTokens, startByte, lenBytes)
    return this
}


fun buildUnknownFunc(name: String, arity: Int, startByte: Int) {
    if (unknownFunctions.containsKey(name)) {
        unknownFunctions[name]!!.add(UnknownFunLocation(totalNodes, arity))
    } else {
        unknownFunctions[name] = arrayListOf(UnknownFunLocation(totalNodes, arity))
    }
    appendNode(idFunc, 0, -1, startByte, name.length)
}


private fun bump() {
    nextInd += 4
    totalNodes++
}


private fun ensureSpaceForNode() {
    if (nextInd < (CHUNKSZ - 4)) return

    val newChunk = ParseChunk()
    currChunk.next = newChunk
    currChunk = newChunk
    nextInd = 0
}


init {
    inp = Lexer()
    currChunk = firstChunk
    i = 0
    nextInd = 0
    totalNodes = 0
    wasError = false
    errMsg = ""

    bindings = ArrayList(12)
    functionBindings = ParserSyntax.builtInBindings()
    indFirstFunction = functionBindings.size
    currFnDef = FunctionDef(indNode = 0, ind = indFirstFunction - 1) // entry+

    scopes = ArrayList(10)
    unknownFunctions = HashMap(8)
    strings = ArrayList(100)
    globalScope = LexicalScope()
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


    /**
     * Equality comparison for parsers.
     */
    fun equality(a: Parser, b: Parser): Boolean {
        if (a.wasError != b.wasError || a.totalNodes != b.totalNodes || a.nextInd != b.nextInd
            || a.errMsg != b.errMsg) {
            return false
        }
        var currA: ParseChunk? = a.firstChunk
        var currB: ParseChunk? = b.firstChunk
        while (currA != null) {
            if (currB == null) {
                return false
            }
            val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
            for (i in 0 until len) {
                if (currA.nodes[i] != currB.nodes[i]) {
                    return false
                }
            }
            currA = currA.next
            currB = currB.next
        }
        if (a.unknownFunctions.size != b.unknownFunctions.size) {
            return false
        }
        for ((key, v) in a.unknownFunctions.entries) {
            if (!b.unknownFunctions.containsKey(key) || b.unknownFunctions[key]!!.size != v.size) {
                return false
            }
            for (j in 0 until v.size) {
                if (v[j] != b.unknownFunctions[key]!![j]) {
                    return false
                }
            }
        }

        if (a.bindings.size != b.bindings.size) {
            return false
        }
        for (i in 0 until a.bindings.size) {
            if (a.bindings[i].name != b.bindings[i].name) {
                return false
            }
        }

        return true
    }


    /**
     * Pretty printer function for debugging purposes
     */
    fun printSideBySide(a: Parser, b: Parser): String {
        val result = StringBuilder()
        result.appendLine("Result | Expected")
        result.appendLine("${if (a.wasError) {a.errMsg} else { "OK" }} | ${if (b.wasError) {b.errMsg} else { "OK" }}")
        result.appendLine("astType [startByte lenBytes] (payload/lenTokens)")
        var currA: ParseChunk? = a.firstChunk
        var currB: ParseChunk? = b.firstChunk
        while (true) {
            if (currA != null) {
                if (currB != null) {
                    val lenA = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                    val lenB = if (currB == b.currChunk) { b.nextInd } else { CHUNKSZ }
                    val len = lenA.coerceAtMost(lenB)
                    for (i in 0 until len step 4) {
                        printNode(currA, i, result)
                        result.append(" | ")
                        printNode(currB, i, result)
                        result.appendLine("")
                    }
                    for (i in len until lenA step 4) {
                        printNode(currA, i, result)
                        result.appendLine(" | ")
                    }
                    for (i in len until lenB step 4) {
                        result.append(" | ")
                        printNode(currB, i, result)
                        result.appendLine("")
                    }
                    currB = currB.next
                } else {
                    val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                    for (i in 0 until len step 4) {
                        printNode(currA, i, result)
                        result.appendLine(" | ")
                    }
                }
                currA = currA.next
            } else if (currB != null) {
                val len = if (currB == b.currChunk) { b.nextInd } else { CHUNKSZ }
                for (i in 0 until len step 4) {
                    result.append(" | ")
                    printNode(currB, i, result)
                    result.appendLine("")
                }
                currB = currB.next
            } else {
                break
            }


        }
        return result.toString()
    }


    private fun printNode(chunk: ParseChunk, ind: Int, wr: StringBuilder) {
        val startByte = chunk.nodes[ind + 1]
        val lenBytes = chunk.nodes[ind] and LOWER27BITS
        val typeBits = (chunk.nodes[ind] ushr 27).toByte()
        if (typeBits <= 6) {
            val regType = RegularAST.values().firstOrNull { it.internalVal == typeBits }
            if (regType != litFloat) {
                val payload: Long = (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            } else {
                val payload: Double = Double.fromBits(
                    (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                )
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            }
        } else if (typeBits <= 13) {
            val punctType = FrameAST.values().firstOrNull { it.internalVal == typeBits }
            val lenTokens = chunk.nodes[ind + 3]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        } else {
            val punctType = PunctuationAST.values().firstOrNull { it.internalVal == typeBits }
            val lenTokens = chunk.nodes[ind + 3]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        }
    }
}
}