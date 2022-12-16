package parser
import lexer.CHUNKSZ
import utils.LOWER24BITS
import utils.LOWER27BITS
import java.lang.StringBuilder


/**
 * AST consists of a functions array, a types array a string array, a numerical constants array (?).
 */
class AST {
    /**
     * The unique string storage for identifier names
     */
    val identifiers = ArrayList<String>(50)

    /** 4-int32 elements of [
     *     (nameId: ind in 'identifiers') i32
     *     (arity) i32
     *     (type) i32
     *     (bodyId: ind in 'functionBodies') i32
     * ]  */
    val functions = ASTChunk()
    var funcCurrChunk: ASTChunk                               // Last array of tokens
        private set
    var funcNextInd: Int                                      // Next ind inside the current token array
        private set
    var funcCurrInd: Int = 0
        private set
    var funcTotalNodes: Int
        private set

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
    var currInd: Int = 0
        private set
    var currTokInd: Int = 0
        private set
    var totalNodes: Int
        private set


    fun getFunc(funcId: Int): FunctionSignature {
        funcCurrInd = funcId*4
        funcCurrChunk = functions
        while (funcCurrInd >= CHUNKSZ) {
            funcCurrInd -= CHUNKSZ
            funcCurrChunk = funcCurrChunk.next!!
        }
        val arity = funcCurrChunk.nodes[funcCurrInd + 1]
        val typeId = funcCurrChunk.nodes[funcCurrInd + 2]
        return FunctionSignature(funcCurrChunk.nodes[funcCurrInd], arity,
                                 typeId, funcCurrChunk.nodes[funcCurrInd + 3])
    }


    fun funcNode(nameId: Int, arity: Int, typeId: Int) {
        ensureSpaceForFunc()
        funcCurrChunk.nodes[funcNextInd    ] = nameId
        funcCurrChunk.nodes[funcNextInd + 1] = arity
        funcCurrChunk.nodes[funcNextInd + 2] = typeId
        bumpFunc()
    }


    fun funcSeek(funcId: Int) {
        funcCurrInd = funcId*4
        funcCurrChunk = functions
        while (funcCurrInd >= CHUNKSZ) {
            funcCurrChunk = funcCurrChunk.next!!
            funcCurrInd -= CHUNKSZ
        }
    }


    fun funcNext() {
        funcCurrInd += 4
        if (funcNextInd >= CHUNKSZ) {
            funcCurrChunk = funcCurrChunk.next!!
            funcNextInd
        }
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


    fun currentFunc(): FunctionSignature {
        val arity = funcCurrChunk.nodes[funcNextInd + 2] ushr 24
        val typeId = funcCurrChunk.nodes[funcNextInd + 2] and LOWER24BITS
        return FunctionSignature(funcCurrChunk.nodes[funcNextInd], arity,
                                 typeId, funcCurrChunk.nodes[funcNextInd + 3])
    }

    /**
     * When a function header is created, index of its body within 'functionBodies' is not known as it's
     * written into the scratch space first. This function sets this index when the function body is complete.
     */
    private fun setBodyId(funcId: Int, bodyInd: Int) {
        var i = funcId*4
        var tmp = functions
        while (i >= CHUNKSZ) {
            tmp = tmp.next!!
            i -= CHUNKSZ
        }
        tmp.nodes[i + 3] = bodyInd
    }



    fun seek(bodyId: Int) {
        currInd = bodyId*4
        currChunk = functionBodies
        while (currInd >= CHUNKSZ) {
            currChunk = currChunk.next!!
            currInd -= CHUNKSZ
        }
        currTokInd = bodyId
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


    fun nextNode() {
        currInd += 4
        if (currInd == CHUNKSZ) {
            currChunk = currChunk.next!!
            currInd = 0
        }
    }

    fun skipNodes(toSkip: Int) {
        currInd += 4*toSkip
        if (currInd == CHUNKSZ) {
            currChunk = currChunk.next!!
            currInd = 0
        }
    }


    fun currNodeType(): Int {
        return currChunk.nodes[currInd] ushr 27
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
     * Copies the nodes from the functionDef's scratch space to the AST proper.
     * This happens only after the functionDef is complete.
     */
    fun storeFreshFunction(freshlyMintedFn: FunctionDef) {
        var src = freshlyMintedFn.firstChunk
        var tgt = currChunk

        val srcLast = freshlyMintedFn.currChunk
        val srcIndLast = freshlyMintedFn.nextInd
        val bodyInd = totalNodes

        // setting the token length of the function body because it's only known now
        freshlyMintedFn.firstChunk.nodes[3] = freshlyMintedFn.totalNodes - 1

        while (src != srcLast) {
            val spaceInTgt = (CHUNKSZ - nextInd)
            if (SCRATCHSZ < spaceInTgt) {
                src.nodes.copyInto(tgt.nodes, nextInd, 0)
                nextInd += SCRATCHSZ
            } else {
                val toCopy = SCRATCHSZ.coerceAtMost(spaceInTgt)
                src.nodes.copyInto(tgt.nodes, nextInd, 0, toCopy)
                tgt = tgt.next!!
                src.nodes.copyInto(tgt.nodes, 0, toCopy)
                nextInd = SCRATCHSZ - toCopy
            }
            src = src.next!!
        }

        val spaceInTgt = (CHUNKSZ - nextInd)
        if (srcIndLast < spaceInTgt) {
            src.nodes.copyInto(tgt.nodes, nextInd, 0)
            nextInd += srcIndLast
        } else {
            val toCopy = srcIndLast.coerceAtMost(spaceInTgt)
            src.nodes.copyInto(tgt.nodes, nextInd, 0, toCopy)
            currChunk = currChunk.next!!
            src.nodes.copyInto(tgt.nodes, 0, toCopy)
            nextInd = srcIndLast - toCopy
        }
        this.totalNodes += freshlyMintedFn.totalNodes

        setBodyId(freshlyMintedFn.funcId, bodyInd)
    }


    /**
     * This is called at the end of parse to walk over and remove indirection from all
     * function ids in all function calls.
     * Precondition: all function IDs in the AST are indices into 'functionsIndex'.
     * Post-condition: all function IDs in the AST are indices into 'functions'.
     */
    private fun fixupFunctionIds() {

    }


    fun getString(stringId: Int): String {
        return identifiers[stringId]
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
            if (a.totalNodes != b.totalNodes || a.funcTotalNodes != b.funcTotalNodes) {
                return false
            }

            if (!equalityFuncs(a, b)) return false
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

        private fun equalityFuncs(a: AST, b: AST): Boolean {
            var currA: ASTChunk? = a.functions
            var currB: ASTChunk? = b.functions
            while (currA != null) {
                if (currB == null) {
                    return false
                }
                val len = if (currA == a.funcCurrChunk) { a.nextInd } else { CHUNKSZ }
                for (i in 0 until len) {
                    if (currA.nodes[i] != currB.nodes[i]) {
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
