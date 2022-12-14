import lexer.FileType
import lexer.Lexer
import org.junit.jupiter.api.Nested
import parser.*
import kotlin.test.Test
import kotlin.test.assertEquals
import parser.RegularAST.*
import parser.FrameAST.*
import utils.LOWER32BITS


class ParserTest {


@Nested
inner class ExprTest {
    @Test
    fun `Simple function call`() {
        testParseWithEnvironment(
            "10 .foo 2 3", arrayListOf(ImportOrBuiltin("foo", funcPrecedence, 3)))
            {
                it.buildExtent(expression, 4, 0, 10)
                  .buildNode(litInt, 0, 10, 4, 2)
                  .buildNode(litInt, 0, 2, 7, 1)
                  .buildNode(litInt, 0, 3, 9, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
    }

    @Test
    fun `Double function call`() {
        testParseWithEnvironment(
            "a .foo (b .bar c d)", arrayListOf(
                ImportOrBuiltin("foo", funcPrecedence, 2),
                ImportOrBuiltin("bar", funcPrecedence, 3),
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("b", 0, 0),
                ImportOrBuiltin("c", 0, 0),
                ImportOrBuiltin("d", 0, 0),
            )
        ) {
            it.buildExtent(expression, 6, 0, 17)
                .buildNode(ident, 0, 0, 4, 1)
                .buildNode(ident, 0, 1, 11, 1)
                .buildNode(ident, 0, 2, 13, 1)
                .buildNode(ident, 0, 3, 15, 1)
                .buildNode(idFunc, 0, it.indFirstFunction + 1, 7, 3)
                .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
        }
    }

    @Test
    fun `Infix function call`() {
        testParseWithEnvironment("a .foo b", arrayListOf(
                ImportOrBuiltin("foo", funcPrecedence, 2),
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("b", 0, 0),
            )) {
                it.buildExtent(expression, 3, 0, 8)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(ident, 0, 1, 7, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 2, 3)
        }
    }

    @Test
    fun `Infix function calls`() {
        testParseWithEnvironment("c .foo b a .bar", arrayListOf(
            ImportOrBuiltin("foo", funcPrecedence, 3),
            ImportOrBuiltin("bar", funcPrecedence, 1),
            ImportOrBuiltin("a", 0, 0),
            ImportOrBuiltin("b", 0, 0),
            ImportOrBuiltin("c", 0, 0),
        )) {
            it.buildExtent(expression, 5, 0, 15)
              .buildNode(ident, 0, 2, 0, 1)
              .buildNode(ident, 0, 1, 7, 1)
              .buildNode(ident, 0, 0, 9, 1)
              .buildNode(idFunc, 0, it.indFirstFunction, 2, 3)
              .buildNode(idFunc, 0, it.indFirstFunction + 1, 11, 3)
        }
    }

    @Test
    fun `Parentheses then infix function call`() {
        testParseWithEnvironment(
            "(a .foo) .bar b", arrayListOf(
                ImportOrBuiltin("foo", funcPrecedence, 1),
                ImportOrBuiltin("bar", funcPrecedence, 2),
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("b", 0, 0),
            ))
            {
                it.buildExtent(expression, 4, 0, 14)
                  .buildNode(ident, 0, 0, 5, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 1, 3)
                  .buildNode(ident, 0, 1, 13, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction + 1, 8, 3)
            }
    }

    @Test
    fun `Operator precedence simple`() {
        testParseWithEnvironment(
            "a + b * c", arrayListOf(
                ImportOrBuiltin("a", 0, 0), ImportOrBuiltin("b", 0, 0), ImportOrBuiltin("c", 0, 0)
            ))
            {
                it.buildExtent(expression, 5, 0, 9)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(ident, 0, 1, 4, 1)
                  .buildNode(ident, 0, 2, 8, 1)
                  .buildNode(idFunc, 0, 6, 6, 1)
                  .buildNode(idFunc, 0, 8, 2, 1)
            }
    }

    @Test
    fun `Operator precedence simple 2`() {
        testParseWithEnvironment("(a + b) * c", arrayListOf(
                ImportOrBuiltin("a", 0, 0), ImportOrBuiltin("b", 0, 0), ImportOrBuiltin("c", 0, 0)
        ))
            {
                it.buildExtent(expression, 5, 0, 11)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(ident, 0, 1, 5, 1)
                  .buildNode(idFunc, 0, 8, 3, 1)
                  .buildNode(ident, 0, 2, 10, 1)
                  .buildNode(idFunc, 0, 6, 8, 1)

            }
        
    }

    @Test
    fun `Operator precedence simple 3`() {
        testParseWithEnvironment(
            "a != 7 && a != 8 && a != 9", arrayListOf(
                ImportOrBuiltin("a", 0, 0),
            ))
            {
                it.buildExtent(expression, 11, 0, 26)
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
    }

    @Test
    fun `Operator prefix 1`() {
        testParseWithEnvironment(
            "!a .foo", arrayListOf(
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("foo", funcPrecedence, 1)
            ))
            {
                it.buildExtent(expression, 3, 0, 6)
                  .buildNode(ident, 0, 0, 5, 1)
                  .buildNode(idFunc, 0, 1, 4, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
        
    }

    @Test
    fun `Operator prefix 2`() {
        testParseWithEnvironment(
            "!a .foo b !c", arrayListOf(
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("b", 0, 0),
                    ImportOrBuiltin("c", 0, 0),
                  ImportOrBuiltin("foo", funcPrecedence, 3),
            ))
            {
                it.buildExtent(expression, 6, 0, 11)
                  .buildNode(ident, 0, 0, 5, 1)
                  .buildNode(idFunc, 0, 1, 4, 1)
                  .buildNode(ident, 0, 1, 7, 1)
                  .buildNode(ident, 0, 2, 10, 1)
                  .buildNode(idFunc, 0, 1, 9, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
    }

    @Test
    fun `Operator prefix 3`() {
        testParseWithEnvironment(
            "!(a || b) .foo", arrayListOf(
                ImportOrBuiltin("a", 0, 0),
                    ImportOrBuiltin("b", 0, 0),
                  ImportOrBuiltin("foo", funcPrecedence, 1),
            ))
            {
                it.buildExtent(expression, 5, 0, 13)
                  .buildNode(ident, 0, 0, 6, 1)
                  .buildNode(ident, 0, 1, 11, 1)
                  .buildNode(idFunc, 0, 25, 8, 2)
                  .buildNode(idFunc, 0, 1, 4, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
    }

    @Test
    fun `Operator arithmetic 1`() {
        testParseWithEnvironment(
            "a + (b - c % 2)**11", arrayListOf(
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("b", 0, 0),
                ImportOrBuiltin("c", 0, 0),
            ))
            {
                it.buildExtent(expression, 9, 0, 19)
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
    }

    @Test
    fun `Operator arithmetic 2`() {
        testParseWithEnvironment(
            "a + (-5)", arrayListOf(
                ImportOrBuiltin("a", 0, 0)
            ))
            {
                it.buildExtent(expression, 3, 0, 8)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 5, 2)
                  .buildNode(idFunc, 0, 8, 2, 1)
            }
    }

    @Test
    fun `Operator arithmetic 3`() {
        testParseWithEnvironment(
            "a + !(-5)", arrayListOf(
                ImportOrBuiltin("a", 0, 0)
            ))
            {
                it.buildExtent(expression, 4, 0, 9)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 6, 2)
                  .buildNode(idFunc, 0, 1, 4, 1)
                  .buildNode(idFunc, 0, 8, 2, 1)
            }
        
    }

    @Test
    fun `Operator arithmetic 4`() {
        testParseWithEnvironment(
            "a + !!(-5)", arrayListOf(
                ImportOrBuiltin("a", 0, 0)
            ))
            {
                it.buildExtent(expression, 5, 0, 10)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 7, 2)
                  .buildNode(idFunc, 0, 1, 5, 1)
                  .buildNode(idFunc, 0, 1, 4, 1)
                  .buildNode(idFunc, 0, 8, 2, 1)
            }
    }

    @Test
    fun `Operator arity error 1`() {
        testParseWithEnvironment(
            "a + 5 100", arrayListOf(
                ImportOrBuiltin("a", 0, 0)
            ))
            {
                it.buildExtent(expression, 0, 0, 9)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 5, 4, 1)
                  .buildError(errorOperatorWrongArity)
            }
    }

    @Test
    fun `Single-item expression 1`() {
        testParseWithEnvironment(
            "a + (5)", arrayListOf(
                ImportOrBuiltin("a", 0, 0)
            ))
            {
                it.buildExtent(expression, 3, 0, 7)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 5, 5, 1)
                  .buildNode(idFunc, 0, 8, 2, 1)
            }
    }

    @Test
    fun `Single-item expression 2`() {
        testParseWithEnvironment(
            "5 .foo !!!(a)", arrayListOf(
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("foo", funcPrecedence, 2)
            ))
            {
                it.buildExtent(expression, 6, 0, 12)
                  .buildNode(litInt, 0, 5, 4, 1)
                  .buildNode(ident, 0, 0, 10, 1)
                  .buildNode(idFunc, 0, 1, 8, 1)
                  .buildNode(idFunc, 0, 1, 7, 1)
                  .buildNode(idFunc, 0, 1, 6, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 0, 3)
            }
    }

    @Test
    fun `Unknown function arity`() {
        testParseWithEnvironment(
            "a .foo b c", arrayListOf(
                ImportOrBuiltin("a", 0, 0),
                ImportOrBuiltin("b", 0, 0),
                ImportOrBuiltin("c", 0, 0),
                ImportOrBuiltin("foo", funcPrecedence, 2),
            ))
            {
                it.buildExtent(expression, 4, 0, 10)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(ident, 0, 1, 7, 1)
                  .buildNode(ident, 0, 2, 9, 1)
            }
    }

}

    
@Nested
inner class AssignmentTest {
    @Test
    fun `Simple assignment 1`() {
        testParseWithEnvironment(
            "a = 1 + 5", arrayListOf())
            {
                it.buildInsertString("a")
                  .buildExtent(statementAssignment, 5, 0, 9)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildExtent(expression, 3, 4, 5)
                  .buildNode(litInt, 0, 1, 4, 1)
                  .buildNode(litInt, 0, 5, 8, 1)
                  .buildNode(idFunc, 0, 8, 6, 1)
            }
    }

    @Test
    fun `Simple assignment 2`() {
        testParseWithEnvironment(
            "a = 9", arrayListOf())
            {
                it.buildInsertString("a")
                  .buildExtent(statementAssignment, 2, 0, 5)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 9, 4, 1)
            }
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
}""", arrayListOf(ImportOrBuiltin("print", funcPrecedence, 1)))
            {
                it.buildInsertString("x")
                  .buildExtent(scope, 6, 1, 24)
                  .buildExtent(statementAssignment, 2, 6, 5)
                  .buildNode(binding, 0, 0, 6, 1)
                  .buildNode(litInt, 0, 5, 10, 1)
                  .buildExtent(expression, 2, 17, 7)
                  .buildNode(ident, 0, 0, 23, 1)
                  .buildNode(idFunc, 0, it.indFirstFunction, 17, 5)
            }
    }

    @Test
    fun `Simple scope 2`() {
        testParseWithEnvironment(
            """{
    x = 123;  yy = x * 10
    yy .print
}""",
            arrayListOf(ImportOrBuiltin("print", funcPrecedence, 1)))
            {
                it.buildInsertString("x")
                  .buildInsertString("yy")
                  .buildExtent(scope, 12, 1, 40)
                  .buildExtent(statementAssignment, 2, 6, 7)
                  .buildNode(binding, 0, 0, 6, 1)
                  .buildNode(litInt, 0, 123, 10, 3)
                  .buildExtent(statementAssignment, 5, 16, 11)
                  .buildNode(binding, 0, 1, 16, 2)
                  .buildExtent(expression, 3, 21, 6)
                  .buildNode(ident, 0, 0, 21, 1)
                  .buildNode(litInt, 0, 10, 25, 2)
                  .buildNode(idFunc, 0, 6, 23, 1)
                  .buildExtent(expression, 2, 32, 8)
                  .buildNode(ident, 0, 1, 38, 2)
                  .buildNode(idFunc, 0, it.indFirstFunction, 32, 5)
            }
    }

    @Test
    fun `Scope inside statement`() {
        testParseWithEnvironment(
            """a = 5 + {
    x = 15
    x*2
}/3""", arrayListOf())
            {
                it.buildInsertString("a")
                  .buildInsertString("x")
                  .buildExtent(statementAssignment, 14, 0, 32)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildExtent(expression, 12, 4, 28)

                  .buildNode(litInt, 0, 5, 4, 1)

                  .buildExtent(scope, 7, 9, 20)
                  .buildExtent(statementAssignment, 2, 14, 6)
                  .buildNode(binding, 0, 1, 14, 1)
                  .buildNode(litInt, 0, 15, 18, 2)
                  .buildExtent(expression, 3, 25, 3)
                  .buildNode(ident, 0, 1, 25, 1)
                  .buildNode(litInt, 0, 2, 27, 1)
                  .buildNode(idFunc, 0, 6, 26, 1)

                  .buildNode(litInt, 0, 3, 31, 1)
                  .buildNode(idFunc, 0, 13, 30, 1)

                  .buildNode(idFunc, 0, 8, 6, 1)
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
    resultBuilder(controlParser)

    val parseCorrect = Parser.equality(testSpecimen, controlParser)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(testSpecimen, controlParser)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}
    

}