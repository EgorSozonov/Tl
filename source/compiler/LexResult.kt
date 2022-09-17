package compiler
import java.lang.StringBuilder
import java.util.*


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
            maybeInsertStatement(startByte)
            appendToken(payload, startByte, lenBytes, tType)
        }
    }

    /**
     * Regular tokens may not exist directly at the top level, or inside curlyBraces.
     * So this function inserts an implicit statement scope to prevent this.
     */
    private fun maybeInsertStatement(startByte: Int) {
        if (backtrack.empty() || backtrack.peek().first == PunctuationToken.curlyBraces) {
            backtrack.add(Pair(PunctuationToken.statement, totalTokens))
            appendToken(startByte, PunctuationToken.statement)
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
        backtrack.add(Pair(tType, totalTokens))
        appendToken(i + 1, tType)
        i += 1
    }

    /**
     * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], a ; or a newline.
     * This doesn't actually add a token to the array, just performs validation and updates the opener token
     * with its token length.
     */
    fun closePunctuation(tType: PunctuationToken) {
        if (tType == PunctuationToken.statement) {
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
        if (top.first == PunctuationToken.statement) {
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
        if (tType == PunctuationToken.curlyBraces && top.first == PunctuationToken.statement) {
            // Since statements always are immediate children of curlyBraces, the '}' symbol closes
            // not just the statement but the parent curlyBraces scope, too.

            val parentCurlyBraces = backtrack.pop()
            assert(parentCurlyBraces.first == PunctuationToken.curlyBraces)

            setPunctuationLengths(parentCurlyBraces.second)
        }

    }


    private fun validateClosingPunct(closingType: PunctuationToken, openType: PunctuationToken) {
        when (closingType) {
            PunctuationToken.curlyBraces -> {
                if (openType != PunctuationToken.curlyBraces && openType != PunctuationToken.statement) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            PunctuationToken.brackets -> {
                if (openType != PunctuationToken.brackets && openType != PunctuationToken.dotBrackets) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            PunctuationToken.parens -> {
                if (openType != PunctuationToken.parens) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            PunctuationToken.statement -> {
                if (openType != PunctuationToken.statement && openType != PunctuationToken.curlyBraces) {
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
    fun add(payload: Long, startByte: Int, lenBytes: Int, tType: RegularToken): LexResult {
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
    fun addPunctuation(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int): LexResult {
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
     * Finalizes the lexing of a single input: checks for unclosed scopes etc.
     */
    fun finalize() {
        if (!backtrack.empty()) {
            errorOut(Lexer.errorPunctuationExtraOpening)
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
            return true

        }

        fun printSideBySide(a: LexResult, b: LexResult): String {
            val result = StringBuilder()
            result.appendLine("Result | Expected")
            result.appendLine("${if (a.wasError) {"Error"} else { "OK" }} | ${if (b.wasError) {"Error"} else { "OK" }}")
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
                    for (i in 0 until len) {
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