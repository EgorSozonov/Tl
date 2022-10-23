package compiler.parser

import compiler.lexer.CHUNKSZ


class LexicalScope {
    val bindings: Map<String, Binding>

    init {
        bindings = HashMap(10)
    }
}

class Binding(val name: String) {

}

class FunctionBinding(val name: String, val countParams: Int) {

}

enum class RegularAST(val internalVal: Byte) { // payload
    litInt(0),                        // int value
    litFloat(1),                      // floating-point value
    litString(2),                     // 0
    ident(3),                         // index in the bindings table of parser
    identFunc(4),                     // index in the functionBindings table of parser
    annot(5),                         // index in the annotations table of parser
    fnDef(6),                         // number of nodes, index in the functionBindings table of parser,
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
data class ParseChunk(val nodes: IntArray = IntArray(CHUNKSZ), var next: ParseChunk? = null)

enum class FileType {
    executable,
    library,
    testing,
}