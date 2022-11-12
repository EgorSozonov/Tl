package compiler.parser
import compiler.lexer.*
import java.lang.StringBuilder
import java.util.*
import kotlin.collections.ArrayList
import kotlin.collections.HashMap
import compiler.lexer.OperatorToken
import compiler.lexer.RegularToken.*
import compiler.parser.RegularAST.*
import java.lang.Exception

class Parser {


private var inp: Lexer
var firstChunk: ParseChunk = ParseChunk()                    // First array of tokens
    private set
var currChunk: ParseChunk                                    // Last array of tokens
    private set
var nextInd: Int                                             // Next ind inside the current token array
    private set
var wasError: Boolean                                        // The lexer's error flag
    private set
var errMsg: String
    private set
private var i: Int                                           // current index inside input byte array
private var totalNodes: Int
    private set
private val backtrack = Stack<Pair<PunctuationAST, Int>>()   // The stack of punctuation scopes
private val scopeBacktrack = Stack<LexicalScope>()  // The stack of symbol tables that form current lex env
private val unknownFunctions: HashMap<String, ArrayList<UnknownFunLocation>>
private var bindings: MutableList<Binding>
private var functionBindings: MutableList<FunctionBinding>
val indFirstFunction: Int
private var scopes: MutableList<LexicalScope>
private var strings: ArrayList<String>


/**
 * Main parser method
 */
fun parse(lexer: Lexer, fileType: FileType) {
    this.inp = lexer
    val lastChunk = inp.currChunk
    val sentinelInd = inp.nextInd
    inp.currChunk = inp.firstChunk
    inp.currInd = 0
    if (this.scopes.isEmpty()) {
        this.scopes.add(LexicalScope())
    }

    scopeBacktrack.push(this.scopes.last())
    while ((inp.currChunk != lastChunk || inp.currInd < sentinelInd) && !wasError) {
        val tokType = inp.currChunk.tokens[inp.currInd] ushr 27
        if (Lexer.isStatement(tokType)) {
            parseTopLevelStatement()
        } else {
            parseUnexpectedToken()
        }

        inp.nextToken()
    }
    /**
     * fn foo x y {
     *     x + 2*y
     * }
     *
     * FunDef
     * Word
     * ParamNames: Word+ or _
     * FunBody: { stmt ... }
     *
     *
     * print $ "bar 1 2 3 = " + (bar 1 2 3) .toString
     * print $ foo 5 10
     */
}


private fun parseTopLevelStatement() {
    val lenTokens = inp.currChunk.tokens[inp.currInd + 3]
    val stmtType = inp.currTokenType()
    val startByte = inp.currChunk.tokens[inp.currInd + 1]
    val lenBytes = inp.currChunk.tokens[inp.currInd] and LOWER27BITS
    inp.nextToken()
    val nextTokType = inp.currTokenType()
    if (nextTokType == RegularToken.word.internalVal.toInt()) {
        val firstLetter = inp.inp[startByte]
        if (firstLetter < aCLower || firstLetter > aWLower) {
            parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
        } else {
            statementDispatch[firstLetter - aCLower](stmtType, lenTokens, startByte, lenBytes)
        }
    } else {
        parseNoncoreStatement(stmtType, lenTokens, startByte, lenBytes)
    }
}


/**
 * Parses any kind of statement (fun, assignment, typedecl) except core forms
 * The @startByte and @lenBytes may both be 0, this means that the first token is not a word.
 * Otherwise they are both nonzero.
 */
private fun parseNoncoreStatement(stmtType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    if (stmtType == PunctuationToken.statementFun.internalVal.toInt()) {
        expr(lenTokens, startByte, lenBytes)
    } else if (stmtType == PunctuationToken.statementAssignment.internalVal.toInt()) {
        assignment(lenTokens, startByte, lenBytes)
    } else if (stmtType == PunctuationToken.statementTypeDecl.internalVal.toInt()) {
        parseStatementTypeDecl(lenTokens, startByte, lenBytes)
    }
}

private fun coreCatch(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (!wasError) return
}

/**
 * Parses a core form: function definition.
 *
 */
private fun coreFnDefinition(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
    val stmtStartByte = inp.currChunk.tokens[inp.currInd + 1]
    val stmtLenBytes = inp.currChunk.tokens[inp.currInd] and LOWER27BITS

    inp.nextToken() // skipping the "fn" keyword

    val names = HashMap<String, Int>(4)
    val wordType = RegularToken.word.internalVal.toInt()
    var j = 0
    var functionName = ""
    if (inp.currTokenType() == wordType) {
        functionName = readString(RegularToken.word)
        j++
        names[functionName] = j
        inp.nextToken()

    }
    while (j < lenTokens && inp.currTokenType() == wordType) {
        val paramName = readString(RegularToken.word)
        if (names.containsKey(paramName)) {
            errorOut(errorFnNameAndParams)
            return
        }
        j++
        names[paramName] = j - 1
        inp.nextToken()
    }
    if (j == 0 || j == lenTokens) {
        errorOut(errorFnNameAndParams)
        return
    }
    if (inp.currTokenType() != PunctuationToken.curlyBraces.internalVal.toInt()) {
        errorOut(errorFnMissingBody)
        return
    }
    val fnBinding = FunctionBinding(functionName, 26, names.count() - 1)
    appendNode(RegularAST.fnDef, 0, functionBindings.size, stmtStartByte, stmtLenBytes)
    functionBindings.add(fnBinding)

    names.remove(functionName)
    parseFnBody(names)
}


private fun coreFnValidateNames() {

}

private fun coreIf(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreIfEq(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreIfPred(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreFor(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreForRaw(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreMatch(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreReturn(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreTry(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}

private fun coreWhile(stmtType: Int, lenTokens: Int) {
    validateCoreForm(stmtType, lenTokens)
    if (wasError) return
}


private fun validateCoreForm(stmtType: Int, lenTokens: Int) {
    if (lenTokens < 3) {
        errorOut(errorCoreFormTooShort)
    }
    if (stmtType != PunctuationToken.statementFun.internalVal.toInt()) {
        errorOut(errorCoreFormAssignment)
    }
}


/**
 * Parses a function definition body
 * @params function parameter names and indices (indices are 1-based)
 */
private fun parseFnBody(params: HashMap<String, Int>) {

}


/**
 * Parses a scope (curly braces)
 */
private fun parseScope(lenTokens: Int, startByte: Int, lenBytes: Int) {

}


private fun parseParens(lenTokens: Int, startByte: Int, lenBytes: Int) {

}


private fun parseBrackets(lenTokens: Int, startByte: Int, lenBytes: Int) {

}


private fun parseDotBrackets(lenTokens: Int, startByte: Int, lenBytes: Int) {

}


/**
 * Parses a statement that is a fun call. Uses the Shunting Yard algo from Dijkstra to
 * flatten all internal parens into a single "Reverse Polish Notation" stream.
 * I.e. into basically a post-order traversal of a function call tree. In the resulting AST nodes, function names are
 * annotated with numbers of their arguments.
 * TODO also flatten dataInits and dataIndexers (i.e. [] and .[])
 */
private fun expr(lenTokens: Int, startByte: Int, lenBytes: Int) {
    appendNode(PunctuationAST.funcall, lenTokens, startByte, lenBytes)
    backtrack.push(Pair(PunctuationAST.funcall, totalNodes))

    val functionStack = ArrayList<FunInStack>()
    var stackInd = 0
    val initConsumedTokens = exprStartSubexpr(lenTokens, false, functionStack)

    var j = initConsumedTokens
    while (j < lenTokens) {
        val tokType = inp.currTokenType()
        exprClosing(functionStack, stackInd)
        stackInd = functionStack.size - 1
        var consumedTokens = 1
        if (tokType == PunctuationToken.parens.internalVal.toInt()) {
            val subExprLenTokens = inp.currChunk.tokens[inp.currInd + 3]
            exprIncrementArity(functionStack[stackInd])
            consumedTokens = exprStartSubexpr(subExprLenTokens, true, functionStack)
            stackInd = functionStack.size - 1
        } else {
            if (tokType == RegularToken.word.internalVal.toInt()) {
                exprWord(functionStack, stackInd)
            } else if (tokType == RegularToken.litInt.internalVal.toInt()) {
                appendNode(
                    RegularAST.litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
                    inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS
                )
                exprOperand(functionStack, stackInd)
            } else if (tokType == RegularToken.dotWord.internalVal.toInt()) {
                exprInfix(readString(RegularToken.dotWord), funcPrecedence, 0, functionStack, stackInd)
            } else if (tokType == RegularToken.operatorTok.internalVal.toInt()) {
                exprOperator(functionStack, stackInd)
            }

            inp.nextToken()
        }
        if (wasError) return

        for (i in 0..stackInd) {
            functionStack[i].indToken += consumedTokens
        }
        j += consumedTokens
    }
    exprClosing(functionStack, stackInd)
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
private fun exprStartSubexpr(lenTokens: Int, isBeforeFirstToken: Boolean, functionStack: ArrayList<FunInStack>): Int{
    var consumedTokens = 0
    if (isBeforeFirstToken) {
        inp.nextToken()
        consumedTokens++
    }
    val firstTok = inp.nextToken(0)
    if (lenTokens == 1) {
        exprSingleItem(firstTok, functionStack)
        return consumedTokens
    } else if (firstTok.tType == RegularToken.operatorTok.internalVal.toInt()
               && functionalityOfOper(firstTok.payload).first == "-"){
        val sndTok = inp.nextToken(1)
        return exprNegationOper(sndTok, lenTokens, functionStack)
    } else {
        val sndTok = inp.nextToken(1)
        val startIndToken = if (isBeforeFirstToken) { -1 } else { 0 }
        val isPrefix = firstTok.tType == RegularToken.word.internalVal.toInt()
                && sndTok.tType != RegularToken.dotWord.internalVal.toInt()
                && (sndTok.tType != RegularToken.operatorTok.internalVal.toInt()
                || functionalityOfOper(sndTok.payload).second == prefixPrecedence)
        functionStack.add(FunInStack(arrayListOf(FunctionParse("", 0, 0, 0, 0)), startIndToken, lenTokens, isPrefix))
        return consumedTokens
    }
}

/**
 * A single-item subexpression, like "(5)" or "x". Cannot be a function call.
 */
private fun exprSingleItem(theTok: TokenLite, functionStack: ArrayList<FunInStack>) {
    if (theTok.tType == RegularToken.litInt.internalVal.toInt()) {
        appendNode(RegularAST.litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
    } else if (theTok.tType == RegularToken.litFloat.internalVal.toInt()) {
        appendNode(RegularAST.litFloat, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
    } else if (theTok.tType == RegularToken.litString.internalVal.toInt()) {
        appendNode(RegularAST.litString, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
            inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
    } else if (theTok.tType == RegularToken.word.internalVal.toInt()) {
        val theWord = readString(RegularToken.word)
        if (theWord == "true") {
            appendNode(RegularAST.litBool, 0, 1, inp.currChunk.tokens[inp.currInd + 1], 4)
        } else if (theWord == "false") {
            appendNode(RegularAST.litString, 0, 0, inp.currChunk.tokens[inp.currInd + 1], 5)
        } else {
            val mbBinding = lookupBinding(theWord)
            if (mbBinding != null) {
                appendNode(RegularAST.ident, 0, mbBinding,
                    inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
            } else {
                errorOut(errorUnknownBinding)
                return
            }
        }
    } else {
        errorOut(errorUnexpectedToken)
        return
    }
    inp.nextToken()
    //exprOperand(functionStack, functionStack.size - 1)
}

/**
 * A subexpression that starts with "-". Can be either a negated numeric literal "(-5)", identifier "(-x)"
 * or parentheses "(-(...))".
 */
private fun exprNegationOper(sndTok: TokenLite, lenTokens: Int, functionStack: ArrayList<FunInStack>): Int {
    val startByteMinus = inp.currChunk.tokens[inp.currInd + 1]
    inp.nextToken()
    if (sndTok.tType == RegularToken.litInt.internalVal.toInt() || sndTok.tType == RegularToken.litFloat.internalVal.toInt()) {
        val startByteLiteral = inp.currChunk.tokens[inp.currInd + 1]
        val lenLiteral = inp.currChunk.tokens[inp.currInd] and LOWER27BITS
        val totalBytes = (startByteLiteral - startByteMinus + lenLiteral)
        val payload =
            (inp.currChunk.tokens[inp.currInd + 2].toLong() shl 32) + inp.currChunk.tokens[inp.currInd + 3].toLong() * (-1)
        if (sndTok.tType == RegularToken.litInt.internalVal.toInt()) {
            appendNode(RegularAST.litInt, (payload ushr 32).toInt(), (payload and LOWER32BITS).toInt(),
                startByteMinus, totalBytes)
        } else {
            val payloadActual: Double = Double.fromBits(payload) * (-1.0)
            val payloadAsLong = payloadActual.toBits()
            appendNode(RegularAST.litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(),
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
    } else if (sndTok.tType == RegularToken.word.internalVal.toInt()) {
        functionStack.last().operators.add(FunctionParse("-", prefixPrecedence, 0, 1, inp.currChunk.tokens[inp.currInd + 1]))
    } else if (sndTok.tType == PunctuationToken.parens.internalVal.toInt()) {
        functionStack.last().operators.add(FunctionParse("-", prefixPrecedence, 0, 1, inp.currChunk.tokens[inp.currInd + 1]))
    } else {
        errorOut(errorUnexpectedToken)
        return 0
    }
    functionStack.add(FunInStack(arrayListOf(FunctionParse("", 0, 0, 0, 0)), 1, lenTokens, false))
    return 2 // the parens and the "-" sign
}


private fun exprWord(functionStack: ArrayList<FunInStack>, stackInd: Int) {
    val theWord = readString(RegularToken.word)
    if (functionStack[stackInd].prefixMode && functionStack[stackInd].indToken == 0) {
        functionStack[stackInd].operators[0] = FunctionParse(theWord, funcPrecedence, 0, 0,
                                                            inp.currChunk.tokens[inp.currInd + 1])
    } else {
        val binding = lookupBinding(theWord)
        if (binding != null) {
            appendNode(RegularAST.ident, 0, binding,
                inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
            exprOperand(functionStack, stackInd)
        } else {
            errorOut(errorUnknownBinding)
        }
    }
}


private fun exprInfix(fnName: String, precedence: Int, maxArity: Int, functionStack: ArrayList<FunInStack>, stackInd: Int) {
    val startByte = inp.currChunk.tokens[inp.currInd + 1]
    val topParen = functionStack[stackInd]

    if (topParen.prefixMode) {
        errorOut(errorExpressionError)
        return
    } else if (topParen.firstFun) {
        val newFun = FunctionParse(
            fnName, precedence, functionStack[stackInd].operators[0].arity, maxArity,
            inp.currChunk.tokens[inp.currInd + 1]
        )
        topParen.firstFun = false
        functionStack[stackInd].operators[0] = newFun
    } else {
        while (topParen.operators.isNotEmpty() && topParen.operators.last().precedence >= precedence) {
            if (topParen.operators.last().precedence == prefixPrecedence) {
                errorOut(errorOperatorUsedInappropriately)
                return
            }
            appendFnName(topParen.operators.last())
            topParen.operators.removeLast()
        }
        topParen.operators.add(FunctionParse(fnName, precedence, 1, maxArity, startByte))
    }
}


/**
 * State modifier that must be called whenever an operand is encountered in a statement parse
 */
private fun exprOperand(functionStack: ArrayList<FunInStack>, stackInd: Int) {
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
private fun exprIncrementArity(topFrame: FunInStack) {
    var n = topFrame.operators.size - 1
    while (n > -1 && topFrame.operators[n].precedence == prefixPrecedence) {
        n--
    }
    if (n < 0) {
        errorOut(errorExpressionError)
        throw Exception()
    }
    val oper = topFrame.operators[n]
    oper.arity++
    if (oper.maxArity > 0 && oper.arity > oper.maxArity) {
        errorOut(errorOperatorWrongArity)
    }
}


private fun exprOperator(functionStack: ArrayList<FunInStack>, stackInd: Int) {
    val operInfo = functionalityOfOper(inp.currChunk.tokens[inp.currInd + 3].toLong())
    if (operInfo.first == "") {
        errorOut(errorOperatorUsedInappropriately)
        return
    }
    if (operInfo.second == prefixPrecedence) {
        val startByte = inp.currChunk.tokens[inp.currInd + 1]
        functionStack[stackInd].operators.add(FunctionParse(operInfo.first, prefixPrecedence, 1, 1, startByte))
    } else {
        exprInfix(operInfo.first, operInfo.second, operInfo.third, functionStack, stackInd)
    }

}

/**
 * Flushes the finished subexpr frames from the top of the stack.
 * A subexpr frame is finished when it has no tokens left.
 * Flushing includes appending its operators, clearing the operator stack, and appending
 * prefix unaries from the previous subexpr frame, if any.
 */
private fun exprClosing(functionStack: ArrayList<FunInStack>, stackInd: Int) {
    for (k in stackInd downTo 0) {
        val funInSt = functionStack[k]
        if (funInSt.indToken != funInSt.lenTokens) break
        for (m in funInSt.operators.size - 1 downTo 0) {
            appendFnName(funInSt.operators[m])
        }
        functionStack.removeLast()

        // flush parent's prefix opers, if any, because this subexp was their operand
        if (functionStack.isNotEmpty()) {
            val opersPrev = functionStack[k - 1].operators
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


private fun readString(tType: RegularToken): String {
    return if (tType == RegularToken.word) {
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
private fun appendFnName(fnParse: FunctionParse) {
    val fnName = fnParse.name
    val mbId = lookupFunction(fnName, fnParse.arity)
    if (mbId != null) {
        appendNode(RegularAST.idFunc, 0, mbId, fnParse.startByte, fnParse.name.length)
    } else {
        if (unknownFunctions.containsKey(fnName)) {
            val lst: ArrayList<UnknownFunLocation> = unknownFunctions[fnName]!!
            lst.add(UnknownFunLocation(totalNodes, fnParse.arity))
        } else {
            unknownFunctions[fnName] = arrayListOf(UnknownFunLocation(totalNodes, fnParse.arity))
        }
        appendNode(RegularAST.idFunc, 0, -1, fnParse.startByte, fnParse.name.length)
    }
}


private fun lookupBinding(name: String): Int? {
    for (j in (scopeBacktrack.size - 1) downTo 0) {
        if (scopeBacktrack[j].bindings.containsKey(name)) {
            return scopeBacktrack[j].bindings[name]
        }
    }
    return null
}


private fun lookupFunction(name: String, arity: Int): Int? {
    for (j in (scopeBacktrack.size - 1) downTo  0) {
        if (scopeBacktrack[j].functions.containsKey(name)) {
            val lst = scopeBacktrack[j].functions[name]!!
            for (funcId in lst) {
                if (this.functionBindings[funcId].arity == arity) {
                    return funcId
                }
            }
        }
    }
    return null
}


private fun assignment(lenTokens: Int, startByte: Int, lenBytes: Int) {
    if (lenTokens < 3) {
        errorOut(errorAssignment)
        return
    }
    val fstTokenType = inp.currTokenType()
    val sndTokenType = inp.nextTokenType(1)
    if (fstTokenType != word.internalVal.toInt() || sndTokenType != operatorTok.internalVal.toInt()) {
        errorOut(errorAssignment)
        return
    }
    val theName = readString(word)
    inp.nextToken()
    val sndOper = OperatorToken.fromInt(inp.currChunk.tokens[inp.currInd + 3])
    if (sndOper.opType != OperatorType.immDefinition) {
        errorOut(errorAssignment)
        return
    }

    val mbBinding = lookupBinding(theName)
    if (mbBinding != null) {
        errorOut(errorAssignmentShadowing)
        return
    }

    inp.nextToken()

    bindings.add(Binding(theName))
    appendNode(PunctuationAST.statementAssignment, lenTokens, startByte, lenBytes)
    appendNode(binding, 0, bindings.size - 1, startByte, theName.length)
    val startOfExpr = inp.currChunk.tokens[inp.currInd + 1]
    expr(lenTokens - 2, startOfExpr, lenBytes - startOfExpr + startByte)
}


private fun parseStatementTypeDecl(lenTokens: Int, startByte: Int, lenBytes: Int) {

}


private fun parseUnexpectedToken() {
    errorOut(errorUnexpectedToken)
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
        coreFnDefinition(stmtType, lenTokens)
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
fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    currChunk.nodes[nextInd + 2] = payload1
    currChunk.nodes[nextInd + 3] = payload2
    bump()
}


fun appendNode(nType: PunctuationAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
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
    if (this.scopeBacktrack.isEmpty()) {
        this.scopeBacktrack.add(LexicalScope())
    }
    val topScope = scopeBacktrack.peek()
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
    appendNode(RegularAST.litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(), startByte, lenBytes)
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
    appendNode(RegularAST.idFunc, 0, -1, startByte, name.length)
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
    scopes = ArrayList(10)
    unknownFunctions = HashMap(8)
    strings = ArrayList(100)
}

companion object {
    private val dispatchTable: Array<Parser.(Int, Int, Int) -> Unit> = Array(7) {
        i -> when(i) {
            0 -> Parser::parseScope
            1 -> Parser::parseParens
            2 -> Parser::parseBrackets
            3 -> Parser::parseDotBrackets
            4 -> Parser:: expr
            5 ->  Parser:: assignment
            else -> Parser::parseStatementTypeDecl
        }
    }

    /**
     * Table for dispatch on the first letter of the first word of a statement
     */
    private val statementDispatch: Array<Parser.(Int, Int, Int, Int) -> Unit> = Array(21) {
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
        if (a.unknownFunctions.size != b.unknownFunctions.size) return false
        for ((key, v) in a.unknownFunctions.entries) {
            if (!b.unknownFunctions.containsKey(key) || b.unknownFunctions[key]!!.size != v.size) return false
            for (j in 0 until v.size) {
                if (v[j] != b.unknownFunctions[key]!![j]) return false
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
            if (regType != RegularAST.litFloat) {
                val payload: Long = (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            } else {
                val payload: Double = Double.fromBits(
                    (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                )
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            }
        } else {
            val punctType = PunctuationAST.values().firstOrNull { it.internalVal == typeBits }
            val lenTokens = chunk.nodes[ind + 3]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        }
    }
}
}