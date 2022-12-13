import lexer.FileType
import lexer.LOWER32BITS
import lexer.Lexer
import org.junit.jupiter.api.Nested
import parser.*
import kotlin.test.Test
import kotlin.test.assertEquals
import parser.RegularAST.*
import parser.FrameAST.*


class ParserTest {


@Nested
inner class ExprTest {
    @Test
    fun `Simple function call`() {
        testParseWithEnvironment(
            "10 .foo 2 3", { it.buildFBinding(BuiltInFunction("foo", funcPrecedence, 3))
                .buildInsertBindingsIntoScope() },
            {
                it.buildNode(expression, 4, 0, 10)
                  .buildNode(litInt, 0, 10, 4, 2)
                  .buildNode(litInt, 0, 2, 7, 1)
                  .buildNode(litInt, 0, 3, 9, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
        )
    }

    @Test
    fun `Double function call`() {
        testParseWithEnvironment(
            "a .foo (b .bar c d)", {
                it.buildFBinding(BuiltInFunction("foo", funcPrecedence, 2))
                  .buildFBinding(BuiltInFunction("bar", funcPrecedence, 3))
                  .buildBinding(Binding("a"))
                  .buildBinding(Binding("b"))
                  .buildBinding(Binding("c"))
                  .buildBinding(Binding("d"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 6, 0, 17)
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
                it.buildFBinding(BuiltInFunction("foo", funcPrecedence, 2))
                  .buildBinding(Binding("a")).buildBinding(Binding("b"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 3, 0, 8)
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
                it.buildFBinding(BuiltInFunction("foo", funcPrecedence, 3))
                  .buildFBinding(BuiltInFunction("bar", funcPrecedence, 1))
                  .buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 5, 0, 15)
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
            "(a .foo) .bar b", {
                it.buildFBinding(BuiltInFunction("foo", funcPrecedence, 1))
                  .buildFBinding(BuiltInFunction("bar", funcPrecedence, 2))
                  .buildBinding(Binding("a")).buildBinding(Binding("b"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 4, 0, 14)
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
                it.buildNode(expression, 5, 0, 9)
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
                it.buildNode(expression, 5, 0, 11)
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
                it.buildNode(expression, 11, 0, 26)
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
            "!a .foo", {
                it.buildBinding(Binding("a"))
                  .buildFBinding(BuiltInFunction("foo", funcPrecedence, 1))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 3, 0, 6)
                  .buildNode(ident, 0, 0, 5, 1)
                  .buildNode(idFunc, 0, 1, 4, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
        )
    }

    @Test
    fun `Operator prefix 2`() {
        testParseWithEnvironment(
            "!a .foo b !c", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                  .buildFBinding(BuiltInFunction("foo", funcPrecedence, 3))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 6, 0, 11)
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
            "!(a || b) .foo", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b"))
                  .buildFBinding(BuiltInFunction("foo", funcPrecedence, 1))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 5, 0, 13)
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
                it.buildNode(expression, 9, 0, 19)
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
                it.buildNode(expression, 3, 0, 8)
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
                it.buildNode(expression, 4, 0, 9)
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
                it.buildNode(expression, 5, 0, 10)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 7, 2)
                  .buildNode(idFunc, 0, 1, 5, 1)
                  .buildNode(idFunc, 0, 1, 4, 1)
                  .buildNode(idFunc, 0, 8, 2, 1)
            }
        )
    }

    @Test
    fun `Operator arity error 1`() {
        testParseWithEnvironment(
            "a + 5 100", {
                it.buildBinding(Binding("a"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 0, 0, 9)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 5, 4, 1)
                  .buildError(errorOperatorWrongArity)
            }
        )
    }

    @Test
    fun `Single-item expression 1`() {
        testParseWithEnvironment(
            "a + (5)", {
                it.buildBinding(Binding("a"))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 3, 0, 7)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 5, 5, 1)
                  .buildNode(idFunc, 0, 8, 2, 1)
            }
        )
    }

    @Test
    fun `Single-item expression 2`() {
        testParseWithEnvironment(
            "5 .foo !!!(a)", {
                it.buildBinding(Binding("a"))
                  .buildFBinding(BuiltInFunction("foo", funcPrecedence, 2))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 6, 0, 12)
                  .buildNode(litInt, 0, 5, 4, 1)
                  .buildNode(ident, 0, 0, 10, 1)
                  .buildNode(idFunc, 0, 1, 8, 1)
                  .buildNode(idFunc, 0, 1, 7, 1)
                  .buildNode(idFunc, 0, 1, 6, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
        )
    }

    @Test
    fun `Unknown function arity`() {
        testParseWithEnvironment(
            "a .foo b c", {
                it.buildBinding(Binding("a")).buildBinding(Binding("b")).buildBinding(Binding("c"))
                  .buildFBinding(BuiltInFunction("foo", funcPrecedence, 2))
                  .buildInsertBindingsIntoScope()
            },
            {
                it.buildNode(expression, 4, 0, 10)
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
    fun `Simple assignment 1`() {
        testParseWithEnvironment(
            "a = 1 + 5", { x -> x.buildInsertBindingsIntoScope()
                         },
            {
                it.buildBinding(Binding("a"))
                  .buildNode(statementAssignment, 5, 0, 9)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildNode(expression, 3, 4, 5)
                  .buildNode(litInt, 0, 1, 4, 1)
                  .buildNode(litInt, 0, 5, 8, 1)
                  .buildNode(idFunc, 0, 8, 6, 1)
            }
        )
    }

    @Test
    fun `Simple assignment 2`() {
        testParseWithEnvironment(
            "a = 9", { x -> x.buildInsertBindingsIntoScope()
                     },
            {
                it.buildBinding(Binding("a"))
                  .buildNode(statementAssignment, 2, 0, 5)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 9, 4, 1)
            }
        )
    }
}

@Nested
inner class ScopeTest {
    @Test
    fun `Simple scope 1`() {
        testParseWithEnvironment(
            """{
    x = 5

    x .print
}""",
            {
                it.buildFBinding(BuiltInFunction("print", funcPrecedence, 1)).buildInsertBindingsIntoScope()
            },
            {
                it.buildBinding(Binding("x"))
                  .buildNode(scope, 6, 1, 24)
                  .buildNode(statementAssignment, 2, 6, 5)
                  .buildNode(binding, 0, 0, 6, 1)
                  .buildNode(litInt, 0, 5, 10, 1)
                  .buildNode(expression, 2, 17, 7)
                  .buildNode(ident, 0, 0, 23, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 17, 5)
            }
        )
    }

    @Test
    fun `Simple scope 2`() {
        testParseWithEnvironment(
            """{
    x = 123;  yy = x * 10
    yy .print
}""",
            {
                it.buildFBinding(BuiltInFunction("print", funcPrecedence, 1)).buildInsertBindingsIntoScope()
            },
            {
                it.buildBinding(Binding("x"))
                  .buildBinding(Binding("yy"))
                  .buildNode(scope, 12, 1, 40)
                  .buildNode(statementAssignment, 2, 6, 7)
                  .buildNode(binding, 0, 0, 6, 1)
                  .buildNode(litInt, 0, 123, 10, 3)
                  .buildNode(statementAssignment, 5, 16, 11)
                  .buildNode(binding, 0, 1, 16, 2)
                  .buildNode(expression, 3, 21, 6)
                  .buildNode(ident, 0, 0, 21, 1)
                  .buildNode(litInt, 0, 10, 25, 2)
                  .buildNode(idFunc, 0, 6, 23, 1)
                  .buildNode(expression, 2, 32, 8)
                  .buildNode(ident, 0, 1, 38, 2)
                  .buildNode(idFunc, 0, it.indFirstFunction, 32, 5)
            }
        )
    }

    @Test
    fun `Scope inside statement`() {
        testParseWithEnvironment(
            """a = 5 + {
    x = 15
    x*2
}/3""",
            {
                it.buildInsertBindingsIntoScope()//.buildBinding(Binding("a")).buildBinding(Binding("x"))
            },
            {
                it.buildBinding(Binding("a"))
                  .buildBinding(Binding("x"))
                  .buildNode(statementAssignment, 14, 0, 32)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildNode(expression, 12, 4, 28)

                  .buildNode(litInt, 0, 5, 4, 1)

                  .buildNode(scope, 7, 9, 20)
                  .buildNode(statementAssignment, 2, 14, 6)
                  .buildNode(binding, 0, 1, 14, 1)
                  .buildNode(litInt, 0, 15, 18, 2)
                  .buildNode(expression, 3, 25, 3)
                  .buildNode(ident, 0, 1, 25, 1)
                  .buildNode(litInt, 0, 2, 27, 1)
                  .buildNode(idFunc, 0, 6, 26, 1)

                  .buildNode(litInt, 0, 3, 31, 1)
                  .buildNode(idFunc, 0, 13, 30, 1)

                  .buildNode(idFunc, 0, 8, 6, 1)
            }
        )
    }
}

private fun testParseWithEnvironment(inp: String, environmentSetup: (Parser) -> Unit, resultBuilder: (Parser) -> Unit) {
    val lr = Lexer(inp.toByteArray(), FileType.executable)
    lr.lexicallyAnalyze()

    val runnerParser = Parser(lr)
    val controlParser = Parser(lr)

    environmentSetup(runnerParser)
    runnerParser.parse()

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
    val lr = Lexer(inp.toByteArray(), FileType.executable)
    lr.lexicallyAnalyze()

    parserWithEnvironment.parse()

    val parseCorrect = Parser.equality(parserWithEnvironment, expected)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(parserWithEnvironment, expected)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}

}