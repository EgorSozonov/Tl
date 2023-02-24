package lexer
import language.*
import java.lang.StringBuilder
import java.util.*
import utils.*

class Lexer(val inp: ByteArray, val fileType: FileType) {
    
    
/**
 * The real element of this array is struct Token - modeled as 4 32-bit ints
 * tType                                                                              | u5
 * lenBytes                                                                           | u27
 * startByte                                                                          | i32
 * payload (for regular tokens) or lenTokens + empty 32 bits (for punctuation tokens) | i64
 */
private var cap = INIT_LIST_SIZE*4
var currInd = 0
    private set
private var tokens = IntArray(INIT_LIST_SIZE*4)

fun add(i1: Int, i2: Int, i3: Int, i4: Int) {
    currInd += 4
    if (currInd == cap) {
        val newArray = IntArray(2*cap)
        tokens.copyInto(newArray, 0, cap)
        cap *= 2
    }
    tokens[currInd    ] = i1
    tokens[currInd + 1] = i2
    tokens[currInd + 2] = i3
    tokens[currInd + 3] = i4
}
var nextInd: Int                                             // Next ind inside the current token array
    private set

var totalTokens: Int
    private set
var wasError: Boolean                                        // The lexer's error flag
    private set
var errMsg: String
    private set
private var i: Int                                           // current index inside input byte array

private val backtrack = Stack<Pair<Int, Int>>() // The stack of [tokPunctuation startTokInd]
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

        // Check for UTF-8 BOM at start of file
        if (inp.size >= 3 && inp[0] == 0xEF.toByte() && inp[1] == 0xBB.toByte() && inp[2] == 0xBF.toByte()) {
            i = 3
        }

        // Main loop over the input
        while (i < inp.size && !wasError) {
            val cByte = inp[i]
            if (cByte >= 0) {
                dispatchTable[cByte.toInt()]()
            } else {
                exitWithError(errorNona)
            }
        }

        finalize()
    } catch (e: Exception) {
        println()
    }
}


private fun lexWord() {
    lexWordInternal(tokWord)
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Examples of acceptable expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of unacceptable expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private fun lexWordInternal(wordType: Int) {
    val startInd = i
    var metUncapitalized = lexWordChunk()
    while (i < (inp.size - 1) && inp[i] == aDot && inp[i + 1] != aBracketLeft && !wasError) {
        i += 1
        val isCurrUncapitalized = lexWordChunk()
        if (metUncapitalized && !isCurrUncapitalized) {
            exitWithError(errorWordCapitalizationOrder)
        }
        metUncapitalized = isCurrUncapitalized
    }
    val realStartInd = if (wordType == tokWord) { startInd } else {startInd - 1} // accounting for the . or @ at the start
    val paylCapitalized = if (metUncapitalized) { 0 } else { 1 }

    val firstByte = inp[startInd]
    val lenBytes = i - realStartInd
    if (wordType == tokAtWord || firstByte < aBLower || firstByte > aWLower) {
        appendToken(wordType, paylCapitalized, realStartInd, lenBytes)
    } else {
        val mbReservedWord = possiblyReservedDispatch[(firstByte - aBLower)](startInd, i - startInd)
        if (mbReservedWord > 0) {
            if (mbReservedWord == reservedTrue) {
                appendToken(tokBool, 1, realStartInd, lenBytes)
            } else if (mbReservedWord == reservedFalse) {
                appendToken(tokBool, 0, realStartInd, lenBytes)
            } else {
                lexReservedWord(mbReservedWord, realStartInd)
            }
        } else  {
            appendToken(wordType, paylCapitalized, realStartInd, lenBytes)
        }
    }
}

/**
 * Lexes a single chunk of a word, i.e. the characters between two dots
 * (or the whole word if there are no dots).
 * Returns True if the lexed chunk was uncapitalized
 */
private fun lexWordChunk(): Boolean {
    var result = false

    if (inp[i] == aUnderscore) {
        checkPrematureEnd(2, inp)
        i += 1
    } else {
        checkPrematureEnd(1, inp)
    }
    if (wasError) return false

    if (isLowercaseLetter(inp[i])) {
        result = true
    } else if (!isCapitalLetter(inp[i])) {
        exitWithError(errorWordChunkStart)
    }
    i += 1

    while (i < inp.size && isAlphanumeric(inp[i])) {
        i += 1
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

    val lengthOfOperator = when (byte0) {
        opDef.bytes[1] -> { 1 }
        opDef.bytes[2] -> { 2 }
        opDef.bytes[3] -> { 3 }
        else           -> { 4 }
    }
    var j = i + lengthOfOperator
    if (isExtensible) {
        if (j < inp.size && inp[j] == aDot) {
            isExtensionAssignment += 2
            j++
        }
        if (j < inp.size && inp[j] == aEqual) {
            isExtensionAssignment += 1
            j++
        }
    }

    if (backtrack.isNotEmpty() && ((isExtensionAssignment & 1 > 0) || opType == opTImmDefinition || opType == opTMutation)) {
            processAssignmentOperator(opType)
    } else {
        appendOperator(opType, isExtensionAssignment, i, lengthOfOperator)
    }

    i = j
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
private fun processAssignmentOperator(opType: Int) {
    val currSpan = backtrack.last()

    val numTokensBeforeEquality = totalTokens - 1 - currSpan.second
    if (currSpan.first == tokStmtAssignment) {
        exitWithError(errorOperatorMultipleAssignment)
    } else if (currSpan.first != tokParens || numTokensBeforeEquality == 0) {
        exitWithError(errorOperatorAssignmentPunct)
    }
    validateNotInsideTypeDecl()
    convertGrandparentToScope()

    setSpanAssignment(currSpan.second, (numTokensBeforeEquality shl 16) + opType)
    backtrack[backtrack.size - 1] = Pair(tokStmtAssignment, currSpan.second)
}

/**
 * Lexer action for "::" the type declaration operator. Implements the validations 2 and 3 from Syntax.txt
 */
private fun processTypeDeclOperator() {
    val currSpan = backtrack.last()
    if (currSpan.first != tokParens) exitWithError(errorOperatorTypeDeclPunct)
    validateNotInsideTypeDecl()
    convertGrandparentToScope()

    setSpanType(currSpan.second, tokStmtTypeDecl)
    backtrack[backtrack.size - 1] = Pair(tokStmtTypeDecl, currSpan.second)
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
    if (backtrack.isEmpty() || backtrack.last().second < (totalTokens - 1)) {
        appendToken(tokReserved, reservedWordType, startInd, i - startInd)
        return
    }

    val currSpan = backtrack.last()
    if (backtrack.last().second < (totalTokens - 1) || backtrack.last().first >= firstCoreFormTokenType) {
        // if this is not the first token inside this span, or if it's already been converted to core form, add
        // the reserved word as a token
        appendToken(tokReserved, reservedWordType, startInd, i - startInd)
        return
    }
    validateNotInsideTypeDecl()

    setSpanCore(currSpan.second, reservedWordType)
    if (reservedWordType == tokStmtReturn) {
        convertGrandparentToScope()
    }
    backtrack[backtrack.size - 1] = Pair(punctType, currSpan.second)
}


private fun convertGrandparentToScope() {
    val indPenultimate = backtrack.size - 2
    if (indPenultimate > -1 && backtrack[indPenultimate].first != tokLexScope) {
        backtrack[indPenultimate] = Pair(tokLexScope, backtrack[indPenultimate].second)
        setSpanType(backtrack[indPenultimate].second, tokLexScope)
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

private fun lexCurlyLeft() {
    openPunctuation(tokCurlyBraces)
}

private fun lexCurlyRight() {
    closeRegularPunctuation(tokCurlyBraces)
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

/**
 * String literals look like 'wasn''t' and may contain arbitrary UTF-8.
 * The insides of the string have two escape sequences: '' for apostrophe and \n for newlines
 * TODO probably need to count UTF-8 codepoints, or worse - grapheme clusters - in order to correctly report to LSP
 */
private fun lexStringLiteral() {
    var j = i + 1
    val szMinusOne = inp.size - 1
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == aApostrophe && j < szMinusOne && inp[j + 1] == aApostrophe) {
            j += 2
        } else if (cByte == aApostrophe) {
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


private fun lexComment() {
    var j = i + 1
    if (j < inp.size && inp[j] == aSemicolon) {
        lexDocComment()
        return
    }
    val szMinusOne = inp.size - 1
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == aDot && j < szMinusOne && inp[j + 1] == aSemicolon) {
            addComment(i + 1, j - i - 1)
            i = j + 2
            return
        } else if (cByte == aNewline) {
            addComment(i + 1, j - i - 1)
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    addComment(i + 1, j - i - 1)
    i = j
}


private fun lexColon() {
    val j = i + 1
    if (j < inp.size) { // TODO make this what used to be the dollar
        if (inp[j] == aColon) {
            processTypeDeclOperator()
            i += 2
            return
        }  else if (inp[j] == aEqual) { // := the mutating assignment token
            processAssignmentOperator(OperatorToken(OperatorType.mutation, 0, false))
            i += 2
            return
        }
    }
    appendToken(operatorTok, OperatorToken(OperatorType.elseBranch, 0, false).toInt(), i, 1)
    i++
}

/**
 *  Doc comments, syntax is /// The comment
 */
private fun lexDocComment() {
    if (backtrack.isEmpty()) exitWithError(errorDocComment)
    val currSpan = backtrack.last()
    if (currSpan.first != tokParens || currSpan.second != totalTokens - 1) exitWithError(errorDocComment)
    if (inp[i - 1] != aParenLeft) exitWithError(errorDocComment)

    for (j in (i + 2) until inp.size) {
        val cByte = inp[j]
        if (cByte == aNewline) {
            newLines.add(j)
        } else if (cByte == aDot && j + 1 < inp.size && inp[j + 1] == aParenRight) {
            i = j + 2
            setDocComment(currSpan.second)
            return
        }
    }
    exitWithError(errorPrematureEndOfInput)
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
    backtrack.add(Pair(tType, totalTokens))
    appendPunctuation(tType, i + 1)
    i++
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], or a ;.
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private fun closeRegularPunctuation(closingType: Int) {
    if (backtrack.empty()) {
        exitWithError(errorPunctuationExtraClosing)
    }
    val top = backtrack.pop()
    validateClosingPunct(closingType, top.first)
    if (wasError) return
    
    setSpanLength(top.second)

    i++
}

/**
 * Validation to catch unmatched closing punctuation
 */
private fun validateClosingPunct(closingType: Int, openType: Int) {
    when (closingType) {
        tokCurlyBraces -> {
            if (openType != tokCurlyBraces) exitWithError(errorPunctuationUnmatched)
        }
        tokBrackets -> {
            if (openType != tokBrackets) exitWithError(errorPunctuationUnmatched)
        }
        tokParens -> {
            if (openType != tokParens) exitWithError(errorPunctuationUnmatched)
        }
        else -> {}
    }
}

/**
 * For programmatic Lexer result construction (builder pattern). Regular tokens
 */
fun build(tType: Int, payload: Int, startByte: Int, lenBytes: Int): Lexer {
    appendToken(tType, payload, startByte, lenBytes)
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
    val lenBytes = i - tokens[j + 1]
    tokens[j    ] += (lenBytes and LOWER27BITS) // lenBytes
    tokens[j + 3] = totalTokens - tokenInd - 1  // lenTokens
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 * Also stores the number of parentheses wrapping the assignment operator in the unused +2nd
 * slot, to be used for validation when the statement will be closed.
 */
private fun setSpanType(tokenInd: Int, tType: Int) {
    val j = tokenInd * 4
    tokens[j    ] = tType shl 27
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 */
private fun setSpanCore(tokenInd: Int, tType: Int) {
    val j = tokenInd * 4
    tokens[j    ] = tType shl 27
}


private fun setSpanAssignment(tokenInd: Int, payload1: Int) {
    val j = tokenInd * 4
    tokens[j    ] = tokStmtAssignment shl 27
    tokens[j + 2] = payload1
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
    tokens[nextInd    ] = (tType shl 27) + lenBytes
    tokens[nextInd + 1] = startByte
    tokens[nextInd + 2] = payload1
    tokens[nextInd + 3] = lenTokens
    bump()
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
    if (!backtrack.empty()) {
        exitWithError(errorPunctuationExtraOpening)
    }
}

/** Append a regular (non-punctuation) token */
private fun appendToken(tType: Int, payload2: Int, startByte: Int, lenBytes: Int) {
    tokens[nextInd    ] = (tType shl 27) + lenBytes
    tokens[nextInd + 1] = startByte
    tokens[nextInd + 3] = payload2
    bump()
}

/** Append an operator token */
private fun appendOperator(opType: Int, isExtensionAssignment: Int, startByte: Int, lenBytes: Int) {
    add((tokOperator shl 27) + lenBytes, startByte, 0, opType)
}

/** Append a 64-bit literal int token */
private fun appendLitIntToken(payload: Long, startByte: Int, lenBytes: Int) {
    tokens[nextInd    ] = (tokInt shl 27) + lenBytes
    tokens[nextInd + 1] = startByte
    tokens[nextInd + 2] = (payload shr 32).toInt()
    tokens[nextInd + 3] = (payload and LOWER32BITS).toInt()
    bump()
}

/** Append a floating-point literal */
private fun appendFloatToken(payload: Double, startByte: Int, lenBytes: Int) {
    val asLong: Long = payload.toBits()
    tokens[nextInd    ] = (tokFloat shl 27) + lenBytes
    tokens[nextInd + 1] = startByte
    tokens[nextInd + 2] = (asLong shr 32).toInt()
    tokens[nextInd + 3] = (asLong and LOWER32BITS).toInt()
    bump()
}

/** Append a punctuation token */
private fun appendPunctuation(tType: Int, startByte: Int) {
    tokens[nextInd    ] = (tType shl 27)
    tokens[nextInd + 1] = startByte
    bump()
}

/** Append a punctuation token when it's fully known already */
private fun appendPunctuationFull(tType: Int, payload1: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    tokens[nextInd    ] = (tType shl 27) + lenBytes
    tokens[nextInd + 1] = startByte
    tokens[nextInd + 2] = payload1
    tokens[nextInd + 3] = lenTokens
    bump()
}

/** The programmatic/builder method for appending a punctuation token */
fun buildPunctuation(startByte: Int, lenBytes: Int, tType: Int, lenTokens: Int): Lexer {
    appendPunctuationFull(tType, 0, lenTokens, startByte, lenBytes)
    return this
}


private fun bump() {
    nextInd += 4
    totalTokens += 1
}


fun nextToken() {
    currInd += 4
}
    

fun currTokenType(): Int {
    return tokens[currInd] ushr 27
}

fun currLenBytes(): Int {
    return tokens[currInd] and LOWER27BITS
}

fun currStartByte(): Int {
    return tokens[currInd + 1]
}

fun currLenTokens(): Int {
    return tokens[currInd + 3]
}


fun lookAhead(skip: Int): TokenLite {
    var nextInd = currInd + 4*skip
    return TokenLite(tokens[nextInd] ushr 27,
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
    var currA: LexChunk? = firstChunk
    while (true) {
        if (currA != null) {
            val lenA = if (currA == currChunk) { nextInd } else { CHUNKSZ }
            for (i in 0 until lenA step 4) {
                result.append((i/4).toString() + " ")
                printToken(currA, i, result)
                result.appendLine("")
            }

            currA = currA.next
        } else {
            break
        }
    }
    return result.toString()
}

private fun determineReservedA(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesAlias)) return Int.stmtBreak
    return 0
}

private fun determineReservedB(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesBreak)) return Int.stmtBreak
    return 0
}


private fun determineReservedC(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesCatch)) return -1
    return 0
}


private fun determineReservedE(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesExport)) return -1
    return 0
}


private fun determineReservedF(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesFalse)) return reservedFalse
    if (lenBytes == 2 && testByteSequence(startByte, reservedBytesFn)) return stmtFn
    return 0
}


private fun determineReservedI(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesImport)) return -1
    return 0
}


private fun determineReservedL(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesLoop)) return stmtLoop
    return 0
}

private fun determineReservedM(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesMatch)) return -1
    return 0
}

private fun determineReservedO(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesMatch)) return -1
    return 0
}

private fun determineReservedR(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesReturn)) return stmtReturn
    return 0
}

private fun determineReservedS(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesReturn)) return stmtReturn
    return 0
}

private fun determineReservedT(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesTrue)) return reservedTrue
    if (lenBytes == 3 && testByteSequence(startByte, reservedBytesTry)) return -1
    return 0
}

private fun determineUnreserved(startByte: Int, lenBytes: Int): Int {
    return 0
}

private fun exitWithError(msg: String): Nothing {
    val startByte = tokens[currInd + 1]
    val endByte = startByte + tokens[currInd] and LOWER27BITS
    wasError = true
    errMsg = "[$startByte $endByte] $msg"
    throw Exception(errMsg)
}

init {
    currChunk = firstChunk
    i = 0
    nextInd = 0
    currInd = 0
    currTokInd = 0
    totalTokens = 0
    wasError = false
    errMsg = ""
}


companion object {
    private val dispatchTable: Array<Lexer.() -> Unit> = Array(127) { Lexer::lexUnrecognizedSymbol }

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
        dispatchTable[aDot.toInt()] = Lexer::lexDotSomething
        dispatchTable[aAt.toInt()] = Lexer::lexAtWord

        for (i in operatorStartSymbols) {
            dispatchTable[i.toInt()] = Lexer::lexOperator
        }
        dispatchTable[aMinus.toInt()] = Lexer::lexMinus
        dispatchTable[aParenLeft.toInt()] = Lexer::lexParenLeft
        dispatchTable[aParenRight.toInt()] = Lexer::lexParenRight
        dispatchTable[aCurlyLeft.toInt()] = Lexer::lexCurlyLeft
        dispatchTable[aCurlyRight.toInt()] = Lexer::lexCurlyRight
        dispatchTable[aBracketLeft.toInt()] = Lexer::lexBracketLeft
        dispatchTable[aBracketRight.toInt()] = Lexer::lexBracketRight

        dispatchTable[aQuestion.toInt()] = Lexer::lexQuestionMark
        dispatchTable[aSpace.toInt()] = Lexer::lexSpace
        dispatchTable[aCarriageReturn.toInt()] = Lexer::lexSpace
        dispatchTable[aNewline.toInt()] = Lexer::lexNewline

        dispatchTable[aApostrophe.toInt()] = Lexer::lexStringLiteral
        dispatchTable[aSemicolon.toInt()] = Lexer::lexComment
        dispatchTable[aColon.toInt()] = Lexer::lexColon
    }

    /**
     * Table for dispatch on the first letter of a word in case it might be a reserved word.
     * It's indexed on the diff between first byte and the letter "c" (the earliest letter a reserved word may start with)
     */
    private val possiblyReservedDispatch: Array<Lexer.(Int, Int) -> Int> = Array(20) {
        i -> when(i) {
            0 -> Lexer::determineReservedA
            1 -> Lexer::determineReservedB
            2 -> Lexer::determineReservedC
            4 -> Lexer::determineReservedE
            5 -> Lexer::determineReservedF
            8 -> Lexer::determineReservedI
            12 -> Lexer::determineReservedM
            14 -> Lexer::determineReservedO
            17 -> Lexer::determineReservedR
            18 -> Lexer::determineReservedS
            19 -> Lexer::determineReservedT
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
        return  tType == lexScope || tType == stmtTypeDecl || tType == parens
                || tType == stmtAssignment
    }

    /** Predicate for an element of a scope, which can be any type of statement, or a split statement (such as an "if" or "match" form) */
    fun isScopeElementVal(tType: Int): Boolean {
        return tType == lexScope
                || tType == stmtTypeDecl
                || tType == parens
                || tType == stmtAssignment
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
                    for (i in len until lenA step 4) {
                        printToken(currA, i, result)
                        result.appendLine(" | ")
                    }
                    for (i in len until lenB step 4) {
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


    private fun printToken(tokens: IntArray, ind: Int, wr: StringBuilder) {
        val startByte = tokens[ind + 1]
        val lenBytes = tokens[ind] and LOWER27BITS
        val typeBits = (tokens[ind] ushr 27)
        if (typeBits < firstIntType) {
            if (typeBits == tokFloat) {
                val payload: Double = Double.fromBits(
                    (tokens[ind + 2].toLong() shl 32) + tokens[ind + 3].toLong()
                )
                wr.append("$regType $payload [${startByte} ${lenBytes}]")
            } else if (regType == operatorTok) {
                val payload2 = chunk.tokens[ind + 3]
                val opToken = OperatorToken.fromInt(payload2 and LOWER16BITS)
                val payloadStr = opToken.opType.toString() + (if (opToken.extended == 1) { "." } else if (opToken.extended == 2) { ":" } else { "" }) +
                        (if (opToken.isAssignment) { "=" } else {""})
                wr.append("$regType $payloadStr [${startByte} ${lenBytes}]")
            } else {
                val payload: Long = (chunk.tokens[ind + 2].toLong() shl 32) + (chunk.tokens[ind + 3].toLong() and LOWER32BITS)
                wr.append("$regType $payload [${startByte} ${lenBytes}]")
            }
        } else {
            val punctType = Int.values().firstOrNull { it.internalVal == typeBits }
            val lenTokens = chunk.tokens[ind + 3]
            val payload1 = chunk.tokens[ind + 2]
            if (punctType == stmtAssignment) {
                val numTokensBefore = payload1 ushr 16
                val opToken = OperatorToken.fromInt(payload1 and LOWER16BITS);
                val payloadStr = opToken.opType.toString() + (if (opToken.extended == 1) { "." } else if (opToken.extended == 2) { ":" } else { "" }) +
                        (if (opToken.isAssignment) { "=" } else {""}) + " " + numTokensBefore.toString()
                wr.append("$punctType $lenTokens [${startByte} ${lenBytes}] $payloadStr")
            } else {
                wr.append("$punctType $lenTokens [${startByte} ${lenBytes}] ${if (payload1 != 0) { payload1.toString() } else {""} }")
            }

        }
    }
}

}