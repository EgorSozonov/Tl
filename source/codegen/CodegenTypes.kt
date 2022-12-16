package codegen

import parser.FrameAST


data class CodegenFunc(val nameId: Int, val bodyInd: Int)

data class CodegenFrame(val eType: FrameAST, val nameId: Int, val arity: Int, var currNode: Int = 0, var sentinelNode: Int = 0)