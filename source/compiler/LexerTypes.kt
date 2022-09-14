package compiler


const val CHUNKSZ: Int = 10000
const val LOWER32BITS: Long = 0x00000000FFFFFFFF
const val UPPER32BITS: Long = (0x00000000FFFFFFFF shl 32)

enum class TokenType(val internalVal: Byte) {
    litInt(0),
    litString(1),
    litFloat(2),
    word(3),
    dotWord(4),
    atWord(5),
    comment(6),
    curlyBraces(7),
    statement(8),
    parens(9),
    brackets(10),
    dotBrackets(11),
    operatorTok(12),
}

// struct Token - modeled as 5 32-bit ints
// payload i64
// startChar i32
// lenChars i32
// tType u8
// lenTokens u24



data class LexChunk(val tokens: IntArray = IntArray(CHUNKSZ), var next: LexChunk? = null)

class LexResult {
    var firstChunk: LexChunk
    var currChunk: LexChunk
    var i: Int // current index inside input byte array
    var nextInd: Int
    var totalTokens: Int
    var wasError: Boolean
    var errMsg: String

    constructor() {
        firstChunk = LexChunk()
        currChunk = firstChunk
        i = 0
        nextInd = 0
        totalTokens = 0
        wasError = false
        errMsg = ""
    }

    fun addToken(payload: Long, startChar: Int, lenChars: Int, tType: TokenType, lenTokens: Int) {
        if (nextInd < (CHUNKSZ - 5)) {
            setNextToken(payload, startChar, lenChars, tType, lenTokens)
        } else {
            val newChunk = LexChunk()
            currChunk.next = newChunk
            currChunk = newChunk
            nextInd = 0
            setNextToken(payload, startChar, lenChars, tType, lenTokens)
        }
        totalTokens += 1
    }

    fun add(payload: Long, startChar: Int, lenChars: Int, tType: TokenType, lenTokens: Int): LexResult {
        if (nextInd < (CHUNKSZ - 5)) {
            setNextToken(payload, startChar, lenChars, tType, lenTokens)
        } else {
            val newChunk = LexChunk()
            currChunk.next = newChunk
            currChunk = newChunk
            nextInd = 0
            setNextToken(payload, startChar, lenChars, tType, lenTokens)
        }
        totalTokens += 1
        return this
    }

    fun addTokens(newTokens: IntArray /* Length must be divisible by 5 */) {
        val spaceInCurrChunk = CHUNKSZ - nextInd
        var intoCurrChunk = spaceInCurrChunk.coerceAtMost(newTokens.size)
        newTokens.copyInto(currChunk.tokens, nextInd, 0, intoCurrChunk)

        var indSource = intoCurrChunk
        while (indSource < newTokens.size) {
            val newChunk = LexChunk()
            currChunk.next = newChunk
            currChunk = newChunk
            nextInd = 0

            intoCurrChunk = CHUNKSZ.coerceAtMost(newTokens.size - indSource)
            newTokens.copyInto(currChunk.tokens, nextInd, indSource, indSource + intoCurrChunk)
            indSource += intoCurrChunk
        }
    }

    fun errorOut(msg: String) {
        this.wasError = true
        this.errMsg = msg
    }

    fun error(msg: String): LexResult {
        errorOut(msg)
        return this
    }

    private fun setNextToken(payload: Long, startChar: Int, lenChars: Int, tType: TokenType, lenTokens: Int) {
        currChunk.tokens[nextInd    ] = ((payload and UPPER32BITS) shr 32).toInt()
        currChunk.tokens[nextInd + 1] = (payload and LOWER32BITS).toInt()
        currChunk.tokens[nextInd + 2] = startChar
        currChunk.tokens[nextInd + 3] = lenChars
        currChunk.tokens[nextInd + 4] = (tType.internalVal.toInt() shl 24) + (lenTokens and 0x00FFFFFF)
        nextInd += 5
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