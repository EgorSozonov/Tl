package runner

import codegen.Codegen
import lexer.FileType
import lexer.Lexer
import parser.Import
import parser.Parser


const val input = """
"Hello world from Tl" .print
    
scopeAssignment = {
    aw = "asdf "
    aw + "bcjk"
}

scopeAssignment .print


a = 1
b = 4
c = 2
d = 2
e = 3
f = 15

result = a*b/c + d*e -f # 1*4/2 + 2*3 - 15 = -7
  
result .print


"""

//const val input = """
//fn foo x y {
//    z = x*y
//    return (z + x) .inner 5 (-2)
//
//    fn inner a b c {
//        return a - 2*b + 3*c
//    }
//}
//
//90 .foo 100 .print
//"""

// foo ~20
// inner 17

fun main(args: Array<String>) {
    println("compilation result:")
    println()
    val lexer = Lexer(input.toByteArray(), FileType.executable)
    lexer.lexicallyAnalyze()

    if (lexer.wasError) {
        println(lexer.errMsg)
        println(Lexer.print(lexer))
        return
    }

    println()
    println("lexing result:")
    println()
    println(Lexer.print(lexer))

    val parser = Parser(lexer)
    parser.parse(arrayListOf(Import("print", "console.log", 1)))

    if (parser.wasError) {
        println(parser.errMsg)
        println(Parser.print(parser))
        return
    }

    println(Parser.print(parser))

    println()
    println("compiled output:")
    println()

    val codegen = Codegen(parser)
    val result = codegen.codeGen()


    println(result)
}