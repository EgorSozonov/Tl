package parser
import lexer.CHUNKSZ
import utils.LOWER27BITS
import utils.LOWER32BITS
import java.lang.StringBuilder


/**
 * AST consists of a functions array, a types array a string array, a numerical constants array (?).
 */
class AST {
    /** 4-int32 elements of [
     *     (type) u5
     *     (lenBytes) i27
     *     (startByte) i32
     *     (payload1) i32
     *     (payload2) i32
     * ]
     * A functionBody starts with the list of identIds of the parameter names.
     * Then come the statements, expressions, scopes of the body.
     * */
    private val functionBodies = ASTChunk()
    var currChunk: ASTChunk                                    // Last array of tokens
        private set
    var nextInd: Int                                      // Next ind inside the current token array
        private set
    var totalNodes: Int
        private set

    /**
     * The unique string storage for identifier names
     */
    val identifiers = ArrayList<String>(50)

    /** 4-int32 elements of [
     *     (name: id in 'identifiers') i32
     *     (arity) i32
     *     (type) i32
     *     (body: ind in 'functionBodies') i32
     * ]  */
    private val functions = ASTChunk()
    var funcCurrChunk: ASTChunk                                    // Last array of tokens
        private set
    var funcNextInd: Int                                      // Next ind inside the current token array
        private set
    var funcTotalNodes: Int
        private set


    fun currentFunc(): FunctionSignature {
        return FunctionSignature(funcCurrChunk.nodes[funcNextInd], funcCurrChunk.nodes[funcNextInd + 1],
                                 funcCurrChunk.nodes[funcNextInd + 2], funcCurrChunk.nodes[funcNextInd + 3])
    }

    fun funcNode(nameId: Int, arity: Int, typeId: Int) {
        ensureSpaceForFunc()
        funcCurrChunk.nodes[funcNextInd    ] = nameId
        funcCurrChunk.nodes[funcNextInd + 1] = arity
        funcCurrChunk.nodes[funcNextInd + 2] = typeId
        bumpFunc()
    }


    private fun ensureSpaceForFunc() {
        if (funcNextInd < (CHUNKSZ - 4)) return

        val newChunk = ASTChunk()
        funcCurrChunk.next = newChunk
        funcCurrChunk = newChunk
        funcNextInd = 0
    }

    private fun bumpFunc() {
        funcNextInd += 4
        funcTotalNodes++
    }

    fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = payload1
        currChunk.nodes[nextInd + 3] = payload2
        bump()
    }


    fun appendExtent(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = 0
        currChunk.nodes[nextInd + 3] = lenTokens
        bump()
    }

    /**
     * Finds the top-level punctuation opener by its index, and sets its node length.
     * Called when the parsing of punctuation is finished.
     */
    fun setPunctuationLength(nodeInd: Int) {
        var curr = functionBodies
        var j = nodeInd * 4
        while (j >= CHUNKSZ) {
            curr = curr.next!!
            j -= CHUNKSZ
        }

        curr.nodes[j + 3] = totalNodes - nodeInd - 1
    }


    private fun bump() {
        nextInd += 4
        totalNodes++
    }


    private fun ensureSpaceForNode() {
        if (nextInd < (CHUNKSZ - 4)) return

        val newChunk = ASTChunk()
        currChunk.next = newChunk
        currChunk = newChunk
        nextInd = 0
    }


    /**
     * Precondition: all function IDs in the AST are indices into 'functionsIndex'.
     * Post-condition: all function IDs in the AST are indices into 'functions'.
     */
    private fun fixupFunctionIds() {

    }


    init {
        currChunk = functionBodies
        nextInd = 0
        totalNodes = 0

        funcCurrChunk = functions
        funcNextInd = 0
        funcTotalNodes = 0
    }
    
    companion object {
        /**
         * Equality comparison for ASTs.
         */
        fun equality(a: AST, b: AST): Boolean {
            if (a.totalNodes != b.totalNodes) {
                return false
            }
            var currA: ASTChunk? = a.functionBodies
            var currB: ASTChunk? = b.functionBodies
            while (currA != null) {
                if (currB == null) {
                    return false
                }
                val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
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
            var currA: ASTChunk? = a.functionBodies
            var currB: ASTChunk? = b.functionBodies
            while (true) {
                if (currA != null) {
                    if (currB != null) {
                        val lenA = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                        val lenB = if (currB == b.currChunk) { b.nextInd } else { CHUNKSZ }
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
                        val len = if (currA == a.currChunk) { a.nextInd } else { CHUNKSZ }
                        for (i in 0 until len step 4) {
                            printNode(currA, i, result)
                            result.appendLine(" | ")
                        }
                    }
                    currA = currA.next
                } else if (currB != null) {
                    val len = if (currB == b.currChunk) { b.nextInd } else { CHUNKSZ }
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
