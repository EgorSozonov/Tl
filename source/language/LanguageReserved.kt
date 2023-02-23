package language

/** The indices of reserved words that are stored in token payload2. Must be positive, unique
 * and below "firstPunctuationTokenType"
 */

//const val reservedBreak       =  1
//const val reservedCatch       =  5
//const val reservedExport      =  6
const val reservedFalse         =  2
//const val reservedFn          =  8
//const val reservedIf          =  2
//const val reservedIfEq        =  9
//const val reservedIfPred      = 10
//const val reservedImport      = 11
//const val reservedLoop        =  4
//const val reservedMatch       = 12
//const val reservedReturn      =  3
//const val reservedServicetest = 13
const val reservedTrue          = 1
//const val reservedTry         = 15
//const val reservedType        = 16
//const val reservedUnittest    = 17
//const val reservedWhile       = 18


/** Reserved words of Tl in ASCII byte form */
val reservedBytesBreak       = byteArrayOf(98, 114, 101, 97, 107)
val reservedBytesCatch       = byteArrayOf(99, 97, 116, 99, 104)
val reservedBytesExport      = byteArrayOf(101, 120, 112, 111, 114, 116)
val reservedBytesFalse       = byteArrayOf(102, 97, 108, 115, 101)
val reservedBytesFn          = byteArrayOf(102, 110)
val reservedBytesFor         = byteArrayOf(102, 111, 114)
val reservedBytesImport      = byteArrayOf(105, 109, 112, 111, 114, 116)
val reservedBytesLoop        = byteArrayOf(108, 111, 111, 112)
val reservedBytesMatch       = byteArrayOf(109, 97, 116, 99, 104)
val reservedBytesReturn      = byteArrayOf(114, 101, 116, 117, 114, 110)
val reservedBytesServicetest = byteArrayOf(115, 101, 114, 118, 105, 99, 101, 116, 101, 115, 116)
val reservedBytesTrue        = byteArrayOf(116, 114, 117, 101)
val reservedBytesTry         = byteArrayOf(116, 114, 121)
val reservedBytesType        = byteArrayOf(116, 121, 112, 101)
val reservedBytesUnittest    = byteArrayOf(117, 110, 105, 116, 116, 101, 115, 116)
val reservedBytesWhile       = byteArrayOf(119, 104, 105, 108, 101)

