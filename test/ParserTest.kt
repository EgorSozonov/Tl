import compiler.lexer.Lexer
import compiler.parser.*
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals


class ParserTest {


@Nested
inner class LexWordTest {
    @Test
    fun `Simple word lexing`() {
        testInpOutp(
            "foo 1 2 3",
            Parser().buildFBinding(FunctionBinding("foo", 26, 3))
                .buildInsertFBindingIntoScope("foo", 0)
                .buildNode(PunctuationAST.funcall, 4, 0, 9)
                .buildNode(RegularAST.identFunc, 0, 0, 0, 3)
                .buildNode(RegularAST.litInt, 0, 1, 4, 1)
                .buildNode(RegularAST.litInt, 0, 2, 6, 1)
                .buildNode(RegularAST.litInt, 0, 3, 8, 1)
        )
    }
}

private fun testInpOutp(inp: String, expected: Parser) {
    val lr = Lexer()
    lr.setInput(inp.toByteArray())
    lr.lexicallyAnalyze()

    val pr = Parser()
    pr.parse(lr, FileType.executable)

    val parseCorrect = Parser.equality(pr, expected)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(pr, expected)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}
}