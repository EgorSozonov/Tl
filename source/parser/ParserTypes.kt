package parser
import lexer.CHUNKSZ
import utils.IntPair
import kotlin.collections.ArrayList
import kotlin.collections.HashMap


class LexicalScope {
    /** Map [name -> identifierId] **/
    val bindings: HashMap<String, Int> = HashMap(12)
    /** Map [name -> List (functionId arity)] */
    val functions: HashMap<String, ArrayList<IntPair>> = HashMap(12)
    /** Array funcId */
    val funDefs: ArrayList<Int> = ArrayList(4)
}

class Binding(val name: String)

class ImportOrBuiltin(val name: String, val precedence: Int, val arity: Int)

class FunctionCall(var nameStringId: Int, var precedence: Int, var arity: Int, var maxArity: Int, var startByte: Int)

enum class RegularAST(val internalVal: Byte) { // payload
    litInt(0),                // int value
    litFloat(1),              // floating-point value
    litString(2),             // 0
    litBool(3),               // 1 or 0
    ident(4),                 // index in the bindings table of parser
    idFunc(5),                // index in the functionBindings table of parser
    binding(6),               // id of binding that is being defined
    annot(7),                 // index in the annotations table of parser
}

/** The types of extentful AST that may be in ParserFrames, i.e. whose parsing may need pausing & resuming */
enum class FrameAST(val internalVal: Byte) {
    functionDef(10),
    scope(11),
    expression(12),
    statementAssignment(13),
    fnDefPlaceholder(14),     // index in the functions table of AST,
    returnExpression(15)
}

/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * astType                                                                            | u5
 * lenBytes                                                                           | u27
 * startByte                                                                          | i32
 * payload (for regular tokens) or empty 32 bits + lenTokens (for punctuation tokens) | i64
 */
data class ASTChunk(val nodes: IntArray = IntArray(CHUNKSZ), var next: ASTChunk? = null)

data class ScratchChunk(val nodes: IntArray = IntArray(SCRATCHSZ), var next: ScratchChunk? = null)

data class ParseFrame(val extentType: FrameAST, val indNode: Int, val lenTokens: Int,
                      /** 1 for lexer extents that have a prefix token, 0 for extents that are inserted by the parser */
                      val additionalPrefixToken: Int,
                      var tokensRead: Int = 0)

data class Subexpr(var operators: ArrayList<FunctionCall>,
                   var tokensRead: Int, val lenTokens: Int,
                   var firstFun: Boolean = true)

data class FunctionSignature(val nameId: Int, val arity: Int, val typeId: Int, val bodyId: Int)

/*

GlobalScope <- from outside, plus functions from this file. all immutable!

Stack Function

Function[ maxLocals maxStack structScope (Stack ParseFrame) ]

Lookup: first in current (Stack ParseFrame), then in current structScope,
then global (those're all immutable).

Toplevel: functions go into the GlobalScope, non-functions into structScope of the entrypoint function.



Closures: todo

 */