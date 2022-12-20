package runner

import codegen.Codegen
import lexer.FileType
import lexer.Lexer
import parser.Import
import parser.Parser


const val input = """
fn foo x y {
    z = x*y
    return (z + x) .inner 5 (-2)
    
    fn inner a b c {
        return a - 2*b + 3*c
    }
}

90 .foo 100 .print
"""

// foo ~20
// inner 17

fun main(args: Array<String>) {
    println("compilation result:")
    println()
    val lexer = Lexer(input.toByteArray(), FileType.executable)
    lexer.lexicallyAnalyze()

    println()
    println("lexing result:")
    println()
    println(Lexer.print(lexer))

    val parser = Parser(lexer)
    parser.parse(arrayListOf(Import("print", "console.log", 1)))

    println(Parser.print(parser))

    println()
    println("compiled output:")
    println()

    val codegen = Codegen(parser)
    val result = codegen.codeGen()


    println(result)
}