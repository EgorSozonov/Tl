package codegen

import parser.ExtentAST


data class CodegenFunc(val nameId: Int, val bodyInd: Int)

class CodegenFrame(val eType: ExtentAST, val nameId: Int, val arity: Int, startNode: Int, val lenNodes: Int) {
    var returnNode: Int = 0
    val sentinelNode: Int = startNode + lenNodes + 1
}