package codegen

import parser.SpanAST


data class CodegenFunc(val nameId: Int, val bodyInd: Int)

class CodegenFrame(val eType: SpanAST, val arity: Int, startNode: Int, lenNodes: Int, val bindingNameIfAssignment: String = "") {
    var returnNode: Int = 0
    val sentinelNode: Int = startNode + lenNodes + 1
}