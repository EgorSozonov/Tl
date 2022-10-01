package compiler.lexer

const val errorLengthOverflow             = "Token length overflow"
const val errorNonASCII                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!"
const val errorPrematureEndOfInput        = "Premature end of input"
const val errorUnrecognizedByte           = "Unrecognized byte in source code!"
const val errorWordChunkStart             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!"
const val errorWordCapitalizationOrder    = "An identifier may not contain a capitalized piece after an uncapitalized one!"
const val errorWordUnderscoresOnlyAtStart = "Underscores are only allowed at start of word (snake case is forbidden)!"
const val errorNumericEndUnderscore       = "Numeric literal cannot end with underscore!"
const val errorNumericBinWidthExceeded    = "Integer literals cannot exceed 64 bit!"
const val errorNumericFloatWidthExceeded  = "Floating-point literals cannot exceed 2**53 in the significant bits, and 22 in the decimal power!"
const val errorNumericEmpty               = "Could not lex a numeric literal, empty sequence!"
const val errorNumericMultipleDots        = "Multiple dots in numeric literals are not allowed!"
const val errorNumericIntWidthExceeded    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!"
const val errorPunctuationExtraOpening    = "Extra opening punctuation"
const val errorPunctuationUnmatched       = "Unmatched closing punctuation"
const val errorPunctuationExtraClosing    = "Extra closing punctuation"
const val errorPunctuationWrongOpen       = "Wrong opening punctuation"


/**
 * The ASCII notation for the lowest 64-bit integer, -9_223_372_036_854_775_808
 */
val minInt = byteArrayOf(
    57, 50, 50, 51, 51, 55, 50, 48, 51, 54,
    56, 53, 52, 55, 55, 53, 56, 48, 56
)

/**
 * The ASCII notation for the highest 64-bit integer, 9_223_372_036_854_775_807
 */
val maxInt = byteArrayOf(
    57, 50, 50, 51, 51, 55, 50, 48, 51, 54,
    56, 53, 52, 55, 55, 53, 56, 48, 55
)

const val asciiALower: Byte = 97
const val asciiBLower: Byte = 98
const val asciiFLower: Byte = 102
const val asciiXLower: Byte = 120
const val asciiZLower: Byte = 122
const val asciiAUpper: Byte = 65
const val asciiFUpper: Byte = 70
const val asciiZUpper: Byte = 90
const val asciiDigit0: Byte = 48
const val asciiDigit1: Byte = 49
const val asciiDigit9: Byte = 57

const val asciiPlus: Byte = 43
const val asciiMinus: Byte = 45
const val asciiTimes: Byte = 42
const val asciiDivBy: Byte = 47
const val asciiDot: Byte = 46
const val asciiPercent: Byte = 37

const val asciiParenLeft: Byte = 40
const val asciiParenRight: Byte = 41
const val asciiCurlyLeft: Byte = 123
const val asciiCurlyRight: Byte = 125
const val asciiBracketLeft: Byte = 91
const val asciiBracketRight: Byte = 93
const val asciiPipe: Byte = 124
const val asciiAmpersand: Byte = 38
const val asciiTilde: Byte = 126
const val asciiBackslash: Byte = 92

const val asciiSpace: Byte = 32
const val asciiNewline: Byte = 10
const val asciiCarriageReturn: Byte = 13

const val asciiApostrophe: Byte = 39
const val asciiQuote: Byte = 34
const val asciiSharp: Byte = 35
const val asciiDollar: Byte = 36
const val asciiUnderscore: Byte = 95
const val asciiCaret: Byte = 94
const val asciiAt: Byte = 64
const val asciiColon: Byte = 58
const val asciiSemicolon: Byte = 59
const val asciiExclamation: Byte = 33
const val asciiQuestion: Byte = 63
const val asciiEquals: Byte = 61

const val asciiLessThan: Byte = 60
const val asciiGreaterThan: Byte = 62


val operatorSymbols = byteArrayOf(
    asciiColon, asciiAmpersand, asciiPlus, asciiMinus, asciiDivBy, asciiTimes, asciiExclamation, asciiTilde,
    asciiPercent, asciiCaret, asciiPipe, asciiGreaterThan, asciiLessThan, asciiQuestion, asciiEquals,
)


const val CHUNKSZ: Int = 10000 // Must be divisible by 4
const val COMMENTSZ: Int = 100 // Must be divisible by 2
const val LOWER32BITS: Long =  0x00000000FFFFFFFF
const val LOWER27BITS: Int = 0x07FFFFFF
const val MAXTOKENLEN = 134217728 // 2**27