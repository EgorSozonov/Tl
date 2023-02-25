package parser
import utils.INIT_LIST_SIZE
import java.util.*
import kotlin.collections.ArrayList


class FunctionDef(var funcId: Int = 0, var arity: Int = 0, val precedence: Int) {
    val spanStack: Stack<ParseFrame> = Stack<ParseFrame>()
    val subscopes: ArrayList<LexicalScope> = ArrayList<LexicalScope>()
    val subexprs: ArrayList<ArrayList<Subexpr>> = ArrayList()

    private var cap = INIT_LIST_SIZE*4
    var nodes = IntArray(INIT_LIST_SIZE *4)

    var currInd: Int                                             // Next ind inside the current token array
        private set
    var totalNodes: Int
        private set


    fun openScope(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(nodScope, 0, startByte, lenBytes)
        subscopes.add(LexicalScope())
        val newFrame = ParseFrame(nodScope, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
    }

    fun openScopeFromExisting(startNodeInd: Int, sentinelToken: Int, existingScope: LexicalScope, startByte: Int, lenBytes: Int) {
        appendSpan(nodScope, 0, startByte, lenBytes)
        subscopes.add(existingScope)
        val newFrame = ParseFrame(nodScope, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
    }


    fun openExpression(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(nodExpr, 0, startByte, lenBytes)
        val newFrame = ParseFrame(nodExpr, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
    }

    /** A span that is neither a scope nor an expression. Emits a node. Example: the "loop" span */
    fun openContainerSpan(spanType: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        val newFrame = ParseFrame(spanType, totalNodes, sentinelToken)
        appendSpan(spanType, 0, startByte, lenBytes)
        spanStack.push(newFrame)
    }

    fun openAssignment(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(nodStmtAssignment, 0, startByte, lenBytes)
        val newFrame = ParseFrame(nodStmtAssignment, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
    }


    fun openRetExpression(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(nodReturn, 0, startByte, lenBytes)
        val newFrame = ParseFrame(nodReturn, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
    }

    fun openTypeDecl(startNodeInd: Int, sentinelToken: Int, startByte: Int, lenBytes: Int) {
        appendSpan(nodTypeDecl, 0, startByte, lenBytes)
        val newFrame = ParseFrame(nodTypeDecl, startNodeInd, sentinelToken)
        spanStack.push(newFrame)
        subexprs.add(ArrayList<Subexpr>())
    }


    fun appendNode(tType: Int, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
        add((tType shl 26) + startByte, lenBytes, payload1, payload2)
    }

    /**
     * Append the function's name or any of its parameter names
     */
    fun appendName(stringId: Int, startByte: Int, lenBytes: Int) {
        add((nodId shl 26) + startByte, lenBytes, 0, stringId)
    }


    fun appendSpan(nType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
        add((nType shl 26) + startByte, lenBytes, 0, lenTokens)
    }

    fun appendSpanWithPayload(nType: Int, payload: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
        add((nType shl 26) + startByte, lenBytes, payload, lenTokens)
    }

    /**
     * Finds the top-level punctuation opener by its index, and sets its node length.
     * Called when the parsing of a span is finished.
     */
    fun setSpanLength(nodeInd: Int) {
        val j = nodeInd * 4

        nodes[j + 3] = totalNodes - nodeInd - 1
    }


    fun setSpanLenBytes(nodeInd: Int, sentinelByte: Int) {
        val j = nodeInd * 4
        nodes[j    ] += (sentinelByte - nodes[j + 1])
        nodes[j + 3] = totalNodes - nodeInd - 1
    }


    fun add(i1: Int, i2: Int, i3: Int, i4: Int) {
        currInd += 4
        if (currInd == cap) {
            val newArray = IntArray(2*cap)
            nodes.copyInto(newArray, 0, cap)
            cap *= 2
        }
        nodes[currInd    ] = i1
        nodes[currInd + 1] = i2
        nodes[currInd + 2] = i3
        nodes[currInd + 3] = i4
        totalNodes++
    }


    init {
        currInd = 0
        totalNodes = 0
    }
}