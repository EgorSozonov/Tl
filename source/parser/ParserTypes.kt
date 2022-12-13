package parser

import lexer.CHUNKSZ
import kotlin.collections.ArrayList
import kotlin.collections.HashMap

const val SCRATCHSZ = 100

class LexicalScope {
    val bindings: HashMap<String, Int> = HashMap(12)        // bindingId
    val functions: HashMap<String, ArrayList<Int>> = HashMap(12) // List (functionBindingId)
}

class Binding(val name: String)

class FunctionBinding(val name: String, val precedence: Int, val arity: Int,
                      /** The maximum number of local vars in any scope of this fun def, to be emitted into bytecode */
                      var maxLocals: Int = 0,
                      /** The max number of stack operands in any scope of this fun def, to be emitted into bytecode */
                      var maxStack: Int = 0,
                      )

class FunctionCall(var name: String, var precedence: Int, var arity: Int, var maxArity: Int, var startByte: Int)

enum class RegularAST(val internalVal: Byte) { // payload
    litInt(0),                        // int value
    litFloat(1),                      // floating-point value
    litString(2),                     // 0
    litBool(3),                       // 1 or 0
    ident(4),                         // index in the bindings table of parser
    idFunc(5),                        // index in the functionBindings table of parser
    binding(6),                       // id of binding that is being defined
    annot(7),                         // index in the annotations table of parser
    fnDefName(8),                     // index in the functionBindings table of parser,
    fnDefParam(9),                    // index in the strings table
}

/** The types of extentful AST that may be in ParserFrames, i.e. whose parsing may need pausing & resuming */
enum class FrameAST(val internalVal: Byte) {
    functionDef(10),
    scope(11),
    expression(12),
    statementAssignment(13),
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


/*

GlobalScope <- from outside, plus functions from this file. all immutable!

Stack Function

Function[ maxLocals maxStack structScope (Stack ParseFrame) ]

Lookup: first in current (Stack ParseFrame), then in current structScope,
then global (those're all immutable).

Toplevel: functions go into the GlobalScope, non-functions into structScope of the entrypoint function.



Closures: todo

 */