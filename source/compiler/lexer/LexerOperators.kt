package compiler.lexer


/**
 * Values must exactly agree with the operatorSymbols array. The order is the same.
 */
enum class OperatorType(val value: Int) {
    notEqualTo(0),           boolNegation(1),        remainder(2),          boolAnd(3),
    composition(4),          exponentiation(5),      times(6),              increment(7),
    plus(8),                 decrement(9),           arrowRight(10),             minus(11),
    rangeHalf(12),           range(13),              divBy(14),             elseBranch(15),
    mutation(16),            typeDecl(17),           colon(18),              lessThanEqInterval(19),
    lessThanInterval(20),    bitshiftLeft(21),       lessThanEq(22),         arrowLeft(23),
    lessThan(24),            equality(25),           arrowFat(26),            immDefinition(27),
    greaterThanEqInterv(28), greaterThanInterv(29),  greaterThanEq(30),       bitshiftRight(31),
    greaterThan(32),         backslash(33),          xor(34),                 boolOr(35),
    pipe(36),
}


/**
 * There is a closed set of operators in the language.
 *
 * For added flexibility, most operators are extended into two more planes,
 * for example + is extended into +. and +:, / to /. and to /:.
 * These extended operators are not defined by the language, but may be defined
 * for any type by the user.
 * For example, the type of 3D vectors may have 3 multiplication
 * operators: * for vector product, *. for multiplication by a scalar, *: for scalar product.
 *
 * Plus, all the extensible operators (and only them) may have '=' appended to them for use
 * in assignment operators. For example, 'a &&:= b' means 'a = a &&: b' for whatever '&&:' means.
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
    fun toInt(): Long {
        return (opType.value shl 3).toLong() + ((extended and 4) shl 1) + (if (isAssignment) { 1 } else { 0 })
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


val operatorStartSymbols = byteArrayOf(
    aColon, aAmpersand, aPlus, aMinus, aDivBy, aTimes, aExclamation,
    aPercent, aCaret, aPipe, aGreaterThan, aLessThan, aQuestion, aEquals,
)

/**
 * This is an array of 3-byte arrays containing operator byte sequences.
 * Sorted as follows: 1. by first byte ASC 2. by length DESC.
 * It's used to lex operator symbols using left-to-right search.
 */
val operatorSymbols = byteArrayOf(
    aExclamation, aEquals, 0,   aExclamation, 0, 0,            aPercent, 0, 0,                aAmpersand, aAmpersand, 0,
    aAmpersand, 0, 0,           aTimes, aTimes, 0,             aTimes, 0, 0,                  aPlus, aPlus, 0,
    aPlus, 0, 0,                aMinus, aMinus, 0,             aMinus, aGreaterThan, 0,       aMinus, 0, 0,
    aDot, aDot, aLessThan,      aDot, aDot, 0,                 aDivBy, 0, 0,                  aColon, aGreaterThan, 0,
    aColon, aEquals, 0,         aColon, aColon, 0,             aColon, 0, 0,                  aLessThan, aEquals, aDot,
    aLessThan, aDot, 0,         aLessThan, aLessThan, 0,       aLessThan, aEquals, 0,         aLessThan, aMinus, 0,
    aLessThan, 0, 0,            aEquals, aEquals, 0,           aEquals, aGreaterThan, 0,      aEquals, 0, 0,
    aGreaterThan, aEquals, aDot,aGreaterThan, aDot, 0,         aGreaterThan, aEquals, 0,      aGreaterThan, aGreaterThan, 0,
    aGreaterThan, 0, 0,         aBackslash, 0, 0,              aCaret, 0, 0,                  aPipe, aPipe, 0,
    aPipe, 0, 0,
)

/**
 * Marks the 10 "equalable & extensible" operators, i.e. the ones with 5 extra variations.
 * Order must exactly agree with 'operatorSymbols'
 */
val operatorExtensibility = byteArrayOf(
    0, 0, 1, 1,
    0, 1, 1, 0,
    1, 0, 0, 1,
    0, 0, 1, 0,
    0, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 1,
    0, 0, 1, 1,
    0,
)


val nonFun = Triple("", 0, 0)

/**
 * Information about the operators that act as functions. Triples of: name, precedence, arity.
 * Order must exactly agree with 'operatorSymbols'
 */
val operatorFunctionality = arrayListOf(
    Triple("__ne", 11, 2), Triple("__negate", 25, 1), Triple("__remainder", 20, 2), Triple("__boolAnd", 9, 2),
    Triple("__compose", 0, 2), Triple("__power", 23, 2), Triple("__times", 20, 2), Triple("__increment", 25, 1),
    Triple("__plus", 17, 2), Triple("__decrement", 25, 2), nonFun, Triple("__minus", 17, 2),
    Triple("__rangeExcl", 1, 2), Triple("__range", 1, 2), Triple("__divide", 20, 2), nonFun,
    nonFun, nonFun, nonFun, Triple("__leRange", 12, 2),
    Triple("__ltRange", 12, 2), Triple("__shiftLeft", 14, 2), Triple("__le", 12, 2), nonFun,
    Triple("__lt", 12, 2), Triple("__equals", 11, 2), nonFun, nonFun,
    Triple("__geRange", 12, 2), Triple("__gtRange", 12, 2), Triple("__ge", 12, 2), Triple("__shiftRight", 14, 2),
    nonFun, nonFun, Triple("__boolXor", 6, 2), Triple("__boolOr", 3, 2),
    nonFun,
)

/**
 * Indices of operators that act as functions in the built-in bindings array. Contains -1 for non-functional
 * operators.
 * Order must exactly agree with 'operatorSymbols'
 */
val operatorBindingIndices = intArrayOf(
    0, 1, 2, 3,
    4, 5, 6, 7,
    8, 9, -1, 10,
    11, 12, 13, -1,
    -1, -1, -1, 14,
    15, 16, 17, -1,
    18, 19, -1, -1,
    20, 21, 22, 23,
    -1, -1, 24, 25,
    -1
)