import compiler.lexer.Lexer
import compiler.parser.*
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals


class ParserTest {


@Nested
inner class ParseFuncallTest {
    @Test
    fun `Simple function call`() {
        testParseWithEnvironment(
            "foo 1 2 3", { it.buildFBinding(FunctionBinding("foo", 26, 3))
                .buildInsertBindingsIntoScope() },
            {
                it.buildNode(PunctuationAST.funcall, 4, 0, 9)
                  .buildNode(RegularAST.litInt, 0, 1, 4, 1)
                  .buildNode(RegularAST.litInt, 0, 2, 6, 1)
                  .buildNode(RegularAST.litInt, 0, 3, 8, 1)
                  .buildNode(RegularAST.idFunc, 0, 26, 0, 3)
            }
        )
    }

    @Test
    fun `Double function call`() {
        testParseWithEnvironment(
            "foo a (bar b c d)", {
                it.buildFBinding(FunctionBinding("foo", 26, 2))
                  .buildFBinding(FunctionBinding("bar", 26, 3))
                  .buildBinding(Binding("a"))
                  .buildBinding(Binding("b"))
                  .buildBinding(Binding("c"))
                  .buildBinding(Binding("d"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(PunctuationAST.funcall, 7, 0, 17)
                  .buildNode(RegularAST.ident, 0, 0, 4, 1)
                  .buildNode(RegularAST.ident, 0, 1, 11, 1)
                  .buildNode(RegularAST.ident, 0, 2, 13, 1)
                  .buildNode(RegularAST.ident, 0, 3, 15, 1)
                  .buildNode(RegularAST.idFunc, 0, it.indFirstFunction + 1, 7, 3)
                  .buildNode(RegularAST.idFunc, 0, it.indFirstFunction, 0, 3)
            }
        )
    }

    @Test
    fun `Infix function call`() {
        testParseWithEnvironment(
            "a .foo b", {
                it.buildFBinding(FunctionBinding("foo", 26, 2))
                    .buildBinding(Binding("a")).buildBinding(Binding("b"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(PunctuationAST.funcall, 3, 0, 8)
                    .buildNode(RegularAST.ident, 0, 0, 0, 1)
                    .buildNode(RegularAST.ident, 0, 1, 7, 1)
                    .buildNode(RegularAST.idFunc, 0, it.indFirstFunction, 2, 3)
            }
        )
    }

    @Test
    fun `Infix function calls`() {
        testParseWithEnvironment(
            "c .foo b a .bar", {
                it.buildFBinding(FunctionBinding("foo", 26, 3))
                    .buildFBinding(FunctionBinding("bar", 26, 1))
                    .buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(PunctuationAST.funcall, 5, 0, 15)
                    .buildNode(RegularAST.ident, 0, 2, 0, 1)
                    .buildNode(RegularAST.ident, 0, 1, 7, 1)
                    .buildNode(RegularAST.ident, 0, 0, 9, 1)
                    .buildNode(RegularAST.idFunc, 0, it.indFirstFunction, 2, 3)
                    .buildNode(RegularAST.idFunc, 0, it.indFirstFunction + 1, 11, 3)
            }
        )
    }

    @Test
    fun `Parentheses then infix function call`() {
        testParseWithEnvironment(
            "(foo a) .bar b", {
                it.buildFBinding(FunctionBinding("foo", funcPrecedence, 1))
                    .buildFBinding(FunctionBinding("bar", funcPrecedence, 2))
                    .buildBinding(Binding("a")).buildBinding(Binding("b"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(PunctuationAST.funcall, 5, 0, 14)
                    .buildNode(RegularAST.ident, 0, 0, 5, 1)
                    .buildNode(RegularAST.idFunc, 0, it.indFirstFunction, 1, 3)
                    .buildNode(RegularAST.ident, 0, 1, 13, 1)
                    .buildNode(RegularAST.idFunc, 0, it.indFirstFunction + 1, 8, 3)
            }
        )
    }
}

private fun testParseWithEnvironment(inp: String, environmentSetup: (Parser) -> Unit, resultBuilder: (Parser) -> Unit) {
    val lr = Lexer()
    lr.setInput(inp.toByteArray())
    lr.lexicallyAnalyze()

    val runnerParser = Parser()
    val controlParser = Parser()

    environmentSetup(runnerParser)
    runnerParser.parse(lr, FileType.executable)

    environmentSetup(controlParser)
    resultBuilder(controlParser)

    val parseCorrect = Parser.equality(runnerParser, controlParser)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(runnerParser, controlParser)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}

private fun testParseResult(inp: String, parserWithEnvironment: Parser, expected: Parser) {
    val lr = Lexer()
    lr.setInput(inp.toByteArray())
    lr.lexicallyAnalyze()

    parserWithEnvironment.parse(lr, FileType.executable)

    val parseCorrect = Parser.equality(parserWithEnvironment, expected)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(parserWithEnvironment, expected)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}
}