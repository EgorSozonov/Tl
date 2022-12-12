package lexer


enum class RegularToken(val internalVal: Byte) {
    intTok(0),
    floatTok(1),
    stringTok(2),
    verbatimString(3),
    word(4),
    dotWord(5),
    atWord(6),
    comment(7),
    operatorTok(8),
}

enum class PunctuationToken(val internalVal: Byte) {
    curlyBraces(10),
    parens(11),
    brackets(12),
    dotBrackets(13),
    statementFun(14),
    statementAssignment(15),
    statementTypeDecl(16),
    dollar(17),
}


/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * tType                                                                              | u5
 * lenBytes                                                                           | u27
 * startByte                                                                          | i32
 * payload (for regular tokens) or lenTokens + empty 32 bits (for punctuation tokens) | i64
 */
data class LexChunk(val tokens: IntArray = IntArray(CHUNKSZ), var next: LexChunk? = null)



/**
 * The real element of this array is struct CommentToken - modeled as 2 32-bit ints
 * startByte                                                          | i32
 * lenBytes                                                           | i32
 */
data class CommentChunk(val tokens: IntArray = IntArray(COMMENTSZ), var next: CommentChunk? = null)


/**
 * Separate object just for comments
 */
class CommentStorage {
    var firstChunk: CommentChunk = CommentChunk()
    var currChunk: CommentChunk
    private var i: Int // current index inside input byte array
    var nextInd: Int // next ind inside the current token chunk
    var totalTokens: Int
    init {
        currChunk = firstChunk
        currChunk = firstChunk
        i = 0
        nextInd = 0
        totalTokens = 0
    }

    fun add(startBytes: Int, lenBytes: Int) {
        ensureSpaceForToken()
        currChunk.tokens[nextInd    ] = startBytes
        currChunk.tokens[nextInd + 1] = lenBytes
        bump()
    }


    private fun ensureSpaceForToken() {
        if (nextInd < (COMMENTSZ - 2)) return

        val newChunk = CommentChunk()
        currChunk.next = newChunk
        currChunk = newChunk
        nextInd = 0
    }


    private fun bump() {
        nextInd += 2
        totalTokens += 1
    }
}

data class Token(var tType: Int, var startByte: Int, var lenBytes: Int, var payload: Long)

data class TokenLite(var tType: Int, var payload: Long)


enum class FileType {
    executable,
    library,
    tests,
}
