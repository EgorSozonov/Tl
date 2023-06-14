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
#define tokFloat        1
#define tokBool         2      // payload2 = value (1 or 0)
#define tokString       3      
#define tokUnderscore   4
#define tokDocComment   5

// This group requires analysis in the parser
#define tokWord         6      // payload1 = 1 if the last chunk is capitalized, payload2 = index in the string table
#define tokDotWord      7      // ".fieldName", payload's the same as tokWord
#define tokOperator     8      // payload1 = OperatorToken, one of the "opT" constants below
#define tokDispose      9

// This is a temporary Token type for use during lexing only. In the final token stream it's replaced with tokParens
#define tokColon       10 

// Punctuation (inner node) Token types
#define tokScope       11       // denoted by (.)
#define tokStmt        12
#define tokParens      13
#define tokData        14       // data initializer, like (: 1 2 3)
#define tokAssignment  15       // payload1 = 1 if mutable assignment, 0 if immutable 
#define tokReassign    16       // :=
#define tokMutation    17       // payload1 = like in topOperator. This is the "+=", operatorful type of mutations
#define tokArrow       18       // not a real scope, but placed here so the parser can dispatch on it
#define tokElse        19       // not a real scope, but placed here so the parser can dispatch on it
 
// Single-shot core syntax forms. payload1 = spanLevel
#define tokAlias       20      
#define tokAssert      21      
#define tokAssertDbg   22     
#define tokAwait       23      
#define tokBreak       24      
#define tokCatch       25       // paren "(catch e. e .print)"
#define tokContinue    26       // noParen
#define tokDefer       27
#define tokEach        28
#define tokEmbed       29       // noParen. Embed a text file as a string literal, or a binary resource file // 200
#define tokExport      30       // paren
#define tokExposePriv  31       // paren
#define tokFnDef       32       // specialCase
#define tokIface       33       
#define tokLambda      34
#define tokMeta        35
#define tokPackage     36       // for single-file packages
#define tokReturn      37
#define tokStruct      38       
#define tokTry         39       // early exit
#define tokYield       40       

// Resumable core forms
#define tokIf          41    // "(if " or "(-i". payload1 = 1 if it's the "(-" variant
#define tokIfPr        42    // like if, but every branch is a value compared using custom predicate
#define tokMatch       43    // "(-m " or "(match " pattern matching on sum type tag 
#define tokImpl        44    // "(-impl " 
#define tokLoop        45    // "(-loop "
// "(-iface"
#define topVerbatimTokenVariant tokUnderscore

/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokScope
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokAlias

#define countCoreForms (tokLoop - tokAlias + 1)

/** The indices of reserved words that are stored in token payload2. Must be positive, unique,
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
#define countLexOperators   41
#define countOperators      44 // count of things that are stored as operators, regardless of how they are lexed
#define opTNotEqual          0 // !=
#define opTBoolNegation      1 // !
#define opTSize              2 // #
#define opTToString          3 // $
#define opTRemainderExt      4 // %.
#define opTRemainder         5 // %
#define opTBinaryAnd         6 // && bitwise and
#define opTTypeAnd           7 // & interface intersection (type-level)
#define opTIsNull            8 // '
#define opTTimesExt          9 // *.
#define opTTimes            10 // *
#define opTIncrement        11 // ++
#define opTPlusExt          12 // +.
#define opTPlus             13 // +
#define opTToFloat          14 // ,
#define opTDecrement        15 // --
#define opTMinusExt         16 // -.
#define opTMinus            17 // -
#define opTDivByExt         18 // /.
#define opTDivBy            19 // /
#define opTBitShiftLeftExt  20 // <<.
#define opTBitShiftLeft     21 // <<
#define opTLTEQ             22 // <=
#define opTComparator       23 // <>
#define opTLessThan         24 // <
#define opTEquality         25 // ==
#define opTIntervalBoth     26 // >=<= inclusive interval check
#define opTIntervalLeft     27 // >=<  left-inclusive interval check
#define opTIntervalRight    28 // ><=  right-inclusive interval check
#define opTIntervalExcl     29 // ><   exclusive interval check
#define opTGTEQ             30 // >=
#define opTBitShiftRightExt 31 // >>.  unsigned right bit shift
#define opTBitshiftRight    32 // >>   right bit shift
#define opTGreaterThan      33 // >
#define opTNullCoalesce     34 // ?:   null coalescing operator
#define opTQuestionMark     35 // ?    nullable type operator
#define opTAccessor         36 // @
#define opTExponentExt      37 // ^.   exponentiation extended
#define opTExponent         38 // ^    exponentiation
#define opTBoolOr           39 // ||   bitwise or
#define opTXor              40 // |    bitwise xor
#define opTAnd              41
#define opTOr               42
#define opTNegation         43

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
