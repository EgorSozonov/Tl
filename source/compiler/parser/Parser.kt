package compiler.parser
import compiler.lexer.*
import java.lang.StringBuilder
import java.util.*
import kotlin.collections.ArrayList

class Parser {


var firstChunk: ParseChunk = ParseChunk()                        // First array of tokens
    private set
var currChunk: ParseChunk                                      // Last array of tokens
    private set
var nextInd: Int                                             // Next ind inside the current token array
    private set
var totalASTs: Int
    private set
var wasError: Boolean                                        // The lexer's error flag
    private set
var errMsg: String
    private set
private var i: Int                                           // current index inside input byte array
private val backtrack = Stack<Pair<PunctuationAST, Int>>() // The stack of punctuation scopes
private val symbolBacktrack = Stack<Map<String, Binding>>() // The stack of symbol tables that form current lex env
var bindings: List<Binding>
var scopes: List<LexicalScope>

/**
 * Main parser method
 */
fun parse(inp: Lexer) {
    var currToken = Token(0, 0, 0, 0L)
    /**
     * fn foo x y {
     *     x + 2*y
     * }
     *
     * FunDef
     * Word
     * ParamNames: Word+ or _
     * FunBody: { stmt ... }
     *
     *
     * print $ "bar 1 2 3 = " + (bar 1 2 3) .toString
     * print $ foo 5 10
     */
}

init {
    currChunk = firstChunk
    i = 0
    nextInd = 0
    totalASTs = 0
    wasError = false
    errMsg = ""
    bindings = ArrayList(10)
    scopes = ArrayList(10)
}

companion object {
    /**
     * Equality comparison for parsers.
     */
    fun equality(a: Parser, b: Parser): Boolean {
        if (a.wasError != b.wasError || a.totalASTs != b.totalASTs || a.nextInd != b.nextInd
            || a.errMsg != b.errMsg) {
            return false
        }
        var currA: ParseChunk? = a.firstChunk
        var currB: ParseChunk? = b.firstChunk
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

        return true
    }


    /**
     * Pretty printer function for debugging purposes
     */
    fun printSideBySide(a: Parser, b: Parser): String {
        val result = StringBuilder()
        result.appendLine("Result | Expected")
        result.appendLine("${if (a.wasError) {a.errMsg} else { "OK" }} | ${if (b.wasError) {b.errMsg} else { "OK" }}")
        result.appendLine("astType [startByte lenBytes] (payload/lenTokens)")
        var currA: ParseChunk? = a.firstChunk
        var currB: ParseChunk? = b.firstChunk
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


    private fun printToken(chunk: ParseChunk, ind: Int, wr: StringBuilder) {
        val startByte = chunk.tokens[ind + 1]
        val lenBytes = chunk.tokens[ind] and LOWER27BITS
        val typeBits = (chunk.tokens[ind] ushr 27).toByte()
        if (typeBits <= 8) {
            val regType = RegularToken.values().firstOrNull { it.internalVal == typeBits }
            if (regType != RegularToken.litFloat) {
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
            val lenTokens = chunk.tokens[ind + 2]
            wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
        }
    }
}
}