#ifndef LEXER_CONSTANTS_H
#define LEXER_CONSTANTS_H


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

//~ /**
 //~ * The ASCII notation for the lowest 64-bit integer, -9_223_372_036_854_775_808
 //~ */
//~ static char minInt[] = {
        //~ 57,
        //~ 50, 50, 51,
        //~ 51, 55, 50,
        //~ 48, 51, 54,
        //~ 56, 53, 52,
        //~ 55, 55, 53,
        //~ 56, 48, 56,
    //~ };

//~ /**
 //~ * The ASCII notation for the highest 64-bit integer, 9_223_372_036_854_775_807
 //~ */
//~ static char maxInt[] = {
        //~ 57,
        //~ 50, 50, 51,
        //~ 51, 55, 50,
        //~ 48, 51, 54,
        //~ 56, 53, 52,
        //~ 55, 55, 53,
        //~ 56, 48, 55,
    //~ };

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

/** Must be divisible by 4 */
#define CHUNKSZ = 10000

/** Must be divisible by 2 and less than CHUNKSZ */
#define COMMENTSZ = 100

#define topVerbatimTokenVariant = 4


#endif
