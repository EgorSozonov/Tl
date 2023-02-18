#ifndef LANGUAGE_OPERATORS_H
#define LANGUAGE_OPERATORS_H
import lexer.*


/** Must equal the index of the "-" oper in the 'operatorDefinitions' array */
const val minusOperatorIndex = 13

/** Function precedence must be higher than that of any infix operator, yet lower than the prefix operators */
const val functionPrec = 26
const val prefixPrec = 27

/**
 * Values must exactly agree with the operatorSymbols array. The order is the same.
 */
enum class OperatorType(val internalVal: Int) {
    notEqualTo(0),               boolNegation(1),           size(2),
    remainder(3),                boolAnd(4),                pointer(5),
    notEmpty(6),                 exponentiation(7),         times(8),
    increment(9),                plus(10),                  arrowRight(11),
    decrement(12),               minus(13),                 rangeHalf(14),
    range(15),                   divBy(16),                 mutation(17),
    elseBranch(18),              lessThanEq(19),            bitshiftLeft(20),
    arrowLeft(21),               lessThan(22),              equality(23),
    immDefinition(24),           intervalBothInclusive(25), intervalLeftInclusive(26),
    intervalRightInclusive(27),  intervalNonInclusive(28),  greaterThanEq(29),
    bitshiftRight(30),           greaterThan(31),           questionMark(32),
    backslash(33),               dereference(34),           boolOr(35),
    pipe(36),
}

/**
 * There is a closed set of operators in the language.
 *
 * For added flexibility, most operators are extended into two more planes,
 * for example + may be extended into +. and +:, while / may be extended into /. and /:.
 * These extended operators are not defined by the language, but may be defined
 * for any type by the user, with the return type being arbitrary.
 * For example, the type of 3D vectors may have three different multiplication
 * operators: * for vector product, *. for multiplication by a scalar, *: for scalar product.
 *
 * Plus, all the extensible operators (and only them) may have '=' appended to them for use
 * as assignment operators. For example, "a &&:= b" means "a = a &&: b" for whatever "&&:" means.
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
    fun toInt(): Int {
        return (opType.internalVal shl 3) + ((extended and 4) shl 1) + (if (isAssignment) { 1 } else { 0 })
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


val noFun = OpDef("", 0, 0, false, -1)

/*
 * Definition of the operators with info for those that act as functions. The order must be exactly the same as
 * OperatorType and "operatorSymbols".
 */
val operatorDefinitions = arrayListOf(
    OpDef("!=", 11, 2, false, 0),                  OpDef("!", prefixPrec, 1, false, 1),  OpDef("#", prefixPrec, 1, false, 2),
    OpDef("%", 20, 2, true, 3),                    OpDef("&&", 9, 2, true, 4),           OpDef("&", prefixPrec, 1, false, 5),
    OpDef("'", prefixPrec, 1, false, 6),           OpDef("**", 23, 2, true, 7),          OpDef("*", 20, 2, true, 8),
    OpDef("++", functionPrec, 1, false, 9, true),  OpDef("+", 17, 2, true, 10),          noFun /* -> */,
    OpDef("--", functionPrec, 1, false, 11, true), OpDef("-", 17, 2, true, 12),          OpDef("..<", 1, 2, false, 13),
    OpDef("..", 1, 2, false, 14),                  OpDef("/", 20, 2, true, 15),          noFun /* := */,
    noFun /* : */,                                 OpDef("<=", 12, 2, false, 16),        OpDef("<<", 14, 2, true, 17),
    noFun /* <- */,                                OpDef("<", 12, 2, false, 18),         OpDef("==", 11, 2, false, 19),
    noFun /* = */,                                 OpDef(">=<=", 12, 3, false, 20),      OpDef(">=<", 12, 3, false, 21),
    OpDef("><=", 12, 3, false, 22),                OpDef("><", 12, 3, false, 23),        OpDef(">=", 12, 2, false, 24),
    OpDef(">>", 14, 2, true, 25),                  OpDef(">", 12, 2, false, 26),         OpDef("?", prefixPrec, 1, false, 27),
    noFun /* \ */,                                 OpDef("^", prefixPrec, 1, false, 28), OpDef("||", 3, 2, true, 29),
    noFun /* | */,
)

/** All the symbols an operator may start with. The : is absent because it's handled by "lexColon".
 * The - is is absent because it's handled by "lexMinus".
 */
val operatorStartSymbols = byteArrayOf(
    aExclamation, aAmpersand, aPlus, aDivBy, aTimes,
    aPercent, aCaret, aPipe, aGreaterThan, aLessThan, aQuestion, aEqual,
    aSharp, aApostrophe,
)

/**
 * This is an array of 4-byte arrays containing operator byte sequences.
 * Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
 * It's used to lex operator symbols using left-to-right search.
 */
val operatorSymbols = byteArrayOf(
    aExclamation, aEqual, 0, 0,          aExclamation, 0, 0, 0,                    aSharp, 0, 0, 0,
    aPercent, 0, 0, 0,                   aAmpersand, aAmpersand, 0, 0,             aAmpersand, 0, 0, 0,
    aApostrophe, 0, 0, 0,                aTimes, aTimes, 0, 0,                     aTimes, 0, 0, 0,
    aPlus, aPlus, 0, 0,                  aPlus, 0, 0, 0,                           aMinus, aGreaterThan, 0, 0,
    aMinus, aMinus, 0, 0,                aMinus, 0, 0, 0,                          aDot, aDot, aLessThan, 0,
    aDot, aDot, 0, 0,                    aDivBy, 0, 0, 0,                          aColon, aEqual, 0, 0,
    aColon, 0, 0, 0,                     aLessThan, aEqual, 0, 0,                  aLessThan, aLessThan, 0, 0,
    aLessThan, aMinus, 0, 0,             aLessThan, 0, 0, 0,                       aEqual, aEqual, 0, 0,
    aEqual, 0, 0, 0,                     aGreaterThan, aEqual, aLessThan, aEqual,  aGreaterThan, aEqual, aLessThan, 0,
    aGreaterThan, aLessThan, aEqual, 0,  aGreaterThan, aLessThan, 0, 0,            aGreaterThan, aEqual, 0, 0,
    aGreaterThan, aGreaterThan, 0, 0,    aGreaterThan, 0, 0, 0,                    aQuestion, 0, 0, 0,
    aBackslash, 0, 0, 0,                 aCaret, 0, 0, 0,                          aPipe, aPipe, 0, 0,
    aPipe, 0, 0, 0,
)

data class OpDef(val name: String, val precedence: Int, val arity: Int,
                 /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
                 val extensible: Boolean,
                 /* Indices of operators that act as functions in the built-in bindings array.
                  * Contains -1 for non-functional operators
                  */
                 val bindingIndex: Int,
                 val overloadable: Boolean = false)



#endif
