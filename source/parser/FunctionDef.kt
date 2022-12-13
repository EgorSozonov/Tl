package parser

import java.util.*
import kotlin.collections.ArrayList


class FunctionDef {
    var arity: Int = 0
        private set
    var precedence: Int = 0
        private set
    val backtrack: Stack<ParseFrame> = Stack<ParseFrame>()
    val subscopes: Stack<LexicalScope> = Stack<LexicalScope>()
    val subexprs: Stack<ArrayList<Subexpr>> = Stack()
    val structScope: LexicalScope = LexicalScope()
    val scratch: ScratchChunk = ScratchChunk()
    var currChunk: ScratchChunk
    var nextInd: Int                                             // Next ind inside the current token array
        private set
    var totalNodes: Int
        private set

    fun setArityPrec(arity: Int, prec: Int) {
        this.arity = arity
        this.precedence = prec
    }
    
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


    fun appendExtent(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = 0
        currChunk.nodes[nextInd + 3] = lenTokens
        bump()
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
        currChunk = scratch
        nextInd = 0
        totalNodes = 0
    }
}