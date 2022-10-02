package compiler.lexer
import java.lang.StringBuilder
import java.util.*

class Lexer {


var firstChunk: LexChunk = LexChunk()
var currChunk: LexChunk
var nextInd: Int // next ind inside the current token chunk
var totalTokens: Int
var wasError: Boolean
var errMsg: String
private var i: Int // current index inside input byte array
private var comments: CommentStorage = CommentStorage()
private val backtrack = Stack<Pair<PunctuationToken, Int>>()
private val numeric: LexerNumerics = LexerNumerics()


/**
 * Main lexer method
 */
fun lexicallyAnalyze(inp: ByteArray) {
    if (inp.isEmpty()) {
        errorOut("Empty input")
        return
    }

    // Check for UTF-8 BOM at start of file
    if (inp.size >= 3 && inp[0] == 0xEF.toByte() && inp[1] == 0xBB.toByte() && inp[2] == 0xBF.toByte()) {
        i = 3
    }

    // Main loop over the input
    while (i < inp.size && !wasError) {
        val cByte = inp[i]
        if (cByte >= 0) {
            dispatchTable[cByte.toInt()](inp)
        } else {
            errorOut(errorNonASCII)
            break
        }
    }

    finalize()
}


private fun lexWord(inp: ByteArray) {
    lexWordInternal(inp, RegularToken.word)
}


/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Examples of accepted expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of NOT accepted expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private fun lexWordInternal(inp: ByteArray, wordType: RegularToken) {
    val startInd = i
    var metUncapitalized = lexWordChunk(inp)
    while (i < (inp.size - 1) && inp[i] == asciiDot && inp[i + 1] != asciiBracketLeft && !wasError) {
        i += 1
        val isCurrUncapitalized = lexWordChunk(inp)
        if (metUncapitalized && !isCurrUncapitalized) {
            errorOut(errorWordCapitalizationOrder)
            return
        }
        metUncapitalized = isCurrUncapitalized
    }
    if (!wasError) {
        val realStartChar = if (wordType == RegularToken.word) { startInd } else {startInd - 1}
        addToken(0, realStartChar, i - realStartChar, wordType)
    }
}


/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was uncapitalized
 */
private fun lexWordChunk(inp: ByteArray): Boolean {
    var result = false

    if (inp[i] == asciiUnderscore) {
        checkPrematureEnd(2, inp)
        i += 1
    } else {
        checkPrematureEnd(1, inp)
    }
    if (wasError) return false

    if (isLowercaseLetter(inp[i])) {
        result = true
    } else if (!isCapitalLetter(inp[i])) {
        errorOut(errorWordChunkStart)
        return result
    }
    i += 1

    while (i < inp.size && isAlphanumeric(inp[i])) {
        i += 1
    }
    if (i < inp.size && inp[i] == asciiUnderscore) {
        errorOut(errorWordUnderscoresOnlyAtStart)
        return result
    }

    return result
}


/**
 * Lexes either a dot-word (like '.asdf') or a dot-bracket (like '.[1 2 3]')
 */
private fun lexDotSomething(inp: ByteArray) {
    checkPrematureEnd(2, inp)
    if (wasError) return

    val nByte = inp[i + 1]
    if (nByte == asciiBracketLeft) {
        lexDotBracket(inp)
    } else if (nByte == asciiUnderscore || isLetter(nByte)) {
        i += 1
        lexWordInternal(inp, RegularToken.dotWord)
    } else {
        errorOut(errorUnrecognizedByte)
    }
}


private fun lexAtWord(inp: ByteArray) {
    checkPrematureEnd(2, inp)
    if (wasError) return

    i += 1
    lexWordInternal(inp, RegularToken.atWord)
}


/**
 * Lexes an integer literal, a hex integer literal, a binary integer literal, or a floating literal.
 * This function can handle being called on the last byte of input.
 */
private fun lexNumber(inp: ByteArray) {
    val cByte = inp[i]
    if (i == inp.size - 1 && isDigit(cByte)) {
        addToken((cByte - asciiDigit0).toLong(), i, 1, RegularToken.litInt)
        i++
        return
    }

    when (inp[i + 1]) {
        asciiXLower -> { lexHexNumber(inp) }
        asciiBLower -> { lexBinNumber(inp) }
        else ->        { lexDecNumber(inp) }
    }
    numeric.clear()
}


/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO: add support for the '1.23E4' format
 */
private fun lexDecNumber(inp: ByteArray) {
    var j = i
    var digitsAfterDot = 0 // this is relative to first digit, so it includes the leading zeroes
    var metDot = false
    var metNonzero = false
    val maximumInd = (i + 40).coerceAtMost(inp.size)
    while (j < maximumInd) {
        val cByte = inp[j]

        if (isDigit(cByte)) {
            if (metNonzero) {
                numeric.add((cByte - asciiDigit0).toByte())
            } else if (cByte != asciiDigit0) {
                metNonzero = true
                numeric.add((cByte - asciiDigit0).toByte())
            }
            if (metDot) { digitsAfterDot++ }
        } else if (cByte == asciiUnderscore) {
            if (j == (inp.size - 1) || !isDigit(inp[j + 1])) {
                errorOut(errorNumericEndUnderscore)
                return
            }
        } else if (cByte == asciiDot) {
            if (metDot) {
                errorOut(errorNumericMultipleDots)
                return
            }
            metDot = true
        } else {
            break
        }
        j++
    }

    if (j < inp.size && isDigit(inp[j])) {
        errorOut(errorNumericWidthExceeded)
        return
    }

    if (metDot) {
        val resultValue = numeric.calcFloating(-digitsAfterDot)
        if (resultValue == null) {
            errorOut(errorNumericFloatWidthExceeded)
            return
        }
        addToken(resultValue, i, j - i)
    } else {
        val resultValue = numeric.calcInteger()
        if (resultValue == null) {
            errorOut(errorNumericIntWidthExceeded)
            return
        }
        addToken(resultValue, i, j - i, RegularToken.litInt)
    }
    i = j
}


/**
 * Lexes a hexadecimal numeric literal (integer or floating-point).
 * Examples of accepted expressions: 0xCAFE_BABE, 0xdeadbeef, 0x123_45A
 * Examples of NOT accepted expressions: 0xCAFE_babe, 0x_deadbeef, 0x123_
 * Checks that the input fits into a signed 64-bit fixnum.
 * TODO add floating-point literals like 0x12FA.
 */
private fun lexHexNumber(inp: ByteArray) {
    checkPrematureEnd(2, inp)
    numeric.clear()
    var j = i + 2
    while (j < inp.size) {
        val cByte = inp[j]
        if (isDigit(cByte)) {

            numeric.add((cByte - asciiDigit0).toByte())
        } else if ((cByte >= asciiALower && cByte <= asciiFLower)) {
            numeric.add((cByte - asciiALower + 10).toByte())
        } else if ((cByte >= asciiAUpper && cByte <= asciiFUpper)) {
            numeric.add((cByte - asciiAUpper + 10).toByte())
        } else if (cByte == asciiUnderscore) {
            if (j == inp.size - 1 || isHexDigit(inp[j + 1])) {
                errorOut(errorNumericEndUnderscore)
                return
            }
        } else {
            break
        }
        if (numeric.ind > 16) {
            errorOut(errorNumericBinWidthExceeded)
            return
        }
        j++
    }
    val resultValue = numeric.calcHexNumber()
    addToken(resultValue, i, j - i, RegularToken.litInt)
    numeric.clear()
    i = j
}


/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO add floating-point literals like 0b0101.
 */
private fun lexBinNumber(inp: ByteArray) {
    numeric.clear()

    var j = i + 2
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == asciiDigit0) {
            numeric.add(0)
        } else if (cByte == asciiDigit1) {
            numeric.add(1)
        } else if (cByte == asciiUnderscore) {
            if ((j == inp.size - 1 || (inp[j + 1] != asciiDigit0 && inp[j + 1] != asciiDigit1))) {
                errorOut(errorNumericEndUnderscore)
                return
            }
        } else {
            break
        }
        if (numeric.ind > 64) {
            errorOut(errorNumericBinWidthExceeded)
            return
        }
        j++
    }
    if (j == i + 2) {
        errorOut(errorNumericEmpty)
        return
    }

    val resultValue = numeric.calcBinNumber()
    addToken(resultValue, i, j - i, RegularToken.litInt)
    i = j
}



private fun lexOperator(inp: ByteArray) {

    i += 1
}

private fun lexParenLeft(inp: ByteArray) {
    openPunctuation(PunctuationToken.parens)
}

private fun lexParenRight(inp: ByteArray) {
    closePunctuation(PunctuationToken.parens)
}

private fun lexDollar(inp: ByteArray) {
    openPunctuation(PunctuationToken.dollar)
}

private fun lexCurlyLeft(inp: ByteArray) {
    openPunctuation(PunctuationToken.curlyBraces)
}

private fun lexCurlyRight(inp: ByteArray) {
    closePunctuation(PunctuationToken.curlyBraces)
}

private fun lexBracketLeft(inp: ByteArray) {
    openPunctuation(PunctuationToken.brackets)
}

private fun lexBracketRight(inp: ByteArray) {
    closePunctuation(PunctuationToken.brackets)
}

private fun lexDotBracket(inp: ByteArray) {
    openPunctuation(PunctuationToken.dotBrackets)
}

private fun lexSpace(inp: ByteArray) {
    i += 1
}

private fun lexNewline(inp: ByteArray) {
    closePunctuation(PunctuationToken.statement)
}

private fun lexStatementTerminator(inp: ByteArray) {
    closePunctuation(PunctuationToken.statement)
}


/**
 * String literals look like 'wasn\'t' and may contain arbitrary UTF-8.
 * The insides of the string have escape sequences and variable interpolation with
 * the 'x = ${x}' syntax.
 * TODO probably need to count UTF-8 codepoints, or worse - grapheme clusters - in order to correctly report to LSP
 */
private fun lexStringLiteral(inp: ByteArray) {
    var j = i + 1
    val szMinusOne = inp.size - 1
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == asciiBackslash && j < szMinusOne && inp[j + 1] == asciiApostrophe) {
            j += 2
        } else if (cByte == asciiApostrophe) {
            addToken(0, i + 1, j - i - 1, RegularToken.litString)
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    errorOut(errorPrematureEndOfInput)
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
private fun lexVerbatimStringLiteral(inp: ByteArray) {
    var j = i + 1
    while (i < inp.size) {
        val cByte = inp[j]
        if (cByte == asciiQuote) {
            if (j < (inp.size - 2) && inp[j + 1] == asciiQuote && inp[j + 2] == asciiQuote) {
                addToken(0, i + 1, j - i - 1, RegularToken.verbatimString)
                i = j + 3
                return
            }
        } else {
            j += 1
        }
    }
    errorOut(errorPrematureEndOfInput)
    i += 1
}


private fun lexUnrecognizedSymbol(inp: ByteArray) {
    errorOut(errorUnrecognizedByte)
}


private fun lexComment(inp: ByteArray) {
    var j = i + 1
    val szMinusOne = inp.size - 1
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == asciiDot && j < szMinusOne && inp[j + 1] == asciiSharp) {
            addToken(0, i + 1, j - i - 1, RegularToken.comment)
            i = j + 2
            return
        } else if (cByte == asciiNewline) {
            addToken(0, i + 1, j - i - 1, RegularToken.comment)
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    addToken(0, i + 1, j - i - 1, RegularToken.comment)
    i = inp.size
}


/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private fun checkPrematureEnd(requiredSymbols: Int, inp: ByteArray) {
    if (i > inp.size - requiredSymbols) {
        errorOut(errorPrematureEndOfInput)
        return
    }
}




/**
 * Adds a regular, non-punctuation token
 */
private fun addToken(payload: Long, startByte: Int, lenBytes: Int, tType: RegularToken) {
    if (tType == RegularToken.comment) {
        comments.add(startByte, lenBytes)
    } else {
        wrapTokenInStatement(startByte, tType)
        appendToken(payload, startByte, lenBytes, tType)
    }
}

/**
 * Adds a floating-point literal token
 */
private fun addToken(payload: Double, startByte: Int, lenBytes: Int) {
    wrapTokenInStatement(startByte, RegularToken.litFloat)
    appendToken(payload, startByte, lenBytes)
}


/**
 * Adds a token which serves punctuation purposes, i.e. either a (, a {, a [, a .[ or a $
 * These tokens are used to define the structure, that is, nesting within the AST.
 * Upon addition, they are saved to the backtracking stack to be updated with their length
 * once it is known.
 * The startByte & lengthBytes don't include the opening and closing delimiters, and
 * the lenTokens also doesn't include the punctuation token itself - only the insides of the
 * scope. Thus, for '(asdf)', the opening paren token will have a byte length of 4 and a
 * token length of 1.
 */
private fun openPunctuation(tType: PunctuationToken) {
    validateOpeningPunct(tType)

    backtrack.add(Pair(tType, totalTokens))
    val insidesStart = if (tType == PunctuationToken.dotBrackets) { i + 2 } else { i + 1 }
    appendToken(insidesStart, tType)
    i = insidesStart
}


/**
 * Regular tokens may not exist directly at the top level, or inside curlyBraces.
 * So this function inserts an implicit statement scope to prevent this.
 */
private fun wrapTokenInStatement(startByte: Int, tType: RegularToken) {
    if (backtrack.empty() || backtrack.peek().first == PunctuationToken.curlyBraces) {
        var realStartByte = startByte
        if (tType == RegularToken.litString || tType == RegularToken.verbatimString) realStartByte -= 1
        addStatement(realStartByte)
    }
}


/**
 * Regular tokens may not exist directly at the top level, or inside curlyBraces.
 * So this function inserts an implicit statement scope to prevent this.
 */
private fun addStatement(startByte: Int) {
    backtrack.add(Pair(PunctuationToken.statement, totalTokens))
    appendToken(startByte, PunctuationToken.statement)
}


/**
 * Validates that opening punctuation obeys the rules
 * and inserts an implicit Statement if immediately inside curlyBraces.
 */
private fun validateOpeningPunct(openingType: PunctuationToken) {
    if (openingType == PunctuationToken.curlyBraces) {
        if (backtrack.isEmpty()) return
        val currScopeType = backtrack.peek().first
        // the '({ stmt })' case
        if (currScopeType == PunctuationToken.parens && getPrevTokenType() == PunctuationToken.parens.internalVal.toInt()) return
        // the 'stmt { new scope }' case - the statement gets closed, and the new scope comes after it
        if (currScopeType == PunctuationToken.statement) {
            closeDollars()
            closeStatement()
            return
        }

        errorOut(errorPunctuationWrongOpen)
    } else if (openingType == PunctuationToken.dotBrackets && getPrevTokenType() != RegularToken.word.internalVal.toInt()) {
        errorOut(errorPunctuationWrongOpen)
    } else {
        if (backtrack.isEmpty() || backtrack.peek().first == PunctuationToken.curlyBraces) {
            addStatement(i)
        }
    }
}


/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], a ; or a newline.
 * This doesn't actually add a token to the array, just performs validation and updates the opener token
 * with its token length.
 */
private fun closePunctuation(tType: PunctuationToken) {
    closeDollars()
    if (tType == PunctuationToken.statement) {
        closeStatement()
    } else {
        closeRegularPunctuation(tType)
    }
    i += 1
}


/**
 * The statement closer function - i.e. called for a newline or a semi-colon.
 * 1) Only close the current scope if it's a statement and non-empty. This protects against
 * newlines that aren't inside curly braces, multiple newlines and sequences like ';;;;'.
 * 2) If it closes a statement, then it also opens up a new statement scope
 */
private fun closeStatement() {
    if (backtrack.isEmpty()) return

    var top = backtrack.peek()
    if (top.first == PunctuationToken.statement) {
        top = backtrack.pop()
        setPunctuationLengths(top.second)
    }
}


private fun closeDollars() {
    var top: Pair<PunctuationToken, Int>
    while (!backtrack.empty() && backtrack.peek().first == PunctuationToken.dollar) {
        top = backtrack.pop()
        setPunctuationLengths(top.second)
    }
}


private fun closeRegularPunctuation(tType: PunctuationToken) {
    if (backtrack.empty()) {
        errorOut(errorPunctuationExtraClosing)
        return
    }
    val top = backtrack.pop()
    validateClosingPunct(tType, top.first)
    if (wasError) return
    setPunctuationLengths(top.second)
    if (tType == PunctuationToken.curlyBraces && top.first == PunctuationToken.statement) {
        // Since statements always are immediate children of curlyBraces, the '}' symbol closes
        // not just the statement but the parent curlyBraces scope, too.

        val parentScope = backtrack.pop()
        assert(parentScope.first == PunctuationToken.curlyBraces)

        setPunctuationLengths(parentScope.second)
    }
}


private fun getPrevTokenType(): Int {
    if (totalTokens == 0) return 0

    return if (nextInd > 0) {
        currChunk.tokens[nextInd - 1] shr 27
    } else {
        var curr: LexChunk? = firstChunk
        while (curr!!.next != currChunk) {
            curr = curr.next!!
        }
        curr.tokens[CHUNKSZ - 1] shr 27
    }
}


/**
 * Validation to catch unmatched closing punctuation
 */
private fun validateClosingPunct(closingType: PunctuationToken, openType: PunctuationToken) {
    when (closingType) {
        PunctuationToken.curlyBraces -> {
            if (openType != PunctuationToken.curlyBraces && openType != PunctuationToken.statement) {
                errorOut(errorPunctuationUnmatched)
                return
            }
        }
        PunctuationToken.brackets -> {
            if (openType != PunctuationToken.brackets && openType != PunctuationToken.dotBrackets) {
                errorOut(errorPunctuationUnmatched)
                return
            }
        }
        PunctuationToken.parens -> {
            if (openType != PunctuationToken.parens) {
                errorOut(errorPunctuationUnmatched)
                return
            }
        }
        PunctuationToken.statement -> {
            if (openType != PunctuationToken.statement && openType != PunctuationToken.curlyBraces) {
                errorOut(errorPunctuationUnmatched)
                return
            }
        }
        else -> {}
    }
}


/**
 * For programmatic LexResult construction (builder pattern)
 */
fun build(payload: Long, startByte: Int, lenBytes: Int, tType: RegularToken): Lexer {
    if (tType == RegularToken.comment) {
        comments.add(startByte, lenBytes)
    } else {
        appendToken(payload, startByte, lenBytes, tType)
    }
    return this
}

/**
 * For programmatic LexResult construction (builder pattern)
 */
fun build(payload: Double, startByte: Int, lenBytes: Int): Lexer {
    appendToken(payload, startByte, lenBytes)
    return this
}

/**
 * Finds the top-level punctuation opener by its index, and sets its lengths.
 * Called when the matching closer is lexed.
 */
private fun setPunctuationLengths(tokenInd: Int) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }

    val lenBytes = i - curr.tokens[j + 2]
    checkLenOverflow(lenBytes)
    curr.tokens[j    ] = totalTokens - tokenInd - 1  // lenTokens
    curr.tokens[j + 3] += (lenBytes and LOWER27BITS) // lenBytes
}


private fun ensureSpaceForToken() {
    if (nextInd < (CHUNKSZ - 4)) return

    val newChunk = LexChunk()
    currChunk.next = newChunk
    currChunk = newChunk
    nextInd = 0
}


private fun checkLenOverflow(lenBytes: Int) {
    if (lenBytes > MAXTOKENLEN) {
        errorOut(errorLengthOverflow)
    }
}


/**
 * For programmatic LexResult construction (builder pattern)
 */
fun buildPunct(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int): Lexer {
    appendToken(startByte, lenBytes, tType, lenTokens)
    return this
}


private fun errorOut(msg: String) {
    this.wasError = true
    this.errMsg = msg
}


/**
 * For programmatic LexResult construction (builder pattern)
 */
fun error(msg: String): Lexer {
    errorOut(msg)
    return this
}

/**
 * Finalizes the lexing of a single input: checks for unclosed scopes, and closes an open statement, if any.
 */
private fun finalize() {
    if (!wasError) {
        closeDollars()
        closeStatement()

        if (!backtrack.empty()) {
            errorOut(errorPunctuationExtraOpening)
        }
    }
}


private fun appendToken(payload: Long, startBytes: Int, lenBytes: Int, tType: RegularToken) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = (payload shr 32).toInt()
    currChunk.tokens[nextInd + 1] = (payload and LOWER32BITS).toInt()
    currChunk.tokens[nextInd + 2] = startBytes
    currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27) + lenBytes
    bump()
}

private fun appendToken(payload: Double, startBytes: Int, lenBytes: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    val asLong: Long = payload.toBits()
    currChunk.tokens[nextInd    ] = (asLong shr 32).toInt()
    currChunk.tokens[nextInd + 1] = (asLong and LOWER32BITS).toInt()
    currChunk.tokens[nextInd + 2] = startBytes
    currChunk.tokens[nextInd + 3] = (RegularToken.litFloat.internalVal.toInt() shl 27) + lenBytes
    bump()
}

private fun appendToken(startByte: Int, tType: PunctuationToken) {
    ensureSpaceForToken()
    currChunk.tokens[nextInd + 2] = startByte
    currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27)
    bump()
}


private fun appendToken(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = lenTokens
    currChunk.tokens[nextInd + 2] = startByte
    currChunk.tokens[nextInd + 3] = (tType.internalVal.toInt() shl 27) + lenBytes
    bump()
}


private fun bump() {
    nextInd += 4
    totalTokens += 1
}


init {
    currChunk = firstChunk
    i = 0
    nextInd = 0
    totalTokens = 0
    wasError = false
    errMsg = ""
}


companion object {
    private val dispatchTable: Array<Lexer.(inp: ByteArray) -> Unit> = Array(127) { Lexer::lexUnrecognizedSymbol }

    init {
        for (i in asciiDigit0..asciiDigit9) {
            dispatchTable[i] = Lexer::lexNumber
        }

        for (i in asciiALower..asciiZLower) {
            dispatchTable[i] = Lexer::lexWord
        }
        for (i in asciiAUpper..asciiZUpper) {
            dispatchTable[i] = Lexer::lexWord
        }
        dispatchTable[asciiUnderscore.toInt()] = Lexer::lexWord
        dispatchTable[asciiDot.toInt()] = Lexer::lexDotSomething
        dispatchTable[asciiAt.toInt()] = Lexer::lexAtWord

        for (i in operatorSymbols) {
            dispatchTable[i.toInt()] = Lexer::lexOperator
        }
        dispatchTable[asciiParenLeft.toInt()] = Lexer::lexParenLeft
        dispatchTable[asciiParenRight.toInt()] = Lexer::lexParenRight
        dispatchTable[asciiDollar.toInt()] = Lexer::lexDollar
        dispatchTable[asciiCurlyLeft.toInt()] = Lexer::lexCurlyLeft
        dispatchTable[asciiCurlyRight.toInt()] = Lexer::lexCurlyRight
        dispatchTable[asciiBracketLeft.toInt()] = Lexer::lexBracketLeft
        dispatchTable[asciiBracketRight.toInt()] = Lexer::lexBracketRight

        dispatchTable[asciiSpace.toInt()] = Lexer::lexSpace
        dispatchTable[asciiCarriageReturn.toInt()] = Lexer::lexSpace
        dispatchTable[asciiNewline.toInt()] = Lexer::lexNewline
        dispatchTable[asciiSemicolon.toInt()] = Lexer::lexStatementTerminator

        dispatchTable[asciiApostrophe.toInt()] = Lexer::lexStringLiteral
        dispatchTable[asciiQuote.toInt()] = Lexer::lexVerbatimStringLiteral
        dispatchTable[asciiSharp.toInt()] = Lexer::lexComment
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

    private fun isHexDigit(a: Byte): Boolean {
        return isDigit(a) || (a >= asciiALower && a <= asciiFLower) || (a >= asciiAUpper && a <= asciiFUpper)
    }

    /**
     * Equality comparison for lexers.
     */
    fun equality(a: Lexer, b: Lexer): Boolean {
        if (a.wasError != b.wasError || a.totalTokens != b.totalTokens || a.nextInd != b.nextInd
            || a.errMsg != b.errMsg) {
            return false
        }
        var currA: LexChunk? = a.firstChunk
        var currB: LexChunk? = b.firstChunk
        while (currA != null) {
            if (currB == null) {
                return false
            }
            val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
            for (i in 0 until len) {
                if (currA.tokens[i] != currB.tokens[i]) {
                    return false
                }
            }
            currA = currA.next
            currB = currB.next
        }

        if (a.comments.totalTokens != b.comments.totalTokens) return false
        var commA: CommentChunk? = a.comments.firstChunk
        var commB: CommentChunk? = b.comments.firstChunk
        while (commA != null) {
            if (commB == null) {
                return false
            }
            val len = if (commA == a.comments.currChunk) { a.comments.nextInd } else { COMMENTSZ }
            for (i in 0 until len) {
                if (commA.tokens[i] != commB.tokens[i]) {
                    return false
                }
            }
            commA = commA.next
            commB = commB.next
        }
        return true
    }


    /**
     * Pretty printer function for debugging purposes
     */
    fun printSideBySide(a: Lexer, b: Lexer): String {
        val result = StringBuilder()
        result.appendLine("Result | Expected")
        result.appendLine("${if (a.wasError) {a.errMsg} else { "OK" }} | ${if (b.wasError) {b.errMsg} else { "OK" }}")
        result.appendLine("tokenType [startByte lenBytes] (payload/lenTokens)")
        var currA: LexChunk? = a.firstChunk
        var currB: LexChunk? = b.firstChunk
        while (true) {
            if (currA != null) {
                if (currB != null) {
                    val lenA = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                    val lenB = if (currB == b.currChunk) { b.nextInd } else { CHUNKSZ }
                    val len = lenA.coerceAtMost(lenB)
                    for (i in 0 until len step 4) {
                        printToken(currA, i, result)
                        result.append(" | ")
                        printToken(currB, i, result)
                        result.appendLine("")
                    }
                    for (i in (len + 1) until lenA step 4) {
                        printToken(currA, i, result)
                        result.appendLine(" | ")
                    }
                    for (i in (len + 1) until lenB step 4) {
                        result.append(" | ")
                        printToken(currB, i, result)
                        result.appendLine("")
                    }
                    currB = currB.next
                } else {
                    val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                    for (i in 0 until len step 4) {
                        printToken(currA, i, result)
                        result.appendLine(" | ")
                    }
                }
                currA = currA.next
            } else if (currB != null) {
                val len = if (currB == b.currChunk) { b.nextInd } else { CHUNKSZ }
                for (i in 0 until len step 4) {
                    result.append(" | ")
                    printToken(currB, i, result)
                    result.appendLine("")
                }
                currB = currB.next
            } else {
                break
            }


        }
        return result.toString()
    }


    private fun printToken(chunk: LexChunk, ind: Int, wr: StringBuilder) {
        val startByte = chunk.tokens[ind + 2]
        val lenBytes = chunk.tokens[ind + 3] and LOWER27BITS
        val typeBits = (chunk.tokens[ind + 3] shr 27).toByte()
        if (typeBits <= 8) {
            val regType = RegularToken.values().firstOrNull { it.internalVal == typeBits }
            if (regType != RegularToken.litFloat) {
                val payload: Long = (chunk.tokens[ind].toLong() shl 32) + chunk.tokens[ind + 1].toLong()
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            } else {
                val payload: Double = Double.fromBits((chunk.tokens[ind].toLong() shl 32) + chunk.tokens[ind + 1].toLong())
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            }

        } else {
            val punctType = PunctuationToken.values().firstOrNull { it.internalVal == typeBits }
            val lenTokens = chunk.tokens[ind]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        }
    }
}

}