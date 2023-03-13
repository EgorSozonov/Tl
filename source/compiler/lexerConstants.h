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
extern const char errorPunctuationUnmatched[];
extern const char errorPunctuationInsideColon[];
extern const char errorPunctuationExtraClosing[];
extern const char errorPunctuationWrongOpen[];
extern const char errorPunctuationDoubleSplit[];
extern const char errorOperatorUnknown[];
extern const char errorOperatorAssignmentPunct[];
extern const char errorOperatorTypeDeclPunct[];
extern const char errorOperatorMultipleAssignment[];
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
#define countOperatorStartSymbols 16
extern const int operatorStartSymbols[16];


#define topVerbatimTokenVariant = 5

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
#define tokWord         6      // payload2: 1 if the word is all capitals
#define tokDotWord      7      // payload2: 1 if the word is all capitals
#define tokAtWord       8
#define tokReserved     9      // payload2: value of a constant from the 'reserved*' group
#define tokOperator    10      // payload1: OperatorToken encoded as an Int

// This is a temporary Token type for use during lexing only. In the final token stream it's replaced with tokParens
#define tokColon       11

// Punctuation (inner node) Token types
#define tokStmt        12
#define tokParens      13
#define tokBrackets    14
#define tokCompoundStr 15
#define tokAccessor    16
#define tokAssignment  17      // payload1: as in tokOperator
#define tokTypeDecl    18
#define tokLexScope    19

// Core syntax form Token types
#define tokStmtAlias   20
#define tokStmtAwait   21
#define tokStmtBreak   22
#define tokStmtCatch   23
#define tokContinue    24
#define tokStmtEmbed   25       // embed a text file as a string literal
#define tokStmtExport  26
#define tokStmtFor     27
#define tokGenerator   28       // generator (like a function but yields instead of returning)
#define tokStmtIf      29
#define tokStmtIfEq    30       // like if, but every branch is a value compared using standard equality
#define tokStmtIfPr    31       // like if, but every branch is a value compared using custom predicate
#define tokStmtImpl    32
#define tokStmtIface   33
#define tokLambda      34
#define tokStmtMatch   35       // pattern matching sum type
#define tokStmtMut     36
#define tokNodestruct  37       // signaling that the value doesn't need its destructor called at scope end
#define tokStmtReturn  38
#define tokStmtStruct  39
#define tokStmtTest    40
#define tokStmtTry     41
#define tokStmtType    42
#define tokYield       43


/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokStmt
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokStmtAlias

/** The indices of reserved words that are stored in token payload2. Must be positive, unique
 * and below "firstPunctuationTokenType"
 */
#define reservedFalse 2
#define reservedTrue  1

/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array in the .c file.
 * The order is defined by ASCII.
 */
#define countOperators    36 // must be equal to the count of following constants
#define opTNotEqual        0 // !=
#define opTBoolNegation    1 // !
#define opTSize            2 // $
#define opTRemainder       3 // %
#define opTBoolAnd         4 // &&
#define opTBinaryAnd       5 // &
#define opTToString        6 // '
#define opTTimes           7 // *
#define opTIncrement       8 // ++
#define opTPlus            9 // +
#define opTDecrement      10 // --
#define opTMinus          11 // -
#define opTDivBy          12 // /
#define opTMutation       13 // :=
#define opTRangeHalf      14 // ;<
#define opTRange          15 // ;
#define opTArrowLeft      16 // <-
#define opTLTEQ           17 // <=
#define opTBitShiftLeft   18 // <<
#define opTLessThan       19 // <
#define opTEquality       20 // ==
#define opTDefinition     21 // =
#define opTIntervalBoth   22 // >=<=
#define opTIntervalLeft   23 // >=<
#define opTIntervalRight  24 // ><=
#define opTIntervalExcl   25 // ><
#define opTGTEQ           26 // >=
#define opTBitshiftRight  27 // >>
#define opTGreaterThan    28 // >
#define opTNullCoalesc    29 // ?:
#define opTQuestionMark   30 // ?
#define opTIsEmpty        31 // backslash
#define opTExponent       32 // ^
#define opTBoolOr         33 // ||
#define opTPipe           34 // |
#define opTNotEmpty       35 // ~


/** Reserved words of Tl in ASCII byte form */
#define countReservedLetters         25 // length of the interval of letters that may be init for reserved words
#define countReservedWords           24 // count of different reserved words below
static const byte reservedBytesAlias[]       = { 97, 108, 105, 97, 115 };
static const byte reservedBytesAwait[]       = { 97, 119, 97, 105, 116 };
static const byte reservedBytesBreak[]       = { 98, 114, 101, 97, 107 };
static const byte reservedBytesCatch[]       = { 99, 97, 116, 99, 104 };
static const byte reservedBytesContinue[]    = { 99, 111, 110, 116, 105, 110, 117, 101 };
static const byte reservedBytesEmbed[]       = { 101, 109, 98, 101, 100 };
static const byte reservedBytesExport[]      = { 101, 120, 112, 111, 114, 116 };
static const byte reservedBytesFalse[]       = { 102, 97, 108, 115, 101 };
static const byte reservedBytesFor[]         = { 102, 111, 114 };
static const byte reservedBytesIf[]          = { 105, 102 };
static const byte reservedBytesIfEq[]        = { 105, 102, 69, 113 };
static const byte reservedBytesIfPr[]        = { 105, 102, 80, 114 };
static const byte reservedBytesImpl[]        = { 105, 109, 112, 108 };
static const byte reservedBytesInterface[]   = { 105, 110, 116, 101, 114, 102, 97, 99, 101 };
static const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
static const byte reservedBytesMut[]         = { 109, 117, 116 };
static const byte reservedBytesNodestruct[]  = { 110, 111, 100, 101, 115, 116, 114, 117, 99, 116 };
static const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
static const byte reservedBytesStruct[]      = { 115, 116, 114, 117, 99, 116 };
static const byte reservedBytesTest[]        = { 116, 101, 115, 116 };
static const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
static const byte reservedBytesTry[]         = { 116, 114, 121 };
static const byte reservedBytesType[]        = { 116, 121, 112, 101 };
static const byte reservedBytesYield[]       = { 121, 105, 101, 108, 100 };


/** Function precedence must be higher than that of any infix operator, yet lower than the prefix operators */
#define functionPrec 26
#define prefixPrec 27


#define aALower 97
#define aBLower 98
#define aCLower 99
#define aFLower 102
#define aNLower 110
#define aXLower 120
#define aYLower 121
#define aZLower 122
#define aAUpper 65
#define aFUpper 70
#define aZUpper 90
#define aDigit0 48
#define aDigit1 49
#define aDigit9 57

#define aPlus 43
#define aMinus 45
#define aTimes 42
#define aDivBy 47
#define aDot 46
#define aPercent 37

#define aParenLeft 40
#define aParenRight 41
#define aCurlyLeft 123
#define aCurlyRight 125
#define aBracketLeft 91
#define aBracketRight 93
#define aPipe 124
#define aAmp 38
#define aTilde 126
#define aBackslash 92

#define aSpace 32
#define aNewline 10
#define aCarriageReturn 13

#define aApostrophe 39
#define aQuote 34
#define aSharp 35
#define aDollar 36
#define aUnderscore 95
#define aCaret 94
#define aAt 64
#define aColon 58
#define aSemicolon 59
#define aExclamation 33
#define aQuestion 63
#define aEqual 61

#define aLT 60
#define aGT 62

#endif
