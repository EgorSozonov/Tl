package compiler.lexer
import java.lang.StringBuilder
import java.util.*
import compiler.lexer.RegularToken.*
import compiler.lexer.PunctuationToken.*

class Lexer {

var inp: ByteArray
    private set
var firstChunk: LexChunk = LexChunk()                        // First array of tokens
    private set
var currChunk: LexChunk                                      // Last array of tokens
var nextInd: Int                                             // Next ind inside the current token array
    private set
var currInd: Int
var totalTokens: Int
    private set
var wasError: Boolean                                        // The lexer's error flag
    private set
var errMsg: String
    private set
private var i: Int                                           // current index inside input byte array
private var comments: CommentStorage = CommentStorage()
private val backtrack = Stack<Pair<PunctuationToken, Int>>() // The stack of punctuation scopes
private val numeric: LexerNumerics = LexerNumerics()


/**
 * Main lexer method
 */
fun lexicallyAnalyze() {
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
            dispatchTable[cByte.toInt()]()
        } else {
            errorOut(errorNona)
            break
        }
    }

    finalize()
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
            errorOut(errorWordCapitalizationOrder)
            return
        }
        metUncapitalized = isCurrUncapitalized
    }
    if (!wasError) {
        val realStartChar = if (wordType == word) { startInd } else {startInd - 1}
        addToken(0, realStartChar, i - realStartChar, wordType)
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
        errorOut(errorWordChunkStart)
        return result
    }
    i += 1

    while (i < inp.size && isAlphanumeric(inp[i])) {
        i += 1
    }
    if (i < inp.size && inp[i] == aUnderscore) {
        errorOut(errorWordUnderscoresOnlyAtStart)
        return result
    }

    return result
}


/**
 * Lexes either a dot-word (like '.asdf') or a dot-bracket (like '.[1 2 3]')
 */
private fun lexDotSomething() {
    checkPrematureEnd(2, inp)
    if (wasError) return

    val nByte = inp[i + 1]
    if (nByte == aBracketLeft) {
        lexDotBracket()
    } else if (nByte == aUnderscore || isLetter(nByte)) {
        i += 1
        lexWordInternal(dotWord)
    } else {
        errorOut(errorUnrecognizedByte)
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
        addToken((cByte - aDigit0).toLong(), i, 1, litInt)
        i++
        return
    }

    when (inp[i + 1]) {
        aXLower -> { lexHexNumber() }
        aBLower -> { lexBinNumber() }
        else ->        { lexDecNumber() }
    }
    numeric.clear()
}


/**
 * Lexes a decimal numeric literal (integer or floating-point).
 * TODO: add support for the '1.23E4' format
 */
private fun lexDecNumber() {
    var j = i
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
                errorOut(errorNumericEndUnderscore)
                return
            }
        } else if (cByte == aDot) {
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
        addToken(resultValue, i, j - i, litInt)
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
    addToken(resultValue, i, j - i, litInt)
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
    addToken(resultValue, i, j - i, litInt)
    i = j
}


private fun lexOperator() {
    val firstSymbol = inp[i]
    val secondSymbol = if (inp.size > i + 1) { inp[i + 1] } else { 0 }
    val thirdSymbol = if (inp.size > i + 2) { inp[i + 2] } else { 0 }
    var k = 0
    var foundInd = -1
    while (k < operatorSymbols.size && operatorSymbols[k] != firstSymbol) {
        k += 3
    }
    while (k < operatorSymbols.size && operatorSymbols[k] == firstSymbol) {
        val secondTentative = operatorSymbols[k + 1]
        if (secondTentative != byte0 && secondTentative != secondSymbol) {
            k += 3
            continue
        }
        val thirdTentative = operatorSymbols[k + 2]
        if (thirdTentative != byte0 && thirdTentative != thirdSymbol) {
            k += 3
            continue
        }

        foundInd = k
        break
    }
    if (foundInd < 0) {
        errorOut(errorOperatorUnknown)
        return
    }
    val isExtensible = operatorExtensibility[k/3]
    var isAssignment = false
    var extension = 0

    val lengthOfOperator = when (byte0) {
        operatorSymbols[foundInd + 1] -> { 1 }
        operatorSymbols[foundInd + 2] -> { 2 }
        else                          -> { 3 }
    }
    var j = i + lengthOfOperator
    if (isExtensible > 0) {
        if (j < inp.size && inp[j] == aEquals) {
            isAssignment = true
            j++
        }

        if (j < inp.size) {
            if (inp[j] == aDot) {
                extension = 1
                j++
            } else if (inp[j] == aColon) {
                extension = 2
                j++
            }
        }
    }
    val opType = OperatorType.values()[foundInd/3]
    val newOperator = OperatorToken(opType, extension, isAssignment)
    if (isAssignment || opType == OperatorType.immDefinition || opType == OperatorType.mutation
        || opType == OperatorType.colon) {
        processAssignmentOperator(opType == OperatorType.colon)
    }
    addToken(newOperator.toInt(), i, j - i, operatorTok)

    i = j
}

/**
 * There are 3 types of statements: function calls, assignments/mutations, and type declarations.
 * At birth, statements are function calls, but when an assignment/typedecl operator is encountered,
 * the type of the statement is mutated.
 * To complicate the matter, it is OK for a statement to look like
 *     ((x = y + 5))
 * so this function unwraps any possible paren (but no other type of punctuation) layers.
 * It also checks to see that all those layers (if any) are tight.
 */
private fun processAssignmentOperator(isTypeDecl: Boolean) {
    var stackInd = backtrack.size - 1
    while (stackInd >= 0 && backtrack[stackInd].first == parens) {
        stackInd--
    }
    if (stackInd < 0) {
        errorOut(errorOperatorAssignmentPunct)
        return
    }

    val statement = backtrack[stackInd]
    val numberParensAssignment = backtrack.size - stackInd - 1

    if (statement.first == statementAssignment || statement.first == statementTypeDecl) {
        errorOut(errorOperatorMultipleAssignment)
        return
    } else if (statement.first != statementFun) {
        errorOut(errorOperatorAssignmentPunct)
        return
    }
    val newType = if (isTypeDecl) {statementTypeDecl} else {statementAssignment}
    backtrack[stackInd] = Pair(newType, statement.second)
    setStatementType(statement.second, newType, numberParensAssignment)
}

private fun lexParenLeft() {
    openPunctuation(parens)
}

private fun lexParenRight() {
    closeRegularPunctuation(parens)
}

private fun lexDollar() {
    openPunctuation(dollar)
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

private fun lexDotBracket() {
    openPunctuation(dotBrackets)
}

private fun lexSpace() {
    i += 1
}

private fun lexNewline() {
    closeStatement()
}

private fun lexStatementTerminator() {
    closeStatement()
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
            addToken(0, i + 1, j - i - 1, litString)
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    errorOut(errorPrematureEndOfInput)
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
                addToken(0, i + 1, j - i - 1, verbatimString)
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


private fun lexUnrecognizedSymbol() {
    errorOut(errorUnrecognizedByte)
}


private fun lexComment() {
    var j = i + 1
    val szMinusOne = inp.size - 1
    while (j < inp.size) {
        val cByte = inp[j]
        if (cByte == aDot && j < szMinusOne && inp[j + 1] == aSharp) {
            addToken(0, i + 1, j - i - 1, comment)
            i = j + 2
            return
        } else if (cByte == aNewline) {
            addToken(0, i + 1, j - i - 1, comment)
            i = j + 1
            return
        } else {
            j += 1
        }
    }
    addToken(0, i + 1, j - i - 1, comment)
    i = inp.size
}


/**
 * Checks that there are at least 'requiredSymbols' symbols left in the input.
 */
private fun checkPrematureEnd(requiredSymbols: Int, inp: ByteArray) {
    if (i > inp.size - requiredSymbols) {
        errorOut(errorPrematureEndOfInput)
    }
}


/**
 * Adds a regular, non-punctuation token
 */
private fun addToken(payload: Long, startByte: Int, lenBytes: Int, tType: RegularToken) {
    if (tType == comment) {
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
    wrapTokenInStatement(startByte, litFloat)
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
    val insidesStart = if (tType == dotBrackets) { i + 2 } else { i + 1 }
    appendToken(insidesStart, tType)
    i = insidesStart
}


/**
 * Regular tokens may not exist directly at the top level, or inside curlyBraces.
 * So this function inserts an implicit statement scope to prevent this.
 */
private fun wrapTokenInStatement(startByte: Int, tType: RegularToken) {
    if (backtrack.empty() || backtrack.peek().first == curlyBraces) {
        var realStartByte = startByte
        if (tType == litString || tType == verbatimString) realStartByte -= 1
        addStatement(realStartByte)
    }
}


/**
 * Regular tokens may not exist directly at the top level, or inside curlyBraces.
 * So this function inserts an implicit statement scope to prevent this.
 */
private fun addStatement(startByte: Int) {
    backtrack.add(Pair(statementFun, totalTokens))
    appendToken(startByte, statementFun)
}


/**
 * Validates that opening punctuation obeys the rules
 * and inserts an implicit Statement if immediately inside curlyBraces.
 */
private fun validateOpeningPunct(openingType: PunctuationToken) {
    if (openingType == curlyBraces) {
        return
    } else if (openingType == dotBrackets && getPrevTokenType() != word.internalVal.toInt()) {
        errorOut(errorPunctuationWrongOpen)
    } else {
        if (backtrack.isEmpty() || backtrack.peek().first == curlyBraces) {
            addStatement(i)
        }
    }
}


/**
 * The statement closer function. It is called for a newline or a semi-colon.
 * Only closes the current scope if it's a statement and non-empty. This protects against
 * newlines that aren't inside curly braces, multiple newlines and sequences like ';;;;'.
 */
private fun closeStatement() {
    closeDollars()

    if (backtrack.isEmpty()) return

    val top = backtrack.peek()
    if (isStatement(top.first)) {
        val stmt = backtrack.pop()
        setPunctuationLength(stmt.second)
        if (stmt.first == statementTypeDecl || stmt.first == statementAssignment) {
            validateAssignmentStmt(stmt.second)
        }
    }
    i++
}


/**
 * Validates an assignment/type declaration with respect to its parentheses.
 * These are OK:
 *     x += y + 5
 *     ((x += y + 5))
 *     x += (y + 5)
 *
 * These are incorrect:
 *     x (+=  y) + 5
 *     ((x += y) + 5)
 */
private fun validateAssignmentStmt(tokenInd: Int) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }

    var numTokens = totalTokens - tokenInd - 1 // total tokens in this statement
    val numParenthesesAssignment = curr.tokens[j + 2]
    curr.tokens[j + 2] = 0 // clean up this number as it's not needed anymore
    for (k in 1..numParenthesesAssignment) {
        if (j < CHUNKSZ) {
            j += 4
        } else {
            curr = curr.next!!
            j = 0
        }
        numTokens--

        val tokTypeInt = curr.tokens[j] ushr 27
        if (tokTypeInt != parens.internalVal.toInt() || curr.tokens[j + 3] != numTokens) {
            errorOut(errorOperatorAssignmentPunct)
            return
        }
    }
}


private fun closeDollars() {
    var top: Pair<PunctuationToken, Int>
    while (!backtrack.empty() && backtrack.peek().first == dollar) {
        top = backtrack.pop()
        setPunctuationLength(top.second)
    }
}

/**
 * Processes a token which serves as the closer of a punctuation scope, i.e. either a ), a }, a ], or a ;.
 * This doesn't actually add any tokens to the array, just performs validation and sets the token length
 * for the opener token.
 */
private fun closeRegularPunctuation(closingType: PunctuationToken) {
    closeDollars()
    if (backtrack.empty()) {
        errorOut(errorPunctuationExtraClosing)
        return
    }
    val top = backtrack.pop()
    validateClosingPunct(closingType, top.first)
    if (wasError) return
    
    setPunctuationLength(top.second)
    if (closingType == curlyBraces && isStatement(top.first)) {
        // Since statements are always immediate children of curlyBraces, the '}' symbol closes
        // not just the statement but the parent curlyBraces scope, too.
        val parentScope = backtrack.pop()
        assert(parentScope.first == curlyBraces)

        setPunctuationLength(parentScope.second)
    }
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
            if (openType != curlyBraces && !isStatement(openType)) {
                errorOut(errorPunctuationUnmatched)
                return
            }
        }
        brackets -> {
            if (openType != brackets && openType != dotBrackets) {
                errorOut(errorPunctuationUnmatched)
                return
            }
        }
        parens -> {
            if (openType != parens) {
                errorOut(errorPunctuationUnmatched)
                return
            }
        }
        statementTypeDecl, statementFun, statementAssignment, -> {
            if (!isStatement(openType)) {
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
    if (tType == comment) {
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
private fun setPunctuationLength(tokenInd: Int) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }

    val lenBytes = i - curr.tokens[j + 1]
    checkLenOverflow(lenBytes)
    curr.tokens[j + 3] = totalTokens - tokenInd - 1  // lenTokens
    curr.tokens[j    ] += (lenBytes and LOWER27BITS) // lenBytes
}


/**
 * Mutates a statement to a more precise type of statement (assignment, type declaration).
 * Also stores the number of parentheses wrapping the assignment operator in the unused +2nd
 * slot, to be used for validation when the statement will be closed.
 */
private fun setStatementType(tokenInd: Int, tType: PunctuationToken, numParenthesesAssignment: Int) {
    var curr = firstChunk
    var j = tokenInd * 4
    while (j >= CHUNKSZ) {
        curr = curr.next!!
        j -= CHUNKSZ
    }
    curr.tokens[j + 2] = numParenthesesAssignment
    curr.tokens[j    ] = tType.internalVal.toInt() shl 27
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
    if (wasError) return

    closeDollars()
    closeStatement()
    if (!backtrack.empty()) {
        errorOut(errorPunctuationExtraOpening)
    }
}


/** Append a regular (non-punctuation) token */
private fun appendToken(payload: Long, startByte: Int, lenBytes: Int, tType: RegularToken) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 2] = (payload shr 32).toInt()
    currChunk.tokens[nextInd + 3] = (payload and LOWER32BITS).toInt()
    bump()
}

/** Append a floating-point literal */
private fun appendToken(payload: Double, startByte: Int, lenBytes: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    val asLong: Long = payload.toBits()
    currChunk.tokens[nextInd    ] = (litFloat.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 2] = (asLong shr 32).toInt()
    currChunk.tokens[nextInd + 3] = (asLong and LOWER32BITS).toInt()
    bump()
}

/** Append a punctuation token */
private fun appendToken(startByte: Int, tType: PunctuationToken) {
    ensureSpaceForToken()
    currChunk.tokens[nextInd    ] = (tType.internalVal.toInt() shl 27)
    currChunk.tokens[nextInd + 1] = startByte
    bump()
}


/** The programmatic/builder method for appending a punctuation token */
private fun appendToken(startByte: Int, lenBytes: Int, tType: PunctuationToken, lenTokens: Int) {
    ensureSpaceForToken()
    checkLenOverflow(lenBytes)
    currChunk.tokens[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
    currChunk.tokens[nextInd + 1] = startByte
    currChunk.tokens[nextInd + 3] = lenTokens
    bump()
}


private fun bump() {
    nextInd += 4
    totalTokens += 1
}

fun setInput(inp: ByteArray) {
    this.inp = inp
}


fun nextToken() {
    currInd += 4
    if (currInd == CHUNKSZ) {
        currChunk = currChunk.next!!
        currInd = 0
    }
}

fun currTokenType(): Int {
    return currChunk.tokens[currInd] ushr 27
}

fun nextTokenType(skip: Int): Int {
    var nextInd = currInd + 4*skip
    var nextChunk = currChunk
    while (nextInd >= CHUNKSZ) {
        nextChunk = currChunk.next!!
        nextInd -= CHUNKSZ
    }
    return nextChunk.tokens[nextInd] ushr 27
}


init {
    inp = byteArrayOf()
    currChunk = firstChunk
    i = 0
    nextInd = 0
    currInd = 0
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
        dispatchTable[aParenLeft.toInt()] = Lexer::lexParenLeft
        dispatchTable[aParenRight.toInt()] = Lexer::lexParenRight
        dispatchTable[aDollar.toInt()] = Lexer::lexDollar
        dispatchTable[aCurlyLeft.toInt()] = Lexer::lexCurlyLeft
        dispatchTable[aCurlyRight.toInt()] = Lexer::lexCurlyRight
        dispatchTable[aBracketLeft.toInt()] = Lexer::lexBracketLeft
        dispatchTable[aBracketRight.toInt()] = Lexer::lexBracketRight

        dispatchTable[aSpace.toInt()] = Lexer::lexSpace
        dispatchTable[aCarriageReturn.toInt()] = Lexer::lexSpace
        dispatchTable[aNewline.toInt()] = Lexer::lexNewline
        dispatchTable[aSemicolon.toInt()] = Lexer::lexStatementTerminator

        dispatchTable[aQuote.toInt()] = Lexer::lexStringLiteral
        dispatchTable[aSharp.toInt()] = Lexer::lexComment
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

    fun isStatement(tType: PunctuationToken): Boolean {
        return tType == statementTypeDecl || tType == statementFun || tType == statementAssignment
    }


    fun isStatement(tType: Int): Boolean {
        return tType == statementTypeDecl.internalVal.toInt()
                || tType == statementFun.internalVal.toInt()
                || tType == statementAssignment.internalVal.toInt()
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
        if (typeBits <= 8) {
            val regType = RegularToken.values().firstOrNull { it.internalVal == typeBits }
            if (regType != litFloat) {
                val payload: Long = (chunk.tokens[ind + 2].toLong() shl 32) + chunk.tokens[ind + 3].toLong()
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            } else {
                val payload: Double = Double.fromBits(
                    (chunk.tokens[ind + 2].toLong() shl 32) + chunk.tokens[ind + 3].toLong()
                )
                wr.append("$regType [${startByte} ${lenBytes}] $payload")
            }
        } else {
            val punctType = PunctuationToken.values().firstOrNull { it.internalVal == typeBits }
            val lenTokens = chunk.tokens[ind + 3]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        }
    }
}

}