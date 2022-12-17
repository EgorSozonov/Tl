package codegen

import parser.FrameAST


data class CodegenFunc(val nameId: Int, val bodyInd: Int)

class CodegenFrame(val eType: FrameAST, val nameId: Int, val arity: Int, startNode: Int, val lenNodes: Int) {
    var currNode: Int = startNode
    val sentinelNode: Int = startNode + lenNodes
}