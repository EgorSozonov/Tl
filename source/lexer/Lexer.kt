package lexer
import language.*
import java.lang.StringBuilder
import java.util.*
import lexer.RegularToken.*
import lexer.PunctuationToken.*
import utils.*

class Lexer(val inp: ByteArray, val fileType: FileType) {


var firstChunk: LexChunk = LexChunk()                        // First array of tokens
    private set
var currChunk: LexChunk                                      // Last array of tokens
var nextInd: Int                                             // Next ind inside the current token array
    private set
var currInd: Int
var currTokInd: Int
var totalTokens: Int
    private set
var wasError: Boolean                                        // The lexer's error flag
    private set
var errMsg: String
    private set
private var i: Int                                           // current index inside input byte array
private var comments: CommentStorage = CommentStorage()
private val backtrack = Stack<Pair<PunctuationToken, Int>>() // The stack of [punctuation scope, startTokInd]
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
    lexWordInternal(word)
}

/**
 * Lexes a word (both reserved and identifier) according to Tl's rules.
 * Examples of acceptable expressions: A.B.c.d, asdf123, ab._cd45
 * Examples of unacceptable expressions: A.b.C.d, 1asdf23, ab.cd_45
 */
private fun lexWordInternal(wordType: RegularToken) {
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
    val realStartInd = if (wordType == word) { startInd } else {startInd - 1} // accounting for the . or @ at the start
    val paylCapitalized = if (metUncapitalized) { 0 } else { 1 }

    val firstByte = inp[startInd]
    val lenBytes = i - realStartInd
    if (wordType == atWord || firstByte < aBLower || firstByte > aWLower) {
        appendToken(wordType, paylCapitalized, realStartInd, lenBytes)
    } else {
        val mbReservedWord = possiblyReservedDispatch[(firstByte - aBLower)](startInd, i - startInd)
        if (mbReservedWord > 0) {
            if (wordType == dotWord) {
                exitWithError(errorWordReservedWithDot)
            } else if (mbReservedWord == reservedTrue) {
                appendToken(boolTok, 1, realStartInd, lenBytes)
            } else if (mbReservedWord == reservedFalse) {
                appendToken(boolTok, 0, realStartInd, lenBytes)
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

/**
 * Lexes either a dot-word (like '.asdf') or a dot-bracket (like '.[1 2 3]')
 */
private fun lexDotSomething() {
    checkPrematureEnd(2, inp)

    val nByte = inp[i + 1]
    if (nByte == aUnderscore || isLetter(nByte)) {
        i += 1
        lexWordInternal(dotWord)
    } else {
        exitWithError(errorUnrecognizedByte)
    }
}


private fun lexAtWord() {
    checkPrematureEnd(2, inp)
    if (wasError) return

    i += 1
    lexWordInternal(atWord)
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
    var foundInd = -1
    while (k < operatorSymbols.size && operatorSymbols[k] != firstSymbol) {
        k += 4
    }
    while (k < operatorSymbols.size && operatorSymbols[k] == firstSymbol) {
        val secondTentative = operatorSymbols[k + 1]
        if (secondTentative != byte0 && secondTentative != secondSymbol) {
            k += 4
            continue
        }
        val thirdTentative = operatorSymbols[k + 2]
        if (thirdTentative != byte0 && thirdTentative != thirdSymbol) {
            k += 4
            continue
        }
        val fourthTentative = operatorSymbols[k + 3]
        if (fourthTentative != byte0 && fourthTentative != fourthSymbol) {
            k += 4
            continue
        }
        foundInd = k
        break
    }
    if (foundInd < 0) exitWithError(errorOperatorUnknown)

    val isExtensible = operatorDefinitions[k/4].extensible
    var isAssignment = false
    var extension = 0

    val lengthOfOperator = when (byte0) {
        operatorSymbols[foundInd + 1] -> { 1 }
        operatorSymbols[foundInd + 2] -> { 2 }
        operatorSymbols[foundInd + 3] -> { 3 }
        else                          -> { 4 }
    }
    var j = i + lengthOfOperator
    if (isExtensible) {
        if (j < inp.size) {
            if (inp[j] == aDot) {
                extension = 1
                j++
            } else if (inp[j] == aColon) {
                extension = 2
                j++
            }
        }
        if (j < inp.size && inp[j] == aEqual) {
            isAssignment = true
            j++
        }
    }
    val opType = OperatorType.values()[foundInd/4]
    val newOperator = OperatorToken(opType, extension, isAssignment)
    if (backtrack.isNotEmpty()
        && (isAssignment || opType == OperatorType.immDefinition || opType == OperatorType.mutation)) {
            processAssignmentOperator(newOperator)
    } else {
        appendToken(operatorTok, newOperator.toInt(), i, j - i)
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
private fun processAssignmentOperator(opType: OperatorToken) {
    val currSpan = backtrack.last()

    val numTokensBeforeEquality = totalTokens - 1 - currSpan.second
    if (currSpan.first == stmtAssignment) {
        exitWithError(errorOperatorMultipleAssignment)
    } else if (currSpan.first != parens || numTokensBeforeEquality == 0) {
        exitWithError(errorOperatorAssignmentPunct)
    }
    validateNotInsideTypeDecl()
    convertGrandparentToScope()

    setSpanAssignment(currSpan.second, (numTokensBeforeEquality shl 16) + opType.toInt())
    backtrack[backtrack.size - 1] = Pair(stmtAssignment, currSpan.second)
}

/**
 * Lexer action for "::" the type declaration operator. Implements the validations 2 and 3 from Syntax.txt
 */
private fun processTypeDeclOperator() {
    val currSpan = backtrack.last()
    if (currSpan.first != parens) exitWithError(errorOperatorTypeDeclPunct)
    validateNotInsideTypeDecl()
    convertGrandparentToScope()

    setSpanType(currSpan.second, stmtTypeDecl)
    backtrack[backtrack.size - 1] = Pair(stmtTypeDecl, currSpan.second)
}

/**
 * Lexer action for a reserved word. If at the start of a statement/expression, it mutates the type of said expression,
 * otherwise it emits a corresponding token.
 * Implements the validation 2 from Syntax.txt
 */
private fun lexReservedWord(reservedWordType: Int, startInd: Int) {
    if (reservedWordType == stmtBreak.internalVal.toInt()) {
        appendPunctuationFull(stmtBreak, 0, 0, startInd, 5)
        return
    }
    if (backtrack.isEmpty() || backtrack.last().second < (totalTokens - 1)) {
        appendToken(reservedWord, reservedWordType, startInd, i - startInd)
        return
    }

    val currSpan = backtrack.last()
    if (backtrack.last().second < (totalTokens - 1) || backtrack.last().first.internalVal.toInt() >= firstCoreFormTokenType) {
        // if this is not the first token inside this span, or if it's already been converted to core form, add
        // the reserved word as a token
        appendToken(reservedWord, reservedWordType, startInd, i - startInd)
        return
    }
    validateNotInsideTypeDecl()

    val punctType = PunctuationToken.values().first{it.internalVal.toInt() == reservedWordType}
    setSpanCore(currSpan.second, punctType)
    if (reservedWordType == stmtReturn.internalVal.toInt()) {
        convertGrandparentToScope()
    }
    backtrack[backtrack.size - 1] = Pair(punctType, currSpan.second)
}


private fun convertGrandparentToScope() {
    val indPenultimate = backtrack.size - 2
    if (indPenultimate > -1 && backtrack[indPenultimate].first != lexScope) {
        backtrack[indPenultimate] = Pair(lexScope, backtrack[indPenultimate].second)
        setSpanType(backtrack[indPenultimate].second, lexScope)
    }
}

/** Validation 2 from Syntax.txt. Since type declarations may contain only simple expressions
 * (i.e. nothing but expressions nested inside of them) then we need to check whether we
 * are inside a type declaration right now. */
private fun validateNotInsideTypeDecl() {

}

private fun lexParenLeft() {
    openPunctuation(parens)
}

private fun lexParenRight() {
    closeRegularPunctuation(parens)
}

private fun lexCurlyLeft() {
    openPunctuation(curlyBraces)
}

private fun lexCurlyRight() {
    closeRegularPunctuation(curlyBraces)
}

private fun lexBracketLeft() {
    openPunctuation(brackets)
}

private fun lexBracketRight() {
    closeRegularPunctuation(brackets)
}

/** ? in initial position means an "if" expression, but otherwise it's an ordinary non-functional operator
 * (used at the type level for nullable types).
 */
private fun lexQuestionMark() {
    val j = i + 1
    if (j < inp.size) {
        if (inp[j] == aQuestion) {
            setSpanCore(backtrack.peek().second, stmtIfPred)
            i += 2
            return
        } else if (inp[j] == aEqual) {
            setSpanCore(backtrack.peek().second, stmtIfEq)
            i += 2
            return
        }
    }
    if (backtrack.isEmpty() || backtrack.last().second < totalTokens - 1) {
        appendToken(operatorTok, OperatorToken(OperatorType.questionMark, 0, false).toInt(), i, 1)
    } else {
        setSpanCore(backtrack.peek().second, stmtIf)
    }

    i += 1
}

private fun lexSpace() {
    i += 1
}

private fun lexNewline() {
    newLines.add(i)
    i++
}

/**
 * String literals look like "wasn't" and may contain arbitrary UTF-8.
 * The insides of the string have escape sequences and variable interpolation with
 * the "x = ${x}" syntax.
 * TODO probably need to count UTF-8 codepoints, or worse - grapheme clusters - in order to correctly report to LSP
 */
private fun lexStringLiteral() {
    var j = i + 1
    val szMinusOne = inp.size - 1
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == aBackslash && j < szMinusOne && inp[j + 1] == aQuote) {
            j += 2
        } else if (cByte == aQuote) {
            appendToken(stringTok, 0, i + 1, j - i - 1)
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    exitWithError(errorPrematureEndOfInput)
}

/**
 * Verbatim strings look like @"asdf" or @"\nasdf" or @4"asdf"""asdf"""".
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
 * The third variety is used to set the sufficient number of closing quotes in order to facilitate
 * the string containing any necessary sequence.
 * TODO implement this
 */
private fun lexVerbatimStringLiteral() {
    var j = i + 1
    while (i < inp.size) {
        val cByte = inp[j]
        if (cByte == aQuote) {
            if (j < (inp.size - 2) && inp[j + 1] == aQuote && inp[j + 2] == aQuote) {
                appendToken(verbatimString, 0, i + 1, j - i - 1)
                i = j + 3
                return
            }
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
    if (j < inp.size) {
        if (inp[j] == aColon) { // :: the type declaration token
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


/** Doc comments, syntax is (;; The comment .)
 * Precondition: i is pointing at the first ";" in "(;;"
 */
private fun lexDocComment() {
    if (backtrack.isEmpty()) exitWithError(errorDocComment)
    val currSpan = backtrack.last()
    if (currSpan.first != parens || currSpan.second != totalTokens - 1) exitWithError(errorDocComment)
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

private fun addComment(startByte: Int, lenBytes: Int) {
    comments.add(startByte, lenBytes)
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
    backtrack.add(Pair(tType, totalTokens))
    appendPunctuation(tType, i + 1)
    i++
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], or a ;.
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private fun closeRegularPunctuation(closingType: PunctuationToken) {
    if (backtrack.empty()) {
        exitWithError(errorPunctuationExtraClosing)
    }
    val top = backtrack.pop()
    validateClosingPunct(closingType, top.first)
    if (wasError) return
    
    setSpanLength(top.second)

    i++
}


private fun getPrevTokenType(): Int {
    if (totalTokens == 0) return 0

    return if (nextInd > 0) {
        currChunk.tokens[nextInd - 4] ushr 27
    } else {
        var curr: LexChunk? = firstChunk
        while (curr!!.next != currChunk) {
            curr = curr.next!!
        }
        curr.tokens[CHUNKSZ - 4] ushr 27
    }
}

/**
 * Validation to catch unmatched closing punctuation
 */
private fun validateClosingPunct(closingType: PunctuationToken, openType: PunctuationToken) {
    when (closingType) {
        curlyBraces -> {
            if (openType != curlyBraces) exitWithError(errorPunctuationUnmatched)
        }
        brackets -> {
            if (openType != brackets) exitWithError(errorPunctuationUnmatched)
        }
        parens -> {
            if (openType == brackets || openType == curlyBraces) exitWithError(errorPunctuationUnmatched)
        }
        else -> {}
    }
}

/**
 * For programmatic Lexer result construction (builder pattern). Regular tokens
 */
fun build(tType: RegularToken, payload: Int, startByte: Int, lenBytes: Int): Lexer {
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
 * For programmatic Lexer result construction (builder pattern). Comments
 */
fun buildComment(startByte: Int, lenBytes: Int): Lexer {
    addComment(startByte, lenBytes)
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
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }

    val lenBytes = i - curr.tokens[j + 1]
    checkLenOverflow(lenBytes)
    curr.tokens[j    ] += (lenBytes and LOWER27BITS) // lenBytes
    curr.tokens[j + 3] = totalTokens - tokenInd - 1  // lenTokens
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 * Also stores the number of parentheses wrapping the assignment operator in the unused +2nd
 * slot, to be used for validation when the statement will be closed.
 */
private fun setSpanTypeLength(tokenInd: Int, tType: PunctuationToken) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }
    val lenBytes = i - curr.tokens[j + 1]
    checkLenOverflow(lenBytes)
    curr.tokens[j    ] = (tType.internalVal.toInt() shl 27) + (lenBytes and LOWER27BITS)
    curr.tokens[j + 3] = totalTokens - tokenInd - 1  // lenTokens
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 * Also stores the number of parentheses wrapping the assignment operator in the unused +2nd
 * slot, to be used for validation when the statement will be closed.
 */
private fun setSpanType(tokenInd: Int, tType: PunctuationToken) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }
    curr.tokens[j    ] = tType.internalVal.toInt() shl 27
}

/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 */
private fun setSpanCore(tokenInd: Int, tType: PunctuationToken) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }
    curr.tokens[j    ] = tType.internalVal.toInt() shl 27
}


private fun setSpanAssignment(tokenInd: Int, payload1: Int) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }
    curr.tokens[j    ] = stmtAssignment.internalVal.toInt() shl 27
    curr.tokens[j + 2] = payload1
}


/**
 * Mutates a span token to the docCommentTok regular token and sets its lenBytes.
 * Precondition: we are standing at the closing ")" of the doc comment
 */
private fun setDocComment(tokenInd: Int) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }
    val lenBytes = i - curr.tokens[j + 1] - 4 // -4 for the the three ;s and one ), so for "(;;ab;)" this will be 2
    checkLenOverflow(lenBytes)
    curr.tokens[j    ] = (docCommentTok.internalVal.toInt() shl 27) + (lenBytes and LOWER27BITS)
    curr.tokens[j + 1] += 2 // moving startByte for the initial ";;" in "(;;..."

    backtrack.pop()
}


private fun ensureSpaceForToken() {
    if (nextInd < CHUNKSZ) return

    val newChunk = LexChunk()
    currChunk.next = newChunk
    currChunk = newChunk
    nextInd = 0
}


private fun checkLenOverflow(lenBytes: Int) {
    if (lenBytes > MAXTOKENLEN) exitWithError(errorLengthOverflow)
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
fun buildPunctPayload(tType: PunctuationToken, payload1: Int, lenTokens: Int, startByte: Int, lenBytes: Int, ): Lexer {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 2] = payload1
    currChunk.tokens[nextInd + 3] = lenTokens
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
private fun appendToken(tType: RegularToken, payload2: Int, startByte: Int, lenBytes: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 3] = payload2
    bump()
}

/** Append a 64-bit literal int token */
private fun appendLitIntToken(payload: Long, startByte: Int, lenBytes: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = (intTok.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 2] = (payload shr 32).toInt()
    currChunk.tokens[nextInd + 3] = (payload and LOWER32BITS).toInt()
    bump()
}

/** Append a floating-point literal */
private fun appendFloatToken(payload: Double, startByte: Int, lenBytes: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    val asLong: Long = payload.toBits()
    currChunk.tokens[nextInd    ] = (floatTok.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 2] = (asLong shr 32).toInt()
    currChunk.tokens[nextInd + 3] = (asLong and LOWER32BITS).toInt()
    bump()
}

/** Append a punctuation token */
private fun appendPunctuation(tType: PunctuationToken, startByte: Int) {
    ensureSpaceForToken()
    currChunk.tokens[nextInd    ] = (tType.internalVal.toInt() shl 27)
    currChunk.tokens[nextInd + 1] = startByte
    bump()
}

/** Append a punctuation token when it's fully known already */
private fun appendPunctuationFull(tType: PunctuationToken, payload1: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 2] = payload1
    currChunk.tokens[nextInd + 3] = lenTokens
    bump()
}

/** The programmatic/builder method for appending a punctuation token */
fun buildPunctuation(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int): Lexer {
    appendPunctuationFull(tType, 0, lenTokens, startByte, lenBytes)
    return this
}


private fun bump() {
    nextInd += 4
    totalTokens += 1
}


fun nextToken() {
    currTokInd++
    currInd += 4
    if (currInd == CHUNKSZ) {
        currChunk = currChunk.next!!
        currInd = 0
    }
}
    

fun currTokenType(): Int {
    return currChunk.tokens[currInd] ushr 27
}


fun currLenBytes(): Int {
    return currChunk.tokens[currInd] and LOWER27BITS
}


fun currStartByte(): Int {
    return currChunk.tokens[currInd + 1]
}


fun currLenTokens(): Int {
    return currChunk.tokens[currInd + 3]
}


fun lookAheadType(skip: Int): Int {
    var nextInd = currInd + 4*skip
    var nextChunk = currChunk
    while (nextInd >= CHUNKSZ) {
        nextChunk = currChunk.next!!
        nextInd -= CHUNKSZ
    }
    return nextChunk.tokens[nextInd] ushr 27
}


fun lookAhead(skip: Int): TokenLite {
    var nextInd = currInd + 4*skip
    var nextChunk = currChunk
    while (nextInd >= CHUNKSZ) {
        nextChunk = currChunk.next!!
        nextInd -= CHUNKSZ
    }
    return TokenLite(nextChunk.tokens[nextInd] ushr 27,
                     (nextChunk.tokens[nextInd + 2].toLong() shl 32) + nextChunk.tokens[nextInd + 3].toLong())
}


fun currentPosition(): Int {
    var tmp = firstChunk
    var result = 0
    while (tmp != currChunk) {
        tmp = currChunk.next!!
        result += CHUNKSZ
    }
    return (result + currInd)/4
}


fun seek(tokenId: Int) {
    currInd = tokenId*4
    currChunk = firstChunk
    while (currInd >= CHUNKSZ) {
        currChunk = currChunk.next!!
        currInd -= CHUNKSZ
    }
    currTokInd = tokenId
}


fun skipTokens(toSkip: Int) {
    currInd += 4*toSkip
    while (currInd >= CHUNKSZ) {
        currChunk = currChunk.next!!
        nextInd -= CHUNKSZ
    }
    currTokInd += toSkip
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

private fun determineReservedB(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesBreak)) return PunctuationToken.stmtBreak.internalVal.toInt()
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
    if (lenBytes == 2 && testByteSequence(startByte, reservedBytesFn)) return stmtFn.internalVal.toInt()
    return 0
}


private fun determineReservedI(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesImport)) return -1
    return 0
}


private fun determineReservedL(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesLoop)) return stmtLoop.internalVal.toInt()
    return 0
}

private fun determineReservedM(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesMatch)) return -1
    return 0
}

private fun determineReservedR(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 6 && testByteSequence(startByte, reservedBytesReturn)) return stmtReturn.internalVal.toInt()
    return 0
}

private fun determineReservedT(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 4 && testByteSequence(startByte, reservedBytesTrue)) return reservedTrue
    if (lenBytes == 3 && testByteSequence(startByte, reservedBytesTry)) return -1
    return 0
}

private fun determineReservedW(startByte: Int, lenBytes: Int): Int {
    if (lenBytes == 5 && testByteSequence(startByte, reservedBytesWhile)) return -1
    return 0
}

private fun determineUnreserved(startByte: Int, lenBytes: Int): Int {
    return 0
}

private fun exitWithError(msg: String): Nothing {
    val startByte = currChunk.tokens[currInd + 1]
    val endByte = startByte + currChunk.tokens[currInd] and LOWER27BITS
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

        dispatchTable[aQuote.toInt()] = Lexer::lexStringLiteral
        dispatchTable[aSemicolon.toInt()] = Lexer::lexComment
        dispatchTable[aColon.toInt()] = Lexer::lexColon
    }

    /**
     * Table for dispatch on the first letter of a word in case it might be a reserved word.
     * It's indexed on the diff between first byte and the letter "c" (the earliest letter a reserved word may start with)
     */
    private val possiblyReservedDispatch: Array<Lexer.(Int, Int) -> Int> = Array(22) {
        i -> when(i) {
            0 -> Lexer::determineReservedB
            1 -> Lexer::determineReservedC
            3 -> Lexer::determineReservedE
            4 -> Lexer::determineReservedF
            7 -> Lexer::determineReservedI
            10 -> Lexer::determineReservedL
            11 -> Lexer::determineReservedM
            16 -> Lexer::determineReservedR
            18 -> Lexer::determineReservedT
            21 -> Lexer::determineReservedW
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
    fun isParensElement(tType: PunctuationToken): Boolean {
        return  tType == lexScope || tType == stmtTypeDecl || tType == parens
                || tType == stmtAssignment
    }

    /** Predicate for an element of a scope, which can be any type of statement, or a split statement (such as an "if" or "match" form) */
    fun isScopeElementVal(tType: Int): Boolean {
        return tType == lexScope.internalVal.toInt()
                || tType == stmtTypeDecl.internalVal.toInt()
                || tType == parens.internalVal.toInt()
                || tType == stmtAssignment.internalVal.toInt()
    }

    /**
     * Equality comparison for lexers.
     */
    fun equality(a: Lexer, b: Lexer): Boolean {
        if (a.wasError != b.wasError || a.totalTokens != b.totalTokens || a.nextInd != b.nextInd
            || !a.errMsg.endsWith(b.errMsg)) {
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


    private fun printToken(chunk: LexChunk, ind: Int, wr: StringBuilder) {
        val startByte = chunk.tokens[ind + 1]
        val lenBytes = chunk.tokens[ind] and LOWER27BITS
        val typeBits = (chunk.tokens[ind] ushr 27).toByte()
        if (typeBits < firstPunctuationTokenType) {
            val regType = RegularToken.values().firstOrNull { it.internalVal == typeBits }
            if (regType == floatTok) {
                val payload: Double = Double.fromBits(
                    (chunk.tokens[ind + 2].toLong() shl 32) + chunk.tokens[ind + 3].toLong()
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
            val punctType = PunctuationToken.values().firstOrNull { it.internalVal == typeBits }
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