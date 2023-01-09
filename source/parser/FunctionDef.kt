package parser
import java.util.*
import kotlin.collections.ArrayList


class FunctionDef(var funcId: Int = 0, var arity: Int = 0, val precedence: Int) {
    val spanStack: Stack<ParseFrame> = Stack<ParseFrame>()
    val subscopes: ArrayList<LexicalScope> = ArrayList<LexicalScope>()
    val subexprs: ArrayList<ArrayList<Subexpr>> = ArrayList()
    val structScope: LexicalScope = LexicalScope()

    val firstChunk: ScratchChunk = ScratchChunk()
    var currChunk: ScratchChunk
    var nextInd: Int                                             // Next ind inside the current token array
        private set
    var totalNodes: Int
        private set


    fun openScope(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(SpanAST.scope, 0, startByte, lenBytes)
        subscopes.add(LexicalScope())
        val newFrame = ParseFrame(SpanAST.scope, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
    }

    fun openScopeFromExisting(startNodeInd: Int, sentinelToken: Int, existingScope: LexicalScope, startByte: Int, lenBytes: Int) {
        appendSpan(SpanAST.scope, 0, startByte, lenBytes)
        subscopes.add(existingScope)
        val newFrame = ParseFrame(SpanAST.scope, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
    }


    fun openExpression(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(SpanAST.expression, 0, startByte, lenBytes)
        val newFrame = ParseFrame(SpanAST.expression, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
    }

    /** A span that is neither a scope nor an expression. Emits a node. Example: the "loop" span */
    fun openContainerSpan(spanType: SpanAST, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        val newFrame = ParseFrame(spanType, totalNodes, sentinelToken)
        appendSpan(spanType, 0, startByte, lenBytes)
        spanStack.push(newFrame)
    }

    fun openAssignment(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(SpanAST.statementAssignment, 0, startByte, lenBytes)
        val newFrame = ParseFrame(SpanAST.statementAssignment, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
    }


    fun openRetExpression(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(SpanAST.returnExpression, 0, startByte, lenBytes)
        val newFrame = ParseFrame(SpanAST.returnExpression, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
    }

    fun openTypeDecl(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(SpanAST.typeDecl, 0, startByte, lenBytes)
        val newFrame = ParseFrame(SpanAST.typeDecl, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
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


    fun appendSpan(nType: SpanAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = 0
        currChunk.nodes[nextInd + 3] = lenTokens
        bump()
    }

    fun appendSpanWithPayload(nType: SpanAST, payload: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[nextInd + 1] = startByte
        currChunk.nodes[nextInd + 2] = payload
        currChunk.nodes[nextInd + 3] = lenTokens
        bump()
    }

    /**
     * Finds the top-level punctuation opener by its index, and sets its node length.
     * Called when the parsing of a span is finished.
     */
    fun setSpanLength(nodeInd: Int) {
        var curr = firstChunk
        var j = nodeInd * 4
        while (j >= SCRATCHSZ) {
            curr = curr.next!!
            j -= SCRATCHSZ
        }

        curr.nodes[j + 3] = totalNodes - nodeInd - 1
    }


    fun setSpanLenBytes(nodeInd: Int, sentinelByte: Int) {
        var curr = firstChunk
        var j = nodeInd * 4
        while (j >= SCRATCHSZ) {
            curr = curr.next!!
            j -= SCRATCHSZ
        }

        curr.nodes[j] += (sentinelByte - curr.nodes[j + 1])
        curr.nodes[j + 3] = totalNodes - nodeInd - 1
    }


    fun bump() {
        nextInd += 4
        totalNodes++
    }


    fun ensureSpaceForNode() {
        if (nextInd < SCRATCHSZ) return

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