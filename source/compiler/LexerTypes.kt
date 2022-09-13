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

    constructor(newTokens: IntArray /* Length must be divisible by 5 */) {
        firstChunk = LexChunk()
        currChunk = firstChunk
        i = 0
        nextInd = 0
        totalTokens = 0
        wasError = false
        errMsg = ""
        addTokens(newTokens)
    }

    fun addToken(payload: Long, startChar: Int, lenChars: Int, tType: TokenType, lenTokens: Int) {
        if (nextInd < (CHUNKSZ - 5)) {
            setNextToken(payload, startChar, lenChars, tType, lenTokens)
        } else {
            var newChunk = LexChunk()
            currChunk.next = newChunk
            currChunk = newChunk
            nextInd = 0
            setNextToken(payload, startChar, lenChars, tType, lenTokens)
        }
        totalTokens += 1
    }

    fun addTokens(newTokens: IntArray /* Length must be divisible by 5 */) {
        val spaceInCurrChunk = CHUNKSZ - nextInd
        var intoCurrChunk = Math.min(spaceInCurrChunk, newTokens.size)
        // move
        var ind = intoCurrChunk
        while (ind < newTokens.size) {
           intoCurrChunk = Math.min(CHUNKSZ, newTokens.size - ind)
            // move
            ind += intoCurrChunk
        }
    }

    fun errorOut(msg: String) {
        this.wasError = true
        this.errMsg = msg
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