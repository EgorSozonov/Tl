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
extern const char errorPunctuationScope[];
extern const char errorOperatorUnknown[];
extern const char errorOperatorAssignmentPunct[];
extern const char errorOperatorTypeDeclPunct[];
extern const char errorOperatorMultipleAssignment[];
extern const char errorOperatorMutableDef[];
extern const char errorCoreNotInsideStmt[];
extern const char errorCoreMisplacedArrow[];
extern const char errorCoreMisplacedElse[];
extern const char errorCoreMissingParen[];
extern const char errorCoreNotAtSpanStart[];
extern const char errorIndentation[];
extern const char errorDocComment[];

/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
extern const byte maxInt[19];

/** 2**53 */
extern const byte maximumPreciselyRepresentedFloatingInt[16];

/** All the symbols an operator may start with. The '-' is absent because it's handled by 'lexMinus' */
#define countOperatorStartSymbols 16
extern const int operatorStartSymbols[countOperatorStartSymbols];

/**
 * Regular (leaf) Token types
 */
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt          0
#define tokLong         1
#define tokFloat        2
#define tokBool         3      // pl2 = value (1 or 0)
#define tokString       4
#define tokUnderscore   5
#define tokDocComment   6

// This group requires analysis in the parser
#define tokWord         7      // pl2 = index in the string table
#define tokTypeName     8
#define tokDotWord      9      // ".fieldName", pl's the same as tokWord
#define tokOperator    10      // pl1 = OperatorToken, one of the "opT" constants below
#define tokDispose     11

// This is a temporary Token type for use during lexing only. In the final token stream it's replaced with tokParens
#define tokColon       12 

// Punctuation (inner node) Token types
#define tokScope       13       // denoted by (.)
#define tokStmt        14
#define tokParens      15
#define tokTypeParens  16
#define tokData        17       // data initializer, like (: 1 2 3)
#define tokAssignment  18       // pl1 = 1 if mutable assignment, 0 if immutable 
#define tokReassign    19       // :=
#define tokMutation    20       // pl1 = like in topOperator. This is the "+=", operatorful type of mutations
#define tokArrow       21       // not a real scope, but placed here so the parser can dispatch on it
#define tokElse        22       // not a real scope, but placed here so the parser can dispatch on it
 
// Single-shot core syntax forms. pl1 = spanLevel
#define tokAlias       23
#define tokAssert      24
#define tokAssertDbg   25
#define tokAwait       26
#define tokBreak       27
#define tokCatch       28       // paren "(catch e. e .print)"
#define tokContinue    29
#define tokDefer       30
#define tokEach        31
#define tokEmbed       32       // Embed a text file as a string literal, or a binary resource file // 200
#define tokExport      33
#define tokExposePriv  34
#define tokFnDef       35
#define tokIface       36
#define tokLambda      37
#define tokMeta        38
#define tokPackage     39       // for single-file packages
#define tokReturn      40
#define tokStruct      41
#define tokTry         42       // early exit
#define tokYield       43

// Resumable core forms
#define tokIf          44    // "(if " or "(-i". pl1 = 1 if it's the "(-" variant
#define tokIfPr        45    // like if, but every branch is a value compared using custom predicate
#define tokMatch       46    // "(-m " or "(match " pattern matching on sum type tag 
#define tokImpl        47    // "(-impl " 
#define tokLoop        48    // "(-loop "
// "(-iface"
#define topVerbatimTokenVariant tokUnderscore

/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokScope
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokAlias

#define countCoreForms (tokLoop - tokAlias + 1)

/** The indices of reserved words that are stored in token pl2. Must be positive, unique,
 * and below "firstPunctuationTokenType"
 */
#define reservedFalse   2
#define reservedTrue    1
#define reservedAnd     3
#define reservedOr      4

/** Span levels */
#define slScope         1 // scopes (denoted by brackets): newlines and commas just have no effect in them
#define slParenMulti    2 // things like "(if)": they're multiline but they cannot contain any brackets
#define slStmt          3 // single-line statements: newlines and commas break 'em
#define slSubexpr       4 // parens and the like: newlines have no effect, dots error out

/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array in the lexer.c file.
 * The order is defined by ASCII.
 */

/** Count of lexical operators, i.e. things that are lexed as operator tokens.
 * must be equal to the count of following constants
 */ 
#define countLexOperators   40
#define countOperators      43 // count of things that are stored as operators, regardless of how they are lexed
#define opTNotEqual          0 // !=
#define opTBoolNegation      1 // !
#define opTSize              2 // #
#define opTToString          3 // $
#define opTRemainder         4 // %
#define opTBinaryAnd         5 // && bitwise and
#define opTTypeAnd           6 // & interface intersection (type-level)
#define opTIsNull            7 // '
#define opTTimesExt          8 // *.
#define opTTimes             9 // *
#define opTIncrement        10 // ++
#define opTPlusExt          11 // +.
#define opTPlus             12 // +
#define opTToFloat          13 // ,
#define opTDecrement        14 // --
#define opTMinusExt         15 // -.
#define opTMinus            16 // -
#define opTDivByExt         17 // /.
#define opTDivBy            18 // /
#define opTBitShiftLeftExt  19 // <<.
#define opTBitShiftLeft     20 // <<
#define opTLTEQ             21 // <=
#define opTComparator       22 // <>
#define opTLessThan         23 // <
#define opTEquality         24 // ==
#define opTIntervalBoth     25 // >=<= inclusive interval check
#define opTIntervalLeft     26 // >=<  left-inclusive interval check
#define opTIntervalRight    27 // ><=  right-inclusive interval check
#define opTIntervalExcl     28 // ><   exclusive interval check
#define opTGTEQ             29 // >=
#define opTBitShiftRightExt 30 // >>.  unsigned right bit shift
#define opTBitshiftRight    31 // >>   right bit shift
#define opTGreaterThan      32 // >
#define opTNullCoalesce     33 // ?:   null coalescing operator
#define opTQuestionMark     34 // ?    nullable type operator
#define opTAccessor         35 // @
#define opTExponentExt      36 // ^.   exponentiation extended
#define opTExponent         37 // ^    exponentiation
#define opTBoolOr           38 // ||   bitwise or
#define opTXor              39 // |    bitwise xor
#define opTAnd              40
#define opTOr               41
#define opTNegation         42

/** Reserved words of Tl in ASCII byte form */
#define countReservedLetters         25 // length of the interval of letters that may be init for reserved words (A to Y)
#define countReservedWords           30 // count of different reserved words below

static const byte reservedBytesAlias[]       = { 97, 108, 105, 97, 115 };
static const byte reservedBytesAnd[]         = { 97, 110, 100 };
static const byte reservedBytesAssert[]      = { 97, 115, 115, 101, 114, 116 };
static const byte reservedBytesAssertDbg[]   = { 97, 115, 115, 101, 114, 116, 68, 98, 103 };
static const byte reservedBytesAwait[]       = { 97, 119, 97, 105, 116 };
static const byte reservedBytesBreak[]       = { 98, 114, 101, 97, 107 };
static const byte reservedBytesCase[]        = { 99, 97, 115, 101 };   // probably won't need, just use "=>"
static const byte reservedBytesCatch[]       = { 99, 97, 116, 99, 104 };
static const byte reservedBytesContinue[]    = { 99, 111, 110, 116, 105, 110, 117, 101 };
static const byte reservedBytesDispose[]     = { 100, 105, 115, 112, 111, 115, 101 }; // change to "defer"
static const byte reservedBytesElse[]        = { 101, 108, 115, 101 };
static const byte reservedBytesEmbed[]       = { 101, 109, 98, 101, 100 };
static const byte reservedBytesExport[]      = { 101, 120, 112, 111, 114, 116 };
static const byte reservedBytesFalse[]       = { 102, 97, 108, 115, 101 };
static const byte reservedBytesFn[]          = { 102, 110 };  // maybe also "Fn" for function types
static const byte reservedBytesIf[]          = { 105, 102 };
static const byte reservedBytesIfEq[]        = { 105, 102, 69, 113 };
static const byte reservedBytesIfPr[]        = { 105, 102, 80, 114 };
static const byte reservedBytesImpl[]        = { 105, 109, 112, 108 };
static const byte reservedBytesInterface[]   = { 105, 110, 116, 101, 114, 102, 97, 99, 101 };
static const byte reservedBytesLambda[]      = { 108, 97, 109 };
static const byte reservedBytesLoop[]        = { 108, 111, 111, 112 };
static const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
static const byte reservedBytesMut[]         = { 109, 117, 116 }; // replace with "immut" or "val" for struct fields
static const byte reservedBytesOr[]          = { 111, 114 };
static const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
static const byte reservedBytesStruct[]      = { 115, 116, 114, 117, 99, 116 };
static const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
static const byte reservedBytesTry[]         = { 116, 114, 121 };
static const byte reservedBytesYield[]       = { 121, 105, 101, 108, 100 };
// setArena, getArena ?

#define aALower       97
#define aFLower      102
#define aILower      105
#define aLLower      108
#define aXLower      120
#define aYLower      121
#define aZLower      122
#define aAUpper       65
#define aFUpper       70
#define aZUpper       90
#define aDigit0       48
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
#define aPipe        124
#define aAmp          38
#define aTilde       126
#define aBackslash    92

#define aSpace        32
#define aNewline      10
#define aCarrReturn   13
#define aTab           9

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
