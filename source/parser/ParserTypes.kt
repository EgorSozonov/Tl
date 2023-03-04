package parser
import utils.IntPair
import kotlin.collections.ArrayList
import kotlin.collections.HashMap


data class ParseFrame(val spanType: Int, val indStartNode: Int, val sentinelToken: Int, val coreType: Int = 0) {
    var clauseInd: Int = 0
}

data class Subexpr(var operators: ArrayList<FunctionCall>,
                   val sentinelToken: Int,
                   var isStillPrefix: Boolean,
                   val isInHeadPosition: Boolean)

class LexicalScope() {
    /** Array funcId - these are the function definitions in this scope (see 'detectNestedFunctions'). */
    val funDefs: ArrayList<Int> = ArrayList(4)
    /** Map [name -> identifierId] **/
    val bindings: HashMap<String, Int> = HashMap(12)
    /** Map [name -> List (functionId arity)] */
    val functions: HashMap<String, ArrayList<IntPair>> = HashMap(12)

    /** Map [name -> identifierId] **/
    val types: HashMap<String, Int> = HashMap(12)
    /** Map [name -> List (functionId arity)] */
    val typeFuncs: HashMap<String, ArrayList<IntPair>> = HashMap(12)
}

/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * astType                                                                            | u6
 * startByte                                                                          | u26
 * lenBytes                                                                           | i32
 * payload (for regular tokens) or empty 32 bits + lenTokens (for punctuation tokens) | i64
 */

data class FunctionSignature(val nameId: Int, val arity: Int, val typeId: Int, val bodyId: Int)

