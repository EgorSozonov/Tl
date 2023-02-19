#include "lexerConstants.h"

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

/** Reserved words of Tl in ASCII byte form */
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

/** All the symbols an operator may start with. The ':' is absent because it's handled by lexColon.
* The '-' is absent because it's handled by 'lexMinus'.
*/
const int operatorStartSymbols[] = {
    aExclamation, aSharp, aDollar, aPercent, aAmpersand, aTimes, aPlus, aDivBy, aSemicolon,
    aLessThan, aEqual, aGreaterThan, aQuestion, aBackslash, aCaret, aPipe
};


/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
const int maxInt[] = {
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
};


/** 2**53 */
const unsigned char maximumPreciselyRepresentedFloatingInt[] = { 9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2 };
