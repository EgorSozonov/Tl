package compiler.lexer
import kotlin.math.absoluteValue
import kotlin.math.pow

class LexerNumerics {


private var buffer: ByteArray
var ind: Int
    private set

init {
    buffer = ByteArray(50)
    ind = 0
}


fun calcInteger(): Long {
    var powerOfTen = 1L
    var result = 0L
    var i = ind - 1

    val loopLimit = -1
    while (i > loopLimit) {
        result += powerOfTen*buffer[i]
        powerOfTen *= 10
        i--
    }
    return result
}

fun calcBinNumber(): Long {
    var result: Long = 0
    var powerOfTwo: Long = 1
    var i = ind - 1

    // If the literal is full 64 bits long, then its upper bit is the sign bit, so we don't read it
    val loopLimit = if (ind == 64) { 0 } else { -1 }
    while (i > loopLimit) {
        if (buffer[i] > 0) {
            result += powerOfTwo
        }
        powerOfTwo = powerOfTwo shl 1
        i--
    }
    if (ind == 64 && buffer[0] > 0) {
        result += Long.MIN_VALUE
    }
    return result
}

fun calcHexNumber(): Long {
    var result: Long = 0
    var powerOfSixteen: Long = 1
    var i = ind - 1

    // If the literal is full 16 bits long, then its upper sign contains the sign bit
    val loopLimit = -1 //if (byteBuf.ind == 16) { 0 } else { -1 }
    while (i > loopLimit) {
        result += powerOfSixteen*buffer[i]
        powerOfSixteen = powerOfSixteen shl 4
        i--
    }
    return result
}


/**
 * Parses the floating-point numbers using just the "fast path" of David Gay's "strtod" function,
 * extended to 16 digits.
 * I.e. it handles only numbers with 15 digits or 16 digits with the first digit not 9,
 * and decimal powers within [-22; 22].
 * Parsing the rest of numbers exactly is a huge and pretty useless effort. Nobody needs these
 * floating literals in text form: they are better input in binary, or at least text-hex or text-binary.
 * Input: array of bytes that are digits (without leading zeroes), and the negative power of ten.
 * So for '0.5' the input would be (5 -1), and for '1.23000' (123000 -5).
 * Example, for input text '1.23' this function would get the args: ([1 2 3] 1)
 * Output: a 64-bit floating-point number, encoded as a long (same bits)
 */
fun calcFloating(powerOfTen: Int): Double? {
    val zero: Byte = 0.toByte()

    var indTrailingZeroes = ind - 1
    while (indTrailingZeroes > -1 && buffer[indTrailingZeroes] == zero) { indTrailingZeroes-- }


    // how many powers of 10 need to be knocked off the significand to make it fit
    val significandNeeds = (ind - 16).coerceAtLeast(0)
    // how many power of 10 significand can knock off (these are just trailing zeroes)
    val significantCan = ind - indTrailingZeroes - 1
    // how many powers of 10 need to be added to the exponent to make it fit
    val exponentNeeds = -22 - powerOfTen
    // how many power of 10 at maximum can be added to the exponent
    val exponentCanAccept = 22 - powerOfTen

    if (significantCan < significandNeeds || significantCan < exponentNeeds || significandNeeds > exponentCanAccept) {
        return null
    }

    // Transfer of decimal powers from significand to exponent to make them both fit within their respective limits
    // (10000 -6) -> (1 -2); (10000 -3) -> (10 0)
    val transfer = if (significandNeeds >= exponentNeeds) {
            if (ind - significandNeeds == 16 && significandNeeds < significantCan && significandNeeds + 1 <= exponentCanAccept) {
                significandNeeds + 1
            } else {
                significandNeeds
            }
        } else {
            exponentNeeds
        }
    ind -= transfer
    val finalPowerTen = powerOfTen + transfer

    val maximumPreciselyRepresentedInt = byteArrayOf(9, 0, 0, 7, 1, 9, 9, 2, 5, 4, 7, 4, 0, 9, 9, 2) // 2**53
    if (!integerWithinDigits(maximumPreciselyRepresentedInt)) {
        return null }

    val significandInt = calcInteger()
    val significand: Double = significandInt.toDouble() // precise
    val exponent: Double = 10.0.pow(finalPowerTen.absoluteValue.toDouble())

    return if (finalPowerTen > 0) { significand*exponent } else {significand/exponent}
}


/**
 * Checks if the current byteBuf <= b if they are regarded as arrays of decimal digits (0 to 9).
 */
private fun integerWithinDigits(b: ByteArray): Boolean {
    if (ind != b.size) return (ind < b.size)
    for (i in 0 until ind) {
        if (buffer[i] < b[i]) return true
        if (buffer[i] > b[i]) return false
    }
    return true
}


fun add(b: Byte) {
    if (ind < buffer.size) {
        buffer[ind] = b
        ind += 1
    } else {

        val newBuffer = ByteArray(buffer.size * 2)
        buffer.copyInto(newBuffer, 0, 0, buffer.size)
        newBuffer[buffer.size] = b
        ind = buffer.size + 1
        buffer = newBuffer

    }
}


fun clear() {
    ind = 0
}


}