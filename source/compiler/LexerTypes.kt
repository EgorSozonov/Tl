package compiler

import java.util.Stack


const val CHUNKSZ: Int = 10000 // Must be divisible by 4
const val LOWER32BITS: Long =  0x00000000FFFFFFFF
const val UPPER32BITS: Long = (0x00000000FFFFFFFF shl 32)
const val LOWER27BITS: Int = 0x07FFFFFF
const val UPPER5BITS: Int = (0x0000001F shl 32)
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
        setNextToken(payload, startByte, lenBytes, tType)
    }

    /**
     * Adds a token which serves punctuation purposes, i.e. either a (, a {, a [, a .[ or a $
     * These tokens are used to define the structure, that is, nesting within the AST.
     * Upon addition, they are saved to the backtracking stack to be updated with their length
     * once it is known.
     * The startByte & lengthBytes don't include the opening and closing delimiters, and
     * the lenTokens also doesn't include the punctuation token itself - only the insides of the
     * scope.
     */
    fun openPunctuation(tType: PunctuationToken) {
        backtrack.add(Pair(tType, nextInd))
        ensureSpaceForToken()
        setNextToken(tType)

        totalTokens += 1
        i += 1
    }

    /**
     * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], a ; or a newline.
     * This doesn't actually add a token to the array, just performs validation and updates the opener token
     * with its token length.
     */
    fun closePunctuation(tType: PunctuationToken) {
        if (backtrack.empty()) {
            errorOut(Lexer.errorPunctuationExtraClosing)
            return
        }
        val top = backtrack.pop()
        when (tType) {
            PunctuationToken.curlyBraces -> {
                if (top.first != PunctuationToken.curlyBraces && top.first != PunctuationToken.statement) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            PunctuationToken.brackets -> {
                if (top.first != PunctuationToken.brackets && top.first != PunctuationToken.dotBrackets) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            PunctuationToken.parens -> {
                if (top.first != PunctuationToken.parens) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            PunctuationToken.statement -> {
                if (top.first != PunctuationToken.statement) {
                    errorOut(Lexer.errorPunctuationUnmatched)
                    return
                }
            }
            else -> {}
        }

        // find the opening token and update it with its length which we now know
        var currChunk = firstChunk
        var i = top.second
        while (i >= CHUNKSZ) {
            currChunk = currChunk.next!!
            i -= CHUNKSZ
        }
        val lenBytes = i - currChunk.tokens[i + 2]
        checkLenOverflow(lenBytes)
        currChunk.tokens[i    ] = totalTokens - top.second  // lenTokens
        currChunk.tokens[i + 3] = lenBytes                  // lenBytes
    }


    fun add(payload: Long, startChar: Int, lenChars: Int, tType: RegularToken): LexResult {
        ensureSpaceForToken()
        setNextToken(payload, startChar, lenChars, tType)
        return this
    }

    private fun ensureSpaceForToken() {
        if (nextInd == (CHUNKSZ - 4)) {
            val newChunk = LexChunk()
            currChunk.next = newChunk
            currChunk = newChunk
            nextInd = 0
        }
    }

    private fun checkLenOverflow(lenBytes: Int) {
        if (lenBytes > MAXTOKENLEN) {
            errorOut(Lexer.errorLengthOverflow)
        }
    }


    fun addPunctuation(payload: Long, startChar: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int): LexResult {
        ensureSpaceForToken()
        setNextToken(payload, startChar, lenBytes, tType)
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


    private fun setNextToken(payload: Long, startBytes: Int, lenBytes: Int, tType: RegularToken) {
        checkLenOverflow(lenBytes)
        currChunk.tokens[nextInd    ] = (payload shr 32).toInt()
        currChunk.tokens[nextInd + 1] = (payload and LOWER32BITS).toInt()
        currChunk.tokens[nextInd + 2] = startBytes
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27) + lenBytes
        nextInd += 4
        totalTokens += 1
    }


    private fun setNextToken(tType: PunctuationToken) {
        currChunk.tokens[nextInd + 2] = i + 1
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27)
        nextInd += 4
        totalTokens += 1
    }


    private fun setNextToken(payload: Long, startByte: Int, lenBytes: Int, tType: PunctuationToken) {
        checkLenOverflow(lenBytes)
        currChunk.tokens[nextInd    ] = ((payload and UPPER32BITS) shr 32).toInt()
        currChunk.tokens[nextInd + 1] = (payload and LOWER32BITS).toInt()
        currChunk.tokens[nextInd + 2] = startByte
        currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27 + (lenBytes and LOWER27BITS))
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


enum class OperatorType {
    plusSign,
    minusSign,
    mulSign,
    divSign,

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
    fun toLong(): Long {
        return 0
    }
}