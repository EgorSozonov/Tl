package parser
import lexer.CHUNKSZ
import utils.IntPair
import kotlin.collections.ArrayList
import kotlin.collections.HashMap


const val SCRATCHSZ = 100 // must be divisible by 4

data class ParseFrame(val spanType: SpanAST, val indStartNode: Int, val sentinelToken: Int, val coreType: Int = 0) {
    var clauseInd: Int = 0
}

data class Subexpr(var operators: ArrayList<FunctionCall>,
                   val sentinelToken: Int,
                   var isStillPrefix: Boolean = false)

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

enum class RegularAST(val internalVal: Byte) {
                   // payload2 at +3:                           // extra payload1 at +2:
    // The following group of variants are transferred from the lexer byte for byte, with no analysis
    // Their values must exactly correspond with the initial group of variants in "RegularToken"
    litInt(0),     // int value
    litFloat(1),   // floating-point value
    litBool(2),    // 1 or 0
    verbatimStr(3),
    underscore(4),

    // This group requires analysis in the parser and doesn't have to match "RegularToken"
    docComment(5),
    litString(6),
    ident(7),      // index in the identifiers table of AST
    idFunc(8),     // index in the identifiers table of AST     // arity, negated if it's an operator
    binding(9),    // index in the identifiers table of AST
    idType(10),    // index in the types table of AST           // 1 if it's a type identifier (uppercase), 0 if it's a param (lowercase)
    typeFunc(11),  // index in the functions table of AST       // 1 bit isOperator, 1 bit is a concrete type (= uppercase), 30 bits arity
    annot(12),     // index in the annotations table of parser
}

/** Must be the lowest value in the PunctuationToken enum */
const val firstSpanASTType = 13

/** The types of spanful AST that may be in ParseFrames, i.e. whose parsing may need pausing & resuming */
enum class SpanAST(val internalVal: Byte) {
                              // extra payload1 at +2:
    scope(13),                // 1 if this scope is the right-hand side of an assignment
    expression(14),
    functionDef(15),          // index in the 'functions' table of AST
    fnDefPlaceholder(16),     // index in the 'functions' table of AST
    statementAssignment(17),
    returnExpression(18),
    typeDecl(19),
    ifSpan(20),              // number of clauses
    ifClauseGroup(21),
    loop(22),
    breakSpan(23),
}

// we met x inside of y
// _| y
// x| .

/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * astType                                                                            | u5
 * lenBytes                                                                           | u27
 * startByte                                                                          | i32
 * payload (for regular tokens) or empty 32 bits + lenTokens (for punctuation tokens) | i64
 */
data class ASTChunk(val nodes: IntArray = IntArray(CHUNKSZ), var next: ASTChunk? = null)

data class ScratchChunk(val nodes: IntArray = IntArray(SCRATCHSZ), var next: ScratchChunk? = null)

data class FunctionSignature(val nameId: Int, val arity: Int, val typeId: Int, val bodyId: Int)

