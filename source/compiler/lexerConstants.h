#ifndef LEXER_CONSTANTS_H
#define LEXER_CONSTANTS_H

#include "../utils/aliases.h"


#define LEXER_INIT_SIZE 2000

extern const char errorLengthOverflow[];
extern const char errorNonAscii[];
extern const char errorPrematureEndOfInput[];
extern const char errorUnrecognizedByte[];
extern const char errorWordChunkStart[];
extern const char errorWordCapitalizationOrder[];
extern const char errorWordUnderscoresOnlyAtStart[];
extern const char errorWordWrongAccessor[];
extern const char errorWordReservedWithDot[];
extern const char errorNumericEndUnderscore[];
extern const char errorNumericWidthExceeded[];
extern const char errorNumericBinWidthExceeded[];
extern const char errorNumericFloatWidthExceeded[];
extern const char errorNumericEmpty[];
extern const char errorNumericMultipleDots[];
extern const char errorNumericIntWidthExceeded[];
extern const char errorPunctuationExtraOpening[];
extern const char errorPunctuationExtraClosing[];
extern const char errorPunctuationOnlyInMultiline[];
extern const char errorPunctuationUnmatched[];
extern const char errorPunctuationWrongOpen[];
extern const char errorPunctuationDoubleSplit[];
extern const char errorOperatorUnknown[];
extern const char errorOperatorAssignmentPunct[];
extern const char errorOperatorTypeDeclPunct[];
extern const char errorOperatorMultipleAssignment[];
extern const char errorCoreNotInsideStmt[];
extern const char errorCoreMissingParen[];
extern const char errorCoreNotAtSpanStart[];
extern const char errorDocComment[];

/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
extern const byte maxInt[19];

/** 2**53 */
extern const byte maximumPreciselyRepresentedFloatingInt[16];

/** All the symbols an operator may start with. The ':' is absent because it's handled by lexColon.
* The '-' is absent because it's handled by 'lexMinus'.
*/
#define countOperatorStartSymbols 15
extern const int operatorStartSymbols[countOperatorStartSymbols];

/**
 * Regular (leaf) Token types
 */
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt          0
#define tokFloat        1
#define tokBool         2
#define tokString       3
#define tokUnderscore   4
#define tokDocComment   5

// This group requires analysis in the parser
#define tokWord         6      // payload2 = 1 if the last chunk of a word is capital
#define tokDotWord      7      // payload2 = 1 if the last chunk of a word is capital
#define tokAtWord       8      // "@annotation"
#define tokFuncWord     9      // ":funcName"
#define tokOperator    10      // payload1 = OperatorToken encoded as an Int
#define tokAnd         11
#define tokOr          12
#define tokNodispose   13       // signaling that this value doesn't need its destructor called at scope end // ???

// This is a temporary Token type for use during lexing only. In the final token stream it's replaced with tokParens
#define tokColon       14 

// 100 atom
// 200 expr
// 300 stmt with control flow
// 400 stmt with definition/declaration
// 500 lexical scope
// 600 only at start

// Punctuation (inner node) Token types
#define tokScope       15       // (:
#define tokStmt        16
#define tokParens      17
#define tokBrackets    18
#define tokAccessor    19
#define tokFuncExpr    20       // the ",(foo,bar)" kind of thing  // 200
#define tokAssignment  21       // payload1 = as in tokOperator     // 400
#define tokElse        22       // the "::" which signifies the "else" clause in if-type things
#define tokSemicolon   23       // separates type conditions from function params. "(x T; T Serializable)"

// Core syntax form Token types
#define tokAlias       24       // noParen   // 400
#define tokAssert      25       // noParen   // 300
#define tokAssertDbg   26       // noParen   // 300
#define tokAwait       27       // noParen   // 300
#define tokBreak       28       // noParen   // 300
#define tokCatch       29       // paren "(catch e.msg,print)"  // 500
#define tokContinue    30       // noParen   // 300
#define tokDispose     31       // noParen   // 300
#define tokEmbed       32       // noParen. Embed a text file as a string literal, or a binary resource file // 200
#define tokExport      33       // paren     // 600
#define tokFinally     34       // paren     // 500
#define tokFnDef       35       // specialCase // 400
#define tokIf          36       // paren    // 200/500
#define tokIfEq        37       // like if, but every branch is a value compared using standard equality // 200/500
#define tokIfPr        38       // like if, but every branch is a value compared using custom predicate  // 200/500
#define tokImpl        39       // paren // 400
#define tokIface       40       // 400
#define tokLambda      41       // 500
#define tokLambda1     42       // 500
#define tokLambda2     43       // 500
#define tokLambda3     44       // 500
#define tokLoop        45       // recur operator for tail recursion // 300
#define tokMatch       46       // pattern matching on sum type tag  // 200/500
#define tokMut         47       // 400
#define tokReturn      48       // 300
#define tokStruct      49       // 400
#define tokTry         50       // 500
#define tokYield       51       // 300

#define topVerbatimTokenVariant tokDocComment

/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokAlias

/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokScope
#define countCoreForms 28 // tokYield - tokAlias + 1

/** The indices of reserved words that are stored in token payload2. Must be positive, unique,
 * below "firstPunctuationTokenType" and not clashing with "tokAnd", "tokOr" and "tokNodispose"
 */
#define reservedFalse   2
#define reservedTrue    1

/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array in the .c file.
 * The order is defined by ASCII.
 */
#define countOperators    32 // must be equal to the count of following constants
#define opTNotEqual        0 // !=
#define opTBoolNegation    1 // !
#define opTSize            2 // #
#define opTToString        3 // $
#define opTRemainder       4 // %
#define opTBinaryAnd       5 // && bitwise and
#define opTTypeAnd         6 // & interface intersection
#define opTNotEmpty        7 // '
#define opTTimes           8 // *
#define opTIncrement       9 // ++
#define opTPlus           10 // +
#define opTDecrement      11 // --
#define opTMinus          12 // -
#define opTDivBy          13 // /
#define opTArrowLeft      14 // <-
#define opTBitShiftLeft   15 // <<
#define opTLTEQ           16 // <=
#define opTComparator     17 // <>
#define opTLessThan       18 // <
#define opTEquality       19 // ==
#define opTIntervalBoth   20 // >=<= inclusive interval check
#define opTIntervalLeft   21 // >=<  left-inclusive interval check
#define opTIntervalRight  22 // ><=  right-inclusive interval check
#define opTIntervalExcl   23 // ><   exclusive interval check
#define opTGTEQ           24 // >=
#define opTBitshiftRight  25 // >>   right bit shift
#define opTGreaterThan    26 // >
#define opTNullCoalesce   27 // ?:   null coalescing operator
#define opTQuestionMark   28 // ?    nullable type operator
#define opTExponent       29 // ^    exponentiation
#define opTBoolOr         30 // ||   bitwise or
#define opTXor            31 // |    bitwise xor

#define opTMutation       40 // Not a real operator, just a tag for :=
#define opTDefinition     41 // Not a real operator, just a tag for  =

/** Reserved words of Tl in ASCII byte form */
#define countReservedLetters         25 // length of the interval of letters that may be init for reserved words (A to Y)
#define countReservedWords           31 // count of different reserved words below

static const byte reservedBytesAlias[]       = { 97, 108, 105, 97, 115 };
static const byte reservedBytesAssert[]      = { 97, 115, 115, 101, 114, 116 };
static const byte reservedBytesAssertDbg[]   = { 97, 115, 115, 101, 114, 116, 68, 98, 103 };
static const byte reservedBytesAwait[]       = { 97, 119, 97, 105, 116 };
static const byte reservedBytesBreak[]       = { 98, 114, 101, 97, 107 };
static const byte reservedBytesCatch[]       = { 99, 97, 116, 99, 104 };
static const byte reservedBytesContinue[]    = { 99, 111, 110, 116, 105, 110, 117, 101 };
static const byte reservedBytesDispose[]     = { 100, 105, 115, 112, 111, 115, 101 };
static const byte reservedBytesEmbed[]       = { 101, 109, 98, 101, 100 };
static const byte reservedBytesExport[]      = { 101, 120, 112, 111, 114, 116 };
static const byte reservedBytesFalse[]       = { 102, 97, 108, 115, 101 };
static const byte reservedBytesFinally[]     = { 102, 105, 110, 97, 108, 108, 121 };
static const byte reservedBytesFn[]          = { 102, 110 };
static const byte reservedBytesIf[]          = { 105, 102 };
static const byte reservedBytesIfEq[]        = { 105, 102, 69, 113 };
static const byte reservedBytesIfPr[]        = { 105, 102, 80, 114 };
static const byte reservedBytesImpl[]        = { 105, 109, 112, 108 };
static const byte reservedBytesInterface[]   = { 105, 110, 116, 101, 114, 102, 97, 99, 101 };
static const byte reservedBytesLambda[]      = { 108, 97, 109 };
static const byte reservedBytesLambda1[]     = { 108, 97, 109, 49 };
static const byte reservedBytesLambda2[]     = { 108, 97, 109, 50 };
static const byte reservedBytesLambda3[]     = { 108, 97, 109, 51 };
static const byte reservedBytesLoop[]        = { 108, 111, 111, 112 };
static const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
static const byte reservedBytesMut[]         = { 109, 117, 116 };
static const byte reservedBytesNoDispose[]   = { 110, 111, 68, 105, 115, 112, 111, 115, 101}; // record?
static const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
static const byte reservedBytesStruct[]      = { 115, 116, 114, 117, 99, 116 };
static const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
static const byte reservedBytesTry[]         = { 116, 114, 121 };
static const byte reservedBytesYield[]       = { 121, 105, 101, 108, 100 };


/** Function precedence must be higher than that of any infix operator, yet lower than the prefix operators */
#define functionPrec  26
#define prefixPrec    27

#define aALower       97
#define aBLower       98
#define aCLower       99
#define aFLower      102
#define aNLower      110
#define aXLower      120
#define aYLower      121
#define aZLower      122
#define aAUpper       65
#define aFUpper       70
#define aZUpper       90
#define aDigit0       48
#define aDigit1       49
#define aDigit9       57

#define aPlus         43
#define aMinus        45
#define aTimes        42
#define aDivBy        47
#define aDot          46
#define aPercent      37

#define aParenLeft    40
#define aParenRight   41
#define aCurlyLeft   123
#define aCurlyRight  125
#define aBracketLeft  91
#define aBracketRight 93
#define aPipe        124
#define aAmp          38
#define aTilde       126
#define aBackslash    92

#define aSpace        32
#define aNewline      10
#define aCarrReturn   13

#define aApostrophe   39
#define aQuote        34
#define aSharp        35
#define aDollar       36
#define aUnderscore   95
#define aCaret        94
#define aAt           64
#define aColon        58
#define aSemicolon    59
#define aExclamation  33
#define aQuestion     63
#define aComma        44
#define aEqual        61
#define aLT           60
#define aGT           62

#endif
