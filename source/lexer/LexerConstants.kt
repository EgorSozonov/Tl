package lexer


const val errorLengthOverflow             = "Token length overflow"
const val errorNona                       = "Non-a symbols are not allowed in code - only inside comments & string literals!"
const val errorPrematureEndOfInput        = "Premature end of input"
const val errorUnrecognizedByte           = "Unrecognized byte in source code!"
const val errorWordChunkStart             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!"
const val errorWordCapitalizationOrder    = "An identifier may not contain a capitalized piece after an uncapitalized one!"
const val errorWordUnderscoresOnlyAtStart = "Underscores are only allowed at start of word (snake case is forbidden)!"
const val errorWordReservedWithDot        = "Reserved words may not be called like functions!"
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
const val errorPunctuationDoubleSplit     = "An expression or statement may contain only one '->' splitting symbol!"
const val errorOperatorUnknown            = "Unknown operator"
const val errorOperatorAssignmentPunct    = "Incorrect assignment operator placement: must be directly inside an ordinary statement, after the binding name!"
const val errorOperatorTypeDeclPunct      = "Incorrect type declaration operator placement: must be the first in a statement!"
const val errorOperatorMultipleAssignment = "Multiple assignment / type declaration operators within one statement are not allowed!"
const val errorDocComment                 = "Doc comments must have the syntax (;; comment .)"

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
const val aEqual: Byte = 61

const val aLessThan: Byte = 60
const val aGreaterThan: Byte = 62

// Token types

// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The top value must be stored in "topVerbatimTokenVariant" constant
const val tokInt = 0
const val tokFloat = 1
const val tokBool = 2
const val tokString = 3
const val tokUnderscore = 4
const val tokDocComment = 5

// This group requires analysis in the parser
const val tokWord = 6
const val tokAtWord = 7
const val tokReserved = 8
const val tokOperator = 11


// Punctuation (inner node) Token types
const val tokCompoundString = 12
const val tokCurlyBraces = 13
const val tokBrackets = 14
const val tokParens = 15
const val tokAddresser = 16 // the [] in x[5]
const val tokStmtAssignment = 17
const val tokStmtTypeDecl = 18
const val tokLexScope = 19
const val tokStmtFn = 20
const val tokStmtFor = 21
const val tokStmtReturn = 22
const val tokStmtIf = 23
const val tokStmtIfEq = 23
const val tokStmtIfPr = 23
const val tokOpen = 24 // maybe remove this
const val tokBreak = 25
const val tokExport = 26


/** 2**53 */
val maximumPreciselyRepresentedFloatingInt = byteArrayOf(9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2)

/** Must be divisible by 4 */
const val CHUNKSZ: Int = 10000

/** Must be divisible by 2 and less than CHUNKSZ */
const val COMMENTSZ: Int = 100




val wordType = RegularToken.word.internalVal.toInt()

val topVerbatimTokenVariant = 4
