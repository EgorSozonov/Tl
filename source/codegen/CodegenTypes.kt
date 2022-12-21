package codegen

import parser.ExtentAST


data class CodegenFunc(val nameId: Int, val bodyInd: Int)

class CodegenFrame(val eType: ExtentAST, val arity: Int, startNode: Int, lenNodes: Int, val bindingNameIfAssignment: String = "") {
    var returnNode: Int = 0
    val sentinelNode: Int = startNode + lenNodes + 1
}