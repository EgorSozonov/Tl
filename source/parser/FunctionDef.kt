package parser

import lexer.CHUNKSZ
import java.util.*
import kotlin.collections.ArrayList


class FunctionDef(var funcId: Int = 0, var arity: Int = 0, val precedence: Int) {
    val backtrack: Stack<ParseFrame> = Stack<ParseFrame>()
    val subscopes: Stack<LexicalScope> = Stack<LexicalScope>()
    val subexprs: Stack<ArrayList<Subexpr>> = Stack()
    val structScope: LexicalScope = LexicalScope()

    val firstChunk: ScratchChunk = ScratchChunk()
    var currChunk: ScratchChunk
    var nextInd: Int                                             // Next ind inside the current token array
        private set
    var totalNodes: Int
        private set

    
    fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = payload1
        currChunk.nodes[nextInd + 3] = payload2
        bump()
    }

    /**
     * Append the function's name or any of its parameter names
     */
    fun appendName(stringId: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (RegularAST.ident.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 3] = stringId
        bump()
    }


    fun appendExtent(nType: ExtentAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = 0
        currChunk.nodes[nextInd + 3] = lenTokens
        bump()
    }

    fun appendExtentWithPayload(nType: ExtentAST, payload: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = payload
        currChunk.nodes[nextInd + 3] = lenTokens
        bump()
    }

    /**
     * Finds the top-level punctuation opener by its index, and sets its node length.
     * Called when the parsing of an extent is finished.
     */
    fun setExtentLength(nodeInd: Int) {
        var curr = firstChunk
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
        if (nextInd < (SCRATCHSZ - 4)) return

        val newChunk = ScratchChunk()
        currChunk.next = newChunk
        currChunk = newChunk
        nextInd = 0
    }

    init {
        currChunk = firstChunk
        nextInd = 0
        totalNodes = 0
    }
}