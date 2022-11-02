package compiler.parser

import compiler.lexer.CHUNKSZ


class LexicalScope {
    val bindings: HashMap<String, Int>        // bindingId
    val functions: HashMap<String, ArrayList<Int>> // List (functionBindingId)


    init {
        bindings = HashMap(12)
        functions = HashMap(12)
    }
}

class Binding(val name: String) {
}

class FunctionBinding(val name: String, val precedence: Int, val arity: Int) {
}

class FunctionParse(val name: String, val precedence: Int, var arity: Int, val maxArity: Int, val startByte: Int) {
}

enum class RegularAST(val internalVal: Byte) { // payload
    litInt(0),                        // int value
    litFloat(1),                      // floating-point value
    litString(2),                     // 0
    ident(3),                         // index in the bindings table of parser
    idFunc(4),                        // index in the functionBindings table of parser
    operatorr(5),                     // internal value of OperatorType enum
    annot(6),                         // index in the annotations table of parser
    fnDef(7),                         // index in the functionBindings table of parser,
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

data class FunInStack(var operators: ArrayList<FunctionParse>,
                      var indToken: Int,  val lenTokens: Int,
                      var haventPushedFn: Boolean = true)

/**
 * A record about an unknown function binding that has been encountered when parsing
 */
data class UnknownFunLocation(val indNode: Int, val numParameters: Int)