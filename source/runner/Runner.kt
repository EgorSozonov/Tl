package runner

import codegen.Codegen
import lexer.FileType
import lexer.Lexer
import parser.ImportOrBuiltin
import parser.Parser
import parser.funcPrecedence


const val input = """
fn foo x y {
    z = x*y
    return (z + x) .inner 5
    
    fn inner a b {
        return a - 2*b
    }
}
"""

fun main(args: Array<String>) {
    println("compilation result:")
    println()
    val lexer = Lexer(input.toByteArray(), FileType.executable)
    lexer.lexicallyAnalyze()
    val parser = Parser(lexer)
    parser.parse(arrayListOf(ImportOrBuiltin("print", funcPrecedence, 1)))
    val codegen = Codegen(parser)
    val result = codegen.codeGen()

    println(result)
}