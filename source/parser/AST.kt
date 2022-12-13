package parser

import lexer.CHUNKSZ
import lexer.LOWER27BITS
import lexer.LOWER32BITS
import java.lang.StringBuilder


/**
 * AST consists of a functions array, a types array a string array, a numerical constants array (?).
 */
class AST {
    private val functions = ASTChunk()
    var currChunk: ASTChunk                                    // Last array of tokens
        private set
    var funcNextInd: Int                                      // Next ind inside the current token array
        private set
    var funcTotalNodes: Int
        private set
    private val functionsIndex = ArrayList<Int>(50)

    val identifiers = ArrayList<String>(50)

    private val imports = ASTChunk()
    var currImpChunk: ASTChunk                                    // Last array of tokens
        private set
    var impNextInd: Int                                      // Next ind inside the current token array
        private set
    var impTotalNodes: Int
        private set

    /**
     * Finds the top-level punctuation opener by its index, and sets its node length.
     * Called when the parsing of punctuation is finished.
     */
    fun setPunctuationLength(nodeInd: Int) {
        var curr = functions
        var j = nodeInd * 4
        while (j >= CHUNKSZ) {
            curr = curr.next!!
            j -= CHUNKSZ
        }

        curr.nodes[j + 3] = funcTotalNodes - nodeInd - 1
    }


    fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[funcNextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[funcNextInd + 1] = startByte
        currChunk.nodes[funcNextInd + 2] = payload1
        currChunk.nodes[funcNextInd + 3] = payload2
        bump()
    }


    fun appendExtent(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[funcNextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[funcNextInd + 1] = startByte
        currChunk.nodes[funcNextInd + 2] = 0
        currChunk.nodes[funcNextInd + 3] = lenTokens
        bump()
    }




    fun buildNode(nType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int): AST {
        appendNode(nType, payload1, payload2, startByte, lenBytes)
        return this
    }


    fun buildNode(payload: Double, startByte: Int, lenBytes: Int): AST {
        val payloadAsLong = payload.toBits()
        appendNode(RegularAST.litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(), startByte, lenBytes)
        return this
    }


    fun buildNode(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int): AST {
        appendExtent(nType, lenTokens, startByte, lenBytes)
        return this
    }


    private fun bump() {
        funcNextInd += 4
        funcTotalNodes++
    }


    private fun ensureSpaceForNode() {
        if (funcNextInd < (CHUNKSZ - 4)) return

        val newChunk = ASTChunk()
        currChunk.next = newChunk
        currChunk = newChunk
        funcNextInd = 0
    }

    /**
     * Precondition: all function IDs in the AST are indices into 'functionsIndex'.
     * Post-condition: all function IDs in the AST are indices into 'functions'.
     */
    private fun fixupFunctionIds() {

    }


    init {
        currChunk = functions
        funcNextInd = 0
        funcTotalNodes = 0

        currImpChunk = imports
        impNextInd = 0
        impTotalNodes = 0
    }
    
    companion object {
        /**
         * Equality comparison for ASTs.
         */
        fun equality(a: AST, b: AST): Boolean {
            if (a.funcTotalNodes != b.funcTotalNodes) {
                return false
            }
            var currA: ASTChunk? = a.functions
            var currB: ASTChunk? = b.functions
            while (currA != null) {
                if (currB == null) {
                    return false
                }
                val len = if (currA == a.currChunk) { a.funcNextInd } else { CHUNKSZ }
                for (i in 0 until len) {
                    if (currA.nodes[i] != currB.nodes[i]) {
                        return false
                    }
                }
                currA = currA.next
                currB = currB.next
            }
            if (a.identifiers.size != b.identifiers.size) {
                return false
            }
            for (i in 0 until a.identifiers.size) {
                if (a.identifiers[i] != b.identifiers[i]) {
                    return false
                }
            }

            return true
        }


        /**
         * Pretty printer function for debugging purposes
         */
        fun printSideBySide(a: AST, b: AST, result: StringBuilder): String {
            var currA: ASTChunk? = a.functions
            var currB: ASTChunk? = b.functions
            while (true) {
                if (currA != null) {
                    if (currB != null) {
                        val lenA = if (currA == a.currChunk) { a.funcNextInd } else { CHUNKSZ }
                        val lenB = if (currB == b.currChunk) { b.funcNextInd } else { CHUNKSZ }
                        val len = lenA.coerceAtMost(lenB)
                        for (i in 0 until len step 4) {
                            printNode(currA, i, result)
                            result.append(" | ")
                            printNode(currB, i, result)
                            result.appendLine("")
                        }
                        for (i in len until lenA step 4) {
                            printNode(currA, i, result)
                            result.appendLine(" | ")
                        }
                        for (i in len until lenB step 4) {
                            result.append(" | ")
                            printNode(currB, i, result)
                            result.appendLine("")
                        }
                        currB = currB.next
                    } else {
                        val len = if (currA == a.currChunk) { a.funcNextInd } else { CHUNKSZ }
                        for (i in 0 until len step 4) {
                            printNode(currA, i, result)
                            result.appendLine(" | ")
                        }
                    }
                    currA = currA.next
                } else if (currB != null) {
                    val len = if (currB == b.currChunk) { b.funcNextInd } else { CHUNKSZ }
                    for (i in 0 until len step 4) {
                        result.append(" | ")
                        printNode(currB, i, result)
                        result.appendLine("")
                    }
                    currB = currB.next
                } else {
                    break
                }


            }
            return result.toString()
        }


        private fun printNode(chunk: ASTChunk, ind: Int, wr: StringBuilder) {
            val startByte = chunk.nodes[ind + 1]
            val lenBytes = chunk.nodes[ind] and LOWER27BITS
            val typeBits = (chunk.nodes[ind] ushr 27).toByte()
            if (typeBits <= 10) {
                val regType = RegularAST.values().firstOrNull { it.internalVal == typeBits }
                if (regType != RegularAST.litFloat) {
                    val payload: Long = (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                    wr.append("$regType [${startByte} ${lenBytes}] $payload")
                } else {
                    val payload: Double = Double.fromBits(
                        (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                    )
                    wr.append("$regType [${startByte} ${lenBytes}] $payload")
                }
            } else {
                val punctType = FrameAST.values().firstOrNull { it.internalVal == typeBits }
                val lenTokens = chunk.nodes[ind + 3]
                wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
            }
        }
    }
}
