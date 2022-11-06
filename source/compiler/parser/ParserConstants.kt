package compiler.parser

const val errorLengthOverflow              = "AST nodes length overflow"
const val errorUnexpectedToken             = "Unexpected token"
const val errorCoreFormTooShort            = "Statement too short: core syntax forms cannot be shorter than 3 tokens!"
const val errorCoreFormAssignment          = "A core form may not contain assignment!"
const val errorFnNameAndParams             = "Function definition must start with more than one unique words: its name and parameters!"
const val errorFnMissingBody               = "Function definition must contain a body which must be a Scope!"
const val errorExpressionError             = "Cannot parse expression!"
const val errorOperatorWrongArity          = "Wrong number of arguments for operator!"
const val errorUnknownBinding              = "Unknown binding!"
const val errorOperatorUsedInappropriately = "Operator used in an inappropriate location!"


val reservedCatch = byteArrayOf(99, 97, 116, 99, 104)
val reservedFor = byteArrayOf(102, 111, 114)
val reservedForRaw = byteArrayOf(102, 111, 114, 82, 97, 119)
val reservedIf = byteArrayOf(105, 102)
val reservedIfEq = byteArrayOf(105, 102, 69, 113)
val reservedIfPred = byteArrayOf(105, 102, 80, 114, 101, 100)
val reservedMatch = byteArrayOf(109, 97, 116, 99, 104)
val reservedReturn = byteArrayOf(114, 101, 116, 117, 114, 110)
val reservedTry = byteArrayOf(116, 114, 121)
val reservedWhile = byteArrayOf(119, 104, 105, 108, 101)

/** Function precedence must be higher than that of any operator */
val funcPrecedence = 26
val prefixPrecedence = 27