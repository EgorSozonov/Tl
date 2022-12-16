package codegen
import lexer.FileType
import lexer.RegularToken
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
    val entryPoint = ast.getFunc(pr.indFirstFunction - 1)
    ast.seek(entryPoint.bodyId)
    val entryPointLenNodes = ast.currChunk.nodes[ast.currInd + 3]

    backtrack.add(CodegenFrame(functionDef, -1, 0, entryPoint.bodyId, entryPointLenNodes))
    while (backtrack.isNotEmpty()) {
        val f = backtrack.last()
        if (f.currNode == f.sentinelNode) {
            backtrack.removeLast()
            closeFrame(f, wr)
        } else {
            writeFrame(f, wr)
        }
    }
}

private fun writeFrame(fr: CodegenFrame, wr: StringBuilder) {
    if (fr.eType == functionDef) {
        writeSignatureAndBody(fr, wr)
    } else if (fr.eType == scope) {
        indentDepth += 4
        writeScope(fr, wr)
    } else if (fr.eType == expression) {
        writeExpression(fr, wr)
    } else if (fr.eType == statementAssignment) {
        writeAssignment(fr, wr)
    } else if (fr.eType == returnExpression) {
        writeReturn(fr, wr)
    }
}

private fun writeExpression(fr: CodegenFrame, wr: StringBuilder) {
    wr.append("expression")
}

private fun writeAssignment(fr: CodegenFrame, wr: StringBuilder) {
    wr.append("assignment")
}

private fun writeReturn(fr: CodegenFrame, wr: StringBuilder) {
    wr.append("return")
}

private fun writeScope(fr: CodegenFrame, wr: StringBuilder) {
    ast.nextNode()
    backtrack.last().currNode++
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


private fun writeSignatureAndBody(fr: CodegenFrame, wr: StringBuilder) {
    val indentation = " ".repeat(indentDepth*4)
    val funcName = if (fr.nameId > -1) { ast.identifiers[fr.nameId] } else { entryPointName }
    wr.append(indentation)
    wr.append("function ")
    wr.append(funcName)
    wr.append("(")


    ast.seek(fr.currNode)
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
    val nextFrameType = FrameAST.values().firstOrNull { it.internalVal == ast.currNodeType().toByte() }
    this.backtrack.add(CodegenFrame(nextFrameType!!, -1, -1))
}

/**
 * Builds the tree of function definitions with data on their bodies and child definitions
 */
//private fun buildFunctionTree(): ArrayList<CodegenFunc> {
//    val baseId = pr.indFirstFunction - 1
//    val entryPoint = ast.getFunc(baseId)
//    val result = ArrayList<CodegenFunc>(ast.funcTotalNodes - baseId + 1)
//    result.add(CodegenFunc(-1, entryPoint.bodyId))
//
//    ast.funcSeek(pr.indFirstFunction)
//    for (i in pr.indFirstFunction until ast.funcTotalNodes) {
//        val newFunc = CodegenFunc(i, ast.funcCurrChunk.nodes[ast.funcCurrInd + 3])
//        result.add(newFunc)
//        val parentId = ast.funcCurrChunk.nodes[ast.funcCurrInd + 1]
//        if (parentId > -1) {
//            result[parentId - baseId].childFunctions.add(newFunc)
//        }
//        ast.funcNext()
//    }
//    return result
//}


}

