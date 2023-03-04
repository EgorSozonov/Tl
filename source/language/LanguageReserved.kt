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
val reservedBytesAlias       = byteArrayOf(97, 108, 105, 97, 115)
val reservedBytesAwait       = byteArrayOf(97, 119, 97, 105, 116)
val reservedBytesBreak       = byteArrayOf(98, 114, 101, 97, 107)
val reservedBytesCatch       = byteArrayOf(99, 97, 116, 99, 104)
val reservedBytesContinue    = byteArrayOf(99, 111, 110, 116, 105, 110, 117, 101)
val reservedBytesEmbed       = byteArrayOf(101, 109, 98, 101, 100)
val reservedBytesExport      = byteArrayOf(101, 120, 112, 111, 114, 116)
val reservedBytesFalse       = byteArrayOf(102, 97, 108, 115, 101)
val reservedBytesFor         = byteArrayOf(102, 111, 114)
val reservedBytesIf          = byteArrayOf(105, 102)
val reservedBytesIfEq        = byteArrayOf(105, 102, 69, 113)
val reservedBytesIfPr        = byteArrayOf(105, 102, 80, 114)
val reservedBytesImpl        = byteArrayOf(105, 109, 112, 108)
val reservedBytesInterface   = byteArrayOf(105, 110, 116, 101, 114, 102, 97, 99, 101)
val reservedBytesMatch       = byteArrayOf(109, 97, 116, 99, 104)
val reservedBytesMut         = byteArrayOf(109, 117, 116)
val reservedBytesNodestruct  = byteArrayOf(110, 111, 100, 101, 115, 116, 114, 117, 99, 116)
val reservedBytesReturn      = byteArrayOf(114, 101, 116, 117, 114, 110)
val reservedBytesStruct      = byteArrayOf(115, 116, 114, 117, 99, 116)
val reservedBytesTest        = byteArrayOf(116, 101, 115, 116)
val reservedBytesTrue        = byteArrayOf(116, 114, 117, 101)
val reservedBytesTry         = byteArrayOf(116, 114, 121)
val reservedBytesType        = byteArrayOf(116, 121, 112, 101)
val reservedBytesYield       = byteArrayOf(121, 105, 101, 108, 100)
