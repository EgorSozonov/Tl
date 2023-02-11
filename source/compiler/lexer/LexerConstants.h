#ifndef LEXER_CONSTANTS_H
#define LEXER_CONSTANTS_H


const char errorLengthOverflow[]             = "Token length overflow"
const char errorNona[]                       = "Non-a symbols are not allowed in code - only inside comments & string literals!"
const char errorPrematureEndOfInput[]        = "Premature end of input"
const char errorUnrecognizedByte[]           = "Unrecognized byte in source code!"
const char errorWordChunkStart[]             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!"
const char errorWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!"
const char errorWordUnderscoresOnlyAtStart[] = "Underscores are only allowed at start of word (snake case is forbidden)!"
const char errorWordReservedWithDot[]        = "Reserved words may not be called like functions!"
const char errorNumericEndUnderscore[]       = "Numeric literal cannot end with underscore!"
const char errorNumericWidthExceeded[]       = "Numeric literal width is exceeded!"
const char errorNumericBinWidthExceeded[]    = "Integer literals cannot exceed 64 bit!"
const char errorNumericFloatWidthExceeded[]  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!"
const char errorNumericEmpty[]               = "Could not lex a numeric literal, empty sequence!"
const char errorNumericMultipleDots[]        = "Multiple dots in numeric literals are not allowed!"
const char errorNumericIntWidthExceeded[]    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!"
const char errorPunctuationExtraOpening[]    = "Extra opening punctuation"
const char errorPunctuationUnmatched[]       = "Unmatched closing punctuation"
const char errorPunctuationExtraClosing[]    = "Extra closing punctuation"
const char errorPunctuationWrongOpen[]       = "Wrong opening punctuation"
const char errorPunctuationDoubleSplit[]     = "An expression or statement may contain only one '->' splitting symbol!"
const char errorOperatorUnknown[]            = "Unknown operator"
const char errorOperatorAssignmentPunct[]    = "Incorrect assignment operator placement: must be directly inside an ordinary statement, after the binding name!"
const char errorOperatorTypeDeclPunct[]      = "Incorrect type declaration operator placement: must be the first in a statement!"
const char errorOperatorMultipleAssignment[] = "Multiple assignment / type declaration operators within one statement are not allowed!"
const char errorDocComment[]                 = "Doc comments must have the syntax (;; comment .)"

/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
const int maxInt[] = byteArrayOf(
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
)

#define aALower: Byte = 97
#define aBLower: Byte = 98
#define aCLower: Byte = 99
#define aFLower: Byte = 102
#define aNLower: Byte = 110
#define aXLower: Byte = 120
#define aWLower: Byte = 119
#define aZLower: Byte = 122
#define aAUpper: Byte = 65
#define aFUpper: Byte = 70
#define aZUpper: Byte = 90
#define aDigit0: Byte = 48
#define aDigit1: Byte = 49
#define aDigit9: Byte = 57

#define aPlus: Byte = 43
#define aMinus: Byte = 45
#define aTimes: Byte = 42
#define aDivBy: Byte = 47
#define aDot: Byte = 46
#define aPercent: Byte = 37

#define aParenLeft: Byte = 40
#define aParenRight: Byte = 41
#define aCurlyLeft: Byte = 123
#define aCurlyRight: Byte = 125
#define aBracketLeft: Byte = 91
#define aBracketRight: Byte = 93
#define aPipe: Byte = 124
#define aAmpersand: Byte = 38
#define aTilde: Byte = 126
#define aBackslash: Byte = 92

#define aSpace: Byte = 32
#define aNewline: Byte = 10
#define aCarriageReturn: Byte = 13

#define aApostrophe: Byte = 39
#define aQuote: Byte = 34
#define aSharp: Byte = 35
#define aDollar: Byte = 36
#define aUnderscore: Byte = 95
#define aCaret: Byte = 94
#define aAt: Byte = 64
#define aColon: Byte = 58
#define aSemicolon: Byte = 59
#define aExclamation: Byte = 33
#define aQuestion: Byte = 63
#define aEqual: Byte = 61

#define aLessThan: Byte = 60
#define aGreaterThan: Byte = 62

/** 2**53 */
const unsigned char maximumPreciselyRepresentedFloatingInt[] = { 9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2 }

/** Must be divisible by 4 */
#define CHUNKSZ: Int = 10000

/** Must be divisible by 2 and less than CHUNKSZ */
#define COMMENTSZ: Int = 100

#define topVerbatimTokenVariant = 4


#endif
