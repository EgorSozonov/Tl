package compiler.parser

import compiler.lexer.CHUNKSZ
import compiler.lexer.COMMENTSZ


class LexicalScope {
    val bindings: List<Binding>

    init {
        bindings = ArrayList(10)
    }
}

class Binding(val name: String) {

}

enum class RegularAST(val internalVal: Byte) {
    litInt(0),
    litFloat(1),
    litString(2),
    ident(3),
    identFunc(4),
    annot(5),
}

enum class PunctuationAST(val internalVal: Byte) {
    scope(10),
    funcall(11),
    dataInit(12),
    dataIndexer(13),
    statementFun(15),
    statementAssignment(16),
    statementTypeDecl(17),
}

/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * astType                                                                            | u5
 * lenBytes                                                                           | u27
 * startByte                                                                          | i32
 * payload (for regular tokens) or lenTokens + empty 32 bits (for punctuation tokens) | i64
 */
data class ParseChunk(val tokens: IntArray = IntArray(CHUNKSZ), var next: ParseChunk? = null)

