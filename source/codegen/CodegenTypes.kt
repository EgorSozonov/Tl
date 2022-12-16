package codegen


data class CodegenFunc(val nameId: Int, val bodyInd: Int, var currBodyInd: Int = 0, var lastBodyInd: Int = 0)

