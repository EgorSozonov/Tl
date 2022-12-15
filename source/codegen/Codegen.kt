package codegen
import lexer.FileType
import parser.AST
import parser.Parser


class Codegen {
    private var indentation = 0

    fun codeGen(pr: Parser): String {
        if (pr.wasError) return ""

        val result = StringBuilder()
        if (pr.getFileType() == FileType.executable) {
            codeGenExecutable(pr, result)
        }
        return result.toString()
    }

    fun codeGenExecutable(pr: Parser, wr: StringBuilder) {
        val ast = pr.ast
        val entryPoint = ast.getFunc(pr.indFirstFunction - 1)
        wr.append(entryPointName).append("() {\n")
        indentation += 4
        wr.append("}")
    }
}

