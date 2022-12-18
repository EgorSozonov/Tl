package codegen
import lexer.FileType
import parser.FrameAST
import parser.Parser
import parser.FrameAST.*;

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
    val entryPoint = ast.getFunc(pr.indFirstFunction)
    ast.seek(entryPoint.bodyId)
    val entryPointLenNodes = ast.currChunk.nodes[ast.currInd + 3]
    backtrack.add(CodegenFrame(functionDef, -1, 0, entryPoint.bodyId, entryPointLenNodes))

    while (ast.currTokInd != (entryPoint.bodyId + entryPointLenNodes + 1)) {
        val extentType = ast.currNodeType()
        val lenNodes = ast.currChunk.nodes[ast.currInd + 3]
        val fr = backtrack.last()
        if (extentType == functionDef.internalVal.toInt()) {
            writeEntryPointSignature(fr, lenNodes, wr)
        } else if (extentType == scope.internalVal.toInt()) {
            writeScope(fr, lenNodes, wr)
        } else if (extentType == expression.internalVal.toInt()) {
            writeExpression(fr, lenNodes, wr)
        } else if (extentType == statementAssignment.internalVal.toInt()) {
            writeAssignment(fr, lenNodes, wr)
        } else if (extentType == fnDefPlaceholder.internalVal.toInt()) {
            writeFnSignature(fr, lenNodes, wr)
        } else if (extentType == returnExpression.internalVal.toInt()) {
            writeReturn(fr, lenNodes, wr)
        } else {
            throw Exception()
        }
        maybeCloseFrames(wr)
    }

}

private fun pushNewFrame(nameId: Int, arity: Int) {
    val eType = FrameAST.values().first {it.internalVal.toInt() == ast.currChunk.nodes[ast.currInd] ushr 27 }
    val lenNodes = ast.currChunk.nodes[ast.currInd + 3]
    backtrack.add(CodegenFrame(eType, nameId, arity, ast.currTokInd, lenNodes))
}


private fun writeEntryPointSignature(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append("function ")
    wr.append(entryPointName)
    wr.append("() ")

    ast.nextNode() // Skipping the "functionDef" node
    fr.currNode = ast.currTokInd
}


private fun writeFnSignature(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    val funcId = ast.currChunk.nodes[ast.currInd + 2]
    val func = ast.getFunc(funcId)
    ast.nextNode() // fnDefPlaceholder
    fr.currNode = ast.currTokInd
    ast.seek(func.bodyId)
    pushNewFrame(func.nameId, func.arity)

    val indentation = " ".repeat(indentDepth*4)
    val funcName = ast.identifiers[func.nameId]
    wr.append(indentation)
    wr.append("function ")
    wr.append(funcName)
    wr.append("(")

    ast.nextNode() // Skipping the "functionDef" node
    for (i in 0 until func.arity) {
        wr.append(ast.getString(ast.currChunk.nodes[ast.currInd + 2]))
        if (i == func.arity - 1) {
            wr.append(")")
        } else {
            wr.append(", ")
        }
        ast.nextNode()
    }

    backtrack.last().currNode = ast.currTokInd
}

/**
 * When we are at the end of a function parsing a parse frame, we might be at the end of said frame
 * (if we are not => we've encountered a nested frame, like in "1 + { x = 2; x + 1}"),
 * in which case this function handles all the corresponding stack poppin'.
 * It also always handles updating all inner frames with consumed tokens.
 */
private fun maybeCloseFrames(wr: StringBuilder) {
    while (true) {
        if (backtrack.isEmpty()) return

        var nodesToAdd = 0

        var i = backtrack.size - 1
        while (i > -1) {
            val frame = backtrack[i]
            frame.currNode += nodesToAdd
            if (frame.currNode < frame.sentinelNode) {
                return
            } else {
                closeFrame(frame, wr)
                backtrack.removeLast()
                if (frame.eType == functionDef) {
                    if (backtrack.isNotEmpty()) {
                        ast.seek(backtrack.last().currNode) // skipping to the node after the fnDefPlaceholder
                    } else {
                        ast.seek(frame.currNode) // skipping to the node after the fnDefPlaceholder
                    }
                    nodesToAdd = 0
                } else {
                    nodesToAdd = frame.lenNodes
                }
            }
            i--
        }
    }
}


private fun moveForward(numNodes: Int, fr: CodegenFrame) {
    ast.skipNodes(numNodes)
    fr.currNode = ast.currTokInd
}


private fun writeExpression(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))
    wr.append("expression;\n")
    moveForward(lenNodes + 1, fr)
}

    
private fun writeAssignment(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))
    wr.append("assignment;\n")
    moveForward(lenNodes + 1, fr)
}


private fun writeReturn(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))
    wr.append("return;\n")
    moveForward(lenNodes + 1, fr)
}


private fun writeScope(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))
    wr.append("{\n")
    if (fr.eType != functionDef || fr.nameId > -1) {
        indentDepth += 4
    }

    pushNewFrame(-1, -1)
    ast.skipNodes(1)
    backtrack.last().currNode = ast.currTokInd

}


private fun closeFrame(fr: CodegenFrame, wr: StringBuilder) {
    if (fr.eType == functionDef) {

    } else if (fr.eType == scope) {
        wr.append(" ".repeat(indentDepth))
        wr.append("}\n")
        indentDepth -= 4
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

