#ifndef LANGUAGE_OPERATORS_H
#define LANGUAGE_OPERATORS_H

#include "../utils/String.h"
#include "../utils/Aliases.h"

/** Function precedence must be higher than that of any infix operator, yet lower than the prefix operators */
#define functionPrec 26
#define prefixPrec 27

/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array below. The order is defined by ASCII.
 */
#define opTNotEqual                0 // !=
#define opTBoolNegation            1 // !
#define opTSize                    2 // #
#define opTNotEmpty                3 // $
#define opTRemainder               4 // %
#define opTBoolAnd                 5 // &&
#define opTPointer                 6 // &
#define opTTimes                   7 // *
#define opTIncrement               8 // ++
#define opTPlus                    9 // +
#define opTDecrement              10 // --
#define opTMinus                  11 // -
#define opTDivBy                  12 // /
#define opTMutation               13 // :=
#define opTRangeHalf              14 // ;<
#define opTRange                  15 // ;
#define opTLessThanEq             16 // <=
#define opTBitShiftLeft           17 // <<
#define opTArrowLeft              18 // <-
#define opTLessThan               19 // <
#define opTArrowRight             20 // =>
#define opTEquality               21 // ==
#define opTImmDefinition          22 // =
#define opTIntervalBothInclusive  23 // >=<=
#define opTIntervalLeftInclusive  24 // >=<
#define opTIntervalRightInclusive 25 // ><=
#define opTIntervalExclusive      26 // ><
#define opTGreaterThanEq          27 // >=
#define opTBitshiftRight          28 // >>
#define opTGreaterThan            29 // >
#define opTQuestionMark           30 // ?
#define opTBackslash              31 // backslash
#define opTExponentiation         32 // ^
#define opTBoolOr                 33 // ||
#define opTPipe                   34 // |


/**
 * There is a closed set of operators in the language.
 *
 * For added flexibility, most operators are extended into two more planes,
 * for example '+' may be extended into '+.', while '/' may be extended into '/.'.
 * These extended operators are declared by the language, and may be defined
 * for any type by the user, with the return type being arbitrary.
 * For example, the type of 3D vectors may have two different multiplication
 * operators: * for vector product and *. for scalar product.
 *
 * Plus, all the extensible operators (and only them) have automatic assignment counterparts.
 * For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
 *
 * This OperatorToken class records the base type of operator, its extension (0, 1 or 2),
 * and whether it is the assignment version of itself.
 * In the token stream, both of these values are stored inside the 32-bit payload2 of the Token.
 */
typedef struct {
    unsigned int opType : 6;
    unsigned int extended : 2;
    unsigned int isAssignment: 1;
} OperatorToken;

typedef struct {
    String* name;
    byte bytes[4];
    int precedence;
    int arity;
    /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
    bool extensible;
    /* Indices of operators that act as functions in the built-in bindings array.
     * Contains -1 for non-functional operators
     */
    int bindingIndex;
    bool overloadable;
} OpDef;

typedef struct {
    int countOperatorStartSymbols;
    int* operatorStartSymbols; // array
    int countOperators;
    OpDef* operators; // array
} LanguageDefinition;



/**
 * Encoding. This is simply ASCII
 */
#define aALower  97
#define aBLower  98
#define aCLower  99
#define aFLower 102
#define aNLower 110
#define aXLower 120
#define aWLower 119
#define aZLower 122
#define aAUpper  65
#define aFUpper  70
#define aZUpper  90
#define aDigit0  48
#define aDigit1  49
#define aDigit9  57

#define aPlus    43
#define aMinus   45
#define aTimes   42
#define aDivBy   47
#define aDot     46
#define aPercent 37

#define aParenLeft    40
#define aParenRight   41
#define aCurlyLeft   123
#define aCurlyRight  125
#define aBracketLeft  91
#define aBracketRight 93
#define aPipe        124
#define aAmpersand    38
#define aTilde       126
#define aBackslash    92

#define aSpace          32
#define aNewline        10
#define aCarriageReturn 13

#define aApostrophe  39
#define aQuote       34
#define aSharp       35
#define aDollar      36
#define aUnderscore  95
#define aCaret       94
#define aAt          64
#define aColon       58
#define aSemicolon   59
#define aExclamation 33
#define aQuestion    63
#define aEqual       61

#define aLessThan    60
#define aGreaterThan 62


const OpDef noFun = {
    .name = &empty,
    .bytes = {0, 0, 0, 0},
    .precedence = 0,
    .arity = 0,
    .extensible = false,
    .bindingIndex = -1,
    .overloadable = false
};


/** The indices of reserved words that are stored in token payload2. Must be positive, unique
 * and below "firstPunctuationTokenType"
 */
//const const byte reservedBreak       =  1
//const const byte reservedCatch       =  5
//const const byte reservedExport      =  6
#define reservedFalse         2
//const const byte reservedFn          =  8
//const const byte reservedIf          =  2
//const const byte reservedIfEq        =  9
//const const byte reservedIfPred      = 10
//const const byte reservedImport      = 11
//const const byte reservedLoop        =  4
//const const byte reservedMatch       = 12
//const const byte reservedReturn      =  3
//const const byte reservedServicetest = 13
#define reservedTrue         1
//const const byte reservedTry         = 15
//const const byte reservedType        = 16
//const const byte reservedUnittest    = 17
//const const byte reservedWhile       = 18


/** Reserved words of Tl in ASCII byte form */
const byte reservedBytesBreak[]       = { 98, 114, 101, 97, 107 };
const byte reservedBytesCatch[]       = { 99, 97, 116, 99, 104 };
const byte reservedBytesExport[]      = { 101, 120, 112, 111, 114, 116 };
const byte reservedBytesFalse[]       = { 102, 97, 108, 115, 101 };
const byte reservedBytesFn[]          = { 102, 110 };
const byte reservedBytesFor[]         = { 102, 111, 114 };
const byte reservedBytesImport[]      = { 105, 109, 112, 111, 114, 116 };
const byte reservedBytesLoop[]        = { 108, 111, 111, 112 };
const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
const byte reservedBytesServicetest[] = { 115, 101, 114, 118, 105, 99, 101, 116, 101, 115, 116 };
const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
const byte reservedBytesTry[]         = { 116, 114, 121 };
const byte reservedBytesType[]        = { 116, 121, 112, 101 };
const byte reservedBytesUnittest[]    = { 117, 110, 105, 116, 116, 101, 115, 116 };
const byte reservedBytesWhile[]       = { 119, 104, 105, 108, 101 };

#endif
