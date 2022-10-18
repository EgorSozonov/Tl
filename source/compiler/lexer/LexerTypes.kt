package compiler.lexer


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


/**
 * Values must exactly agree with the LexerConstants.operatorSymbols array. The order is the same.
 */
enum class OperatorType(val value: Int) {
    notEqualTo(0),           boolNegation(1),        remainder(2),          boolAnd(3),
    composition(4),          exponentiation(5),      times(6),              increment(7),
    plus(8),                 decrement(9),           arrowRight(10),             minus(11),
    rangeHalf(12),           range(13),              divBy(14),             elseBranch(15),
    mutation(16),            colon(17),              lessThanEqInterval(18), lessThanInterval(19),
    bitshiftLeft(20),        lessThanEq(21),         arrowLeft(22),         lessThan(23),
    equality(24),            arrowFat(25),            immDefinition(26),      greaterThanEqInterv(27),
    greaterThanInterv(28),   greaterThanEq(29),       bitshiftRight(30),      greaterThan(31),
    backslash(32),           xor(33),                 boolOr(34),              pipe(35),
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
    fun toInt(): Long {
        return (opType.value shl 3).toLong() + ((extended and 4) shl 1) + (if (isAssignment) { 1 } else { 0 })
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


data class Token(var tType: Int, var startByte: Int, var lenBytes: Int, var payload: Long)