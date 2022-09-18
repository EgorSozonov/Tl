package compiler
import utils.ByteBuffer

object Lexer {


private val dispatchTable: Array<(inp: ByteArray, lr: LexResult) -> Unit>
private val byteBuf: ByteBuffer = ByteBuffer(50) // Thread-unsafe


/**
 * Main lexer function
 */
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

    // Main loop over the input
    while (result.i < inp.size && !result.wasError) {
        val cByte = inp[result.i]
        if (cByte >= 0) {
            dispatchTable[cByte.toInt()](inp, result)
        } else {
            result.errorOut(errorNonASCII)
            break
        }
    }

    result.finalize()

    return result
}


private fun lexWord(inp: ByteArray, lr: LexResult) {
    lexWordInternal(inp, lr, RegularToken.word)
}


/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Examples of accepted expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of NOT accepted expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private fun lexWordInternal(inp: ByteArray, lr: LexResult, wordType: RegularToken) {
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
    if (!lr.wasError) {
        val realStartChar = if (wordType == RegularToken.word) { startInd } else {startInd - 1}
        lr.addToken(0, realStartChar, lr.i - realStartChar, wordType)
    }
}


/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was uncapitalized
 */
private fun lexWordChunk(inp: ByteArray, lr: LexResult): Boolean {
    var result = false
    var i = lr.i

    if (inp[lr.i] == asciiUnderscore) {
        checkPrematureEnd(2, inp, lr)
        i += 1
    } else {
        checkPrematureEnd(1, inp, lr)
    }
    if (lr.wasError) return false

    if (isLowercaseLetter(inp[i])) {
        result = true
    } else if (!isCapitalLetter(inp[i])) {
        lr.errorOut(errorWordChunkStart)
        return result
    }
    lr.i = i + 1

    while (lr.i < inp.size && isAlphanumeric(inp[lr.i])) {
        lr.i += 1
    }
    if (lr.i < inp.size && inp[lr.i] == asciiUnderscore) {
        lr.errorOut(errorWordUnderscoresOnlyAtStart)
        return result
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
        lexWordInternal(inp, lr, RegularToken.dotWord)
    } else {
        lr.errorOut(errorUnrecognizedByte)
    }
}


private fun lexAtWord(inp: ByteArray, lr: LexResult) {
    checkPrematureEnd(2, inp, lr)
    if (lr.wasError) return

    lr.i += 1
    lexWordInternal(inp, lr, RegularToken.atWord)
}


/**
 * Lexes an integer literal, a hex integer literal, a binary integer literal, or a floating literal.
 * This function can handle being called on the last byte of input.
 */
private fun lexNumber(inp: ByteArray, lr: LexResult) {
    val cByte = inp[lr.i]
    if (lr.i == inp.size - 1 && isDigit(cByte)) {
        lr.addToken((cByte - asciiDigit0).toLong(), lr.i, 1, RegularToken.litInt)
        return
    }

    when (inp[lr.i + 1]) {
        asciiXLower -> { lexHexNumber(inp, lr) }
        asciiBLower -> { lexBinNumber(inp, lr) }
        else ->        { lexDecNumber(inp, lr) }
    }
}


/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO: add support for the '1.23E4' format
 */
private fun lexDecNumber(inp: ByteArray, lr: LexResult) {
    var i = lr.i

    while (i < inp.size && byteBuf.ind < 20) {
        val cByte = inp[i]
        if (isDigit(cByte)) {
            byteBuf.add(cByte)
        } else if (cByte == asciiUnderscore) {
            if (i == (inp.size - 1) || !isDigit(inp[i + 1])) {
                lr.errorOut(errorNumericEndUnderscore)
                return
            }
        } else {
            break
        }
        i++
    }
    if (i < inp.size && isDigit(inp[i])) {
        lr.errorOut(errorNumericIntWidthExceeded)
        return
    }
}

/**
 * Lexes a floating-point literal.

 */
private fun lexFloatLiteral(inp: ByteArray, lr: LexResult) {

}

/**
 * Lexes a 64-bit signed integer literal.
 */
private fun lexIntegerLiteral(inp: ByteArray, lr: LexResult) {

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
    byteBuf.clear()
    var j = lr.i + 2
}


/**
 * Lexes a decimal numeric literal (integer or floating-point).
 */
private fun lexBinNumber(inp: ByteArray, lr: LexResult) {
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
            lr.errorOut(errorNumericBinWidthExceeded)
            return
        }
        j++
    }
    if (j == lr.i + 2) {
        lr.errorOut(errorNumericEmpty)
        return
    }

    val resultValue = calcBinNumber()
    lr.addToken(resultValue, lr.i, j - lr.i, RegularToken.litInt)
    lr.i = j
}


private fun calcBinNumber(): Long {
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
    lr.i += 1
}

private fun lexParenLeft(inp: ByteArray, lr: LexResult) {
    lr.openPunctuation(PunctuationToken.parens)
}

private fun lexParenRight(inp: ByteArray, lr: LexResult) {
    lr.closePunctuation(PunctuationToken.parens)
}

private fun lexDollar(inp: ByteArray, lr: LexResult) {
    lr.openPunctuation(PunctuationToken.dollar)
}

private fun lexCurlyLeft(inp: ByteArray, lr: LexResult) {
    lr.openPunctuation(PunctuationToken.curlyBraces)
}

private fun lexCurlyRight(inp: ByteArray, lr: LexResult) {
    lr.closePunctuation(PunctuationToken.curlyBraces)
}

private fun lexBracketLeft(inp: ByteArray, lr: LexResult) {
    lr.openPunctuation(PunctuationToken.brackets)
}

private fun lexBracketRight(inp: ByteArray, lr: LexResult) {
    lr.closePunctuation(PunctuationToken.brackets)
}

private fun lexDotBracket(inp: ByteArray, lr: LexResult) {
    lr.openPunctuation(PunctuationToken.dotBrackets)
}

private fun lexSpace(inp: ByteArray, lr: LexResult) {
    lr.i += 1
}

private fun lexNewline(inp: ByteArray, lr: LexResult) {
    lr.closePunctuation(PunctuationToken.statement)
}

private fun lexStatementTerminator(inp: ByteArray, lr: LexResult) {
    lr.closePunctuation(PunctuationToken.statement)
}


/**
 * String literals look like 'wasn\'t' and may contain arbitrary UTF-8.
 * The insides of the string have escape sequences and variable interpolation with
 * the 'x = ${x}' syntax.
 * TODO probably need to count UTF-8 codepoints, or worse - grapheme clusters - in order to correctly report to LSP
 */
private fun lexStringLiteral(inp: ByteArray, lr: LexResult) {
    var i = lr.i + 1
    val szMinusOne = inp.size - 1
    while (i < inp.size) {
        val cByte = inp[i]
        if (cByte == asciiBackslash && i < szMinusOne && inp[i + 1] == asciiApostrophe) {
            i += 2
        } else if (cByte == asciiApostrophe) {
            lr.addToken(0, lr.i + 1, i - lr.i - 1, RegularToken.litString)
            lr.i = i + 1
            return
        } else {
            i += 1
        }
    }
    lr.errorOut(errorPrematureEndOfInput)
}

/**
 * Verbatim strings look like "asdf""" or "\nasdf""".
 * In the second variety (i.e. if the opening " is followed by the newline symbol),
 * the lexer ignores the newline symbol. Also, the minimal number of leading spaces among
 * the lines of text are also ignored. Combined, this means that the following:
 *     myStringVar = "
 *         A
 *          big
 *         grey wolf
 *     """
 * is equivalent to myStringVar = 'A\n big \ngrey wolf'
 * The inside of the string has variables interpolated into it like the common string literal:
 *     "x = ${x}"""
 * but is otherwise unaltered (no symbols are escaped).
 * The literal may not contain the '"""' combination. In the rare cases that string literals with
 * this sequence are needed, it's possible to use string concatenation or txt file inclusion.
 */
private fun lexVerbatimStringLiteral(inp: ByteArray, lr: LexResult) {
    var i = lr.i + 1
    while (i < inp.size) {
        val cByte = inp[i]
        if (cByte == asciiQuote) {
            if (i < (inp.size - 2) && inp[i + 1] == asciiQuote && inp[i + 2] == asciiQuote) {
                lr.addToken(0, lr.i + 1, i - lr.i - 1, RegularToken.verbatimString)
                lr.i = i + 3
                return
            }
        } else {
            i += 1
        }
    }
    lr.errorOut(errorPrematureEndOfInput)
    lr.i += 1
}


private fun lexUnrecognizedSymbol(inp: ByteArray, lr: LexResult) {
    lr.errorOut(errorUnrecognizedByte)
}

private fun lexComment(inp: ByteArray, lr: LexResult) {
    var i = lr.i + 1
    val szMinusOne = inp.size - 1
    while (i < inp.size) {
        val cByte = inp[i]
        if (cByte == asciiDot && i < szMinusOne && inp[i + 1] == asciiSharp) {
            lr.addToken(0, lr.i + 1, i - lr.i - 1, RegularToken.comment)
            lr.i = i + 2
            return
        } else if (cByte == asciiNewline) {
            lr.addToken(0, lr.i + 1, i - lr.i - 1, RegularToken.comment)
            lr.i = i + 1
            return
        } else {
            i += 1
        }
    }
    lr.addToken(0, lr.i + 1, i - lr.i - 1, RegularToken.comment)
    lr.i = inp.size
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

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private fun checkPrematureEnd(requiredSymbols: Int, inp: ByteArray, lr: LexResult) {
    if (lr.i > inp.size - requiredSymbols) {
        lr.errorOut(errorPrematureEndOfInput)
        return
    }
}



const val errorLengthOverflow             = "Token length overflow"
const val errorNonASCII                   = "Non-ASCII symbols are not allowed in code - only inside comments & string literals!"
const val errorPrematureEndOfInput        = "Premature end of input"
const val errorUnrecognizedByte           = "Unrecognized byte in source code!"
const val errorWordChunkStart             = "In an identifier, each word piece must start with a letter, optionally prefixed by 1 underscore!"
const val errorWordCapitalizationOrder    = "An identifier may not contain a capitalized piece after an uncapitalized one!"
const val errorWordUnderscoresOnlyAtStart = "Underscores are only allowed at start of word (snake case is forbidden)!"
const val errorNumericEndUnderscore       = "Numeric literal cannot end with underscore!"
const val errorNumericBinWidthExceeded    = "Integer literals cannot exceed 64 bit!"
const val errorNumericEmpty               = "Could not lex a numeric literal, empty sequence!"
const val errorNumericIntWidthExceeded    = "Integer literals must be within the range [-9,223,372,036,854,775,808; 9,223,372,036,854,775,807]!"
const val errorPunctuationExtraOpening    = "Extra opening punctuation"
const val errorPunctuationUnmatched       = "Unmatched closing punctuation"
const val errorPunctuationExtraClosing    = "Extra closing punctuation"
const val errorPunctuationWrongOpen       = "Wrong opening punctuation"


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

private const val asciiALower: Byte = 97
private const val asciiBLower: Byte = 98
private const val asciiFLower: Byte = 102
private const val asciiXLower: Byte = 120
private const val asciiZLower: Byte = 122
private const val asciiAUpper: Byte = 65
private const val asciiFUpper: Byte = 70
private const val asciiZUpper: Byte = 90
private const val asciiDigit0: Byte = 48
private const val asciiDigit1: Byte = 49
private const val asciiDigit9: Byte = 57

private const val asciiPlus: Byte = 43
private const val asciiMinus: Byte = 45
private const val asciiTimes: Byte = 42
private const val asciiDivBy: Byte = 47
private const val asciiDot: Byte = 46
private const val asciiPercent: Byte = 37

private const val asciiParenLeft: Byte = 40
private const val asciiParenRight: Byte = 41
private const val asciiCurlyLeft: Byte = 123
private const val asciiCurlyRight: Byte = 125
private const val asciiBracketLeft: Byte = 91
private const val asciiBracketRight: Byte = 93
private const val asciiPipe: Byte = 124
private const val asciiAmpersand: Byte = 38
private const val asciiTilde: Byte = 126
private const val asciiBackslash: Byte = 92

private const val asciiSpace: Byte = 32
private const val asciiNewline: Byte = 10
private const val asciiCarriageReturn: Byte = 13

private const val asciiApostrophe: Byte = 39
private const val asciiQuote: Byte = 34
private const val asciiSharp: Byte = 35
private const val asciiDollar: Byte = 36
private const val asciiUnderscore: Byte = 95
private const val asciiCaret: Byte = 94
private const val asciiAt: Byte = 64
private const val asciiColon: Byte = 58
private const val asciiSemicolon: Byte = 59
private const val asciiExclamation: Byte = 33
private const val asciiQuestion: Byte = 63
private const val asciiEquals: Byte = 61

private const val asciiLessThan: Byte = 60
private const val asciiGreaterThan: Byte = 62

private val operatorSymbols = byteArrayOf(
    asciiColon, asciiAmpersand, asciiPlus, asciiMinus, asciiDivBy, asciiTimes, asciiExclamation, asciiTilde,
    asciiPercent, asciiCaret, asciiPipe, asciiGreaterThan, asciiLessThan, asciiQuestion, asciiEquals,
)

init {
    dispatchTable = Array(127) { ::lexUnrecognizedSymbol }
    for (i in asciiDigit0..asciiDigit9) {
        dispatchTable[i] = ::lexNumber
    }

    for (i in asciiALower..asciiZLower) {
        dispatchTable[i] = ::lexWord
    }
    for (i in asciiAUpper..asciiZUpper) {
        dispatchTable[i] = ::lexWord
    }
    dispatchTable[asciiUnderscore.toInt()    ] = ::lexWord
    dispatchTable[asciiDot.toInt()           ] = ::lexDotSomething
    dispatchTable[asciiAt.toInt()            ] = ::lexAtWord

    for (i in operatorSymbols) {
        dispatchTable[i.toInt()] = ::lexOperator
    }
    dispatchTable[asciiParenLeft.toInt()     ] = ::lexParenLeft
    dispatchTable[asciiParenRight.toInt()    ] = ::lexParenRight
    dispatchTable[asciiDollar.toInt()        ] = ::lexDollar
    dispatchTable[asciiCurlyLeft.toInt()     ] = ::lexCurlyLeft
    dispatchTable[asciiCurlyRight.toInt()    ] = ::lexCurlyRight
    dispatchTable[asciiBracketLeft.toInt()   ] = ::lexBracketLeft
    dispatchTable[asciiBracketRight.toInt()  ] = ::lexBracketRight

    dispatchTable[asciiSpace.toInt()         ] = ::lexSpace
    dispatchTable[asciiCarriageReturn.toInt()] = ::lexSpace
    dispatchTable[asciiNewline.toInt()       ] = ::lexNewline
    dispatchTable[asciiSemicolon.toInt()     ] = ::lexStatementTerminator

    dispatchTable[asciiApostrophe.toInt()    ] = ::lexStringLiteral
    dispatchTable[asciiQuote.toInt()         ] = ::lexVerbatimStringLiteral
    dispatchTable[asciiSharp.toInt()         ] = ::lexComment
}


}