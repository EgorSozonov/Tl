import compiler.lexer.Lexer
import compiler.lexer.PunctuationToken
import compiler.lexer.RegularToken
import compiler.parser.Parser
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals


class ParserTest {


@Nested
inner class LexWordTest {
    @Test
    fun `Simple word lexing`() {
        testInpOutp(
            "asdf Abc",
            Lexer().buildPunct(0, 8, PunctuationToken.statementFun, 2)
                .build(0, 0, 4, RegularToken.word)
                .build(0, 5, 3, RegularToken.word)
        )
    }
}

private fun testInpOutp(inp: String, expected: Parser) {
    val lr = Lexer()
    lr.setInput(inp.toByteArray())
    lr.lexicallyAnalyze()

    val pr = Parser()
    pr.parse(lr)

    val parseCorrect = Parser.equality(pr, expected)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(pr, expected)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}
}