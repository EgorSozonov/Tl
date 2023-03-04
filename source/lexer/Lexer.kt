package lexer
import language.*
import java.lang.StringBuilder
import java.util.*
import utils.*

class Lexer(val inp: ByteArray, val fileType: FileType) {
    
    
/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * tType              | u6
 * startByte          | u26
 * lenBytes           | i32
 * payload1           | i32
 * payload2           | i32
 */
private var cap = INIT_LIST_SIZE*4
var currTokInd = 0
var tokens = IntArray(INIT_LIST_SIZE*4)

var nextInd: Int                                             // Next ind inside the current token array
    private set

var totalTokens: Int
    private set
var wasError: Boolean                                        // The lexer's error flag
    private set
var errMsg: String
    private set
private var i: Int                                           // current index inside input byte array

private val backtrack = Stack<LexFrame>() // The stack of [tokPunctuation startTokInd wasOriginallyColon]
private val numeric: LexerNumerics = LexerNumerics()
private val newLines: ArrayList<Int> = ArrayList<Int>(50)


/**
 * Main lexer method
 */
fun lexicallyAnalyze() {
    try {
        if (inp.isEmpty()) {
            exitWithError("Empty input")
        }

        if (inp.size >= 67_108_864) { // 2^26 is the largest byte index representable in a token
            exitWithError("Source code file size is too big, 2^26 bytes is the largest allowed size")
        }
        i = -1
        // Check for UTF-8 BOM at start of file
        if (inp.size >= 3 && inp[0] == 0xEF.toByte() && inp[1] == 0xBB.toByte() && inp[2] == 0xBF.toByte()) {
            i = 2
        }

        // Main loop over the input
        while (i < inp.size) {
            guaranteeAStatement()
            dispatchTable[inp[i].toInt()]()
        }

        finalize()
    } catch (e: Exception) {
        println()
    }
}

/** Guarantees that all tokens are wrapped up at least into a statement */
private fun guaranteeAStatement() {
    if (backtrack.isNotEmpty()) return;
    openPunctuation(tokStmt)
}


private fun lexWord() {
    lexWordInternal(tokWord)
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Examples of acceptable tokens: a.b.c.D, asdf123, ab._Cd45
 * Examples of unacceptable tokens:
 *     A.b.c.d (bad because only last chunk may be capitalized),
 *     1asdf23 (bad because starts with digit),
 *     ab.cd_45 (bad because snake case).
 */
private fun lexWordInternal(wordType: Int) {
    val startInd = i
    var wasCapitalized = lexWordChunk()
    while (i < (inp.size - 1) && inp[i] == aDot && inp[i + 1] != aBracketLeft) {
        i += 1
        val isCurrCapitalized = lexWordChunk()
        if (wasCapitalized && isCurrCapitalized) {
            exitWithError(errorWordCapitalizationOrder)
        }
        wasCapitalized = isCurrCapitalized
    }
    val isAlsoAccessor = (i < inp.size && inp[i] == aBracketLeft) // data accessor like a[5]

    val realStartInd = if (wordType == tokWord) { startInd } else {startInd - 1} // accounting for the @ at the start
    val paylCapitalized = if (wasCapitalized) { 1 } else { 0 }

    val firstByte = inp[startInd]
    val lenBytes = i - realStartInd
    if (wordType != tokWord && isAlsoAccessor) exitWithError(errorWordWrongAccessor)
    if (wordType == tokAtWord || firstByte < aALower || firstByte > aTLower) {
        appendToken(wordType, paylCapitalized, realStartInd, lenBytes)
        if (isAlsoAccessor) {
            openPunctuation(tokAccessor)
        }
    } else {
        val mbReservedWord = possiblyReservedDispatch[(firstByte - aALower)](startInd, i - startInd)
        if (mbReservedWord > 0) {
            if (isAlsoAccessor) exitWithError(errorWordWrongAccessor)
            if (mbReservedWord == reservedTrue) {
                appendToken(tokBool, 1, realStartInd, lenBytes)
            } else if (mbReservedWord == reservedFalse) {
                appendToken(tokBool, 0, realStartInd, lenBytes)
            } else {
                lexReservedWord(mbReservedWord, realStartInd)
            }
        } else  {
            appendToken(wordType, paylCapitalized, realStartInd, lenBytes)
            if (isAlsoAccessor) {
                openPunctuation(tokAccessor)
            }
        }
    }
}

/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns if the lexed chunk was capitalized.
 */
private fun lexWordChunk(): Boolean {
    var result = false

    if (inp[i] == aUnderscore) {
        checkPrematureEnd(2, inp)
        i++
    } else {
        checkPrematureEnd(1, inp)
    }

    if (isCapitalLetter(inp[i])) {
        result = true
    } else if (!isLowercaseLetter(inp[i])) {
        exitWithError(errorWordChunkStart)
    }
    i++

    while (i < inp.size && isAlphanumeric(inp[i])) {
        i++
    }
    if (i < inp.size && inp[i] == aUnderscore) {
        exitWithError(errorWordUnderscoresOnlyAtStart)
    }

    return result
}



private fun lexAtWord() {
    checkPrematureEnd(2, inp)
    if (wasError) return

    i += 1
    lexWordInternal(tokAtWord)
}


/**
 * Lexes an integer literal, a hex integer literal, a binary integer literal, or a floating literal.
 * This function can handle being called on the last byte of input.
 */
private fun lexNumber() {
    val cByte = inp[i]
    if (i == inp.size - 1 && isDigit(cByte)) {
        appendLitIntToken((cByte - aDigit0).toLong(), i, 1, )
        i++
        return
    }

    when (inp[i + 1]) {
        aXLower -> { lexHexNumber() }
        aBLower -> { lexBinNumber() }
        else ->    { lexDecNumber(false) }
    }
    numeric.clear()
}

/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO: add support for the '1.23E4' format
 */
private fun lexDecNumber(isNegative: Boolean) {
    var j = if (isNegative) { i + 1 } else { i }
    var digitsAfterDot = 0 // this is relative to first digit, so it includes the leading zeroes
    var metDot = false
    var metNonzero = false
    val maximumInd = (i + 40).coerceAtMost(inp.size)
    while (j < maximumInd) {
        val cByte = inp[j]

        if (isDigit(cByte)) {
            if (metNonzero) {
                numeric.add((cByte - aDigit0).toByte())
            } else if (cByte != aDigit0) {
                metNonzero = true
                numeric.add((cByte - aDigit0).toByte())
            }
            if (metDot) { digitsAfterDot++ }
        } else if (cByte == aUnderscore) {
            if (j == (inp.size - 1) || !isDigit(inp[j + 1])) {
                exitWithError(errorNumericEndUnderscore)
            }
        } else if (cByte == aDot) {
            if (metDot) {
                exitWithError(errorNumericMultipleDots)
            }
            metDot = true
        } else {
            break
        }
        j++
    }

    if (j < inp.size && isDigit(inp[j])) {
        exitWithError(errorNumericWidthExceeded)
    }

    if (metDot) {
        val resultValue = numeric.calcFloating(-digitsAfterDot) ?: exitWithError(errorNumericFloatWidthExceeded)

        appendFloatToken(if (isNegative) { -resultValue } else { resultValue }, i, j - i)
    } else {
        val resultValue = numeric.calcInteger() ?: exitWithError(errorNumericIntWidthExceeded)
        appendLitIntToken(if (isNegative) { -resultValue } else { resultValue }, i, j - i)
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
private fun lexHexNumber() {
    checkPrematureEnd(2, inp)
    numeric.clear()
    var j = i + 2
    while (j < inp.size) {
        val cByte = inp[j]
        if (isDigit(cByte)) {

            numeric.add((cByte - aDigit0).toByte())
        } else if ((cByte >= aALower && cByte <= aFLower)) {
            numeric.add((cByte - aALower + 10).toByte())
        } else if ((cByte >= aAUpper && cByte <= aFUpper)) {
            numeric.add((cByte - aAUpper + 10).toByte())
        } else if (cByte == aUnderscore) {
            if (j == inp.size - 1 || isHexDigit(inp[j + 1])) {
                exitWithError(errorNumericEndUnderscore)
            }
        } else {
            break
        }
        if (numeric.ind > 16) {
            exitWithError(errorNumericBinWidthExceeded)
        }
        j++
    }
    val resultValue = numeric.calcHexNumber()
    appendLitIntToken(resultValue, i, j - i)
    numeric.clear()
    i = j
}

/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO add floating-point literals like 0b0101.
 */
private fun lexBinNumber() {
    numeric.clear()

    var j = i + 2
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == aDigit0) {
            numeric.add(0)
        } else if (cByte == aDigit1) {
            numeric.add(1)
        } else if (cByte == aUnderscore) {
            if ((j == inp.size - 1 || (inp[j + 1] != aDigit0 && inp[j + 1] != aDigit1))) {
                exitWithError(errorNumericEndUnderscore)
            }
        } else {
            break
        }
        if (numeric.ind > 64) {
            exitWithError(errorNumericBinWidthExceeded)
        }
        j++
    }
    if (j == i + 2) {
        exitWithError(errorNumericEmpty)
    }

    val resultValue = numeric.calcBinNumber()
    appendLitIntToken(resultValue, i, j - i)
    i = j
}


private fun lexOperator() {
    val firstSymbol = inp[i]
    val secondSymbol = if (inp.size > i + 1) { inp[i + 1] } else { 0 }
    val thirdSymbol = if (inp.size > i + 2) { inp[i + 2] } else { 0 }
    val fourthSymbol = if (inp.size > i + 3) { inp[i + 3] } else { 0 }
    var k = 0
    var opType = -1 // corresponds to the opT... operator types
    while (k < operatorDefinitions.size && operatorDefinitions[k].bytes[0] != firstSymbol) {
        k++
    }
    while (k < operatorDefinitions.size && operatorDefinitions[k].bytes[0] == firstSymbol) {
        val opDef = operatorDefinitions[k]
        val secondTentative = opDef.bytes[1]
        if (secondTentative != byte0 && secondTentative != secondSymbol) {
            k++
            continue
        }
        val thirdTentative = opDef.bytes[2]
        if (thirdTentative != byte0 && thirdTentative != thirdSymbol) {
            k++
            continue
        }
        val fourthTentative = opDef.bytes[3]
        if (fourthTentative != byte0 && fourthTentative != fourthSymbol) {
            k++
            continue
        }
        opType = k
        break
    }
    if (opType < 0) exitWithError(errorOperatorUnknown)

    val opDef = operatorDefinitions[opType]
    val isExtensible = operatorDefinitions[k].extensible
    var isExtensionAssignment = 0

    val lengthOfBaseOperator = when (byte0) {
        opDef.bytes[1] -> { 1 }
        opDef.bytes[2] -> { 2 }
        opDef.bytes[3] -> { 3 }
        else           -> { 4 }
    }
    var j = i + lengthOfBaseOperator
    if (isExtensible) {
        if (j < inp.size && inp[j] == aDot) {
            isExtensionAssignment += 2
            j++
        }
    }
    if (isExtensible || operatorDefinitions[k].assignable) {
        if (j < inp.size && inp[j] == aEqual) {
            isExtensionAssignment += 1
            j++
        }
    }

    if (opType == opTImmDefinition || opType == opTMutation || (isExtensionAssignment and 1) > 0) {
        processAssignmentOperator(opType, isExtensionAssignment)
    } else {
        appendOperator(opType, isExtensionAssignment, i, j - i)
    }

    i = j
}


private fun lexLambda() {
    val j = i + 1
    if (j < inp.size && inp[j] == aAsterisk) {
        lexGenerator()
    } else {
        i++
        val top = backtrack.peek()
        setSpanLambda(top.startTokInd, tokStmtLambda)
        backtrack.push(LexFrame(tokStmt, i))
        appendPunctuation(tokStmt, i)
    }
}


private fun lexGenerator() {
    i += 2
    val top = backtrack.peek()
    setSpanLambda(top.startTokInd, tokStmtGenerator)
    backtrack.push(LexFrame(tokStmt, i))
    appendPunctuation(tokStmt, i)
}


private fun lexMinus() {
    val j = i + 1
    if (j < inp.size && isDigit(inp[j])) {
        lexDecNumber(true)
        numeric.clear()
    } else {
        lexOperator()
    }
}

/**
 * Lexer action for assignment operators. Implements the validations 2 and 3 from Syntax.txt
 */
private fun processAssignmentOperator(opType: Int, isExtensionAssignment: Int) {
    val currSpan = backtrack.last()

    if (currSpan.tokType == tokStmtAssignment) {
        exitWithError(errorOperatorMultipleAssignment)
    } else if (currSpan.tokType != tokStmt) {
        exitWithError(errorOperatorAssignmentPunct)
    }
    validateNotInsideTypeDecl()
    convertGrandparentToScope()

    setSpanAssignment(currSpan.startTokInd, isExtensionAssignment, opType)
    backtrack[backtrack.size - 1] = LexFrame(tokStmtAssignment, currSpan.startTokInd)
}

/**
 * Lexer action for a reserved word. If at the start of a statement/expression, it mutates the type of said expression,
 * otherwise it emits a corresponding token.
 * Implements the validation 2 from Syntax.txt
 */
private fun lexReservedWord(reservedWordType: Int, startInd: Int) {
    if (reservedWordType == tokStmtBreak) {
        appendPunctuationFull(tokStmtBreak, 0, 0, startInd, 5)
        return
    }
    if (backtrack.isEmpty() || backtrack.last().startTokInd < (totalTokens - 1)) {
        appendToken(tokReserved, reservedWordType, startInd, i - startInd)
        return
    }

    val currSpan = backtrack.last()
    if (backtrack.last().startTokInd < (totalTokens - 1) || backtrack.last().tokType >= firstCoreFormTok) {
        // if this is not the first token inside this span, or if it's already been converted to core form, add
        // the reserved word as a token
        appendToken(tokReserved, reservedWordType, startInd, i - startInd)
        return
    }
    validateNotInsideTypeDecl()

    setSpanCore(currSpan.startTokInd, reservedWordType)
    if (reservedWordType == tokStmtReturn) {
        convertGrandparentToScope()
    }
    backtrack[backtrack.size - 1] = LexFrame(reservedWordType, currSpan.startTokInd)
}


private fun convertGrandparentToScope() {
    val indPenultimate = backtrack.size - 2
    if (indPenultimate > -1 && backtrack[indPenultimate].tokType != tokLexScope) {
        backtrack[indPenultimate].tokType = tokLexScope
        setSpanType(backtrack[indPenultimate].startTokInd, tokLexScope)
    }
}

/** Validation 2 from Syntax.txt. Since type declarations may contain only simple expressions
 * (i.e. nothing but expressions nested inside of them) then we need to check whether we
 * are inside a type declaration right now. */
private fun validateNotInsideTypeDecl() {

}

private fun lexParenLeft() {
    openPunctuation(tokParens)
}

private fun lexParenRight() {
    closeRegularPunctuation(tokParens)
}

private fun lexBracketLeft() {
    openPunctuation(tokBrackets)
}

private fun lexBracketRight() {
    closeRegularPunctuation(tokBrackets)
}

private fun lexSpace() {
    i += 1
}

private fun lexNewline() {
    newLines.add(i)
    i++
}

private fun lexDot() {
    i++
    // TODO
}

/**
 * String literals look like 'wasn''t' and may contain arbitrary UTF-8.
 * The insides of the string have two escape sequences: '' for apostrophe and \n for newlines
 * TODO probably need to count UTF-8 codepoints, or worse - grapheme clusters - in order to correctly report to LSP
 */
private fun lexStringLiteral() {
    var j = i + 1
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == aQuote) {
            appendToken(tokString, 0, i + 1, j - i - 1)
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    exitWithError(errorPrematureEndOfInput)
}

private fun lexUnrecognizedSymbol() {
    exitWithError(errorUnrecognizedByte)
}

/**
 * Doc-comments start with ;; and are included as a single token. Simple comments start with ; and are excluded from the lexing result.
 * All comments go until the end of line.
 */
private fun lexComment() {
    var j = i + 1
    if (j < inp.size && inp[j] == aSharp) {
        lexDocComment()
        return
    }
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == aNewline) {
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    i = j
}

private fun lexNonAscii() {
    exitWithError(errorNonAscii)
}

/** Colon = parens until the end of current subexpression (so Tl's colon is equivalent to Haskell's dollar) */
private fun lexColon() {
    if (i + 1 < inp.size && inp[i + 1] == aEqual) {
        lexOperator()
    } else {
        openPunctuation(tokColonOpened)
    }
}

/**
 *  Doc comments, syntax is ;; The comment
 */
private fun lexDocComment() {
    val startByte = i + 2 // +2 for the '##'
    for (j in startByte until inp.size) {
        if (inp[j] == aNewline) {
            newLines.add(j)
            if (j > startByte) {
                appendDocComment(startByte, j - 1)
            }
            i = j + 1
            return
        }
    }
    i = inp.size
    appendDocComment(startByte, inp.size - 1)
}

/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private fun checkPrematureEnd(requiredSymbols: Int, inp: ByteArray) {
    if (i > inp.size - requiredSymbols) {
        exitWithError(errorPrematureEndOfInput)
    }
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
private fun openPunctuation(tType: Int) {
    backtrack.add(LexFrame(tType, totalTokens, tType == tokColonOpened))
    appendPunctuation(tType, i + 1)
    i++
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], or a ;.
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private fun closeRegularPunctuation(closingType: Int) {
    closeColons()
    if (backtrack.empty()) {
        exitWithError(errorPunctuationExtraClosing)
    }

    val top = backtrack.pop()
    validateClosingPunct(closingType, top.tokType)

    setSpanLength(top.startTokInd)

    i++
}

/** For all the colons at the top of the backtrack, turns them into parentheses, sets their lengths and closes them */
private fun closeColons() {
    while (backtrack.isNotEmpty() && backtrack.peek().wasOriginallyColon) {
        val top = backtrack.pop()
        val tokenInd = top.startTokInd
        val j = tokenInd * 4
        tokens[j    ] = (tokens[j] and LOWER26BITS) + (tokParens shl 26)
        tokens[j + 1] = i - (tokens[j] and LOWER26BITS) // lenBytes
        tokens[j + 3] = totalTokens - tokenInd - 1  // lenTokens
    }
}

/**
 * Validation to catch unmatched closing punctuation
 */
private fun validateClosingPunct(closingType: Int, openType: Int) {
    when (closingType) {
        tokParens -> {
            if (openType < firstCoreFormTok && openType != tokParens && openType != tokColonOpened) {
                exitWithError(errorPunctuationUnmatched)
            }
        }
        tokBrackets -> {
            if (openType != tokBrackets && openType != tokAccessor) exitWithError(errorPunctuationUnmatched)
        }
        tokStmt -> {
            if (openType != tokStmt && openType != tokStmtAssignment) exitWithError(errorPunctuationUnmatched)
        }
        else -> {}
    }
}

/**
 * For programmatic Lexer result construction (builder pattern). Regular tokens
 */
fun build(tType: Int, payload2: Int, startByte: Int, lenBytes: Int): Lexer {
    appendToken(tType, payload2, startByte, lenBytes)
    return this
}

/**
 * For programmatic Lexer result construction (builder pattern). Any token
 */
fun buildAll(i1: Int, i2: Int, i3: Int, i4: Int): Lexer {
    add(i1, i2, i3, i4)
    return this
}

/**
 * For programmatic Lexer result construction (builder pattern). Regular tokens
 */
fun buildOperator(opType: Int, isExtended: Boolean, isAssignment: Boolean, startByte: Int, lenBytes: Int): Lexer {
    var operData = if (isExtended) { 2 } else { 0 }
    if (isAssignment) { operData++ }
    appendOperator(opType, operData, startByte, lenBytes)
    return this
}

/**
 * For programmatic Lexer result construction (builder pattern). Literal ints
 */
fun buildLitInt(payload: Long, startByte: Int, lenBytes: Int): Lexer {
    appendLitIntToken(payload, startByte, lenBytes)
    return this
}

/**
 * For programmatic Lexer result construction (builder pattern). Literal floats
 */
fun buildLitFloat(payload: Double, startByte: Int, lenBytes: Int): Lexer {
    appendFloatToken(payload, startByte, lenBytes)
    return this
}

/**
 * Finds the top-level punctuation opener by its index, and sets its lengths.
 * Called when the matching closer is lexed.
 */
private fun setSpanLength(tokenInd: Int) {
    var j = tokenInd * 4
    tokens[j + 1] = i - (tokens[j] and LOWER26BITS) // lenBytes
    tokens[j + 3] = totalTokens - tokenInd - 1  // lenTokens
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 * Also stores the number of parentheses wrapping the assignment operator in the unused +2nd
 * slot, to be used for validation when the statement will be closed.
 */
private fun setSpanType(tokenInd: Int, tType: Int) {
    val j = tokenInd * 4
    tokens[j    ] = tType shl 26
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 */
private fun setSpanCore(tokenInd: Int, tType: Int) {
    val j = tokenInd * 4
    tokens[j    ] = tType shl 26
}


private fun setSpanAssignment(tokenInd: Int, isExtensionAssignment: Int, opType: Int) {
    val j = tokenInd * 4
    tokens[j    ] = (tokens[j] and LOWER26BITS) + (tokStmtAssignment shl 26)
    tokens[j + 2] = isExtensionAssignment + (opType shl 2)
}


private fun setSpanLambda(tokenInd: Int, spanType: Int) {
    val j = tokenInd * 4
    tokens[j] = (tokens[j] and LOWER26BITS) + (spanType shl 26)
}

/** Tests if the following several bytes in the input match an array. This is for reserved word detection */
private fun testByteSequence(startByte: Int, letters: ByteArray): Boolean {
    if (startByte + letters.size > inp.size) return false

    for (j in (letters.size - 1) downTo 0) {
        if (inp[startByte + j] != letters[j]) return false
    }
    return true
}

/**
 * For programmatic Lexer result construction (builder pattern)
 */
fun buildPunctPayload(tType: Int, payload1: Int, lenTokens: Int, startByte: Int, lenBytes: Int, ): Lexer {
    add((tType shl 26) + startByte, lenBytes, payload1, lenTokens)
    return this
}

/**
 * For programmatic Lexer result construction (builder pattern)
 */
fun bError(msg: String): Lexer {
    wasError = true
    errMsg = msg
    return this
}

/**
 * Finalizes the lexing of a single input: checks for unclosed scopes, and closes an open statement, if any.
 */
private fun finalize() {
    if (backtrack.empty()) return
    closeColons()
    val top = backtrack.peek().tokType
    if (backtrack.size == 1 && (top == tokStmt || top == tokStmtAssignment)) {
        closeRegularPunctuation(tokStmt)
    } else {
        exitWithError(errorPunctuationExtraOpening)
    }
}

/** Append a regular (non-punctuation) token */
private fun appendToken(tType: Int, payload2: Int, startByte: Int, lenBytes: Int) {
    add((tType shl 26) + startByte, lenBytes, 0, payload2)
}

/** Append an operator token */
private fun appendOperator(opType: Int, isExtensionAssignment: Int, startByte: Int, lenBytes: Int) {
    add((tokOperator shl 26) + startByte, lenBytes, isExtensionAssignment + (opType shl 2), 0)
}

/**
 * Appends a doc comment token if it's the first one, or elongates the existing token if we've just had one already.
 * This logic handles multiple consecutive lines of doc comments.
 * NOTE: this is the only function that needs the endByte instead of lenBytes!
 */
private fun appendDocComment(startByte: Int, endByte: Int) {
    if (nextInd == 0 || tokens[nextInd - 4] ushr 26 != tokDocComment) {
        add((tokDocComment shl 26) + startByte, endByte - startByte + 1, 0, 0)
    } else {
        tokens[nextInd - 4] = (tokDocComment shl 26) + endByte - tokens[nextInd - 3] + 1
    }
}

/** Append a 64-bit literal int token */
private fun appendLitIntToken(payload: Long, startByte: Int, lenBytes: Int) {
    add((tokInt shl 26) + startByte, lenBytes, (payload shr 32).toInt(), (payload and LOWER32BITS).toInt())
}

/** Append a floating-point literal */
private fun appendFloatToken(payload: Double, startByte: Int, lenBytes: Int) {
    val asLong: Long = payload.toBits()
    add((tokFloat shl 26) + startByte, lenBytes, (asLong shr 32).toInt(), (asLong and LOWER32BITS).toInt())
}

/** Append a punctuation token */
private fun appendPunctuation(tType: Int, startByte: Int) {
    add((tType shl 26) + startByte, 0, 0, 0)
}

/** Append a punctuation token when it's fully known already */
private fun appendPunctuationFull(tType: Int, payload1: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    add((tType shl 26) + startByte, lenBytes, payload1, lenTokens)
}

/** The programmatic/builder method for appending a punctuation token */
fun buildPunctuation(tType: Int, lenTokens: Int, startByte: Int, lenBytes: Int): Lexer {
    appendPunctuationFull(tType, 0, lenTokens, startByte, lenBytes)
    return this
}


fun nextToken() {
    currTokInd++
}
    

fun currTokenType(): Int {
    return tokens[currTokInd*4] ushr 26
}


fun currLenBytes(): Int {
    return tokens[currTokInd*4 + 1]
}

fun currStartByte(): Int {
    return tokens[currTokInd*4] and LOWER26BITS
}

fun currPayload1(): Int {
    return tokens[currTokInd*4 + 2]
}

fun currPayload2(): Int {
    return tokens[currTokInd*4 + 3]
}


fun lookAhead(skip: Int): TokenLite {
    val nextInd = 4*(currTokInd + skip)
    return TokenLite(tokens[nextInd] ushr 26,
                     (tokens[nextInd + 2].toLong() shl 32) + tokens[nextInd + 3].toLong())
}

/**
 * Pretty printer function for debugging purposes
 */
fun toDebugString(): String {
    val result = StringBuilder()
    result.appendLine("Lex result")
    result.appendLine(if (wasError) {errMsg} else { "OK" })
    result.appendLine("tokenType [startByte lenBytes] (payload/lenTokens)")

    for (i in 0 until totalTokens) {
        result.append((i).toString() + " ")
        printToken(tokens, i*4, result)
        result.appendLine("")
    }
    return result.toString()
}

private fun determineReservedA(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesAlias)) return tokStmtAlias
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesAwait)) return tokStmtAwait
    return 0
}

    
private fun determineReservedB(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesBreak)) return tokStmtBreak
    return 0
}


private fun determineReservedC(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesCatch)) return tokStmtCatch
    if (lenBytes == 8 && testByteSequence(startByte, reservedBytesContinue)) return tokStmtContinue
    return 0
}


private fun determineReservedE(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesExport)) return tokStmtExport
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesEmbed)) return tokStmtEmbed
    return 0
}


private fun determineReservedF(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesFalse)) return reservedFalse
    if (lenBytes == 3 && testByteSequence(startByte, reservedBytesFor)) return tokStmtFor
    return 0
}


private fun determineReservedI(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 2 && testByteSequence(startByte, reservedBytesIf)) return tokStmtIf
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesIfEq)) return tokStmtIfEq
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesIfPr)) return tokStmtIfPr
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesImpl)) return tokStmtImpl
    return 0
}


private fun determineReservedM(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesMatch)) return tokStmtMatch
    if (lenBytes == 3 && testByteSequence(startByte, reservedBytesMut)) return tokStmtMut
    return 0
}


private fun determineReservedN(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 10 && testByteSequence(startByte, reservedBytesNodestruct)) return tokStmtNodestruct
    return 0
}


private fun determineReservedR(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesReturn)) return tokStmtReturn
    return 0
}

private fun determineReservedS(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesReturn)) return tokStmtStruct
    return 0
}

private fun determineReservedT(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesTrue)) return reservedTrue
    if (lenBytes == 3 && testByteSequence(startByte, reservedBytesTry)) return tokStmtTry
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesType)) return tokStmtType
    return 0
}

private fun determineReservedY(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesYield)) return tokStmtYield
    return 0
}

private fun determineUnreserved(startByte: Int, lenBytes: Int): Int {
    return 0
}

private fun exitWithError(msg: String): Nothing {
    wasError = true
    errMsg = msg
    throw Exception(msg)
}

fun add(i1: Int, i2: Int, i3: Int, i4: Int) {

    if ((nextInd + 4) == cap) {
        val newArray = IntArray(2*cap)
        tokens.copyInto(newArray, 0, cap)
        cap *= 2
    }
    tokens[nextInd    ] = i1
    tokens[nextInd + 1] = i2
    tokens[nextInd + 2] = i3
    tokens[nextInd + 3] = i4
    nextInd += 4
    totalTokens++
}

init {
    i = 0
    nextInd = 0
    totalTokens = 0
    wasError = false
    errMsg = ""
}


companion object {
    private val dispatchTable: Array<Lexer.() -> Unit> = Array(256) { Lexer::lexUnrecognizedSymbol }

    init {
        for (i in aDigit0..aDigit9) {
            dispatchTable[i] = Lexer::lexNumber
        }

        for (i in aALower..aZLower) {
            dispatchTable[i] = Lexer::lexWord
        }
        for (i in aAUpper..aZUpper) {
            dispatchTable[i] = Lexer::lexWord
        }
        dispatchTable[aUnderscore.toInt()] = Lexer::lexWord

        dispatchTable[aAt.toInt()] = Lexer::lexAtWord

        for (i in operatorStartSymbols) {
            dispatchTable[i.toInt()] = Lexer::lexOperator
        }
        dispatchTable[aMinus.toInt()] = Lexer::lexMinus
        dispatchTable[aParenLeft.toInt()] = Lexer::lexParenLeft
        dispatchTable[aParenRight.toInt()] = Lexer::lexParenRight
        dispatchTable[aColon.toInt()] = Lexer::lexColon
        dispatchTable[aBracketLeft.toInt()] = Lexer::lexBracketLeft
        dispatchTable[aBracketRight.toInt()] = Lexer::lexBracketRight

        dispatchTable[aSpace.toInt()] = Lexer::lexSpace
        dispatchTable[aCarriageReturn.toInt()] = Lexer::lexSpace
        dispatchTable[aNewline.toInt()] = Lexer::lexNewline
        dispatchTable[aDot.toInt()] = Lexer::lexDot

        dispatchTable[aBackslash.toInt()] = Lexer::lexLambda

        dispatchTable[aQuote.toInt()] = Lexer::lexStringLiteral
        dispatchTable[aSharp.toInt()] = Lexer::lexComment

        for (i in 128 until 256) {
            dispatchTable[i] = Lexer::lexNonAscii
        }
    }

    /**
     * Table for dispatch on the first letter of a word in case it might be a reserved word.
     * It's indexed on the diff between first byte and the letter "c" (the earliest letter a reserved word may start with)
     */
    private val possiblyReservedDispatch: Array<Lexer.(Int, Int) -> Int> = Array(25) {
        i -> when(i) {
            0 -> Lexer::determineReservedA
            1 -> Lexer::determineReservedB
            2 -> Lexer::determineReservedC
            4 -> Lexer::determineReservedE
            5 -> Lexer::determineReservedF
            8 -> Lexer::determineReservedI
            12 -> Lexer::determineReservedM
            13 -> Lexer::determineReservedN
            17 -> Lexer::determineReservedR
            18 -> Lexer::determineReservedS
            19 -> Lexer::determineReservedT
            24 -> Lexer::determineReservedY
            else -> Lexer::determineUnreserved
        }
    }

    private fun isLetter(a: Byte): Boolean {
        return ((a >= aALower && a <= aZLower) || (a >= aAUpper && a <= aZUpper))
    }

    private fun isCapitalLetter(a: Byte): Boolean {
        return a >= aAUpper && a <= aZUpper
    }

    private fun isLowercaseLetter(a: Byte): Boolean {
        return a >= aALower && a <= aZLower
    }

    private fun isAlphanumeric(a: Byte): Boolean {
        return isLetter(a) || isDigit(a)
    }

    private fun isDigit(a: Byte): Boolean {
        return a >= aDigit0 && a <= aDigit9
    }

    private fun isHexDigit(a: Byte): Boolean {
        return isDigit(a) || (a >= aALower && a <= aFLower) || (a >= aAUpper && a <= aFUpper)
    }

    /** Predicate for an element of a scope, which can be any type of statement, or a split statement (such as an "if" or "match" form) */
    fun isParensElement(tType: Int): Boolean {
        return  tType == tokLexScope || tType == tokStmtTypeDecl || tType == tokParens
                || tType == tokStmtAssignment
    }

    /** Predicate for an element of a scope, which can be any type of statement, or a split statement (such as an "if" or "match" form) */
    fun isScopeElementVal(tType: Int): Boolean {
        return tType == tokLexScope
                || tType == tokStmtTypeDecl
                || tType == tokParens
                || tType == tokStmtAssignment
    }

    /**
     * Equality comparison for lexers.
     */
    fun equality(a: Lexer, b: Lexer): Boolean {
        if (a.wasError != b.wasError || a.totalTokens != b.totalTokens || !a.errMsg.endsWith(b.errMsg)) {
            return false
        }
        val intLen = a.totalTokens*4
        for (i in 0 until intLen) {
            if (a.tokens[i] != b.tokens[i]) {
                return false
            }
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

        val len = a.totalTokens.coerceAtMost(b.totalTokens)
        for (i in 0 until len) {
            printToken(a.tokens, i*4, result)
            result.append(" | ")
            printToken(b.tokens, i*4, result)
            result.appendLine("")
        }
        for (i in len until a.totalTokens) {
            printToken(a.tokens, i*4, result)
            result.appendLine(" | ")
        }
        for (i in len until b.totalTokens) {
            result.append(" | ")
            printToken(b.tokens, i, result)
            result.appendLine("")
        }

        return result.toString()
    }


    private fun printToken(tokens: IntArray, ind: Int, wr: StringBuilder) {
        val startByte = tokens[ind] and LOWER26BITS
        val lenBytes = tokens[ind + 1]
        val typeBits = (tokens[ind] ushr 26)
        val tokTypeName = tokNames[typeBits]
        if (typeBits < firstPunctTok) {

            if (typeBits == tokFloat) {
                val payload: Double = Double.fromBits(
                    (tokens[ind + 2].toLong() shl 32) + tokens[ind + 3].toLong()
                )
                wr.append("$tokTypeName $payload [${startByte} ${lenBytes}]")
            } else if (typeBits == tokOperator) {
                val opType = tokens[ind + 2] ushr 2
                val opExtAssignment = tokens[ind + 2] and 3

                val payloadStr = operatorDefinitions[opType].name + (if ((opExtAssignment ushr 1) == 1) { "." } else { "" }) +
                        (if ((opExtAssignment and 1) == 1) { "=" } else {""})
                wr.append("$tokTypeName $payloadStr [${startByte} ${lenBytes}]")
            } else {
                val payload: Long = (tokens[ind + 2].toLong() shl 32) + (tokens[ind + 3].toLong() and LOWER32BITS)
                wr.append("$tokTypeName $payload [${startByte} ${lenBytes}]")
            }
        } else {
            val lenTokens = tokens[ind + 3]
            val payload1 = tokens[ind + 2]
            if (typeBits == tokStmtAssignment) {
                val opType = payload1 ushr 2

                val payloadStr = operatorDefinitions[opType].name + (if (payload1 and 2 == 2) { "." } else { "" }) +
                        (if (payload1 and 1 == 1) { "=" } else {""})
                wr.append("$tokTypeName $lenTokens [${startByte} ${lenBytes}] $payloadStr")
            } else {
                wr.append("$tokTypeName $lenTokens [${startByte} ${lenBytes}] ${if (payload1 != 0) { payload1.toString() } else {""} }")
            }

        }
    }
}

}