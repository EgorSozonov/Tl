package compiler

object Lexer {



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
    val walkLen = inp.size - 1
    while (lr.i < walkLen) {
        val cByte = inp[lr.i]
        val nByte = inp[lr.i + 1]
        if (cByte == asciiSpace || cByte == asciiCarriageReturn) {
            lr.i += 1
        } else if (cByte == asciiNewline) {
            lr.i += 1
        } else if (isLetter(cByte) || cByte == asciiUnderscore) {
            lexWord(inp, lr, TokenType.word)
        } else if (cByte == asciiDot && (isLetter(nByte) || nByte == asciiUnderscore)) {
            lexDotWord(inp, lr)
        } else if (cByte == asciiAt && (isLetter(nByte) || nByte == asciiUnderscore)) {
            lexAtWord(inp, lr)
        } else if (isDigit(cByte)) {
            lexNumber(inp, lr)
        } else {
            lr.errorOut("Unrecognized byte ${cByte} at index ${lr.i}")
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
            lr.errorOut("An identifier may not contain a capitalized piece after an uncapitalized one!")
            return
        }
        metUncapitalized = isCurrUncapitalized
    }
    if (lr.i > startInd) {
        val realStartChar = if (wordType == TokenType.word) { startInd } else {startInd - 1}
        lr.addToken(0, startInd, lr.i - realStartChar, wordType, 0)
    } else {
        lr.errorOut("Could not lex a word at position $startInd")
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
        lr.i += 1
        if (lr.i == inp.size) {
            lr.errorOut("Identifier cannot end with underscore!")
            return result
        }
    }
    if (isLowercaseLetter(inp[lr.i])) {
        result = true
    } else if (!isCapitalLetter(inp[lr.i])) {
        lr.errorOut("In an identifier, each word chunk must start with a letter!")
        return result
    }
    lr.i += 1

    while (lr.i < inp.size && isAlphanumeric(inp[lr.i])) {
        lr.i += 1
    }

    return result
}

private fun lexDotWord(inp: ByteArray, lr: LexResult) {
    lr.i += 1
    lexWord(inp, lr, TokenType.dotWord)
}

private fun lexAtWord(inp: ByteArray, lr: LexResult) {
    lr.i += 1
    lexWord(inp, lr, TokenType.atWord)
}

/**
 * Lexes an integer literal, a hex integer literal, a binary integer literal, or a floating literal.
 * This function can handle being called on the last byte of input.
 */
private fun lexNumber(inp: ByteArray, lr: LexResult) {
    if (lr.i == inp.size - 1) {
        lr.addToken((inp[lr.i] - asciiDigit0).toLong(), lr.i, 1, TokenType.litInt, 0)
        return
    }
}

/**
 * Lexes a decimal numeric literal (integer or floating-point).
 */
private fun lexDecNumber(inp: ByteArray, lr: LexResult) {

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
private fun lexHexNumber(inp: ByteArray, lr: LexResult) {

}

/**
 * Lexes a decimal numeric literal (integer or floating-point).
 */
private fun lexBinNumber(inp: ByteArray, lr: LexResult) {

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


private const val asciiALower: Byte = 97;
private const val asciiZLower: Byte = 122;
private const val asciiAUpper: Byte = 65;
private const val asciiZUpper: Byte = 90;
private const val asciiDigit0: Byte = 48;
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
    
    
}