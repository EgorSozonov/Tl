package parser
import lexer.*
import utils.IntPair
import kotlin.collections.ArrayList
import kotlin.collections.HashMap


const val SCRATCHSZ = 100 // must be divisible by 4

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

data class BuiltinOperator(val name: String, val arity: Int)

data class Import(val name: String, val nativeName: String, val arity: Int)

class FunctionCall(var nameStringId: Int, var precedence: Int, var arity: Int, var maxArity: Int,
                   /** Used to distinguish concrete type constructors from type parameters that are constructors */
                   var isCapitalized: Boolean,
                   var startByte: Int)

const val nodInt = tokInt
const val nodFloat = tokFloat
const val nodBool = tokBool // payload2 = 1 or 0
const val nodString = tokString
const val nodUnderscore = tokUnderscore
const val nodDocComment = tokDocComment

const val nodId = 6                 // payload2 = index in the identifiers table of AST
const val nodFunc = 7               // payload1 = arity, negated if it's an operator. payload2 = index in the identifiers table
const val nodBinding = 8            // payload2 = index in the identifiers table
const val nodTypeId = 9             // payload1 = 1 if it's a type identifier (uppercase), 0 if it's a param. payload2 = index in the identifiers table
const val nodTypeFunc = 10          // payload1 = 1 bit isOperator, 1 bit is a concrete type (uppercase), 30 bits arity
const val nodAnnotation = 11        // index in the annotations table of parser

const val nodScope = 12             // payload1 = 1 if this scope is the right-hand side of an assignment
const val nodExpr = 13
const val nodFunctionDef = 14       // payload1 = index in the 'functions' table of AST
const val nodFnDefPlaceholder = 15  // payload1 = index in the 'functions' table of AST
const val nodStmtAssignment = 16
const val nodReturn = 17
const val nodTypeDecl = 18
const val nodIfSpan = 19            // payload1 = number of clauses
const val nodIfClause = 20
const val nodFor = 21
const val nodBreak = 22

/** Must be the lowest value of the Span AST node types above */
const val firstSpanASTType = nodScope

/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * astType                                                                            | u6
 * startByte                                                                          | u26
 * lenBytes                                                                           | i32
 * payload (for regular tokens) or empty 32 bits + lenTokens (for punctuation tokens) | i64
 */

data class FunctionSignature(val nameId: Int, val arity: Int, val typeId: Int, val bodyId: Int)

