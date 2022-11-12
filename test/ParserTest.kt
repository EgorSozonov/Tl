import compiler.lexer.LOWER32BITS
import compiler.lexer.Lexer
import compiler.parser.*
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals
import compiler.parser.RegularAST.*
import compiler.parser.PunctuationAST.*


class ParserTest {


@Nested
inner class ExprTest {
    @Test
    fun `Simple function call`() {
        testParseWithEnvironment(
            "foo 10 2 3", { it.buildFBinding(FunctionBinding("foo", 26, 3))
                .buildInsertBindingsIntoScope() },
            {
                it.buildNode(funcall, 4, 0, 10)
                  .buildNode(litInt, 0, 10, 4, 2)
                  .buildNode(litInt, 0, 2, 7, 1)
                  .buildNode(litInt, 0, 3, 9, 1)
                  .buildNode(idFunc, 0, 26, 0, 3)
            }
        )
    }

    @Test
    fun `Double function call`() {
        testParseWithEnvironment(
            "foo a (bar b c d)", {
                it.buildFBinding(FunctionBinding("foo", funcPrecedence, 2))
                  .buildFBinding(FunctionBinding("bar", funcPrecedence, 3))
                  .buildBinding(Binding("a"))
                  .buildBinding(Binding("b"))
                  .buildBinding(Binding("c"))
                  .buildBinding(Binding("d"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 7, 0, 17)
                  .buildNode(ident, 0, 0, 4, 1)
                  .buildNode(ident, 0, 1, 11, 1)
                  .buildNode(ident, 0, 2, 13, 1)
                  .buildNode(ident, 0, 3, 15, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction + 1, 7, 3)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
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
                it.buildNode(funcall, 3, 0, 8)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(ident, 0, 1, 7, 1)
                    .buildNode(idFunc, 0, it.indFirstFunction, 2, 3)
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
                it.buildNode(funcall, 5, 0, 15)
                    .buildNode(ident, 0, 2, 0, 1)
                    .buildNode(ident, 0, 1, 7, 1)
                    .buildNode(ident, 0, 0, 9, 1)
                    .buildNode(idFunc, 0, it.indFirstFunction, 2, 3)
                    .buildNode(idFunc, 0, it.indFirstFunction + 1, 11, 3)
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
                it.buildNode(funcall, 5, 0, 14)
                    .buildNode(ident, 0, 0, 5, 1)
                    .buildNode(idFunc, 0, it.indFirstFunction, 1, 3)
                    .buildNode(ident, 0, 1, 13, 1)
                    .buildNode(idFunc, 0, it.indFirstFunction + 1, 8, 3)
            }
        )
    }

    @Test
    fun `Operator precedence simple`() {
        testParseWithEnvironment(
            "a + b * c", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 5, 0, 9)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(ident, 0, 1, 4, 1)
                    .buildNode(ident, 0, 2, 8, 1)
                    .buildNode(idFunc, 0, 6, 6, 1)
                    .buildNode(idFunc, 0, 8, 2, 1)
            }
        )
    }

    @Test
    fun `Operator precedence simple 2`() {
        testParseWithEnvironment(
            "(a + b) * c", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 6, 0, 11)
                    .buildNode(ident, 0, 0, 1, 1)
                    .buildNode(ident, 0, 1, 5, 1)
                    .buildNode(idFunc, 0, 8, 3, 1)
                    .buildNode(ident, 0, 2, 10, 1)
                    .buildNode(idFunc, 0, 6, 8, 1)

            }
        )
    }

    @Test
    fun `Operator precedence simple 3`() {
        testParseWithEnvironment(
            "a != 7 && a != 8 && a != 9", {
                it.buildBinding(Binding("a"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 11, 0, 26)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(litInt, 0, 7, 5, 1)
                    .buildNode(idFunc, 0, 0, 2, 2)
                    .buildNode(ident, 0, 0, 10, 1)
                    .buildNode(litInt, 0, 8, 15, 1)
                    .buildNode(idFunc, 0, 0, 12, 2)
                    .buildNode(idFunc, 0, 3, 7, 2)
                    .buildNode(ident, 0, 0, 20, 1)
                    .buildNode(litInt, 0, 9, 25, 1)
                    .buildNode(idFunc, 0, 0, 22, 2)
                    .buildNode(idFunc, 0, 3, 17, 2)

            }
        )
    }

    @Test
    fun `Operator prefix 1`() {
        testParseWithEnvironment(
            "foo !a", {
                it.buildBinding(Binding("a"))
                    .buildFBinding(FunctionBinding("foo", funcPrecedence, 1))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 3, 0, 6)
                    .buildNode(ident, 0, 0, 5, 1)
                    .buildNode(idFunc, 0, 1, 4, 1)
                    .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
        )
    }

    @Test
    fun `Operator prefix 2`() {
        testParseWithEnvironment(
            "foo !a b !c", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                    .buildFBinding(FunctionBinding("foo", funcPrecedence, 3))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 6, 0, 11)
                    .buildNode(ident, 0, 0, 5, 1)
                    .buildNode(idFunc, 0, 1, 4, 1)
                    .buildNode(ident, 0, 1, 7, 1)
                    .buildNode(ident, 0, 2, 10, 1)
                    .buildNode(idFunc, 0, 1, 9, 1)
                    .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
        )
    }

    @Test
    fun `Operator prefix 3`() {
        testParseWithEnvironment(
            "foo !(a || b)", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b"))
                    .buildFBinding(FunctionBinding("foo", funcPrecedence, 1))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 6, 0, 13)
                    .buildNode(ident, 0, 0, 6, 1)
                    .buildNode(ident, 0, 1, 11, 1)
                    .buildNode(idFunc, 0, 25, 8, 2)
                    .buildNode(idFunc, 0, 1, 4, 1)
                    .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
        )
    }

    @Test
    fun `Operator arithmetic 1`() {
        testParseWithEnvironment(
            "a + (b - c % 2)**11", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 10, 0, 19)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(ident, 0, 1, 5, 1)
                    .buildNode(ident, 0, 2, 9, 1)
                    .buildNode(litInt, 0, 2, 13, 1)
                    .buildNode(idFunc, 0, 2, 11, 1)
                    .buildNode(idFunc, 0, 10, 7, 1)
                    .buildNode(litInt, 0, 11, 17, 2)
                    .buildNode(idFunc, 0, 5, 15, 2)
                    .buildNode(idFunc, 0, 8, 2, 1)
            }
        )
    }

    @Test
    fun `Operator arithmetic 2`() {
        testParseWithEnvironment(
            "a + (-5)", {
                it.buildBinding(Binding("a"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 5, 0, 8)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 5, 2)
                    .buildNode(idFunc, 0, 8, 2, 1)
            }
        )
    }

    @Test
    fun `Operator arithmetic 3`() {
        testParseWithEnvironment(
            "a + !(-5)", {
                it.buildBinding(Binding("a"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 6, 0, 9)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 6, 2)
                    .buildNode(idFunc, 0, 1, 4, 1)
                    .buildNode(idFunc, 0, 8, 2, 1)
            }
        )
    }

    @Test
    fun `Operator arithmetic 4`() {
        testParseWithEnvironment(
            "a + !!(-5)", {
                it.buildBinding(Binding("a"))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 7, 0, 10)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 7, 2)
                    .buildNode(idFunc, 0, 1, 5, 1)
                    .buildNode(idFunc, 0, 1, 4, 1)
                    .buildNode(idFunc, 0, 8, 2, 1)
            }
        )
    }

    @Test
    fun `Unknown function arity`() {
        testParseWithEnvironment(
            "a .foo b c", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                    .buildFBinding(FunctionBinding("foo", funcPrecedence, 2))
                    .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(funcall, 4, 0, 10)
                    .buildNode(ident, 0, 0, 0, 1)
                    .buildNode(ident, 0, 1, 7, 1)
                    .buildNode(ident, 0, 2, 9, 1)
                    .buildUnknownFunc("foo", 3, 2)

            }
        )
    }

}

    
@Nested
inner class AssignmentTest {
    @Test
    fun `Simple assignment`() {
        testParseWithEnvironment(
            "a = 1 + 5", { x -> x.buildInsertBindingsIntoScope() },
            {
                it.buildNode(statementAssignment, 5, 0, 9)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildNode(funcall, 3, 4, 5)
                  .buildNode(litInt, 0, 1, 4, 1)
                  .buildNode(litInt, 0, 5, 8, 1)
                  .buildNode(idFunc, 0, 8, 6, 1)
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