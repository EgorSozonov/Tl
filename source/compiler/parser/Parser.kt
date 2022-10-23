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
private var bindings: MutableList<Binding>
private var functionBindings: MutableList<FunctionBinding>
private val unknownFunctions: HashMap<String, UnknownFunLocation>
private var scopes: MutableList<LexicalScope>
private var strings: ArrayList<String>

/**
 * Main parser method
 */
fun parse(inp: Lexer, fileType: FileType) {
    val lastChunk = inp.currChunk
    val sentinelInd = inp.nextInd
    inp.currChunk = inp.firstChunk
    inp.currInd = 0
    val rootScope = LexicalScope()
    this.scopes.add(rootScope)
    scopeBacktrack.push(rootScope)
    while ((inp.currChunk != lastChunk || inp.nextInd < sentinelInd) && !wasError) {
        val tokType = currChunk.nodes[inp.nextInd] ushr 27
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
    val lenTokens = inp.currChunk.tokens[inp.currInd + 2]
    val stmtType = inp.currTokenType()
    inp.nextToken()
    val typeAndLen = inp.currChunk.tokens[inp.currInd]
    val tokType = typeAndLen ushr 27
    if (tokType == RegularToken.word.internalVal.toInt()) {
        val startByte = inp.currChunk.tokens[inp.currInd + 1]
        val lenBytes = inp.currChunk.tokens[inp.currInd] and LOWER27BITS
        val firstLetter = inp.inp[startByte]
        if (firstLetter < aCLower || firstLetter > aWLower) {
            parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
        } else {
            statementDispatch[firstLetter - aCLower](stmtType, startByte, lenBytes, lenTokens)
        }
    } else {
        parseNoncoreStatement(stmtType, 0, 0, lenTokens)
    }
}


/**
 * Parses any kind of statement (fun, assignment, typedecl) except core forms
 * The @startByte and @lenBytes may both be 0, this means that the first token is not a word.
 * Otherwise they are both nonzero.
 */
private fun parseNoncoreStatement(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    if (stmtType == PunctuationToken.statementFun.internalVal.toInt()) {
        parseStatementFun()
    } else if (stmtType == PunctuationToken.statementAssignment.internalVal.toInt()) {
        parseStatementAssignment()
    } else if (stmtType == PunctuationToken.statementTypeDecl.internalVal.toInt()) {
        parseStatementTypeDecl()
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
        functionName = String(inp.inp, inp.currChunk.tokens[inp.currInd + 1],
                                 inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
        j++
        names[functionName] = j
        inp.nextToken()

    }
    while (j < lenTokens && inp.currTokenType() == wordType) {
        val paramName = String(inp.inp, inp.currChunk.tokens[inp.currInd + 1],
                                  inp.currChunk.tokens[inp.currInd] and LOWER27BITS)
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
private fun parseScope() {

}


private fun parseParens() {

}


private fun parseBrackets() {

}


private fun parseDotBrackets() {

}


/**
 * Parses a statement that is a fun call. Uses the Shunting Yard algo from Dijkstra to
 * flatten all internal parens into a single "Reverse Polish Notation" stream.
 * I.e. into basically a post-order traversal of a function call tree. In the resulting AST nodes, function names are
 * annotated with numbers of their arguments.
 * TODO also flatten dataInits and dataIndexers (i.e. [] and .[])
 */
private fun parseStatementFun() {
    val functionStack = ArrayList<Int>()
    var stackInd = 0
    val numTokensStmt = inp.currChunk.tokens[inp.currInd + 2]

    if (numTokensStmt >= 2 && inp.nextTokenType() == RegularToken.dotWord.internalVal.toInt()) {

    }


    var jTok = 0
    while (jTok < numTokensStmt) {

        jTok++
    }
    // two stacks: stack of operators and
    // flattening of internal structure
    // flattening of
}

/**
 *
  */
private fun findFunction(String name) {

}


private fun parseStatementAssignment() {

}


private fun parseStatementTypeDecl() {

}


private fun parseUnexpectedToken() {
    errorOut(errorUnexpectedToken)
}


/**
 * Returns the index of the "catch" parser if the word starting with "c" under cursor is indeed "catch"
 */
fun parseFromC(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
}


/**
 * Parse a statement where the first word is possibly "fn", "for" or "forRaw"
 */
fun parseFromF(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    if (lenBytes == 2 && inp.inp[startByte + 1] == aNLower) {
        coreFnDefinition(stmtType, lenTokens)
    }
    parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
}


/**
 * Parse a statement where the first word is possibly "if", "ifEq" or "ifPred"
 */
fun parseFromI(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
}


/**
 * Parse a statement where the first word is possibly "match"
 */
fun parseFromM(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
}


/**
 * Parse a statement where the first word is possibly "return"
 */
fun parseFromR(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
}


/**
 * Parse a funcall statement where the first word is possibly "try"
 */
fun parseFromT(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
}


/**
 * Parse a funcall statement where the first word is possibly "while"
 */
fun parseFromW(stmtType: Int, startByte: Int, lenBytes: Int, lenTokens: Int) {
    parseNoncoreStatement(stmtType, startByte, lenBytes, lenTokens)
}


private fun parseFuncall() {

    errorOut(errorUnrecognizedByte)
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
private fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    currChunk.nodes[nextInd + 2] = payload1
    currChunk.nodes[nextInd + 3] = payload2
    bump()
}


/** Append a punctuation AST node */
private fun appendNode(startByte: Int, lenBytes: Int, tType: PunctuationAST) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    bump()
}


/** The programmatic/builder method for appending a punctuation token */
private fun appendNode(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int) {
    ensureSpaceForNode()
    currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.nodes[nextInd + 1] = startByte
    currChunk.nodes[nextInd + 2] = lenTokens
    bump()
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
    scopes = ArrayList(10)
    unknownFunctions = HashMap(8)
    strings = ArrayList(100)
}

companion object {
    private val dispatchTable: Array<Parser.() -> Unit> = Array(7) {
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
                        printToken(currA, i, result)
                        result.append(" | ")
                        printToken(currB, i, result)
                        result.appendLine("")
                    }
                    for (i in (len + 1) until lenA step 4) {
                        printToken(currA, i, result)
                        result.appendLine(" | ")
                    }
                    for (i in (len + 1) until lenB step 4) {
                        result.append(" | ")
                        printToken(currB, i, result)
                        result.appendLine("")
                    }
                    currB = currB.next
                } else {
                    val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                    for (i in 0 until len step 4) {
                        printToken(currA, i, result)
                        result.appendLine(" | ")
                    }
                }
                currA = currA.next
            } else if (currB != null) {
                val len = if (currB == b.currChunk) { b.nextInd } else { CHUNKSZ }
                for (i in 0 until len step 4) {
                    result.append(" | ")
                    printToken(currB, i, result)
                    result.appendLine("")
                }
                currB = currB.next
            } else {
                break
            }


        }
        return result.toString()
    }


    private fun printToken(chunk: ParseChunk, ind: Int, wr: StringBuilder) {
        val startByte = chunk.nodes[ind + 1]
        val lenBytes = chunk.nodes[ind] and LOWER27BITS
        val typeBits = (chunk.nodes[ind] ushr 27).toByte()
        if (typeBits <= 8) {
            val regType = RegularToken.values().firstOrNull { it.internalVal == typeBits }
            if (regType != RegularToken.litFloat) {
                val payload: Long = (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            } else {
                val payload: Double = Double.fromBits(
                    (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                )
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            }
        } else {
            val punctType = PunctuationToken.values().firstOrNull { it.internalVal == typeBits }
            val lenTokens = chunk.nodes[ind + 2]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        }
    }
}
}