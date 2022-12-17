package codegen
import lexer.FileType
import parser.FrameAST
import parser.Parser
import parser.FrameAST.*;
import parser.errorInconsistentExtent

class Codegen(val pr: Parser) {

private val ast = pr.ast
private var indentDepth = 0
private val backtrack = ArrayList<CodegenFrame>(4)


fun codeGen(): String {
    if (pr.wasError) return ""

    val result = StringBuilder()

    when (pr.getFileType()) {
        FileType.executable -> { codeGenExecutable(result) }
        FileType.library ->    { codeGenLibrary(result) }
        else ->                { codeGenTests(result) }
    }
    return result.toString()
}


private fun codeGenExecutable(wr: StringBuilder) {
    val entryPoint = ast.getFunc(pr.indFirstFunction - 1)
    ast.seek(entryPoint.bodyId)
    val entryPointLenNodes = ast.currChunk.nodes[ast.currInd + 3]
    backtrack.add(CodegenFrame(functionDef, -1, 0, entryPoint.bodyId, entryPointLenNodes))

    while (ast.currTokInd != entryPoint.bodyId + entryPointLenNodes) {
        val extentType = ast.currNodeType()
        val lenNodes = ast.currChunk.nodes[ast.currInd + 3]
        val fr = backtrack.last()
        if (extentType == functionDef.internalVal.toInt()) {
            writeFnSignature(fr, lenNodes, wr)
        } else if (extentType == scope.internalVal.toInt()) {
            writeScope(fr, lenNodes, wr)
        } else if (extentType == expression.internalVal.toInt()) {
            writeExpression(fr, lenNodes, wr)
        } else if (extentType == statementAssignment.internalVal.toInt()) {
            writeAssignment(fr, lenNodes, wr)
        } else if (extentType == fnDefPlaceholder.internalVal.toInt()) {
            moveIntoFunctionDefinition()
        } else if (extentType == returnExpression.internalVal.toInt()) {
            writeReturn(fr, lenNodes, wr)
        }
        maybeCloseFrames(wr)
    }

}

private fun pushNewFrame() {
    val eType = FrameAST.values().first {it.internalVal.toInt() == ast.currChunk.nodes[ast.currInd] ushr 27 }
    val lenNodes = ast.currChunk.nodes[ast.currInd + 3]
    var nameId = -1
    var arity = -1
    if (eType == fnDefPlaceholder) {
        val functionSignature = ast.getFunc(ast.currChunk.nodes[ast.currInd + 2])
        nameId = functionSignature.nameId
        arity = functionSignature.arity
    }
    backtrack.add(CodegenFrame(eType, nameId, arity, ast.currTokInd, lenNodes))
}


private fun moveIntoFunctionDefinition() {
    val newTokenId = ast.currChunk.nodes[ast.currInd + 2]
    ast.seek(newTokenId)
    pushNewFrame()
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
private fun maybeCloseFrames(wr: StringBuilder) {
    if (backtrack.isEmpty()) return

    val topFrame = backtrack.last()
    if (topFrame.currNode < topFrame.sentinelNode) return

    var nodesToAdd = topFrame.lenNodes
    var isFunClose = false

    if (topFrame.eType == functionDef) {
        isFunClose = true
    }
    closeFrame(topFrame, wr)
    backtrack.removeLast()

    var i = backtrack.size - 1
    while (i > -1) {
        val frame = backtrack[i]
        frame.currNode += nodesToAdd
        if (frame.currNode < frame.sentinelNode) {
            break
        } else {
            if (frame.eType == FrameAST.functionDef) {
                isFunClose = true
            }
            closeFrame(topFrame, wr)
            backtrack.removeLast()
            nodesToAdd = frame.lenNodes + 1
        }
        i--
    }

    if (isFunClose && backtrack.isNotEmpty()) {
        ast.seek(backtrack.last().currNode + 1) // skipping to the node after the fnDefPlaceholder
    }
}


private fun writeFnSignature(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    pushNewFrame()

    val indentation = " ".repeat(indentDepth*4)
    val funcName = if (fr.nameId > -1) { ast.identifiers[fr.nameId] } else { entryPointName }
    wr.append(indentation)
    wr.append("function ")
    wr.append(funcName)
    wr.append("(")

    ast.nextNode() // Skipping the "functionDef" node
    var consumedTokens = 1
    for (i in 0 until fr.arity) {
        wr.append(ast.getString(ast.currChunk.nodes[ast.currInd + 2]))
        if (i == fr.arity - 1) {
            wr.append(")\n")
        } else {
            wr.append(", ")
        }
        ast.nextNode()
        consumedTokens++
    }

    backtrack.last().currNode += consumedTokens
}

private fun moveForward(numNodes: Int, fr: CodegenFrame) {
    ast.skipNodes(numNodes)
    fr.currNode += numNodes
}


private fun writeExpression(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append("expression")
    moveForward(lenNodes, fr)
}

    
private fun writeAssignment(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append("assignment")
    moveForward(lenNodes, fr)
}


private fun writeReturn(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append("return")
    moveForward(lenNodes, fr)
}


private fun writeScope(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    pushNewFrame()

    indentDepth += 4
    moveForward(1, fr)
}


private fun closeFrame(fr: CodegenFrame, wr: StringBuilder) {
    if (fr.eType == functionDef) {

    } else if (fr.eType == scope) {
        indentDepth -= 4
        wr.append("}\n")
    } else if (fr.eType == expression) {
        wr.append(";\n")
    } else if (fr.eType == statementAssignment) {
        wr.append(";\n")
    } else if (fr.eType == returnExpression) {
        wr.append(";\n")
    }
}


private fun codeGenLibrary(wr: StringBuilder) {

}


private fun codeGenTests(wr: StringBuilder) {

}




}

