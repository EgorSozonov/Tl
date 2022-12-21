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
            "10 .foo 2 3", arrayListOf(Import("foo", "foo", 3)
        )) {
            it.buildExtent(functionDef, 6, 0, 11)
              .buildExtent(scope, 5, 0, 11)
              .buildExtent(expression, 4, 0, 11)
              .buildNode(litInt, 0, 10, 0, 2)
              .buildNode(litInt, 0, 2, 8, 1)
              .buildNode(litInt, 0, 3, 10, 1)
              .buildNode(idFunc, 3, it.indFirstFunction + 1, 3, 4)
        }
    }

    @Test
    fun `Double function call`() {
        testParseWithEnvironment(
            "a .foo (b .bar c d)", arrayListOf(
                Import("foo", "foo", 2),
                Import("bar", "bar", 3),
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
                Import("d", "", -1),
            )
        ) {
            it.buildExtent(functionDef, 8, 0, 19)
              .buildExtent(scope, 7, 0, 19)
              .buildExtent(expression, 6, 0, 19)
              .buildNode(ident, 0, 2, 0, 1)
              .buildNode(ident, 0, 3, 8, 1)
              .buildNode(ident, 0, 4, 15, 1)
              .buildNode(ident, 0, 5, 17, 1)
              .buildNode(idFunc, 3, it.indFirstFunction + 2, 10, 4)
              .buildNode(idFunc, 2, it.indFirstFunction + 1, 2, 4)
        }
    }

    @Test
    fun `Infix function call`() {
        testParseWithEnvironment("a .foo b", arrayListOf(
                Import("foo", "foo", 2),
                Import("a", "", -1),
                Import("b", "", -1),
        )) {
            it.buildExtent(functionDef, 5, 0, 8)
              .buildExtent(scope, 4, 0, 8)
              .buildExtent(expression, 3, 0, 8)
              .buildNode(ident, 0, 1, 0, 1)
              .buildNode(ident, 0, 2, 7, 1)
              .buildNode(idFunc, 2, it.indFirstFunction + 1, 2, 4)
        }
    }

    @Test
    fun `Infix function calls`() {
        testParseWithEnvironment("c .foo b a .bar", arrayListOf(
            Import("a", "", -1),
            Import("b", "", -1),
            Import("c", "", -1),
            Import("foo", "foo", 3),
            Import("bar", "bar", 1),
        )) {
            it.buildExtent(functionDef, 7, 0, 15)
              .buildExtent(scope, 6, 0, 15)
              .buildExtent(expression, 5, 0, 15)
              .buildNode(ident, 0, 2, 0, 1)
              .buildNode(ident, 0, 1, 7, 1)
              .buildNode(ident, 0, 0, 9, 1)
              .buildNode(idFunc, 3, it.indFirstFunction + 1, 2, 4)
              .buildNode(idFunc, 1, it.indFirstFunction + 2, 11, 4)
        }
    }

    @Test
    fun `Parentheses then infix function call`() {
        testParseWithEnvironment(
            "(a .foo) .barbar b", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("foo", "foo", 1),
                Import("barbar", "barbar", 2),
            )) {
                it.buildExtent(functionDef, 6, 0, 18)
                  .buildExtent(scope, 5, 0, 18)
                  .buildExtent(expression, 4, 0, 18)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(idFunc, 1, it.indFirstFunction + 1, 3, 4)
                  .buildNode(ident, 0, 1, 17, 1)
                  .buildNode(idFunc, 2, it.indFirstFunction + 2, 9, 7)
            }
    }

    @Test
    fun `Operator precedence simple`() {
        testParseWithEnvironment(
            "a + b * c", arrayListOf(
                Import("a", "", -1), Import("b", "", -1), Import("c", "", -1)
            )) {
                it.buildExtent(functionDef, 7, 0, 9)
                  .buildExtent(scope, 6, 0, 9)
                  .buildExtent(expression, 5, 0, 9)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(ident, 0, 1, 4, 1)
                  .buildNode(ident, 0, 2, 8, 1)
                  .buildNode(idFunc, -2, 6, 6, 1)
                  .buildNode(idFunc, -2, 8, 2, 1)
            }
    }

    @Test
    fun `Operator precedence simple 2`() {
        testParseWithEnvironment("(a + b) * c", arrayListOf(
                Import("a", "", -1), Import("b", "", -1), Import("c", "", -1)
        )) {
            it.buildExtent(functionDef, 7, 0, 11)
              .buildExtent(scope, 6, 0, 11)
              .buildExtent(expression, 5, 0, 11)
              .buildNode(ident, 0, 0, 1, 1)
              .buildNode(ident, 0, 1, 5, 1)
              .buildNode(idFunc, -2, 8, 3, 1)
              .buildNode(ident, 0, 2, 10, 1)
              .buildNode(idFunc, -2, 6, 8, 1)

        }
        
    }

    @Test
    fun `Operator precedence simple 3`() {
        testParseWithEnvironment(
            "a != 7 && a != 8 && a != 9", arrayListOf(
                Import("a", "", -1),
            )) {
                it.buildExtent(functionDef, 13, 0, 26)
                  .buildExtent(scope, 12, 0, 26)
                  .buildExtent(expression, 11, 0, 26)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 7, 5, 1)
                  .buildNode(idFunc, -2, 0, 2, 2)
                  .buildNode(ident, 0, 0, 10, 1)
                  .buildNode(litInt, 0, 8, 15, 1)
                  .buildNode(idFunc, -2, 0, 12, 2)
                  .buildNode(idFunc, -2, 3, 7, 2)
                  .buildNode(ident, 0, 0, 20, 1)
                  .buildNode(litInt, 0, 9, 25, 1)
                  .buildNode(idFunc, -2, 0, 22, 2)
                  .buildNode(idFunc, -2, 3, 17, 2)

            }
    }

    @Test
    fun `Operator prefix 1`() {
        testParseWithEnvironment(
            "!a .foo", arrayListOf(
                Import("a", "", -1),
                Import("foo", "foo", 1)
            )) {
                it.buildExtent(functionDef, 5, 0, 7)
                  .buildExtent(scope, 4, 0, 7)
                  .buildExtent(expression, 3, 0, 7)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(idFunc, -1, 1, 0, 1)
                  .buildNode(idFunc, 1, it.indFirstFunction + 1, 3, 4)
            }
        
    }

    @Test
    fun `Operator prefix 2`() {
        testParseWithEnvironment(
            "!a .foo b !c", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
                Import("foo", "foo", 3),
            )) {
                it.buildExtent(functionDef, 8, 0, 12)
                  .buildExtent(scope, 7, 0, 12)
                  .buildExtent(expression, 6, 0, 12)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(idFunc, -1, 1, 0, 1)
                  .buildNode(ident, 0, 1, 8, 1)
                  .buildNode(ident, 0, 2, 11, 1)
                  .buildNode(idFunc, -1, 1, 10, 1)
                  .buildNode(idFunc, 3, it.indFirstFunction + 1, 3, 4)
            }
    }

    @Test
    fun `Operator prefix 3`() {
        testParseWithEnvironment(
            "!(a || b) .foo", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("foo", "foo", 1),
            )) {
                it.buildExtent(functionDef, 7, 0, 14)
                  .buildExtent(scope, 6, 0, 14)
                  .buildExtent(expression, 5, 0, 14)
                  .buildNode(ident, 0, 0, 2, 1)
                  .buildNode(ident, 0, 1, 7, 1)
                  .buildNode(idFunc, -2, 35, 4, 2)
                  .buildNode(idFunc, -1, 1, 0, 1)
                  .buildNode(idFunc, 1, it.indFirstFunction + 1, 10, 4)
            }
    }

    @Test
    fun `Operator arithmetic 1`() {
        testParseWithEnvironment(
            "a + (b - c % 2)**11", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
            )) {
                it.buildExtent(functionDef, 11, 0, 19)
                  .buildExtent(scope, 10, 0, 19)
                  .buildExtent(expression, 9, 0, 19)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(ident, 0, 1, 5, 1)
                  .buildNode(ident, 0, 2, 9, 1)
                  .buildNode(litInt, 0, 2, 13, 1)
                  .buildNode(idFunc, -2, 2, 11, 1)
                  .buildNode(idFunc, -2, 11, 7, 1)
                  .buildNode(litInt, 0, 11, 17, 2)
                  .buildNode(idFunc, -2, 5, 15, 2)
                  .buildNode(idFunc, -2, 8, 2, 1)
            }
    }

    @Test
    fun `Operator arithmetic 2`() {
        testParseWithEnvironment(
            "a + (-5)", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildExtent(functionDef, 5, 0, 8)
                  .buildExtent(scope, 4, 0, 8)
                  .buildExtent(expression, 3, 0, 8)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 5, 2)
                  .buildNode(idFunc, -2, 8, 2, 1)
            }
    }

    @Test
    fun `Operator arithmetic 3`() {
        testParseWithEnvironment(
            "a + !(-5)", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildExtent(functionDef, 6, 0, 9)
                  .buildExtent(scope, 5, 0, 9)
                  .buildExtent(expression, 4, 0, 9)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 6, 2)
                  .buildNode(idFunc, -1, 1, 4, 1)
                  .buildNode(idFunc, -2, 8, 2, 1)
            }
        
    }

    @Test
    fun `Operator arithmetic 4`() {
        testParseWithEnvironment(
            "a + !!(-5)", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildExtent(functionDef, 7, 0, 10)
                  .buildExtent(scope, 6, 0, 10)
                  .buildExtent(expression, 5, 0, 10)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, ((-5).toLong() ushr 32).toInt(), ((-5).toLong() and LOWER32BITS).toInt(), 7, 2)
                  .buildNode(idFunc, -1, 1, 5, 1)
                  .buildNode(idFunc, -1, 1, 4, 1)
                  .buildNode(idFunc, -2, 8, 2, 1)
            }
    }

    @Test
    fun `Operator arity error 1`() {
        testParseWithEnvironment(
            "a + 5 100", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildError(errorOperatorWrongArity)
            }
    }

    @Test
    fun `Single-item expression 1`() {
        testParseWithEnvironment(
            "a + (5)", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildExtent(functionDef, 5, 0, 7)
                  .buildExtent(scope, 4, 0, 7)
                  .buildExtent(expression, 3, 0, 7)
                  .buildNode(ident, 0, 0, 0, 1)
                  .buildNode(litInt, 0, 5, 5, 1)
                  .buildNode(idFunc, -2, 8, 2, 1)
            }
    }

    @Test
    fun `Single-item expression 2`() {
        testParseWithEnvironment(
            "5 .foo !!!(a)", arrayListOf(
                Import("a", "", -1),
                Import("foo", "foo", 2)
            )) {
                it.buildExtent(functionDef, 8, 0, 13)
                  .buildExtent(scope, 7, 0, 13)
                  .buildExtent(expression, 6, 0, 13)
                  .buildNode(litInt, 0, 5, 0, 1)
                  .buildNode(ident, 0, 0, 11, 1)
                  .buildNode(idFunc, -1, 1, 9, 1)
                  .buildNode(idFunc, -1, 1, 8, 1)
                  .buildNode(idFunc, -1, 1, 7, 1)
                  .buildNode(idFunc, 2, it.indFirstFunction + 1, 2, 4)
            }
    }

    @Test
    fun `Unknown function arity`() {
        testParseWithEnvironment(
            "a .foo b c", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
                Import("foo", "foo", 2),
            ))
            {
                it.buildError(errorUnknownFunction)
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
                  .buildExtent(functionDef, 7, 0, 9)
                  .buildExtent(scope, 6, 0, 9)
                  .buildExtent(statementAssignment, 5, 0, 9)
                  .buildNode(binding, 0, 0, 0, 1)
                  .buildExtent(expression, 3, 4, 5)
                  .buildNode(litInt, 0, 1, 4, 1)
                  .buildNode(litInt, 0, 5, 8, 1)
                  .buildNode(idFunc, -2, 8, 6, 1)
            }
    }

    @Test
    fun `Simple assignment 2`() {
        testParseWithEnvironment(
            "a = 9", arrayListOf())
            {
                it.buildInsertString("a")
                  .buildExtent(functionDef, 4, 0, 5)
                  .buildExtent(scope, 3, 0, 5)
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
}""", arrayListOf(Import("print", "console.log", 1)))
            {
                it.buildInsertString("x")
                  .buildExtent(functionDef, 8, 0, 27)
                  .buildExtent(scope, 7, 0, 27)
                  .buildExtent(scope, 6, 1, 25)
                  .buildExtent(statementAssignment, 2, 6, 5)
                  .buildNode(binding, 0, 2, 6, 1)
                  .buildNode(litInt, 0, 5, 10, 1)
                  .buildExtent(expression, 2, 17, 8)
                  .buildNode(ident, 0, 2, 17, 1)
                  .buildNode(idFunc, 1, it.indFirstFunction + 1, 19, 6)
            }
    }

    @Test
    fun `Simple scope 2`() {
        testParseWithEnvironment(
            """{
    x = 123;  yy = x * 10
    yy .print
}""",
            arrayListOf(Import("print", "console.log", 1)))
            {
                it.buildInsertString("x")
                  .buildInsertString("yy")
                  .buildExtent(functionDef, 14, 0, 43)
                  .buildExtent(scope, 13, 0, 43)
                  .buildExtent(scope, 12, 1, 41)
                  .buildExtent(statementAssignment, 2, 6, 7)
                  .buildNode(binding, 0, 2, 6, 1)
                  .buildNode(litInt, 0, 123, 10, 3)
                  .buildExtent(statementAssignment, 5, 16, 11)
                  .buildNode(binding, 0, 3, 16, 2)
                  .buildExtent(expression, 3, 21, 6)
                  .buildNode(ident, 0, 2, 21, 1)
                  .buildNode(litInt, 0, 10, 25, 2)
                  .buildNode(idFunc, -2, 6, 23, 1)
                  .buildExtent(expression, 2, 32, 9)
                  .buildNode(ident, 0, 3, 32, 2)
                  .buildNode(idFunc, 1, it.indFirstFunction + 1, 35, 6)
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
                it.buildInsertString("a").buildError(errorExpressionCannotContain)
            }
    }
}


@Nested
inner class FunctionTest {
    @Test
    fun `Simple function def`() {
        testParseWithEnvironment(
            """fn newFn x {
    return x + 3
}""",
            arrayListOf())
        {
            it.buildInsertString("x")
              .buildInsertString("newFn")
              .buildFunction(1, 1, 0)

              .buildExtent(functionDef, 6, 0, 31)
              .buildNode(ident, 0, 0, 9, 1)
              .buildExtent(scope, 4, 12, 18)
              .buildExtent(returnExpression, 3, 17, 12)
              .buildNode(ident, 0, 0, 24, 1)
              .buildNode(litInt, 0, 3, 28, 1)
              .buildNode(idFunc, -2, 8, 26, 1)

              .buildExtent(functionDef, 2, 0, 31)
              .buildExtent(scope, 1, 0, 31)
              .buildFnDefPlaceholder(it.indFirstFunction + 1)
        }
    }


    @Test
    fun `Nested function def`() {
        testParseWithEnvironment(
            """fn foo x y {
    z = x*y
    return (z + x) .inner 5 (-2)

    fn inner a b c {
        return a - 2*b + 3*c
    }
}""",
            arrayListOf())
        {
            it.buildInsertString("x").buildInsertString("y").buildInsertString("foo")
              .buildInsertString("a").buildInsertString("b").buildInsertString("c")
              .buildInsertString("inner").buildInsertString("z")
              .buildFunction(2, 2, 15)
              .buildFunction(6, 3, 0)

              .buildExtent(functionDef, 14, 63, 51) // inner
              .buildNode(ident, 0, 3, 72, 1)
              .buildNode(ident, 0, 4, 74, 1)
              .buildNode(ident, 0, 5, 76, 1)
              .buildExtent(scope, 10, 79, 34)
              .buildExtent(returnExpression, 9, 88, 20)
              .buildNode(ident, 0, 3, 95, 1) // a
              .buildNode(litInt, 0, 2, 99, 1)
              .buildNode(ident, 0, 4, 101, 1) // b
              .buildNode(idFunc, -2, 6, 100, 1) // *
              .buildNode(idFunc, -2, 11, 97, 1) // -
              .buildNode(litInt, 0, 3, 105, 1)
              .buildNode(ident, 0, 5, 107, 1) // c
              .buildNode(idFunc, -2, 6, 106, 1) // *
              .buildNode(idFunc, -2, 8, 103, 1) // +

              .buildExtent(functionDef, 17, 0, 116) // foo
              .buildNode(ident, 0, 0, 7, 1)
              .buildNode(ident, 0, 1, 9, 1)
              .buildExtent(scope, 14, 12, 103)
              .buildExtent(statementAssignment, 5, 17, 7)
              .buildNode(binding, 0, 7, 17, 1) // z
              .buildExtent(expression, 3, 21, 3)
              .buildNode(ident, 0, 0, 21, 1)
              .buildNode(ident, 0, 1, 23, 1)
              .buildNode(idFunc, -2, 6, 22, 1) // *
              .buildExtent(returnExpression, 6, 29, 28)
              .buildNode(ident, 0, 7, 37, 1) // z
              .buildNode(ident, 0, 0, 41, 1) // x
              .buildNode(idFunc, -2, 8, 39, 1) // +
              .buildNode(litInt, 0, 5, 51, 1)
              .buildNode(litInt, ((-2).toLong() ushr 32).toInt(), ((-2).toLong() and LOWER32BITS).toInt(), 54, 2)
              .buildNode(idFunc, 3, it.indFirstFunction + 2, 44, 6)
              .buildFnDefPlaceholder(it.indFirstFunction + 2) // definition of inner

              .buildExtent(functionDef, 2, 0, 116) // entrypoint
              .buildExtent(scope, 1, 0, 116)
              .buildFnDefPlaceholder(it.indFirstFunction + 1) // definition of foo
            it.ast.setBodyId(it.indFirstFunction, 33)

        }
    }


    @Test
    fun `Function def error 1`() {
        testParseWithEnvironment(
            """fn newFn x y x {
    return x + 3
}""",
            arrayListOf())
        {
            it.buildInsertString("x")
              .buildInsertString("y")
              .buildError(errorFnNameAndParams)
        }
    }


    @Test
    fun `Function def error 2`() {
        testParseWithEnvironment(
            """fn newFn x newFn {
    return x + 3
}""",
            arrayListOf())
        {
            it.buildInsertString("x")
              .buildError(errorFnNameAndParams)
        }
    }


    @Test
    fun `Function def error 3`() {
        testParseWithEnvironment(
            """fn newFn x 5""",
            arrayListOf())
        {
            it.buildInsertString("x")
                .buildError(errorFnMissingBody)
        }
    }
}

private fun testParseWithEnvironment(inp: String, imports: ArrayList<Import>, resultBuilder: (Parser) -> Unit) {
    val lr = Lexer(inp.toByteArray(), FileType.executable)
    lr.lexicallyAnalyze()

    val testSpecimen = Parser(lr)
    val controlParser = Parser(lr)

    testSpecimen.parse(imports)

    controlParser.insertImports(imports, FileType.executable)
    //controlParser.ast.funcNode(-1, 0, 0)
    resultBuilder(controlParser)

    val parseCorrect = Parser.equality(testSpecimen, controlParser)
    if (!parseCorrect) {
        val printOut = Parser.printSideBySide(testSpecimen, controlParser)
        println(printOut)
    }
    assertEquals(parseCorrect, true)
}
    

}