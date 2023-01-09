package utils

import lexer.CHUNKSZ
import lexer.RegularToken
import parser.ASTChunk
import parser.RegularAST
import parser.SpanAST

class FourIntStorage {
    val firstChunk = ASTChunk()
    var currChunk: ASTChunk                                    // Last array of tokens
    var nextInd: Int                                           // Next ind inside the current token array
    var currInd: Int = 0
    var currNodeInd: Int = 0
    var totalNodes: Int

    init {
        currChunk = firstChunk
        nextInd = 0
        totalNodes = 0
    }


    fun nextNode() {
        currInd += 4
        currNodeInd++
        if (currInd == CHUNKSZ) {
            currChunk = currChunk.next!!
            currInd = 0
        }
    }


    fun skipNodes(toSkip: Int) {
        currInd += 4*toSkip
        currNodeInd += toSkip
        if (currInd == CHUNKSZ) {
            currChunk = currChunk.next!!
            currInd = 0
        }
    }


    fun currNodeType(): Int {
        return currChunk.nodes[currInd] ushr 27
    }


    fun bump() {
        nextInd += 4
        totalNodes++
        if (nextInd >= CHUNKSZ) {
            currChunk = currChunk.next!!
            nextInd = 0
        }
    }


    fun ensureSpaceForNode() {
        if (nextInd < CHUNKSZ) return

        val newChunk = ASTChunk()
        currChunk.next = newChunk
        currChunk = newChunk
        nextInd = 0
    }

    fun seek(bodyId: Int) {
        currInd = bodyId*4
        currChunk = firstChunk
        while (currInd >= CHUNKSZ) {
            currChunk = currChunk.next!!
            currInd -= CHUNKSZ
        }
        currNodeInd = bodyId
    }

    /**
     * When a function header is created, index of its body within 'functionBodies' is not known as it's
     * written into the scratch space first. This function sets this index when the function body is complete.
     */
    fun setFourth(funcId: Int, fourth: Int) {
        var i = funcId*4
        var tmp = firstChunk
        while (i >= CHUNKSZ) {
            tmp = tmp.next!!
            i -= CHUNKSZ
        }
        tmp.nodes[i + 3] = fourth
    }


    fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = payload1
        currChunk.nodes[nextInd + 3] = payload2
        bump()
    }

    /** Append a 64-bit literal int node */
    private fun appendLitIntToken(payload: Long, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (RegularToken.intTok.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = (payload shr 32).toInt()
        currChunk.nodes[nextInd + 3] = (payload and LOWER32BITS).toInt()
        bump()
    }

    /** Append a floating-point literal */
    private fun appendFloatToken(payload: Double, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        val asLong: Long = payload.toBits()
        currChunk.nodes[nextInd    ] = (RegularToken.floatTok.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = (asLong shr 32).toInt()
        currChunk.nodes[nextInd + 3] = (asLong and LOWER32BITS).toInt()
        bump()
    }


    fun appendSpan(nType: SpanAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = 0
        currChunk.nodes[nextInd + 3] = lenTokens
        bump()
    }

    fun appendThree(first: Int, second: Int, third: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = first
        currChunk.nodes[nextInd + 1] = second
        currChunk.nodes[nextInd + 2] = third
        bump()
    }

    fun appendFirst(first: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = first
        bump()
    }

    fun appendFirstLast(first: Int, last: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = first
        currChunk.nodes[nextInd + 3] = last
        bump()
    }

    fun build(first: Int, second: Int, third: Int, fourth: Int): FourIntStorage {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = first
        currChunk.nodes[nextInd + 1] = second
        currChunk.nodes[nextInd + 2] = third
        currChunk.nodes[nextInd + 3] = fourth
        bump()
        return this
    }

    fun buildFirstLast(first: Int, last: Int): FourIntStorage {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = first
        currChunk.nodes[nextInd + 3] = last
        bump()
        return this
    }

    fun lookaheadType(nodesToSkip: Int): Int {
        var ind = (currNodeInd + nodesToSkip)*4
        var chunk = currChunk
        while (ind > CHUNKSZ) {
            chunk = chunk.next!!
            ind -= CHUNKSZ
        }
        return chunk.nodes[ind] ushr 27
    }
}