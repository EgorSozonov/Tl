package compiler
import utils.ByteBuffer

object Lexer {


private val dispatchTable = Array(127) { ::lexUnrecognizedSymbol }
private val byteBuffer: ByteBuffer = ByteBuffer(50)


fun lexicallyAnalyze(inp: ByteArray): LexResult {
    val result = LexResult()
    if (inp.isEmpty()) {
        result.errorOut("Empty input")
        return result
    }

    // Check for UTF-8 BOM at start of file
    if (inp.size >= 3 && inp[0] == 0xEF.toByte() && inp[1] == 0xBB.toByte() && inp[2] == 0xBF.toByte()) {
        result.i = 3
    }

    lexLoop(inp, result)

    return result
}

private fun lexLoop(inp: ByteArray, lr: LexResult) {
    val byteBuffer = ByteBuffer(50)
    while (lr.i < inp.size) {
        val cByte = inp[lr.i]
        if (cByte < 0) {
            lr.errorOut(errorNonASCII)
            return
        }
        if (isLetter(cByte) || cByte == asciiUnderscore) {
            lexWord(inp, lr, TokenType.word)
        } else if (cByte == asciiDot) {
            lexDotSomething(inp, lr)
        } else if (cByte == asciiAt) {
            lexAtWord(inp, lr)
        } else if (isDigit(cByte)) {
            lexNumber(inp, lr, byteBuffer)
        } else if (cByte == asciiParenLeft) {
            lexParenLeft(inp, lr)
        } else if (cByte == asciiParenRight) {
            lexParenRight(inp, lr)
        } else if (cByte == asciiCurlyLeft) {
            lexCurlyLeft(inp, lr)
        } else if (cByte == asciiCurlyRight) {
            lexCurlyRight(inp, lr)
        } else if (cByte == asciiBracketLeft) {
            lexBracketLeft(inp, lr)
        } else if (cByte == asciiBracketRight) {
            lexBracketRight(inp, lr)
        } else if (cByte == asciiSpace || cByte == asciiCarriageReturn) {
            lr.i += 1
        } else if (cByte == asciiNewline) {
            lr.i += 1
        } else if (isOperator(cByte)) {
            lexOperator(inp, lr)
        } else {
            lr.errorOut(errorUnrecognizedByte)
            return
        }
    }
}

/**
 * Lexes a word (both reserved and identifier) according to the Tl's rules.
 * Examples of accepted expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of NOT accepted expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private fun lexWord(inp: ByteArray, lr: LexResult, wordType: TokenType) {
    val startInd = lr.i
    var metUncapitalized = lexWordChunk(inp, lr)
    while (lr.i < (inp.size - 1) && inp[lr.i] == asciiDot && !lr.wasError) {
        lr.i += 1
        val isCurrUncapitalized = lexWordChunk(inp, lr)
        if (metUncapitalized && !isCurrUncapitalized) {
            lr.errorOut(errorWordCapitalizationOrder)
            return
        }
        metUncapitalized = isCurrUncapitalized
    }
    if (lr.i > startInd) {
        val realStartChar = if (wordType == TokenType.word) { startInd } else {startInd - 1}
        lr.addToken(0, realStartChar, lr.i - realStartChar, wordType, 0)
    } else {
        lr.errorOut(errorWordEmpty)
    }
}

/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was uncapitalized
 */
private fun lexWordChunk(inp: ByteArray, lr: LexResult): Boolean {
    var result = false
    if (inp[lr.i] == asciiUnderscore) {
        checkPrematureEnd(2, inp, lr)
        lr.i += 1
    } else {
        checkPrematureEnd(1, inp, lr)
    }
    if (lr.wasError) return false

    if (isLowercaseLetter(inp[lr.i])) {
        result = true
    } else if (!isCapitalLetter(inp[lr.i])) {
        lr.errorOut(errorWordChunkStart)
        return result
    }
    lr.i += 1

    while (lr.i < inp.size && isAlphanumeric(inp[lr.i])) {
        lr.i += 1
    }

    return result
}

/**
 * Lexes either a dot-word (like '.asdf') or a dot-bracket (like '.[1 2 3]')
 */
private fun lexDotSomething(inp: ByteArray, lr: LexResult) {
    checkPrematureEnd(2, inp, lr)
    if (lr.wasError) return

    val nByte = inp[lr.i + 1]
    if (nByte == asciiBracketLeft) {
        lexDotBracket(inp, lr)
    } else if (nByte == asciiUnderscore || isLetter(nByte)) {
        lr.i += 1
        lexWord(inp, lr, TokenType.dotWord)
    } else {
        lr.errorOut(errorUnrecognizedByte)
    }
}


private fun lexAtWord(inp: ByteArray, lr: LexResult) {
    checkPrematureEnd(2, inp, lr)
    if (lr.wasError) return

    lr.i += 1
    lexWord(inp, lr, TokenType.atWord)
}

/**
 * Lexes an integer literal, a hex integer literal, a binary integer literal, or a floating literal.
 * This function can handle being called on the last byte of input.
 */
private fun lexNumber(inp: ByteArray, lr: LexResult, byteBuf: ByteBuffer) {
    val cByte = inp[lr.i]
    if (lr.i == inp.size - 1 && isDigit(cByte)) {
        lr.addToken((cByte - asciiDigit0).toLong(), lr.i, 1, TokenType.litInt, 0)
        return
    }

    when (inp[lr.i + 1]) {
        asciiXLower -> { lexHexNumber(inp, lr, byteBuf) }
        asciiBLower -> { lexBinNumber(inp, lr, byteBuf) }
        else ->        { lexDecNumber(inp, lr, byteBuf) }
    }
}

/**
 * Lexes a decimal numeric literal (integer or floating-point).
 */
private fun lexDecNumber(inp: ByteArray, lr: LexResult, byteBuf: ByteBuffer) {

}

/**
 * Parses the floating-point numbers using just the "fast path" of David Gay's "strtod" function,
 * extended to 16 digits.
 * I.e. it handles only numbers with 15 digits or 16 digits with the first digit not 9,
 * and decimal powers up to 10^22.
 * Parsing the rest of numbers exactly is a huge and pretty useless effort. Nobody needs these
 * floating literals in text form.
 * Input: array of bytes that are digits (without leading zeroes), and the signed exponent base 10.
 * Example, for input text '1.23' this function would get the args: ([49 50 51] -2)
 */
private fun calcFloating(inp: ByteArray, exponent: Int): Double {
    return 0.0
}

/**
 * Lexes a hexadecimal numeric literal (integer or floating-point).
 * Examples of accepted expressions: 0xCAFE_BABE, 0xdeadbeef, 0x123_45A
 * Examples of NOT accepted expressions: 0xCAFE_babe, 0x_deadbeef, 0x123_
 * Checks that the input fits into a signed 64-bit fixnum.
 */
private fun lexHexNumber(inp: ByteArray, lr: LexResult, byteBuf: ByteBuffer) {
    byteBuf.clear()
    var j = lr.i + 2
}



/**
 * Lexes a decimal numeric literal (integer or floating-point).
 */
private fun lexBinNumber(inp: ByteArray, lr: LexResult, byteBuf: ByteBuffer) {
    byteBuf.clear()

    var j = lr.i + 2
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == asciiDigit0) {
            byteBuf.add(0)
        } else if (cByte == asciiDigit1) {
            byteBuf.add(1)
        } else if (cByte == asciiUnderscore) {
            if ((j == inp.size - 1 || (inp[j + 1] != asciiDigit0 && inp[j + 1] != asciiDigit1))) {
                lr.errorOut(errorNumericEndUnderscore)
                return
            }
        } else {
            break
        }
        if (byteBuf.ind > 64) {
            lr.errorOut(errorNumericIntWidthExceeded)
            return
        }
        j++
    }
    if (j == lr.i + 2) {
        lr.errorOut(errorNumericEmpty)
        return
    }

    val resultValue = calcBinNumber(byteBuf)
    lr.addToken(resultValue, lr.i, j - lr.i, TokenType.litInt, 0)
    lr.i = j
}


private fun calcBinNumber(byteBuf: ByteBuffer): Long {
    var result: Long = 0
    var powerOfTwo: Long = 1
    var i = byteBuf.ind - 1

    // If the literal is full 64 bits long, then its upper bit is the sign bit, so we don't read it
    val loopLimit = if (byteBuf.ind == 64) { 0 } else { -1 }
    while (i > loopLimit) {
        if (byteBuf.buffer[i] > 0) {
            result += powerOfTwo
        }
        powerOfTwo = powerOfTwo shl 1
        i--
    }
    if (byteBuf.ind == 64 && byteBuf.buffer[0] > 0) {
        result += Long.MIN_VALUE
    }
    return result
}

private fun lexOperator(inp: ByteArray, lr: LexResult) {

}

private fun lexParenLeft(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexParenRight(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexCurlyLeft(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexCurlyRight(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexBracketLeft(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexBracketRight(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexDotBracket(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexUnrecognizedSymbol(inp: ByteArray, lr: LexResult) {
    lr.errorOut(errorUnrecognizedByte)
    return
}

private fun isLetter(a: Byte): Boolean {
    return ((a >= asciiALower && a <= asciiZLower) || (a >= asciiAUpper && a <= asciiZUpper))
}

private fun isCapitalLetter(a: Byte): Boolean {
    return a >= asciiAUpper && a <= asciiZUpper
}

private fun isLowercaseLetter(a: Byte): Boolean {
    return a >= asciiALower && a <= asciiZLower
}

private fun isAlphanumeric(a: Byte): Boolean {
    return isLetter(a) || isDigit(a)
}

private fun isDigit(a: Byte): Boolean {
    return a >= asciiDigit0 && a <= asciiDigit9
}

private fun isOperator(a: Byte): Boolean {
    for (opSym in operatorSymbols) {
        if (opSym == a) return true
    }
    return false
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private fun checkPrematureEnd(requiredSymbols: Int, inp: ByteArray, lr: LexResult) {
    if (lr.i > inp.size - requiredSymbols) {
        lr.errorOut(errorPrematureEndOfInput)
        return
    }
}

private const val asciiALower: Byte = 97;
private const val asciiBLower: Byte = 98;
private const val asciiFLower: Byte = 102;
private const val asciiXLower: Byte = 120;
private const val asciiZLower: Byte = 122;
private const val asciiAUpper: Byte = 65;
private const val asciiFUpper: Byte = 70;
private const val asciiZUpper: Byte = 90;
private const val asciiDigit0: Byte = 48;
private const val asciiDigit1: Byte = 49;
private const val asciiDigit9: Byte = 57;

private const val asciiPlus: Byte = 43;
private const val asciiMinus: Byte = 45;
private const val asciiTimes: Byte = 42;
private const val asciiDivBy: Byte = 47;
private const val asciiDot: Byte = 46;
private const val asciiPercent: Byte = 37;

private const val asciiParenLeft: Byte = 40;
private const val asciiParenRight: Byte = 41;
private const val asciiCurlyLeft: Byte = 91;
private const val asciiCurlyRight: Byte = 93;
private const val asciiBracketLeft: Byte = 40;
private const val asciiBracketRight: Byte = 41;
private const val asciiPipe: Byte = 124;
private const val asciiAmpersand: Byte = 38;
private const val asciiTilde: Byte = 126;
private const val asciiBackslash: Byte = 92;

private const val asciiSpace: Byte = 32;
private const val asciiNewline: Byte = 10;
private const val asciiCarriageReturn: Byte = 13;

private const val asciiApostrophe: Byte = 39;
private const val asciiQuote: Byte = 34;
private const val asciiSharp: Byte = 35;
private const val asciiDollar: Byte = 36;
private const val asciiUnderscore: Byte = 95;
private const val asciiCaret: Byte = 94;
private const val asciiAt: Byte = 64;
private const val asciiColon: Byte = 58;
private const val asciiSemicolon: Byte = 59;
private const val asciiExclamation: Byte = 33;
private const val asciiQuestion: Byte = 63;
private const val asciiEquals: Byte = 61;

private const val asciiLessThan: Byte = 60;
private const val asciiGreaterThan: Byte = 62;

private const val errorNonASCII = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!"
private const val errorPrematureEndOfInput = "Premature end of input"
private const val errorUnrecognizedByte = "Unrecognized byte in source code!"
private const val errorWordChunkStart = "In an identifier, each word chunk must start with a letter!"
private const val errorWordEndUnderscore = "Identifier cannot end with underscore!"
private const val errorWordCapitalizationOrder = "An identifier may not contain a capitalized piece after an uncapitalized one!"
private const val errorWordEmpty = "Could not lex a word, empty sequence!"
private const val errorNumericEndUnderscore = "Numeric literal cannot end with underscore!"
private const val errorNumericIntWidthExceeded = "Integer literals cannot exceed 64 bit!"
private const val errorNumericEmpty = "Could not lex a numeric literal, empty sequence!"


private val operatorSymbols = byteArrayOf(
    asciiColon, asciiAmpersand, asciiPlus, asciiMinus, asciiDivBy, asciiTimes, asciiExclamation, asciiTilde,
    asciiPercent, asciiCaret, asciiPipe, asciiGreaterThan, asciiLessThan, asciiQuestion, asciiEquals,
)

/**
 * The ASCII notation for the lowest 64-bit integer, -9_223_372_036_854_775_808
 */
private val minInt = byteArrayOf(
    57, 50, 50, 51, 51, 55, 50, 48, 51, 54,
    56, 53, 52, 55, 55, 53, 56, 48, 56
)

/**
 * The ASCII notation for the highest 64-bit integer, 9_223_372_036_854_775_807
 */
private val maxInt = byteArrayOf(
    57, 50, 50, 51, 51, 55, 50, 48, 51, 54,
    56, 53, 52, 55, 55, 53, 56, 48, 55
)

    
}