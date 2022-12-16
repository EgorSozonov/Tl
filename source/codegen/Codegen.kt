package codegen
import lexer.FileType
import parser.AST
import parser.Parser


class Codegen(val pr: Parser) {


private var indentation = 0
private val backtrack = ArrayList<CodegenFunc>(4)

fun codeGen(): String {
    if (pr.wasError) return ""

    val result = StringBuilder()
    if (pr.getFileType() == FileType.executable) {
        codeGenExecutable(result)
    }
    return result.toString()
}

private fun codeGenExecutable(wr: StringBuilder) {
    val entryPoint = pr.ast.getFunc(pr.indFirstFunction - 1)
    pr.ast.seek(entryPoint.bodyId)

    backtrack.add(CodegenFunc(entryPoint.nameId, entryPoint.bodyId))
    while (backtrack.isNotEmpty()) {
        val f = backtrack.last()

    }
}


private fun writeSignatureAndBody(f: CodegenFunc, backtrackSize: Int, wr: StringBuilder) {
    val indentation = " ".repeat(backtrackSize*4 - 4)
    val funcName = if (f.nameId > -1) { pr.ast.identifiers[f.nameId] } else { entryPointName }
    wr.append(indentation)
    wr.append(funcName)
    wr.append("(")

    wr.append(") {\n")
    wr.append(indentation)
    wr.append("}\n")
}

/**
 * Builds the tree of function definitions with data on their bodies and child definitions
 */
//private fun buildFunctionTree(): ArrayList<CodegenFunc> {
//    val baseId = pr.indFirstFunction - 1
//    val entryPoint = pr.ast.getFunc(baseId)
//    val result = ArrayList<CodegenFunc>(pr.ast.funcTotalNodes - baseId + 1)
//    result.add(CodegenFunc(-1, entryPoint.bodyId))
//
//    pr.ast.funcSeek(pr.indFirstFunction)
//    for (i in pr.indFirstFunction until pr.ast.funcTotalNodes) {
//        val newFunc = CodegenFunc(i, pr.ast.funcCurrChunk.nodes[pr.ast.funcCurrInd + 3])
//        result.add(newFunc)
//        val parentId = pr.ast.funcCurrChunk.nodes[pr.ast.funcCurrInd + 1]
//        if (parentId > -1) {
//            result[parentId - baseId].childFunctions.add(newFunc)
//        }
//        pr.ast.funcNext()
//    }
//    return result
//}


}

