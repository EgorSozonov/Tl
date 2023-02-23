package lexer


enum class RegularToken(val internalVal: Byte) {
    // The following group of variants are transferred to the AST byte for byte, with no analysis
    // Their values must exactly correspond with the initial group of variants in "RegularAST"
    // The top value must be stored in "topVerbatimTokenVariant" constant
    intTok(0),
    floatTok(1),
    boolTok(2),           // payload2: 1 or 0
    verbatimString(3),
    underscoreTok(4),

    // This group requires analysis in the parser
    docCommentTok(5),
    stringTok(6),
    word(7),              // payload2: 1 if the word is all capitals
    dotWord(8),           // payload2: 1 if the word is all capitals
    atWord(9),
    reservedWord(10),     // payload2: value of a constant from the 'reserved*' group
    operatorTok(11),      // payload2: OperatorToken encoded as an Int
}

/** Must be the lowest value in the PunctuationToken enum */
const val firstPunctuationTokenType = 12
const val firstCoreFormTokenType = 18

/** Don't forget to update the 'firstPunctuationTokenType' when changing lowest value. stmtCore must be the last */
enum class PunctuationToken(val internalVal: Byte) {
    curlyBraces(12),
    brackets(13),
    parens(14),
    stmtAssignment(15), // payload1: (number of tokens before the assignment operator) shl 16 + (OperatorType)
    stmtTypeDecl(16),
    lexScope(17),
    stmtFn(18),
    stmtReturn(19),
    stmtIf(20),
    stmtLoop(21),
    stmtBreak(22),
    stmtIfEq(23),
    stmtIfPred(24),
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
        if (nextInd < COMMENTSZ) return

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
