#include <stdint.h>
#include "lexerConstants.h"

const char errorNonAscii[]                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!";
const char errorPrematureEndOfInput[]        = "Premature end of input";
const char errorUnrecognizedByte[]           = "Unrecognized byte in source code!";
const char errorWordChunkStart[]             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!";
const char errorWordCapitalizationOrder[]    = "An identifier may not contain a capitalized piece after an uncapitalized one!";
const char errorWordUnderscoresOnlyAtStart[] = "Underscores are only allowed at start of word (snake case is forbidden)!";
const char errorWordWrongAccessor[]          = "Only regular identifier words may be used for data access with []!";
const char errorWordReservedWithDot[]        = "Reserved words may not be called like functions!";
const char errorNumericEndUnderscore[]       = "Numeric literal cannot end with underscore!";
const char errorNumericWidthExceeded[]       = "Numeric literal width is exceeded!";
const char errorNumericBinWidthExceeded[]    = "Integer literals cannot exceed 64 bit!";
const char errorNumericFloatWidthExceeded[]  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!";
const char errorNumericEmpty[]               = "Could not lex a numeric literal, empty sequence!";
const char errorNumericMultipleDots[]        = "Multiple dots in numeric literals are not allowed!";
const char errorNumericIntWidthExceeded[]    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!";
const char errorPunctuationExtraOpening[]    = "Extra opening punctuation";
const char errorPunctuationExtraClosing[]    = "Extra closing punctuation";
const char errorPunctuationOnlyInMultiline[] = "The dot separator is only allowed in multi-line syntax forms like []";
const char errorPunctuationUnmatched[]       = "Unmatched closing punctuation";
const char errorPunctuationWrongOpen[]       = "Wrong opening punctuation";
const char errorPunctuationScope[]           = "Scopes may only be opened in multi-line syntax forms";
const char errorOperatorUnknown[]            = "Unknown operator";
const char errorOperatorAssignmentPunct[]    = "Incorrect assignment operator: must be directly inside an ordinary statement, after the binding name!";
const char errorOperatorTypeDeclPunct[]      = "Incorrect type declaration operator placement: must be the first in a statement!";
const char errorOperatorMultipleAssignment[] = "Multiple assignment / type declaration operators within one statement are not allowed!";
const char errorOperatorMutableDef[]         = "Definition of a mutable var should look like this: `mut x = 10`";
const char errorCoreNotInsideStmt[]          = "Core form must be directly inside statement";
const char errorCoreMisplacedArrow[]         = "The arrow separator (=>) must be inside an if, ifEq, ifPr or match form";
const char errorCoreMisplacedElse[]          = "The else statement must be inside an if, ifEq, ifPr or match form";
const char errorCoreMissingParen[]           = "Core form requires opening parenthesis/curly brace before keyword!"; 
const char errorCoreNotAtSpanStart[]         = "Reserved word must be at the start of a parenthesized span";
const char errorIndentation[]                = "Indentation error: must be divisible by 4 (tabs also count as 4) and not greater than the current indentation level!";
const char errorDocComment[]                 = "Doc comments must have the syntax: (*comment)";


/** All the symbols an operator may start with. "-" is absent because it's handled by lexMinus.
 */
const int operatorStartSymbols[] = {
    aExclamation, aSharp, aDollar, aPercent, aAmp, aTimes, aApostrophe, aPlus, aDivBy, 
    aSemicolon, aLT, aGT, aQuestion, aCaret, aPipe
};


/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
const byte maxInt[19] = (byte []){
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
};


/** 2**53 */
const byte maximumPreciselyRepresentedFloatingInt[16] = (byte []){ 9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2 };
