#ifndef LEXER_CONSTANTS_H
#define LEXER_CONSTANTS_H

#include "../../utils/aliases.h"


#define LEXER_INIT_SIZE 2000

const char errorLengthOverflow[]             = "Token length overflow";
const char errorNonAscii[]                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!";
const char errorPrematureEndOfInput[]        = "Premature end of input";
const char errorUnrecognizedByte[]           = "Unrecognized byte in source code!";
const char errorWordChunkStart[]             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!";
const char errorWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!";
const char errorWordUnderscoresOnlyAtStart[] = "Underscores are only allowed at start of word (snake case is forbidden)!";
const char errorWordReservedWithDot[]        = "Reserved words may not be called like functions!";
const char errorNumericEndUnderscore[]       = "Numeric literal cannot end with underscore!";
const char errorNumericWidthExceeded[]       = "Numeric literal width is exceeded!";
const char errorNumericBinWidthExceeded[]    = "Integer literals cannot exceed 64 bit!";
const char errorNumericFloatWidthExceeded[]  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!";
const char errorNumericEmpty[]               = "Could not lex a numeric literal, empty sequence!";
const char errorNumericMultipleDots[]        = "Multiple dots in numeric literals are not allowed!";
const char errorNumericIntWidthExceeded[]    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!";
const char errorPunctuationExtraOpening[]    = "Extra opening punctuation";
const char errorPunctuationUnmatched[]       = "Unmatched closing punctuation";
const char errorPunctuationExtraClosing[]    = "Extra closing punctuation";
const char errorPunctuationWrongOpen[]       = "Wrong opening punctuation";
const char errorPunctuationDoubleSplit[]     = "An expression or statement may contain only one '->' splitting symbol!";
const char errorOperatorUnknown[]            = "Unknown operator";
const char errorOperatorAssignmentPunct[]    = "Incorrect assignment operator placement: must be directly inside an ordinary statement, after the binding name!";
const char errorOperatorTypeDeclPunct[]      = "Incorrect type declaration operator placement: must be the first in a statement!";
const char errorOperatorMultipleAssignment[] = "Multiple assignment / type declaration operators within one statement are not allowed!";
const char errorDocComment[]                 = "Doc comments must have the syntax (;; comment .)";

/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
const int maxInt[] = {
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
};


#define aALower 97
#define aBLower 98
#define aCLower 99
#define aFLower 102
#define aNLower 110
#define aXLower 120
#define aWLower 119
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
#define aAmpersand 38
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

#define aLessThan 60
#define aGreaterThan 62

/** 2**53 */
const unsigned char maximumPreciselyRepresentedFloatingInt[] = { 9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2 };


/** All the symbols an operator may start with. The ':' is absent because it's handled by lexColon.
* The '-' is absent because it's handled by 'lexMinus'.
*/
const int operatorStartSymbols[] = {
    aExclamation, aSharp, aDollar, aPercent, aAmpersand, aTimes, aPlus, aDivBy, aSemicolon,
    aLessThan, aEqual, aGreaterThan, aQuestion, aBackslash, aCaret, aPipe
};



/** Reserved words of Tl in ASCII byte form */
#define countReservedLetters         19 // length of the interval of letters that may be init for reserved words
const byte reservedBytesBreak[]       = { 98, 114, 101, 97, 107 };
const byte reservedBytesCatch[]       = { 99, 97, 116, 99, 104 };
const byte reservedBytesContinue[]    = { 99, 111, 110, 116, 105, 110, 117, 101 };
const byte reservedBytesEmbed[]       = { 101, 109, 98, 101, 100 };
const byte reservedBytesExport[]      = { 101, 120, 112, 111, 114, 116 };
const byte reservedBytesFalse[]       = { 102, 97, 108, 115, 101 };
const byte reservedBytesFn[]          = { 102, 110 };
const byte reservedBytesFor[]         = { 102, 111, 114 };
const byte reservedBytesIf[]          = { 105, 102 };
const byte reservedBytesIfEq[]        = { 105, 102, 69, 113 };
const byte reservedBytesIfPr[]        = { 105, 102, 80, 114 };
const byte reservedBytesImpl[]        = { 105, 109, 112, 108 };
const byte reservedBytesInterface[]   = { 105, 110, 116, 101, 114, 102, 97, 99, 101 };
const byte reservedBytesOpen[]        = { 111, 112, 101, 110 };
const byte reservedBytesMatch[]       = { 109, 97, 116, 99, 104 };
const byte reservedBytesReturn[]      = { 114, 101, 116, 117, 114, 110 };
const byte reservedBytesStruct[]      = { 115, 116, 114, 117, 99, 116 };
const byte reservedBytesTest[]        = { 116, 101, 115, 116 };
const byte reservedBytesTrue[]        = { 116, 114, 117, 101 };
const byte reservedBytesTry[]         = { 116, 114, 121 };
const byte reservedBytesType[]        = { 116, 121, 112, 101 };


#define topVerbatimTokenVariant = 4

/**
 * Regular (leaf) Token types
 */
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt 0
#define tokFloat 1
#define tokBool 2
#define tokString 3
#define tokUnderscore 4

// This group requires analysis in the parser
#define tokDocComment 5
#define tokCompoundString 6
#define tokWord 7              // payload2: 1 if the word is all capitals
#define tokDotWord 8           // payload2: 1 if the word is all capitals
#define tokAtWord 9
#define tokReserved 10         // payload2: value of a constant from the 'reserved*' group
#define tokOperator 11         // payload2: OperatorToken encoded as an Int

/**
 * Punctuation (inner node) Token types
 */
#define tokCurlyBraces 12
#define tokBrackets 13
#define tokParens 14
#define tokAddresser 15
#define tokStmtAssignment 16 // payload1: (number of tokens before the assignment operator) shl 16 + (OperatorType)
#define tokStmtTypeDecl 17
#define tokLexScope 18
#define tokStmtFn 19
#define tokStmtReturn 20
#define tokStmtIf 21
#define tokStmtLoop 22
#define tokStmtBreak 23
#define tokStmtIfEq 24
#define tokStmtIfPred 25


/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokCurlyBraces
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokStmtFn

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
#define countOperators            35
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




/** Function precedence must be higher than that of any infix operator, yet lower than the prefix operators */
#define functionPrec 26
#define prefixPrec 27



#endif
