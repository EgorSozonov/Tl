package codegen


data class CodegenFunc(val nameId: Int, val bodyInd: Int, val childFunctions: ArrayList<CodegenFunc> = ArrayList(), var currChild: Int = -1)

