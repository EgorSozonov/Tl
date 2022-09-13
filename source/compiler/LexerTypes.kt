package compiler

enum class TokenType(val ttype: Byte) {
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

const val CHUNK_SZ: Int = 10000

data class LexChunk(val tokens: IntArray = IntArray(CHUNK_SZ), var next: LexChunk? = null)

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

    fun addToken(newToken: IntArray /* length must be = 5 */) {
        if (nextInd < (CHUNK_SZ - 1)) {
            currChunk.tokens[nextInd] = newToken[0]
            nextInd++
        }
    }

    fun addTokens(newTokens: IntArray /* Length must be divisible by 5 */) {

    }

}