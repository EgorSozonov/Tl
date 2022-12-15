import lexer.*
import lexer.Lexer
import lexer.PunctuationToken.*
import lexer.RegularToken.*
import org.junit.jupiter.api.Nested
import parser.ImportOrBuiltin
import parser.Parser
import kotlin.test.Test
import kotlin.test.assertEquals

internal class CodegenTest {


@Nested
inner class LexWordTest {
    @Test
    fun `Simple word lexing`() {
        testInpOutp("asdf Abc") {
            it.buildPunct(0, 8, statementFun, 2)
                .build(0, 0, 4, word)
                .build(0, 5, 3, word)

        }
    }
}

private fun testParseWithEnvironment(inp: String, imports: ArrayList<ImportOrBuiltin>, resultBuilder: (Parser) -> Unit) {
    val lr = Lexer(inp.toByteArray(), FileType.executable)
    lr.lexicallyAnalyze()

    val testSpecimen = Parser(lr)
    val controlParser = Parser(lr)

    testSpecimen.parse(imports)

    controlParser.insertImports(imports)
    controlParser.ast.funcNode(-1, 0, 0)
    resultBuilder(controlParser)

    val parseCorrect = Parser.equality(testSpecimen, controlParser)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(testSpecimen, controlParser)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}

}