package compiler.lexer

const val errorLengthOverflow             = "Token length overflow"
const val errorNona                       = "Non-a symbols are not allowed in code - only inside comments & string literals!"
const val errorPrematureEndOfInput        = "Premature end of input"
const val errorUnrecognizedByte           = "Unrecognized byte in source code!"
const val errorWordChunkStart             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!"
const val errorWordCapitalizationOrder    = "An identifier may not contain a capitalized piece after an uncapitalized one!"
const val errorWordUnderscoresOnlyAtStart = "Underscores are only allowed at start of word (snake case is forbidden)!"
const val errorNumericEndUnderscore       = "Numeric literal cannot end with underscore!"
const val errorNumericWidthExceeded       = "Numeric literal width is exceeded!"
const val errorNumericBinWidthExceeded    = "Integer literals cannot exceed 64 bit!"
const val errorNumericFloatWidthExceeded  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!"
const val errorNumericEmpty               = "Could not lex a numeric literal, empty sequence!"
const val errorNumericMultipleDots        = "Multiple dots in numeric literals are not allowed!"
const val errorNumericIntWidthExceeded    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!"
const val errorPunctuationExtraOpening    = "Extra opening punctuation"
const val errorPunctuationUnmatched       = "Unmatched closing punctuation"
const val errorPunctuationExtraClosing    = "Extra closing punctuation"
const val errorPunctuationWrongOpen       = "Wrong opening punctuation"
const val errorOperatorUnknown            = "Unknown operator"
const val errorOperatorAssignmentPunct    = "Incorrect assignment / type declaration operator placement: must be directly inside statement or inside parens that envelop the whole statement!"
const val errorOperatorMultipleAssignment = "Multiple assignment / type declaration operators within one statement are not allowed!"


/**
 * The ASCII notation for the highest signed 64-bit integer absolute value, 9_223_372_036_854_775_807
 */
val maxInt = byteArrayOf(
    9, 2, 2, 3, 3, 7, 2, 0, 3, 6,
    8, 5, 4, 7, 7, 5, 8, 0, 7
)

const val aALower: Byte = 97
const val aBLower: Byte = 98
const val aCLower: Byte = 99
const val aFLower: Byte = 102
const val aNLower: Byte = 110
const val aXLower: Byte = 120
const val aWLower: Byte = 119
const val aZLower: Byte = 122
const val aAUpper: Byte = 65
const val aFUpper: Byte = 70
const val aZUpper: Byte = 90
const val aDigit0: Byte = 48
const val aDigit1: Byte = 49
const val aDigit9: Byte = 57

const val aPlus: Byte = 43
const val aMinus: Byte = 45
const val aTimes: Byte = 42
const val aDivBy: Byte = 47
const val aDot: Byte = 46
const val aPercent: Byte = 37

const val aParenLeft: Byte = 40
const val aParenRight: Byte = 41
const val aCurlyLeft: Byte = 123
const val aCurlyRight: Byte = 125
const val aBracketLeft: Byte = 91
const val aBracketRight: Byte = 93
const val aPipe: Byte = 124
const val aAmpersand: Byte = 38
const val aTilde: Byte = 126
const val aBackslash: Byte = 92

const val aSpace: Byte = 32
const val aNewline: Byte = 10
const val aCarriageReturn: Byte = 13

const val aApostrophe: Byte = 39
const val aQuote: Byte = 34
const val aSharp: Byte = 35
const val aDollar: Byte = 36
const val aUnderscore: Byte = 95
const val aCaret: Byte = 94
const val aAt: Byte = 64
const val aColon: Byte = 58
const val aSemicolon: Byte = 59
const val aExclamation: Byte = 33
const val aQuestion: Byte = 63
const val aEquals: Byte = 61

const val aLessThan: Byte = 60
const val aGreaterThan: Byte = 62


val maximumPreciselyRepresentedFloatingInt = byteArrayOf(9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2) // 2**53

val operatorStartSymbols = byteArrayOf(
    aColon, aAmpersand, aPlus, aMinus, aDivBy, aTimes, aExclamation,
    aPercent, aCaret, aPipe, aGreaterThan, aLessThan, aQuestion, aEquals,
)

/**
 * This is an array of 3-byte arrays containing operator byte sequences.
 * Sorted as follows: 1. by first byte ASC 2. by length DESC.
 * It's used to lex operator symbols using left-to-right search.
 */
val operatorSymbols = byteArrayOf(
    aExclamation, aEquals, 0,   aExclamation, 0, 0,            aPercent, 0, 0,                aAmpersand, aAmpersand, 0,
    aAmpersand, 0, 0,           aTimes, aTimes, 0,             aTimes, 0, 0,                  aPlus, aPlus, 0,
    aPlus, 0, 0,                aMinus, aMinus, 0,             aMinus, aGreaterThan, 0,       aMinus, 0, 0,
    aDot, aDot, aLessThan,      aDot, aDot, 0,                 aDivBy, 0, 0,                  aColon, aGreaterThan, 0,
    aColon, aEquals, 0,         aColon, aColon, 0,             aColon, 0, 0,                  aLessThan, aEquals, aDot,
    aLessThan, aDot, 0,         aLessThan, aLessThan, 0,       aLessThan, aEquals, 0,         aLessThan, aMinus, 0,
    aLessThan, 0, 0,            aEquals, aEquals, 0,           aEquals, aGreaterThan, 0,      aEquals, 0, 0,
    aGreaterThan, aEquals, aDot,aGreaterThan, aDot, 0,         aGreaterThan, aEquals, 0,      aGreaterThan, aGreaterThan, 0,
    aGreaterThan, 0, 0,         aBackslash, 0, 0,              aCaret, 0, 0,                  aPipe, aPipe, 0,
    aPipe, 0, 0,
)

/**
 * Marks the 10 "equalable & extensible" operators, i.e. the ones with 5 extra variations.
 * Order must exactly agree with 'operatorSymbols'
 */
val operatorExtensibility = byteArrayOf(
    0, 0, 1, 1,
    0, 1, 1, 0,
    1, 0, 0, 1,
    0, 0, 1, 0,
    0, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 1,
    0, 0, 1, 1,
    0,
)

const val CHUNKSZ: Int = 10000 // Must be divisible by 4
const val COMMENTSZ: Int = 100 // Must be divisible by 2
const val LOWER32BITS: Long =  0x00000000FFFFFFFF
const val LOWER27BITS: Int = 0x07FFFFFF
const val MAXTOKENLEN = 134217728 // 2**27
const val byte0: Byte = 0