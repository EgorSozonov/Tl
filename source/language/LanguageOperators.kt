package language
import lexer.*


/** Function precedence must be higher than that of any infix operator, yet lower than the prefix operators */
const val functionPrec = 26
const val prefixPrec = 27

/**
 * Values must exactly agree with the operatorSymbols array. The order is the same.
 */
const val countOperators = 36
const val opTNotEqual               = 0 // !=
const val opTBoolNegation           = 1 // !
const val opTSize                   = 2 // #
const val opTNotEmpty               =  3 // $
const val opTRemainder              =  4 // %
const val opTBoolAnd                =  5 // &&
const val opTTypeAnd                =  6 // &
const val opTTimes                  =  7 // *
const val opTIncrement              =  8 // ++
const val opTPlus                   =  9 // +
const val opTDecrement              = 10 // --
const val opTMinus                  = 11 // -
const val opTDivBy                  = 12 // /
const val opTMutation               = 13 // :=
const val opTRangeHalf              = 14 // ;<
const val opTRange                  = 15 // ;
const val opTLessThanEq             = 16 // <=
const val opTBitShiftLeft           = 17 // <<
const val opTArrowLeft              = 18 // <-
const val opTLessThan               = 19 // <
const val opTArrowRight             = 20 // =>
const val opTEquality               = 21 // ==
const val opTImmDefinition          = 22 // =
const val opTIntervalBothInclusive  = 23 // >=<=
const val opTIntervalLeftInclusive  = 24 // >=<
const val opTIntervalRightInclusive = 25 // ><=
const val opTIntervalExclusive      = 26 // ><
const val opTGreaterThanEq          = 27 // >=
const val opTBitshiftRight          = 28 // >>
const val opTGreaterThan            = 29 // >
const val opTQuestionMark           = 30 // ?
const val opTBackslash              = 31 // backslash
const val opTExponentiation         = 32 // ^
const val opTBoolOr                 = 33 // ||
const val opTPipe                   = 34 // |
const val opTTilde                  = 35 // ~



//data class OperatorToken(val opType: OperatorType, val extended: Int, val isAssignment: Boolean) {
//    /**
//     * The lowest bit encodes isAssignment, the adjacent 2 bits encode 'extended', and the
//     * next higher bits encode 'operatorType'
//     */
//    fun toInt(): Int {
//        return (opType.internalVal shl 3) + ((extended and 4) shl 1) + (if (isAssignment) { 1 } else { 0 })
//    }
//
//    companion object {
//        fun fromInt(i: Int): OperatorToken {
//            val isAssignment = (i and 1) == 1
//            val extendedInt = (i and 6) shr 1
//            val opType = OperatorType.values().getOrNull(i shr 3)!!
//            return OperatorToken(opType, extendedInt, isAssignment)
//        }
//    }
//}


/*
 * Definition of the operators with info for those that act as functions. The order must be exactly the same as
 * the "opT" constants above. Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
 *
 * There is a closed set of operators in the language.
 *
 * For added flexibility, most operators are extended into another plane,
 * for example "+" may be extended into "+." while "/" may be extended into "/.".
 * These extended operators are not defined by the language, but may be defined
 * for any type by the user, with the return type being arbitrary.
 * For example, the type of 3D vectors may have three different multiplication
 * operators: * for vector product, *. for multiplication by a scalar, *: for scalar product.
 *
 * Plus, all the extensible operators (and only them) may have "=" appended to them for use
 * as assignment operators. For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
 *
 * This OperatorToken class records the base type of operator, its extension (0, 1 or 2),
 * and whether it is the assignment version of itself.
 * In the token stream, both of these values are stored inside the 64-bit payload of the Token.
 */
val operatorDefinitions = arrayListOf(
    OpDef("!=", 11, 2, false,            0, byteArrayOf(aExclamation, aEqual, 0, 0)),               // boolean inequality
    OpDef("!", prefixPrec,               1, false, 1, byteArrayOf(aExclamation, 0, 0, 0)),          // boolean negation
    OpDef("#", prefixPrec, 1, false,     2, byteArrayOf(aSharp, 0, 0, 0)),                          // size of, length, absolute value etc
    OpDef("$", prefixPrec, 1, false,     3, byteArrayOf(aDollar, 0, 0, 0)),                         // is not empty/null
    OpDef("%", 20, 2, true,              4, byteArrayOf(aPercent, 0, 0, 0)),                        // remainder of division
    OpDef("&&", 9, 2, true,              5, byteArrayOf(aAmpersand, aAmpersand, 0, 0)),             // boolean and
    OpDef("&", 9, 2, false,              6, byteArrayOf(aAmpersand, 0, 0, 0)),                      // bitwise and, type intersection
    OpDef("*", 20, 2, true,              7, byteArrayOf(aTimes, 0, 0, 0)),                          // multiplication
    OpDef("++", functionPrec, 1, false,  8, byteArrayOf(aPlus, aPlus, 0, 0), true),                 // increment
    OpDef("+", 17, 2, false,             9, byteArrayOf(aPlus, 0, 0, 0)),                           // addition
    OpDef("--", functionPrec, 1, false, 10, byteArrayOf(aMinus, aMinus, 0, 0), true),               // decrement
    OpDef("-", prefixPrec, 1, false,    11, byteArrayOf(aMinus, 0, 0, 0)),                          // subtraction
    OpDef("/", 20, 2, true,             12, byteArrayOf(aDivBy, 0, 0, 0)),                          // division
    OpDef(":=", 0, 0, false,            -1, byteArrayOf(aColon, aEqual, 0, 0)),                     // mutable assignment
    OpDef(";<", 1, 2, false,            13, byteArrayOf(aColon, aLessThan, 0, 0)),                  // half-exclusive interval/range
    OpDef(";", 1, 2, false,             14, byteArrayOf(aColon, 0, 0, 0)),                          // inclusive interval/range
    OpDef("<=", 12, 2, false,           15, byteArrayOf(aLessThan, aEqual, 0, 0)),                  // less than or equal
    OpDef("<<", 14, 2, true,            16, byteArrayOf(aLessThan, aLessThan, 0, 0)),               // bitwise left shift
    OpDef("<-", 0, 0, false,            -1, byteArrayOf(aLessThan, aMinus, 0, 0)),                  // receive from channel
    OpDef("<", 12, 2, false,            17, byteArrayOf(aLessThan, 0, 0, 0)),                       // less than
    OpDef("=>", 0, 0, false,            -1, byteArrayOf(aEqual, aGreaterThan, 0, 0)),               // right arrow
    OpDef("==", 11, 2, false,           18, byteArrayOf(aEqual, aEqual, 0, 0)),                     // boolean equality
    OpDef("=", 0, 0, false,             -1, byteArrayOf(aEqual, 0, 0, 0)),                          // immutable assignment
    OpDef(">=<=", 12, 3, false,         19, byteArrayOf(aGreaterThan, aEqual, aLessThan, aEqual)),  // is within inclusive range
    OpDef(">=<", 12, 3, false,          20, byteArrayOf(aGreaterThan, aEqual, aLessThan, 0)),       // is within half-exclusive range
    OpDef("><=", 12, 3, false,          21, byteArrayOf(aGreaterThan, aLessThan, aEqual, 0)),       // is within half-exclusive range
    OpDef("><", 12, 3, false,           22, byteArrayOf(aGreaterThan, aLessThan, 0, 0)),            // is within exclusive range
    OpDef(">=", 12, 2, false,           23, byteArrayOf(aGreaterThan, aEqual, 0, 0)),               // greater than or equal
    OpDef(">>", 14, 2, true,            24, byteArrayOf(aGreaterThan, aGreaterThan, 0, 0)),         // bitwise right shift
    OpDef(">", 12, 2, false,            25, byteArrayOf(aGreaterThan, 0, 0, 0)),                    // greater than
    OpDef("?", prefixPrec, 1, false,    -1, byteArrayOf(aQuestion, 0, 0, 0)),                       // nullable type operator
    OpDef("\\", 0, 0, false,            -1, byteArrayOf(aBackslash, 0, 0, 0)),                      // lambda
    OpDef("^", 21, 2, false,            26, byteArrayOf(aCaret, 0, 0, 0)),                          // exponentiation
    OpDef("||", 3, 2, true,             27, byteArrayOf(aPipe, aPipe, 0, 0)),                       // boolean or
    OpDef("|", 9, 2, false,             28, byteArrayOf(aPipe, 0, 0, 0)),                           // bitwise xor
    OpDef("~", prefixPrec, 1, false,    29, byteArrayOf(aTilde, 0, 0, 0))                           // type operator for pointers
)

/** All the symbols an operator may start with. The : is absent because it's handled by "lexColon".
 * The - is is absent because it's handled by "lexMinus".
 */
val operatorStartSymbols = byteArrayOf(
    aExclamation, aSharp, aDollar, aPercent, aAmpersand, aTimes, aPlus, aDivBy,
    aSemicolon, aLessThan, aEqual, aGreaterThan, aQuestion, aBackslash, aCaret, aPipe
)


data class OpDef(val name: String, val precedence: Int, val arity: Int,
                 /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
                 val extensible: Boolean,
                 /* Indices of operators that act as functions in the built-in bindings array.
                  * Contains -1 for non-functional operators
                  */
                 val bindingIndex: Int,
                 val bytes: ByteArray,
                 val overloadable: Boolean = false)





