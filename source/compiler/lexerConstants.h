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
#define tokBool         2      // payload2 = value (1 or 0)
#define tokString       3      
#define tokUnderscore   4
#define tokDocComment   5

// This group requires analysis in the parser
#define tokWord         6      // payload1 = 1 if the last chunk is capitalized, payload2 = index in the string table
#define tokDotWord      7      // payload1 = 1 if the last chunk is capitalized, payload2 = index in the string table
#define tokAtWord       8      // "@annotation", payloads same as tokWord
#define tokFuncWord     9      // ".funcName", payloads same as tokWord
#define tokOperator    10      // payload1 = OperatorToken encoded as an Int
#define tokAnd         11
#define tokOr          12
#define tokDispose     13
#define tokElse        14    

// This is a temporary Token type for use during lexing only. In the final token stream it's replaced with tokParens
#define tokColon       15 

// Punctuation (inner node) Token types
#define tokScope       16       // denoted by []
#define tokStmt        17
#define tokParens      18
#define tokData        19       // data initializer, like (: 1 2 3)
#define tokAccessor    20       // data accessor, like array(1 2)
#define tokFuncExpr    21       // the ".(foo .bar)" kind of thing
#define tokAssignment  22       // payload1 = 1 if mutable assignment, 0 if immutable 
#define tokReassign    23       // :=
#define tokMutation    24       // payload1 = like in topOperator. This is the "+=", operatorful type of mutations
 
// Single-shot core syntax forms
#define tokAlias       25      
#define tokAssert      26      
#define tokAssertDbg   27     
#define tokAwait       28      
#define tokBreak       29      
#define tokCatch       30       // paren "(catch e. e .print)"
#define tokContinue    31       // noParen
#define tokDefer       32       // noParen
#define tokEmbed       33       // noParen. Embed a text file as a string literal, or a binary resource file // 200
#define tokExport      34       // paren
#define tokExposePriv  35       // paren
#define tokFnDef       36       // specialCase
#define tokIface       37       
#define tokLambda      38       
#define tokPackage     39       // for single-file packages
#define tokReturn      40
#define tokStruct      41       
#define tokTry         42       // early exit
#define tokYield       43       

// Resumable core forms
#define tokIf          44       // paren 
#define tokIfEq        45       // like if, but every branch is a value compared using standard equality
#define tokIfPr        46       // like if, but every branch is a value compared using custom predicate
#define tokMatch       47       // pattern matching on sum type tag 
#define tokImpl        48       // paren 
#define tokLoop        49       // 

#define topVerbatimTokenVariant tokUnderscore

/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokScope
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokAlias

#define countCoreForms 28 // tokLoop - tokAlias + 1

/** The indices of reserved words that are stored in token payload2. Must be positive, unique,
 * below "firstPunctuationTokenType" and not clashing with "tokAnd", "tokOr" and "tokNodispose"
 */
#define reservedFalse   2
#define reservedTrue    1

/** Span levels */
#define slBrackets      1 // scopes (denoted by brackets): newlines and commas just have no effect in them
#define slParenMulti    2 // things like "(if)": they're multiline but they cannot contain any brackets
#define slStmt          3 // single-line statements: newlines and commas break 'em
#define slSubexpr       4 // parens and the like: newlines have no effect, dots error out

/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array in the .c file.
 * The order is defined by ASCII.
 */
#define countOperators    33 // must be equal to the count of following constants
#define opTNotEqual        0 // !=
#define opTBoolNegation    1 // !
#define opTSize            2 // #
#define opTToString        3 // $
#define opTRemainder       4 // %
#define opTBinaryAnd       5 // && bitwise and
#define opTTypeAnd         6 // & interface intersection (type-level)
#define opTNotEmpty        7 // '
#define opTTimes           8 // *
#define opTIncrement       9 // ++
#define opTPlus           10 // +
#define opTDecrement      11 // --
#define opTMinus          12 // -
#define opTDivBy          13 // /
#define opTBitShiftLeft   14 // <<
#define opTLTEQ           15 // <=
#define opTComparator     16 // <>
#define opTLessThan       17 // <
#define opTEquality       18 // ==
#define opTIntervalBoth   19 // >=<= inclusive interval check
#define opTIntervalLeft   20 // >=<  left-inclusive interval check
#define opTIntervalRight  21 // ><=  right-inclusive interval check
#define opTIntervalExcl   22 // ><   exclusive interval check
#define opTGTEQ           23 // >=
#define opTBitshiftRight  24 // >>   right bit shift
#define opTGreaterThan    25 // >
#define opTNullCoalesce   26 // ?:   null coalescing operator
#define opTQuestionMark   27 // ?    nullable type operator
#define opTExponent       28 // ^    exponentiation
#define opTBoolOr         29 // ||   bitwise or
#define opTXor            30 // |    bitwise xor
#define opTToFloat        31 // ~

/** Reserved words of Tl in ASCII byte form */
#define countReservedLetters         25 // length of the interval of letters that may be init for reserved words (A to Y)
#define countReservedWords           31 // count of different reserved words below

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

/** Function precedence must be higher than that of any infix operator, yet lower than the prefix operators */
#define functionPrec  26
#define prefixPrec    27

#define aALower       97
#define aFLower      102
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
#define aBracketLeft  91
#define aBracketRight 93
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
