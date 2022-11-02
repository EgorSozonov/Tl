package compiler.parser
import compiler.lexer.*
import java.lang.StringBuilder
import java.util.*
import kotlin.collections.ArrayList
import kotlin.collections.HashMap

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
        parseStatementFun(lenTokens, startByte, lenBytes)
    } else if (stmtType == PunctuationToken.statementAssignment.internalVal.toInt()) {
        parseStatementAssignment(lenTokens, startByte, lenBytes)
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
private fun parseStatementFun(lenTokens: Int, startByte: Int, lenBytes: Int) {
    appendNode(PunctuationAST.funcall, lenTokens, startByte, lenBytes)
    backtrack.push(Pair(PunctuationAST.funcall, totalNodes))

    val functionStack = ArrayList<FunInStack>()
    functionStack.add(FunInStack(arrayListOf(), 0, lenTokens))
    var stackInd = 0

    var j = 0
    while (j < lenTokens) {
        val tokType = inp.currTokenType()
        parseStatementFunClosing(functionStack, stackInd)
        stackInd = functionStack.size - 1

        if (tokType == PunctuationToken.parens.internalVal.toInt()) {
            statementFunOperand(functionStack, stackInd)
            functionStack.add(FunInStack(arrayListOf(), -1,
                                         inp.currChunk.tokens[inp.currInd + 3]))
            stackInd++
        } else if (tokType == RegularToken.word.internalVal.toInt()) {
            parseStatementFunWord(functionStack, stackInd)
        } else if (tokType == RegularToken.litInt.internalVal.toInt()) {
            appendNode(RegularAST.litInt, inp.currChunk.tokens[inp.currInd + 2], inp.currChunk.tokens[inp.currInd + 3],
                       inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
            statementFunOperand(functionStack, stackInd)
        } else if (tokType == RegularToken.dotWord.internalVal.toInt()) {
            statementFunInfix(readString(RegularToken.dotWord), funcPrecedence, 0, functionStack, stackInd)
        } else if (tokType == RegularToken.operatorTok.internalVal.toInt()) {
            statementFunOperator(functionStack, stackInd)
        }

        if (wasError) return
        inp.nextToken()
        for (i in 0..stackInd) {
            functionStack[i].indToken++
        }
        j++
    }
    parseStatementFunClosing(functionStack, stackInd)
}


private fun parseStatementFunWord(functionStack: ArrayList<FunInStack>, stackInd: Int) {
    val theWord = readString(RegularToken.word)
    if (functionStack[stackInd].indToken == 0) {
        functionStack[stackInd].operators.add(FunctionParse(theWord, 26, 0, 0,
                                                            inp.currChunk.tokens[inp.currInd + 1]))
    } else {
        val binding = lookupBinding(theWord)
        if (binding != null) {
            appendNode(RegularAST.ident, 0, binding,
                inp.currChunk.tokens[inp.currInd + 1], inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
            statementFunOperand(functionStack, stackInd)
        } else {
            errorOut(errorUnknownBinding)
        }
    }
}


private fun statementFunInfix(fnName: String, precedence: Int, maxArity: Int, functionStack: ArrayList<FunInStack>, stackInd: Int) {
    val startByte = inp.currChunk.tokens[inp.currInd + 1]
    val topParen = functionStack[stackInd]

    if (topParen.haventPushedFn) {
        // it's the first dot-word we've encountered, so we need to juxtapose it with the first word, if any
        if (topParen.operators.last().arity != 0) {
            // There mustn't have yet been any operands for what we used to think was the function name
            errorOut(errorStatementFunError)
            return
        }

        val nonOperator = topParen.operators[0]
        val newFun = FunctionParse(fnName, precedence, functionStack[stackInd].operators[0].arity, maxArity,
            inp.currChunk.tokens[inp.currInd + 1])
        if (nonOperator.name != "") {
            val binding = lookupBinding(nonOperator.name)
            if (binding == null) {
                errorOut(errorUnknownBinding)
                return
            }
            appendNode(RegularAST.ident, 0, binding,
                nonOperator.startByte, nonOperator.name.length)
        }
        functionStack[stackInd].operators[0] = newFun
        statementFunOperand(functionStack, stackInd)
        topParen.haventPushedFn = false
    } else {
        while (topParen.operators.isNotEmpty() && topParen.operators.last().precedence >= funcPrecedence) {
            appendFnName(topParen.operators.last())
            topParen.operators.removeLast()
        }
        topParen.operators.add(FunctionParse(fnName, funcPrecedence, 1, maxArity, startByte))
    }
}


/**
 * State modifier that must be called whenever an operand is encountered in a statement parse
 */
private fun statementFunOperand(functionStack: ArrayList<FunInStack>, stackInd: Int) {
    if (functionStack[stackInd].operators.isEmpty()) {
        // this is for the "(foo 5) .bar" case, i.e. a placeholder for the lack of function id in first position
        functionStack[stackInd].operators.add(FunctionParse("", 0, 0, 0, 0))
    } else {
        val funBinding = functionStack[stackInd].operators.last()
        funBinding.arity++
        if (funBinding.maxArity > 0 && funBinding.maxArity < funBinding.arity) {
            errorOut(errorStatementFunWrongArity)
        }
    }
}


private fun statementFunOperator(functionStack: ArrayList<FunInStack>, stackInd: Int) {
    val operatorVal = inp.currChunk.tokens[inp.currInd + 3]
    val operInfo = operatorFunctionality[operatorVal]
    if (operInfo.first == "") {
        errorOut(errorOperatorUsedInappropriately)
        return
    }
    statementFunInfix(operInfo.first, operInfo.second, operInfo.third, functionStack, stackInd)
}

private fun parseStatementFunClosing(functionStack: ArrayList<FunInStack>, stackInd: Int) {
    for (k in stackInd downTo 0) {
        val funInSt = functionStack[k]
        if (funInSt.indToken == funInSt.lenTokens) {
            for (m in funInSt.operators.size - 1 downTo 0) {
                appendFnName(funInSt.operators[m])
            }
            functionStack.removeLast()
        } else {
            break
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


private fun parseStatementAssignment(lenTokens: Int, startByte: Int, lenBytes: Int) {

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


private fun errorOut(msg: String) {
    this.wasError = true
    this.errMsg = msg
}


/**
 * For programmatic LexResult construction (builder pattern)
 */
fun error(msg: String): Parser {
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
    for (id in this.indFirstFunction until this.functionBindings.size) {
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


fun buildNode(nType: PunctuationAST, lenTokens: Int, startByte: Int, lenBytes: Int): Parser {
    appendNode(nType, lenTokens, startByte, lenBytes)
    return this
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
            4 -> Parser:: parseStatementFun
            5 ->  Parser:: parseStatementAssignment
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
            val lenTokens = chunk.nodes[ind + 2]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        }
    }
}
}