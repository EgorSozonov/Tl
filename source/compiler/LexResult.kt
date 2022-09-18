package compiler
import java.lang.StringBuilder
import java.util.*
import compiler.PunctuationToken.*;

/**
 * The object with the result of lexical analysis of a source code file
 */
class LexResult {


    var firstChunk: LexChunk = LexChunk()
    var currChunk: LexChunk
    var i: Int // current index inside input byte array
    var nextInd: Int // next ind inside the current token chunk
    var totalTokens: Int
    var wasError: Boolean
    var errMsg: String

    private var comments: CommentStorage = CommentStorage()
    private val backtrack = Stack<Pair<PunctuationToken, Int>>()

    init {
        currChunk = firstChunk
        i = 0
        nextInd = 0
        totalTokens = 0
        wasError = false
        errMsg = ""
    }

    /**
     * Adds a regular, non-punctuation token
     */
    fun addToken(payload: Long, startByte: Int, lenBytes: Int, tType: RegularToken) {
        if (tType == RegularToken.comment) {
            comments.add(startByte, lenBytes)
        } else {
            wrapTokenInStatement(startByte, tType)
            appendToken(payload, startByte, lenBytes, tType)
        }
    }


    /**
     * Adds a token which serves punctuation purposes, i.e. either a (, a {, a [, a .[ or a $
     * These tokens are used to define the structure, that is, nesting within the AST.
     * Upon addition, they are saved to the backtracking stack to be updated with their length
     * once it is known.
     * The startByte & lengthBytes don't include the opening and closing delimiters, and
     * the lenTokens also doesn't include the punctuation token itself - only the insides of the
     * scope. Thus, for '(asdf)', the opening paren token will have a byte length of 4 and a
     * token length of 1.
     */
    fun openPunctuation(tType: PunctuationToken) {
        validateOpeningPunct(tType)

        backtrack.add(Pair(tType, totalTokens))
        val insidesStart = if (tType == dotBrackets) { i + 2 } else { i + 1 }
        appendToken(insidesStart, tType)
        i = insidesStart
    }


    /**
     * Regular tokens may not exist directly at the top level, or inside curlyBraces.
     * So this function inserts an implicit statement scope to prevent this.
     */
    private fun wrapTokenInStatement(startByte: Int, tType: RegularToken) {
        if (backtrack.empty() || backtrack.peek().first == curlyBraces) {
            var realStartByte = startByte
            if (tType == RegularToken.litString || tType == RegularToken.verbatimString) realStartByte -= 1
            addStatement(realStartByte)
        }
    }


    /**
     * Regular tokens may not exist directly at the top level, or inside curlyBraces.
     * So this function inserts an implicit statement scope to prevent this.
     */
    private fun addStatement(startByte: Int) {
        backtrack.add(Pair(statement, totalTokens))
        appendToken(startByte, statement)
    }


    /**
     * Validates that opening punctuation obeys the rules
     * and inserts an implicit Statement if immediately inside curlyBraces.
     */
    private fun validateOpeningPunct(openingType: PunctuationToken) {
        if (openingType == curlyBraces) {
            if (backtrack.isEmpty()) return
            if (backtrack.peek().first == parens && getPrevTokenType() == parens.internalVal.toInt()) return

            errorOut(Lexer.errorPunctuationWrongOpen)
        } else if (openingType == dotBrackets && getPrevTokenType() != RegularToken.word.internalVal.toInt()) {
            errorOut(Lexer.errorPunctuationWrongOpen)
        } else {
            if (backtrack.isEmpty() || backtrack.peek().first == curlyBraces) {
                addStatement(i)
            }
        }
    }


    /**
     * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], a ; or a newline.
     * This doesn't actually add a token to the array, just performs validation and updates the opener token
     * with its token length.
     */
    fun closePunctuation(tType: PunctuationToken) {
        if (tType == statement) {
            closeStatement()
        } else {
            closeRegularPunctuation(tType)
        }
        i += 1
    }

    /**
     * The statement closer function - i.e. called for a newline or a semi-colon.
     * 1) Only close the current scope if it's a statement and non-empty. This protects against
     * newlines that aren't inside curly braces, multiple newlines and sequences like ';;;;'.
     * 2) If it closes a statement, then it also opens up a new statement scope
     */
    private fun closeStatement() {
        var top = backtrack.peek()
        if (top.first == statement) {
            top = backtrack.pop()
            setPunctuationLengths(top.second)
        }
    }

    private fun closeRegularPunctuation(tType: PunctuationToken) {
        if (backtrack.empty()) {
            errorOut(Lexer.errorPunctuationExtraClosing)
            return
        }
        val top = backtrack.pop()
        validateClosingPunct(tType, top.first)
        if (wasError) return
        setPunctuationLengths(top.second)
        if (tType == curlyBraces && top.first == statement) {
            // Since statements always are immediate children of curlyBraces, the '}' symbol closes
            // not just the statement but the parent curlyBraces scope, too.

            val parentScope = backtrack.pop()
            assert(parentScope.first == curlyBraces)

            setPunctuationLengths(parentScope.second)
        }

    }


    private fun getPrevTokenType(): Int {
        if (totalTokens == 0) return 0

        if (nextInd > 0) {
            return currChunk.tokens[nextInd - 1] shr 27
        } else {
            var curr: LexChunk? = firstChunk
            while (curr!!.next != currChunk) {
                curr = curr.next!!
            }
            return curr.tokens[CHUNKSZ - 1] shr 27
        }

    }

    /**
     * Validation to catch unmatched closing punctuation
     */
    private fun validateClosingPunct(closingType: PunctuationToken, openType: PunctuationToken) {
        when (closingType) {
            curlyBraces -> {
                if (openType != curlyBraces && openType != statement) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            brackets -> {
                if (openType != brackets && openType != dotBrackets) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            parens -> {
                if (openType != parens) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            statement -> {
                if (openType != statement && openType != curlyBraces) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            else -> {}
        }
    }


    /**
     * For programmatic LexResult construction (builder pattern)
     */
    fun build(payload: Long, startByte: Int, lenBytes: Int, tType: RegularToken): LexResult {
        if (tType == RegularToken.comment) {
            comments.add(startByte, lenBytes)
        } else {
            appendToken(payload, startByte, lenBytes, tType)
        }
        return this
    }


    /**
     * Finds the top-level punctuation opener by its index, and sets its lengths.
     * Called when the matching closer is lexed.
     */
    private fun setPunctuationLengths(tokenInd: Int) {
        var curr = firstChunk
        var j = tokenInd * 4
        while (j >= CHUNKSZ) {
            curr = curr.next!!
            j -= CHUNKSZ
        }

        val lenBytes = i - curr.tokens[j + 2]
        checkLenOverflow(lenBytes)
        curr.tokens[j    ] = totalTokens - tokenInd - 1  // lenTokens
        curr.tokens[j + 3] += (lenBytes and LOWER27BITS) // lenBytes
    }


    private fun ensureSpaceForToken() {
        if (nextInd < (CHUNKSZ - 4)) return

        val newChunk = LexChunk()
        currChunk.next = newChunk
        currChunk = newChunk
        nextInd = 0
    }


    private fun checkLenOverflow(lenBytes: Int) {
        if (lenBytes > MAXTOKENLEN) {
            errorOut(Lexer.errorLengthOverflow)
        }
    }


    /**
     * For programmatic LexResult construction (builder pattern)
     */
    fun buildPunct(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int): LexResult {
        appendToken(startByte, lenBytes, tType, lenTokens)
        return this
    }


    fun errorOut(msg: String) {
        this.wasError = true
        this.errMsg = msg
    }


    /**
     * For programmatic LexResult construction (builder pattern)
     */
    fun error(msg: String): LexResult {
        errorOut(msg)
        return this
    }

    /**
     * Finalizes the lexing of a single input: checks for unclosed scopes, and closes an open statement, if any.
     */
    fun finalize() {
        if (!wasError && !backtrack.empty()) {
            closeStatement()

            if (!backtrack.empty()) {
                errorOut(Lexer.errorPunctuationExtraOpening)
            }
        }
    }


    private fun appendToken(payload: Long, startBytes: Int, lenBytes: Int, tType: RegularToken) {
        ensureSpaceForToken()
        checkLenOverflow(lenBytes)
        currChunk.tokens[nextInd    ] = (payload shr 32).toInt()
        currChunk.tokens[nextInd + 1] = (payload and LOWER32BITS).toInt()
        currChunk.tokens[nextInd + 2] = startBytes
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27) + lenBytes
        bump()
    }


    private fun appendToken(startByte: Int, tType: PunctuationToken) {
        ensureSpaceForToken()
        currChunk.tokens[nextInd + 2] = startByte
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27)
        bump()
    }


    private fun appendToken(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int) {
        ensureSpaceForToken()
        checkLenOverflow(lenBytes)
        currChunk.tokens[nextInd    ] = lenTokens
        currChunk.tokens[nextInd + 2] = startByte
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27) + lenBytes
        bump()
    }


    private fun bump() {
        nextInd += 4
        totalTokens += 1
    }


    companion object {
        fun equality(a: LexResult, b: LexResult): Boolean {
            if (a.wasError != b.wasError || a.totalTokens != b.totalTokens || a.nextInd != b.nextInd
                || a.errMsg != b.errMsg) {
                return false
            }
            var currA: LexChunk? = a.firstChunk
            var currB: LexChunk? = b.firstChunk
            while (currA != null) {
                if (currB == null) {
                    return false
                }
                val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                for (i in 0 until len) {
                    if (currA.tokens[i] != currB.tokens[i]) {
                        return false
                    }
                }
                currA = currA.next
                currB = currB.next
            }

            if (a.comments.totalTokens != b.comments.totalTokens) return false
            var commA: CommentChunk? = a.comments.firstChunk
            var commB: CommentChunk? = b.comments.firstChunk
            while (commA != null) {
                if (commB == null) {
                    return false
                }
                val len = if (commA == a.comments.currChunk) { a.comments.nextInd } else { COMMENTSZ }
                for (i in 0 until len) {
                    if (commA.tokens[i] != commB.tokens[i]) {
                        return false
                    }
                }
                commA = commA.next
                commB = commB.next
            }
            return true
        }

        fun printSideBySide(a: LexResult, b: LexResult): String {
            val result = StringBuilder()
            result.appendLine("Result | Expected")
            result.appendLine("${if (a.wasError) {a.errMsg} else { "OK" }} | ${if (b.wasError) {b.errMsg} else { "OK" }}")
            result.appendLine("tokenType [startByte lenBytes] (payload/lenTokens)")
            var currA: LexChunk? = a.firstChunk
            var currB: LexChunk? = b.firstChunk
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


        private fun printToken(chunk: LexChunk, ind: Int, wr: StringBuilder) {
            val startByte = chunk.tokens[ind + 2]
            val lenBytes = chunk.tokens[ind + 3] and LOWER27BITS
            val typeBits = (chunk.tokens[ind + 3] shr 27).toByte()
            if (typeBits <= 8) {
                val regType = RegularToken.values().firstOrNull { it.internalVal == typeBits }
                val payload: Long = (chunk.tokens[ind].toLong() shl 32) + chunk.tokens[ind + 1]
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            } else {
                val punctType = PunctuationToken.values().firstOrNull { it.internalVal == typeBits }
                val lenTokens = chunk.tokens[ind]
                wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
            }
        }
    }
}