package compiler

import java.util.Stack


const val CHUNKSZ: Int = 10000 // Must be divisible by 4
const val LOWER32BITS: Long =  0x00000000FFFFFFFF
const val LOWER27BITS: Int = 0x07FFFFFF
const val MAXTOKENLEN = 134217728 // 2**27


enum class RegularToken(val internalVal: Byte) {
    litInt(0),
    litFloat(1),
    litString(2),
    verbatimString(3),
    word(4),
    dotWord(5),
    atWord(6),
    comment(7),
    operatorTok(8),
}

enum class PunctuationToken(val internalVal: Byte) {
    curlyBraces(10),
    statement(11),
    parens(12),
    brackets(13),
    dotBrackets(14),
    dollar(15),
}


// struct Token - modeled as 4 32-bit ints
// payload (for regular tokens) or lenTokens (for punctuation tokens) | i64
// startByte                                                          | i32
// tType                                                              | u5
// lenBytes                                                           | u27


data class LexChunk(val tokens: IntArray = IntArray(CHUNKSZ), var next: LexChunk? = null)

class LexResult {
    var firstChunk: LexChunk = LexChunk()
    var currChunk: LexChunk
    var i: Int // current index inside input byte array
    var nextInd: Int
    var totalTokens: Int
    var wasError: Boolean
    var errMsg: String
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
        ensureSpaceForToken()
        appendToken(payload, startByte, lenBytes, tType)
    }

    /**
     * When we are in curlyBraces, we need to insert a statement
     */
    private fun maybeInsertStatement() {

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
        ensureSpaceForToken()
        appendToken(tType)
        if (tType == PunctuationToken.curlyBraces) {
            appendToken(PunctuationToken.statement)
        }

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
        if (top.first == PunctuationToken.statement && top.second < (totalTokens - 1)) {
            top = backtrack.pop()
            if (wasError) return

            setPunctuationLengths(top.second)
            openPunctuation(PunctuationToken.statement)
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
            // If the last statement is empty, it is deleted. This is because when we added it, we
            // didn't know if it would contain any tokens or just be an empty line.
            if (top.second == totalTokens - 1) {
                deleteLastToken()
            }
            val parentCurlyBraces = backtrack.pop()
            assert(parentCurlyBraces.first == PunctuationToken.curlyBraces)

            setPunctuationLengths(parentCurlyBraces.second)
        }
    }


    private fun deleteLastToken() {
        if (nextInd > 0) {
            nextInd -= 4
        } else {
            var curr = firstChunk
            while (curr.next != currChunk) {
                curr = curr.next!!
            }
            currChunk = curr
            nextInd = CHUNKSZ - 4
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


    fun add(payload: Long, startChar: Int, lenChars: Int, tType: RegularToken): LexResult {
        ensureSpaceForToken()
        appendToken(payload, startChar, lenChars, tType)
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
        if (currChunk.next == null) {
            val newChunk = LexChunk()
            currChunk.next = newChunk
            currChunk = newChunk
            nextInd = 0
        } else {
            // this is a rare case, it happens when we've had to delete a token that was the last in its chunk
            currChunk = currChunk.next!!
            nextInd = 0
        }
    }

    private fun checkLenOverflow(lenBytes: Int) {
        if (lenBytes > MAXTOKENLEN) {
            errorOut(Lexer.errorLengthOverflow)
        }
    }


    fun addPunctuation(startChar: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int): LexResult {
        ensureSpaceForToken()
        appendToken(startChar, lenBytes, tType, lenTokens)
        return this
    }


    fun errorOut(msg: String) {
        this.wasError = true
        this.errMsg = msg
    }


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
        checkLenOverflow(lenBytes)
        currChunk.tokens[nextInd    ] = (payload shr 32).toInt()
        currChunk.tokens[nextInd + 1] = (payload and LOWER32BITS).toInt()
        currChunk.tokens[nextInd + 2] = startBytes
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27) + lenBytes
        bump()
    }


    private fun appendToken(tType: PunctuationToken) {
        currChunk.tokens[nextInd + 2] = i + 1
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27)
        bump()
    }


    private fun appendToken(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int) {
        checkLenOverflow(lenBytes)
        currChunk.tokens[nextInd    ] = lenTokens
        currChunk.tokens[nextInd + 2] = startByte
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27) + (lenBytes and LOWER27BITS)
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
                if (currB == null) return false
                val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ };
                for (i in 0 until len) {
                    if (currA.tokens[i] != currB.tokens[i]) { return false }
                }
                currA = currA.next
                currB = currB.next
            }
            return true

        }
    }
}


enum class OperatorType(val value: Int) {
    plusSign(0),
    minusSign(1),
    mulSign(2),
    divSign(3),

}


/**
 * There is a closed set of operators in the language.
 *
 * For added flexibility, most operators are extended into two more planes,
 * for example + is extended into +. and +:, / to /. and to /:.
 * These extended operators are not defined by the language, but may be defined
 * for any type by the user.
 * For example, the type of 3D vectors may have 3 multiplication
 * operators: * for vector product, *. for multiplication by a scalar, *: for scalar product.
 *
 * Plus, all the extensible operators (and only them) may have '=' appended to them for use
 * in assignment operators. For example, 'a &&:= b' means 'a = a &&: b' for whatever '&&:' means.
 *
 * This OperatorToken class records the base type of operator, its extension (0, 1 or 2),
 * and whether it is the assignment version of itself.
 * In the token stream, both of these values are stored inside the 64-bit payload of the Token.
 */
data class OperatorToken(val opType: OperatorType, val extended: Int, val isAssignment: Boolean) {
    /**
     * The lowest bit encodes isAssignment, the adjacent 2 bits encode 'extended', and the
     * next higher bits encode 'operatorType'
     */
    fun toInt(): Int {

        return (opType.value shl 3) + ((extended and 4) shl 1) + (if (isAssignment) { 1 } else { 0 })
    }

    companion object {
        fun fromInt(i: Int): OperatorToken {
            val isAssignment = (i and 1) == 1
            val extendedInt = (i and 6) shr 1
            val opType = OperatorType.values().getOrNull(i shr 3)!!
            return OperatorToken(opType, extendedInt, isAssignment)
        }
    }
}