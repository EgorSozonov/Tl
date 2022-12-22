package codegen
import lexer.FileType
import lexer.operatorFunctionality
import parser.ExtentAST
import parser.Parser
import parser.ExtentAST.*;
import parser.RegularAST.*;
import utils.LOWER27BITS
import utils.LOWER32BITS
import java.util.*
import kotlin.collections.ArrayList
import kotlin.math.abs

class Codegen(private val pr: Parser) {

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
    backtrack.add(CodegenFrame(functionDef, 0, entryPoint.bodyId, entryPointLenNodes))

    while (ast.currNodeInd != (entryPoint.bodyId + entryPointLenNodes + 1)) {
        val extentType = ast.currNodeType()
        val lenNodes = ast.currChunk.nodes[ast.currInd + 3]
        val fr = backtrack.last()
        if (extentType == functionDef.internalVal.toInt()) {
            writeEntryPointSignature(fr, lenNodes, wr)
        } else if (extentType == scope.internalVal.toInt()) {
            writeScope(fr, lenNodes, wr)
        } else if (extentType == expression.internalVal.toInt()) {
            writeExpression(fr, lenNodes, wr)
        } else if (extentType == returnExpression.internalVal.toInt()) {
            writeReturn(fr, lenNodes, wr)
        } else if (extentType == exturnExpression.internalVal.toInt()) {
            writeExturn(fr, lenNodes, wr)
        } else if (extentType == statementAssignment.internalVal.toInt()) {
            writeAssignment(fr, lenNodes, wr)
        } else if (extentType == fnDefPlaceholder.internalVal.toInt()) {
            writeFnSignature(fr, lenNodes, wr)
        } else {
            throw Exception()
        }
        maybeCloseFrames(wr)
    }
}


private fun pushNewFrame(arity: Int) {
    val eType = ExtentAST.values().first {it.internalVal.toInt() == ast.currChunk.nodes[ast.currInd] ushr 27 }
    val lenNodes = ast.currChunk.nodes[ast.currInd + 3]
    backtrack.add(CodegenFrame(eType, arity, ast.currNodeInd, lenNodes))
}


private fun writeEntryPointSignature(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append("function ")
    wr.append(entryPointName)
    wr.append("() ")

    ast.nextNode() // Skipping the "functionDef" node
}


private fun writeFnSignature(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    val funcId = ast.currChunk.nodes[ast.currInd + 2]
    val func = ast.getFunc(funcId)
    ast.nextNode() // fnDefPlaceholder
    fr.returnNode = ast.currNodeInd // the node to return to after we're done with this nested function
    ast.seek(func.bodyId)
    pushNewFrame(func.arity)

    val indentation = " ".repeat(indentDepth)
    val funcName = ast.identifiers[func.nameId]
    wr.append(indentation)
    wr.append("function ")
    wr.append(funcName)
    wr.append("(")

    ast.nextNode() // Skipping the "functionDef" node
    for (i in 0 until func.arity) {
        wr.append(ast.getString(ast.currChunk.nodes[ast.currInd + 3]))
        if (i == func.arity - 1) {
            wr.append(")")
        } else {
            wr.append(", ")
        }
        ast.nextNode()
    }
}

/**
 * Assignments of the type 'a = b + 5' are written out as 'const a = b + 5;'
 * Assignments of the type 'a = { b = a + 1; 4*b } are written as 'let a; { const b = a + 1; a = 4*b }'
 */
private fun writeAssignment(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))

    val isRightHandScope = ast.lookaheadType(2) == scope.internalVal.toInt()

    ast.nextNode() // the "assignment node
    val bindingName = ast.identifiers[ast.currChunk.nodes[ast.currInd + 3]]
    if (isRightHandScope) {
        wr.append("let ")
        wr.append(bindingName)
        wr.append(";\n")
    } else {
        wr.append("const ")
        wr.append(bindingName)
        wr.append(" = ")
    }

    ast.nextNode()  // the binding name node

    val nodeType = ast.currNodeType()
    if (nodeType == litInt.internalVal.toInt()) {
        wr.append(ast.getPayload())
        wr.append(";\n")
        ast.nextNode()
    } else if (nodeType == litString.internalVal.toInt()) {
        val literal = pr.getStringLiteral(ast.currChunk.nodes[ast.currInd + 1], ast.currChunk.nodes[ast.currInd] and LOWER27BITS)
        wr.append("\"")
        wr.append(literal)
        wr.append("\";\n")
        ast.nextNode()
    } else if (nodeType == ident.internalVal.toInt()) {
        val identName = ast.identifiers[ast.currChunk.nodes[ast.currInd + 3]]
        wr.append(identName)
        wr.append(";\n")
        ast.nextNode()
    } else {
        val lenExtent = ast.currChunk.nodes[ast.currInd + 3]

        if (isRightHandScope) {
            writeScopeRightHandInAssignment(bindingName, fr, wr)
        } else {
            ast.nextNode()  // the "expression" node
            writeExpressionBody(lenExtent, wr)
            wr.append(";\n")
        }
    }
}


private fun writeReturn(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))
    wr.append("return ")
    val lenExpression = ast.currChunk.nodes[ast.currInd + 3]
    ast.nextNode() // the returnExpression
    writeExpressionBody(lenExpression, wr)
    wr.append(";\n")
}

/** Writes out the reverse Polish notation expression in JS prefix+infix form.
 *  Consumes all nodes of the expression */
private fun writeExpressionBody(lenNodes: Int, wr: StringBuilder) {
    val stringBuildup = Stack<String>()
    for (i in 0 until lenNodes) {
        val nodeType = ast.currNodeType()
        val payload1 = ast.currChunk.nodes[ast.currInd + 2]
        val payload2 = ast.currChunk.nodes[ast.currInd + 3]
        if (nodeType == ident.internalVal.toInt()) {
            stringBuildup.push(ast.getString(payload2))
        } else if (nodeType == litInt.internalVal.toInt()) {
            val literalValue = (payload1.toLong() shl 32) + (payload2.toLong() and LOWER32BITS)
            stringBuildup.push(literalValue.toString() )
        } else if (nodeType == litString.internalVal.toInt()) {
            val stringLiteral = pr.getStringLiteral(ast.currChunk.nodes[ast.currInd + 1], ast.currChunk.nodes[ast.currInd] and LOWER27BITS)
            stringBuildup.push("\"" + stringLiteral + "\"")
        } else if (nodeType == idFunc.internalVal.toInt()) {
            val sb = StringBuilder()

            val arity = abs(payload1)
            val operands = Array(arity) { "" }
            for (j in 0 until arity) {
                operands[arity - j - 1] = stringBuildup.pop()
            }
            val func = ast.getFunc(payload2)
            val needParentheses = (i < (lenNodes - 1)) || payload1 >= 0
            if (payload1 >= 0) {
                // function
                sb.append(ast.getString(func.nameId))

                sb.append("(")
                sb.append(operands[0])
                for (j in 1 until payload1) {
                    sb.append(", ")
                    sb.append(operands[j])
                }
            } else {
                // operator
                val operString: String = operatorFunctionality[payload2].first
                if (needParentheses) sb.append("(")
                if (payload1 == -1) {
                    sb.append(operString)
                    sb.append(operands[0])
                } else if (payload1 == -2) {
                    sb.append(operands[0])
                    sb.append(operString)
                    sb.append(operands[1])
                }
            }
            if (needParentheses) sb.append(")")
            stringBuildup.push(sb.toString())
        } else {
            throw Exception(errorUnknownNodeInExpression)
        }
        ast.nextNode()
    }
    wr.append(stringBuildup.pop())
}


private fun writeExpression(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))
    ast.nextNode()
    writeExpressionBody(lenNodes, wr)
    wr.append(";\n")
}

private fun writeExturn(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    wr.append(" ".repeat(indentDepth))
    wr.append(fr.bindingNameIfAssignment)
    wr.append(" = ")

    ast.nextNode()
    writeExpressionBody(lenNodes, wr)
    wr.append(";\n")
}


private fun writeScope(fr: CodegenFrame, lenNodes: Int, wr: StringBuilder) {
    if (fr.eType != functionDef) {
        wr.append(" ".repeat(indentDepth))
    } else {
        wr.append(" ")
    }
    wr.append("{\n")

    indentDepth += 4

    pushNewFrame(-1)
    ast.skipNodes(1)
}

/**
 * Assignments of the type 'a = { b = a + 1; 4*b } are written as 'let a; { const b = a + 1; a = 4*b }'
 */
private fun writeScopeRightHandInAssignment(bindingName: String, fr: CodegenFrame, wr: StringBuilder) {
    if (fr.eType != functionDef) {
        wr.append(" ".repeat(indentDepth))
    } else {
        wr.append(" ")
    }
    wr.append("{\n")

    indentDepth += 4

    val eType = ExtentAST.values().first {it.internalVal.toInt() == ast.currChunk.nodes[ast.currInd] ushr 27 }
    val lenNodes = ast.currChunk.nodes[ast.currInd + 3]
    backtrack.add(CodegenFrame(eType, -1, ast.currNodeInd, lenNodes, bindingName))
    ast.skipNodes(1)
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

        var i = backtrack.size - 1
        while (i > -1) {
            val frame = backtrack[i]
            if (ast.currNodeInd < frame.sentinelNode) {
                return
            } else {
                closeFrame(frame, wr)
                backtrack.removeLast()
                if (frame.eType == functionDef) {
                    if (backtrack.isNotEmpty()) {
                        ast.seek(backtrack.last().returnNode) // skipping to the node after the fnDefPlaceholder
                    } else {
                        ast.seek(frame.sentinelNode) // since this is the outermost func, we finish up its codegen
                    }
                }
            }
            i--
        }
    }
}


private fun closeFrame(fr: CodegenFrame, wr: StringBuilder) {
    if (fr.eType == functionDef) {

    } else if (fr.eType == scope) {
        indentDepth -= 4
        wr.append(" ".repeat(indentDepth))
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

