import lexer.FileType
import lexer.Lexer
import org.junit.jupiter.api.Nested
import parser.*
import kotlin.test.Test
import kotlin.test.assertEquals
import parser.RegularAST.*
import parser.SpanAST.*

class ParserTest {


@Nested
inner class ExprTest {
    @Test
    fun `Simple function call`() {
        testParseWithEnvironment(
            "(foo 10 2 3)", arrayListOf(Import("foo", "foo", 3)
        )) {
            it.buildSpan(functionDef, 6, 0, 12)
              .buildSpan(scope, 5, 0, 12)
              .buildSpan(expression, 4, 1, 10)
              .buildNode(litInt, 0, 10, 5, 2)
              .buildNode(litInt, 0, 2, 8, 1)
              .buildNode(litInt, 0, 3, 10, 1)
              .buildNode(idFunc, 3, 0, 1, 3)
        }
    }

    @Test
    fun `Double function call`() {
        testParseWithEnvironment(
            "(foo a (buzz b c d))", arrayListOf(
                Import("foo", "foo", 2),
                Import("buzz", "buzz", 3),
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
                Import("d", "", -1),
            )
        ) {
            it.buildSpan(functionDef, 8, 0, 20)
              .buildSpan(scope, 7, 0, 20)
              .buildSpan(expression, 6, 1, 18)
              .buildNode(ident, 0, 2, 5, 1)
              .buildNode(ident, 0, 3, 13, 1)
              .buildNode(ident, 0, 4, 15, 1)
              .buildNode(ident, 0, 5, 17, 1)
              .buildNode(idFunc, 3, 1, 8, 4) // buzz
              .buildNode(idFunc, 2, 0, 1, 3) // foo
        }
    }

    @Test
    fun `Infix function call`() {
        testParseWithEnvironment("(foo a .bar)", arrayListOf(
                Import("foo", "foo", 1),
                Import("bar", "bar", 1),
                Import("a", "", -1),
        )) {
            it.buildSpan(functionDef, 5, 0, 12)
              .buildSpan(scope, 4, 0, 12)
              .buildSpan(expression, 3, 1, 10)
              .buildNode(ident, 0, 2, 5, 1)
              .buildNode(idFunc, 1, 0, 1, 3)
              .buildNode(idFunc, 1, 1, 7, 4)
        }
    }

    @Test
    fun `Infix function calls`() {
        testParseWithEnvironment("(foo c b a .bar 5 .baz)", arrayListOf(
            Import("a", "", -1),
            Import("b", "", -1),
            Import("c", "", -1),
            Import("foo", "foo", 3),
            Import("bar", "bar", 2),
            Import("baz", "baz", 1),
        )) {
            it.buildSpan(functionDef, 9, 0, 23)
              .buildSpan(scope, 8, 0, 23)
              .buildSpan(expression, 7, 1, 21)
              .buildNode(ident, 0, 2, 5, 1)
              .buildNode(ident, 0, 1, 7, 1)
              .buildNode(ident, 0, 0, 9, 1)
              .buildNode(idFunc, 3, 3, 1, 3) // foo
              .buildIntNode(5, 16, 1)
              .buildNode(idFunc, 2, 4, 11, 4) // bar
              .buildNode(idFunc, 1, 5, 18, 4) // baz
        }
    }

    @Test
    fun `Parentheses then infix function call`() {
        testParseWithEnvironment(
            "(barbar (foo a) b)", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("foo", "foo", 1),
                Import("barbar", "barbar", 2),
            )) {
                it.buildSpan(functionDef, 6, 0, 18)
                  .buildSpan(scope, 5, 0, 18)
                  .buildSpan(expression, 4, 1, 16)
                  .buildNode(ident, 0, 0, 13, 1) // a
                  .buildNode(idFunc, 1, 2, 9, 3) // foo
                  .buildNode(ident, 0, 1, 16, 1) // b
                  .buildNode(idFunc, 2, 3, 1, 6) // barbar
            }
    }

    @Test
    fun `Operator precedence simple`() {
        testParseWithEnvironment(
            "(a + b * c)", arrayListOf(
                Import("a", "", -1), Import("b", "", -1), Import("c", "", -1)
            )) {
                it.buildSpan(functionDef, 7, 0, 11)
                  .buildSpan(scope, 6, 0, 11)
                  .buildSpan(expression, 5, 1, 9)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(ident, 0, 1, 5, 1)
                  .buildNode(ident, 0, 2, 9, 1)
                  .buildNode(idFunc, -2, 8, 7, 1)
                  .buildNode(idFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Operator precedence simple 2`() {
        testParseWithEnvironment("((a + b) * c)", arrayListOf(
                Import("a", "", -1), Import("b", "", -1), Import("c", "", -1)
        )) {
            it.buildSpan(functionDef, 7, 0, 13)
              .buildSpan(scope, 6, 0, 13)
              .buildSpan(expression, 5, 1, 11)
              .buildNode(ident, 0, 0, 2, 1)
              .buildNode(ident, 0, 1, 6, 1)
              .buildNode(idFunc, -2, 10, 4, 1)
              .buildNode(ident, 0, 2, 11, 1)
              .buildNode(idFunc, -2, 8, 9, 1)

        }

    }

    @Test
    fun `Operator precedence simple 3`() {
        testParseWithEnvironment(
            "(a != 7 && a != 8 && a != 9)", arrayListOf(
                Import("a", "", -1),
            )) {
                it.buildSpan(functionDef, 13, 0, 28)
                  .buildSpan(scope, 12, 0, 28)
                  .buildSpan(expression, 11, 1, 26)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(litInt, 0, 7, 6, 1)
                  .buildNode(idFunc, -2, 0, 3, 2)
                  .buildNode(ident, 0, 0, 11, 1)
                  .buildNode(litInt, 0, 8, 16, 1)
                  .buildNode(idFunc, -2, 0, 13, 2)
                  .buildNode(idFunc, -2, 4, 8, 2)
                  .buildNode(ident, 0, 0, 21, 1)
                  .buildNode(litInt, 0, 9, 26, 1)
                  .buildNode(idFunc, -2, 0, 23, 2)
                  .buildNode(idFunc, -2, 4, 18, 2)
        }
    }

    @Test
    fun `Operator prefix 1`() {
        testParseWithEnvironment(
            "(foo !a)", arrayListOf(
                Import("a", "", -1),
                Import("foo", "foo", 1)
            )) {
                it.buildSpan(functionDef, 5, 0, 8)
                  .buildSpan(scope, 4, 0, 8)
                  .buildSpan(expression, 3, 1, 6)
                  .buildNode(ident, 0, 0, 6, 1)
                  .buildNode(idFunc, -1, 1, 5, 1)
                  .buildNode(idFunc, 1, 1, 1, 3)
            }

    }

    @Test
    fun `Operator prefix 2`() {
        testParseWithEnvironment(
            "(foo !a b !c)", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
                Import("foo", "foo", 3),
            )) {
                it.buildSpan(functionDef, 8, 0, 13)
                  .buildSpan(scope, 7, 0, 13)
                  .buildSpan(expression, 6, 1, 11)
                  .buildNode(ident, 0, 0, 6, 1) // a
                  .buildNode(idFunc, -1, 1, 5, 1)
                  .buildNode(ident, 0, 1, 8, 1) // b
                  .buildNode(ident, 0, 2, 11, 1) // c
                  .buildNode(idFunc, -1, 1, 10, 1)
                  .buildNode(idFunc, 3, 3, 1, 3)
            }
    }

    @Test
    fun `Operator prefix 3`() {
        testParseWithEnvironment(
            "(foo !(a || b))", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("foo", "foo", 1),
            )) {
                it.buildSpan(functionDef, 7, 0, 15)
                  .buildSpan(scope, 6, 0, 15)
                  .buildSpan(expression, 5, 1, 13)
                  .buildNode(ident, 0, 0, 7, 1) // a
                  .buildNode(ident, 0, 1, 12, 1) // b
                  .buildNode(idFunc, -2, 35, 9, 2) // ||
                  .buildNode(idFunc, -1, 1, 5, 1) // !
                  .buildNode(idFunc, 1, 2, 1, 3)
            }
    }

    @Test
    fun `Operator arithmetic 1`() {
        testParseWithEnvironment(
            "(a + (b - c % 2)**11)", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
            )) {
                it.buildSpan(functionDef, 11, 0, 21)
                  .buildSpan(scope, 10, 0, 21)
                  .buildSpan(expression, 9, 1, 19)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(ident, 0, 1, 6, 1)
                  .buildNode(ident, 0, 2, 10, 1)
                  .buildNode(litInt, 0, 2, 14, 1)
                  .buildNode(idFunc, -2, 3, 12, 1)
                  .buildNode(idFunc, -2, 13, 8, 1)
                  .buildNode(litInt, 0, 11, 18, 2)
                  .buildNode(idFunc, -2, 7, 16, 2)
                  .buildNode(idFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Operator arithmetic 2`() {
        testParseWithEnvironment(
            "(a + -5)", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(functionDef, 5, 0, 8)
                  .buildSpan(scope, 4, 0, 8)
                  .buildSpan(expression, 3, 1, 6)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildIntNode(-5, 5, 2)
                  .buildNode(idFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Operator arithmetic 3`() {
        testParseWithEnvironment(
            "(a + !(-5))", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(functionDef, 6, 0, 11)
                  .buildSpan(scope, 5, 0, 11)
                  .buildSpan(expression, 4, 1, 9)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildIntNode(-5, 7, 2)
                  .buildNode(idFunc, -1, 1, 5, 1)
                  .buildNode(idFunc, -2, 10, 3, 1)
            }

    }

    @Test
    fun `Operator arithmetic 4`() {
        testParseWithEnvironment(
            "(a + !!(-3))", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(functionDef, 7, 0, 12)
                  .buildSpan(scope, 6, 0, 12)
                  .buildSpan(expression, 5, 1, 10)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildIntNode(-3, 8, 2)
                  .buildNode(idFunc, -1, 1, 6, 1)
                  .buildNode(idFunc, -1, 1, 5, 1)
                  .buildNode(idFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Operator arity error 1`() {
        testParseWithEnvironment(
            "(a + 5 100)", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildError(errorOperatorWrongArity)
            }
    }

    @Test
    fun `Single-item expression 1`() {
        testParseWithEnvironment(
            "(a + (5))", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(functionDef, 5, 0, 9)
                  .buildSpan(scope, 4, 0, 9)
                  .buildSpan(expression, 3, 1, 7)
                  .buildNode(ident, 0, 0, 1, 1)
                  .buildNode(litInt, 0, 5, 6, 1)
                  .buildNode(idFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Single-item expression 2`() {
        testParseWithEnvironment(
            "(foo 5 !!!(a))", arrayListOf(
                Import("a", "", -1),
                Import("foo", "foo", 2)
            )) {
                it.buildSpan(functionDef, 8, 0, 14)
                  .buildSpan(scope, 7, 0, 14)
                  .buildSpan(expression, 6, 1, 12)
                  .buildNode(litInt, 0, 5, 5, 1)
                  .buildNode(ident, 0, 0, 11, 1)
                  .buildNode(idFunc, -1, 1, 9, 1)
                  .buildNode(idFunc, -1, 1, 8, 1)
                  .buildNode(idFunc, -1, 1, 7, 1)
                  .buildNode(idFunc, 2, 1, 1, 4)
            }
    }

    @Test
    fun `Unknown function arity`() {
        testParseWithEnvironment(
            "(foo a b c)", arrayListOf(
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
                Import("foo", "foo", 2),
            ))
            {
                it.buildError(errorUnknownFunction)
            }
    }

    @Test
    fun `Head-function expression 1`() {
        testParseWithEnvironment(
            "((f a) 5)", arrayListOf(
                Import("f", "f", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(functionDef, 5, 0, 9)
              .buildSpan(scope, 4, 0, 9)
              .buildSpan(expression, 3, 1, 7)
              .buildNode(ident, 0, 1, 4, 1)
              .buildIntNode(5, 7, 1)
              .buildNode(idFunc, 2, 0, 2, 1)
        }
    }

    @Test
    fun `Head-function expression 2`() {
        testParseWithEnvironment(
            "(f ((g a) 4))", arrayListOf(
                Import("f", "f", 1),
                Import("g", "g", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(functionDef, 6, 0, 13)
              .buildSpan(scope, 5, 0, 13)
              .buildSpan(expression, 4, 1, 11)
              .buildNode(ident, 0, 2, 7, 1)
              .buildIntNode(4, 10, 1)
              .buildNode(idFunc, 2, 1, 5, 1)
              .buildNode(idFunc, 1, 0, 1, 1)
        }
    }

    @Test
    fun `Head-function expression 3`() {
        testParseWithEnvironment(
            "((1 +) 5)", arrayListOf(
                Import("f", "f", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(functionDef, 5, 0, 9)
                .buildSpan(scope, 4, 0, 9)
                .buildSpan(expression, 3, 1, 7)
                .buildIntNode(1, 2, 1)
                .buildIntNode(5, 7, 1)
                .buildNode(idFunc, -2, 10, 4, 1)
        }
    }

    @Test
    fun `Head-function expression 4`() {
        testParseWithEnvironment(
            "((3*a +) 5)", arrayListOf(
                Import("f", "f", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(functionDef, 7, 0, 11)
                .buildSpan(scope, 6, 0, 11)
                .buildSpan(expression, 5, 1, 9)
                .buildIntNode(3, 2, 1)
                .buildNode(ident, 0, 1, 4, 1)
                .buildNode(idFunc, -2, 8, 3, 1) // *
                .buildIntNode(5, 9, 1)
                .buildNode(idFunc, -2, 10, 6, 1)// +
        }
    }

    @Test
    fun `Head-function error 1`() {
        testParseWithEnvironment(
            "((3+a *) 5)", arrayListOf(
                Import("f", "f", 2),
                Import("a", "", -1),
            )) {
            it.buildError(errorExpressionHeadFormOperators)
        }
    }

//    @Test
//    fun `Head-function error 1`() {
//        testParseWithEnvironment(
//            "((1 + x *) 5)", arrayListOf(
//                Import("f", "f", 2),
//                Import("a", "", -1),
//            )) {
//            it.buildSpan(functionDef, 5, 0, 9)
//                .buildSpan(scope, 4, 0, 9)
//                .buildSpan(expression, 3, 1, 7)
//                .buildIntNode(1, 2, 1)
//                .buildIntNode(5, 7, 1)
//                .buildNode(idFunc, -2, 10, 4, 1)
//        }
//    }

}


@Nested
inner class AssignmentTest {
    @Test
    fun `Simple assignment 1`() {
        testParseWithEnvironment(
            "(a = 1 + 5)", arrayListOf())
            {
                it.buildInsertString("a")
                  .buildSpan(functionDef, 7, 0, 11)
                  .buildSpan(scope, 6, 0, 11)
                  .buildSpan(statementAssignment, 5, 1, 9)
                  .buildNode(binding, 0, it.indFirstName, 1, 1)
                  .buildSpan(expression, 3, 5, 5)
                  .buildNode(litInt, 0, 1, 5, 1)
                  .buildNode(litInt, 0, 5, 9, 1)
                  .buildNode(idFunc, -2, 10, 7, 1)
            }
    }

    @Test
    fun `Simple assignment 2`() {
        testParseWithEnvironment(
            "(a = 9)", arrayListOf())
            {
                it.buildInsertString("a")
                  .buildSpan(functionDef, 4, 0, 7)
                  .buildSpan(scope, 3, 0, 7)
                  .buildSpan(statementAssignment, 2, 1, 5)
                  .buildNode(binding, 0, it.indFirstName, 1, 1)
                  .buildNode(litInt, 0, 9, 5, 1)
            }
    }
}

@Nested
inner class ScopeTest {
    @Test
    fun `Simple scope 1`() {
        testParseWithEnvironment(
            """(
    (x = 5)

    (print x)
)""", arrayListOf(Import("print", "console.log", 1)))
            {
                it.buildInsertString("x")
                  .buildSpan(functionDef, 8, 0, 30)
                  .buildSpan(scope, 7, 0, 30)
                  .buildSpan(scope, 6, 1, 28)
                  .buildSpan(statementAssignment, 2, 7, 5)
                  .buildNode(binding, 0, it.indFirstName, 7, 1)
                  .buildNode(litInt, 0, 5, 11, 1)
                  .buildSpan(expression, 2, 20, 7)
                  .buildNode(ident, 0, it.indFirstName, 26, 1)
                  .buildNode(idFunc, 1, 0, 20, 5)
            }
    }

    @Test
    fun `Simple scope 2`() {
        testParseWithEnvironment(
            """(
    (x = 123)  (yy = x * 10)
    (print yy)
)""",
            arrayListOf(Import("print", "console.log", 1)))
            {
                it.buildInsertString("x")
                  .buildInsertString("yy")
                  .buildSpan(functionDef, 14, 0, 47)
                  .buildSpan(scope, 13, 0, 47)
                  .buildSpan(scope, 12, 1, 45)
                  .buildSpan(statementAssignment, 2, 7, 7)
                  .buildNode(binding, 0, it.indFirstName, 7, 1)
                  .buildNode(litInt, 0, 123, 11, 3)
                  .buildSpan(statementAssignment, 5, 18, 11)
                  .buildNode(binding, 0, it.indFirstName + 1, 18, 2)
                  .buildSpan(expression, 3, 23, 6)
                  .buildNode(ident, 0, it.indFirstName, 23, 1)
                  .buildNode(litInt, 0, 10, 27, 2)
                  .buildNode(idFunc, -2, 8, 25, 1)
                  .buildSpan(expression, 2, 36, 8)
                  .buildNode(ident, 0, it.indFirstName + 1, 42, 2)
                  .buildNode(idFunc, 1, 0, 36, 5)
            }
    }

    @Test
    fun `Scope inside statement`() {
        testParseWithEnvironment(
            """(a = 5 + (
    (x = 15)
    (x*2)
)/3)""", arrayListOf())
            {
                it.buildInsertString("a").buildError(errorExpressionCannotContain)
            }
    }
}

@Nested
inner class TypesTest {
    @Test
    fun `Simple type declaration 1`() {
        testParseWithEnvironment(
            "(:: Int)", arrayListOf())
        {
            it.buildSpan(functionDef, 3, 0, 8)
              .buildSpan(scope, 2, 0, 8)
              .buildSpan(typeDecl, 1, 1, 6)
              .buildNode(idType, 1, 0, 4, 3)
        }
    }

    @Test
    fun `Simple type declaration 2`() {
        testParseWithEnvironment(
            "(:: List Int)", arrayListOf(Import("List", "List", 1)))
        {
            it.buildSpan(functionDef, 4, 0, 13)
              .buildSpan(scope, 3, 0, 13)
              .buildSpan(typeDecl, 2, 1, 11)
              .buildNode(idType, 1, 0, 9, 3)
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
        }
    }

    @Test
    fun `Simple type declaration 3`() {
        testParseWithEnvironment(
            "(:: &Int)", arrayListOf())
        {
            it.buildSpan(functionDef, 4, 0, 9)
              .buildSpan(scope, 3, 0, 9)
              .buildSpan(typeDecl, 2, 1, 7)
              .buildNode(idType, 1, 0, 5, 3)
              .buildNode(typeFunc, 1 + (1 shl 31), 5, 4, 1)
        }
    }

    @Test
    fun `Simple type declaration 4`() {
        testParseWithEnvironment(
            "(:: List a)", arrayListOf(Import("List", "List", 1)))
        {
            it.buildInsertString("a")
              .buildSpan(functionDef, 4, 0, 11)
              .buildSpan(scope, 3, 0, 11)
              .buildSpan(typeDecl, 2, 1, 9)
              .buildNode(idType, 0, it.indFirstName, 9, 1)
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
        }
    }

    @Test
    fun `Simple type declaration 5`() {
        testParseWithEnvironment(
            "(:: a Int)", arrayListOf())
        {
            it.buildInsertString("a")
              .buildSpan(functionDef, 4, 0, 10)
              .buildSpan(scope, 3, 0, 10)
              .buildSpan(typeDecl, 2, 1, 8)
              .buildNode(idType, 1, 0, 6, 3)
              .buildNode(typeFunc, 1, it.indFirstName, 4, 1)
        }
    }

    @Test
    fun `Complex type declaration 1`() {
        testParseWithEnvironment(
            "(:: List Int .Maybe)", arrayListOf(Import("List", "List", 1), Import("Maybe", "Maybe", 1)))
        {
            it.buildSpan(functionDef, 5, 0, 20)
              .buildSpan(scope, 4, 0, 20)
              .buildSpan(typeDecl, 3, 1, 18)
              .buildNode(idType, 1, 0, 9, 3)
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 2, 13, 6)
        }
    }

    @Test
    fun `Complex type declaration 2`() {
        testParseWithEnvironment(
            "(:: Pair Float (List Int))", arrayListOf(Import("List", "List", 1), Import("Pair", "Pair", 2)))
        {
            it.buildSpan(functionDef, 6, 0, 26)
              .buildSpan(scope, 5, 0, 26)
              .buildSpan(typeDecl, 4, 1, 24)
              .buildNode(idType, 1, 1, 9, 5) // Float
              .buildNode(idType, 1, 0, 21, 3) // Int
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 16, 4)
              .buildNode(typeFunc, 2 + (1 shl 30), it.indFirstFunction + 2, 4, 4)
        }
    }

    @Test
    fun `Complex type declaration 3`() {
        testParseWithEnvironment(
            "(:: List &Int)", arrayListOf(Import("List", "List", 1), Import("Pair", "Pair", 2)))
        {
            it.buildSpan(functionDef, 5, 0, 14)
              .buildSpan(scope, 4, 0, 14)
              .buildSpan(typeDecl, 3, 1, 12)
              .buildNode(idType, 1, 0, 10, 3) // Int
              .buildNode(typeFunc, 1 + (1 shl 31), 5, 9, 1) // &
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
        }
    }

    @Test
    fun `Complex type declaration 4`() {
        testParseWithEnvironment(
            "(:: &Int * Float)", arrayListOf())
        {
            it.buildSpan(functionDef, 6, 0, 17)
              .buildSpan(scope, 5, 0, 17)
              .buildSpan(typeDecl, 4, 1, 15)
              .buildNode(idType, 1, 0, 5, 3) // Int
              .buildNode(typeFunc, 1 + (1 shl 31), 5, 4, 1) // &
              .buildNode(idType, 1, 1, 11, 5) // Float
              .buildNode(typeFunc, 2 + (1 shl 31), 8, 9, 1) // *
        }
    }

    @Test
    fun `Nested type declaration 1`() {
        testParseWithEnvironment(
            "(:: Pair &(List Int) (List Bool))", arrayListOf(Import("List", "List", 1), Import("Pair", "Pair", 2)))
        {
            it.buildSpan(functionDef, 8, 0, 33)
              .buildSpan(scope, 7, 0, 33)
              .buildSpan(typeDecl, 6, 1, 31)
              .buildNode(idType, 1, 0, 16, 3) // Int
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 11, 4) // List
              .buildNode(typeFunc, 1 + (1 shl 31), 5, 9, 1) // &
              .buildNode(idType, 1, 2, 27, 4) // Float
              .buildNode(typeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 22, 4) // List
              .buildNode(typeFunc, 2 + (1 shl 30), it.indFirstFunction + 2, 4, 4) // Pair
        }
    }

    @Test
    fun `Type declaration error 1`() {
        testParseWithEnvironment(
            "(:: Int Float)", arrayListOf())
        {
            it.buildError(errorUnknownTypeFunction)
        }
    }

    @Test
    fun `Type declaration error 2`() {
        testParseWithEnvironment(
            "(:: &Int Float)", arrayListOf())
        {
            it.buildError(errorExpressionFunctionless)
        }
    }
}

@Nested
inner class FunctionTest {
    @Test
    fun `Simple function def`() {
        testParseWithEnvironment(
            """(fn newFn x (
    (return x + 3)
))""",
            arrayListOf())
        {
            it.buildInsertString("x")
              .buildInsertString("newFn")
              .buildFunction(1, 1, 0)

              .buildSpan(functionDef, 6, 1, 33)
              .buildNode(ident, 0, it.indFirstName, 10, 1)
              .buildSpan(scope, 4, 13, 20)
              .buildSpan(returnExpression, 3, 19, 12)
              .buildNode(ident, 0, it.indFirstName, 26, 1)
              .buildNode(litInt, 0, 3, 30, 1)
              .buildNode(idFunc, -2, 10, 28, 1)

              .buildSpan(functionDef, 2, 0, 35)
              .buildSpan(scope, 1, 0, 35)
              .buildFnDefPlaceholder(it.indFirstFunction + 1)
        }
    }


    @Test
    fun `Nested function def`() {
        testParseWithEnvironment(
            """(fn foo x y (
    (z = x*y)
    (return inner (z + x) 5 -2)

    (fn inner a b c (
        (return a - 2*b + 3*c)
    ))
))""",
            arrayListOf())
        {
            it.buildInsertString("x").buildInsertString("y").buildInsertString("foo")
              .buildInsertString("a").buildInsertString("b").buildInsertString("c")
              .buildInsertString("inner").buildInsertString("z")
              .buildFunction(it.indFirstName + 2, 2, 15)
              .buildFunction(it.indFirstName + 6, 3, 0)

              .buildSpan(functionDef, 14, 66, 53) // inner
              .buildNode(ident, 0, it.indFirstName + 3, 75, 1)
              .buildNode(ident, 0, it.indFirstName + 4, 77, 1)
              .buildNode(ident, 0, it.indFirstName + 5, 79, 1)
              .buildSpan(scope, 10, 82, 36)
              .buildSpan(returnExpression, 9, 92, 20)
              .buildNode(ident, 0, it.indFirstName + 3, 99, 1) // a
              .buildNode(litInt, 0, 2, 103, 1)
              .buildNode(ident, 0, it.indFirstName + 4, 105, 1) // b
              .buildNode(idFunc, -2, 8, 104, 1) // *
              .buildNode(idFunc, -2, 13, 101, 1) // -
              .buildNode(litInt, 0, 3, 109, 1)
              .buildNode(ident, 0, it.indFirstName + 5, 111, 1) // c
              .buildNode(idFunc, -2, 8, 110, 1) // *
              .buildNode(idFunc, -2, 10, 107, 1) // +

              .buildSpan(functionDef, 17, 1, 121) // foo
              .buildNode(ident, 0, it.indFirstName, 8, 1) // param x
              .buildNode(ident, 0, it.indFirstName + 1, 10, 1) // param y
              .buildSpan(scope, 14, 13, 108)
              .buildSpan(statementAssignment, 5, 19, 7)
              .buildNode(binding, 0, it.indFirstName + 7, 19, 1) // z
              .buildSpan(expression, 3, 23, 3)
              .buildNode(ident, 0, it.indFirstName, 23, 1)     // x
              .buildNode(ident, 0, it.indFirstName + 1, 25, 1) // y
              .buildNode(idFunc, -2, 8, 24, 1) // *
              .buildSpan(returnExpression, 6, 33, 25)
              .buildNode(ident, 0, it.indFirstName + 7, 47, 1) // z
              .buildNode(ident, 0, it.indFirstName, 51, 1) // x
              .buildNode(idFunc, -2, 10, 49, 1) // +
              .buildIntNode(5L, 54, 1)
              .buildIntNode(-2L, 56, 2)
              .buildNode(idFunc, 3, it.indFirstName + 6, 40, 5) // inner
              .buildFnDefPlaceholder(it.indFirstFunction + 2) // definition of inner

              .buildSpan(functionDef, 2, 0, 123) // entrypoint
              .buildSpan(scope, 1, 0, 123)
              .buildFnDefPlaceholder(it.indFirstFunction + 1) // definition of foo
            it.ast.functions.setFourth(it.indFirstFunction, 33)

        }
    }


    @Test
    fun `Function def error 1`() {
        testParseWithEnvironment(
            """(fn newFn x y x (
    (return x + 3)
))""",
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
            """(fn newFn x newFn (
    (return x + 3)
))""",
            arrayListOf())
        {
            it.buildInsertString("x")
              .buildError(errorFnNameAndParams)
        }
    }


    @Test
    fun `Function def error 3`() {
        testParseWithEnvironment(
            """(fn newFn x 5)""",
            arrayListOf())
        {
            it.buildInsertString("x")
              .buildError(errorFnMissingBody)
        }
    }
}

@Nested
inner class IfTest {
    @Test
    fun `Simple if 1`() {
        testParseWithEnvironment(
            "(? true -> 6)", arrayListOf()
        )
        {
            it.buildSpan(functionDef, 5, 0, 13)
              .buildSpan(scope, 4, 0, 13)
              .buildSpan(ifSpan, 3, 1, 11)
              .buildSpan(ifClauseGroup, 2, 3, 9)
              .buildNode(litBool, 0, 1, 3, 4)
              .buildIntNode(6, 11, 1)
        }
    }

    @Test
    fun `Simple if 2`() {
        testParseWithEnvironment(
            """(x = false)
(? x -> 88)""", arrayListOf()
        )
        {
            it.buildInsertString("x")
              .buildSpan(functionDef, 8, 0, 23)
              .buildSpan(scope, 7, 0, 23)
              .buildSpan(statementAssignment, 2, 1, 9)
              .buildNode(binding, 0, it.indFirstName, 1, 1)
              .buildNode(litBool, 0, 0, 5, 5)
              .buildSpan(ifSpan, 3, 13, 9)
              .buildSpan(ifClauseGroup, 2, 15, 7)
              .buildNode(ident, 0, it.indFirstName, 15, 1)
              .buildIntNode(88, 20, 2)
        }
    }

    @Test
    fun `Simple if 3`() {
        testParseWithEnvironment(
            """(x = false)
(? !x -> 88)""", arrayListOf()
        )
        {
            it.buildInsertString("x")
              .buildSpan(functionDef, 10, 0, 24)
              .buildSpan(scope, 9, 0, 24)
              .buildSpan(statementAssignment, 2, 1, 9)
              .buildNode(binding, 0, it.indFirstName, 1, 1)
              .buildNode(litBool, 0, 0, 5, 5)
              .buildSpan(ifSpan, 5, 13, 10)
              .buildSpan(ifClauseGroup, 4, 15, 8)
              .buildSpan(expression, 2, 15, 2)
              .buildNode(ident, 0, it.indFirstName, 16, 1)
              .buildNode(idFunc, -1, 1, 15, 1)
              .buildIntNode(88, 21, 2)
        }
    }

    @Test
    fun `If-else 1`() {
        testParseWithEnvironment(
            """(x = false)
(? !x -> 88 : 1)""", arrayListOf()
        )
        {
            it.buildInsertString("x")
              .buildSpan(functionDef, 13, 0, 28)
              .buildSpan(scope, 12, 0, 28)
              .buildSpan(statementAssignment, 2, 1, 9)
              .buildNode(binding, 0, it.indFirstName, 1, 1)
              .buildNode(litBool, 0, 0, 5, 5)
              .buildSpan(ifSpan, 8, 13, 14)
              .buildSpan(ifClauseGroup, 4, 15, 8)
              .buildSpan(expression, 2, 15, 2)
              .buildNode(ident, 0, it.indFirstName, 16, 1)
              .buildNode(idFunc, -1, 1, 15, 1)
              .buildIntNode(88, 21, 2)
              .buildSpan(ifClauseGroup, 2, 24, 3)
              .buildNode(underscore, 0, 0, 24, 1)
              .buildIntNode(1, 26, 1)
        }
    }

    @Test
    fun `If-else 2`() {
        testParseWithEnvironment(
            """(x = 10)
(? (x > 8) -> 88
   (x > 7) -> 77
            : 1)""", arrayListOf()
        )
        {
            it.buildInsertString("x")
              .buildSpan(functionDef, 20, 0, 60)
              .buildSpan(scope, 19, 0, 60)
              .buildSpan(statementAssignment, 2, 1, 6)
              .buildNode(binding, 0, it.indFirstName, 1, 1)
              .buildIntNode(10, 5, 2)

              .buildSpan(ifSpan, 15, 10, 49)

              .buildSpan(ifClauseGroup, 5, 13, 12)
              .buildSpan(expression, 3, 13, 5)
              .buildNode(ident, 0, it.indFirstName, 13, 1)
              .buildIntNode(8, 17, 1)
              .buildNode(idFunc, -2, 31, 15, 1) // >
              .buildIntNode(88, 23, 2)

              .buildSpan(ifClauseGroup, 5, 31, 12)
              .buildSpan(expression, 3, 31, 5)
              .buildNode(ident, 0, it.indFirstName, 31, 1)
              .buildIntNode(7, 35, 1)
              .buildNode(idFunc, -2, 31, 33, 1) // >
              .buildIntNode(77, 41, 2)


              .buildSpan(ifClauseGroup, 2, 56, 3)
              .buildNode(underscore, 0, 0, 56, 1)
              .buildIntNode(1, 58, 1)
        }
    }

}


    @Nested
    inner class LoopTest {
        @Test
        fun `Simple loop 1`() {
            testParseWithEnvironment(
                "(loop break)", arrayListOf()
            )
            {
                it.buildSpan(functionDef, 4, 0, 12)
                  .buildSpan(scope, 3, 0, 12)
                  .buildSpan(loop, 2, 1, 10)
                  .buildSpan(scope, 1, 6, 5)
                  .buildSpan(breakSpan, 0, 6, 5)
            }
        }

        @Test
        fun `Simple loop 2`() {
            testParseWithEnvironment(
                """(loop
    (print "hw")
    break)""", arrayListOf(Import("print", "print", 1))
            )
            {
                it.buildSpan(functionDef, 7, 0, 50)
                  .buildSpan(scope, 6, 0, 50)
                  .buildSpan(loop, 5, 1, 48)
                  .buildSpan(scope, 4, 6, 43)
                  .buildSpan(expression, 2, 12, 10)
                  .buildNode(litString, 0, 0, 19, 2)
                  .buildNode(idFunc, 1, 0, 12, 5)
                  .buildSpan(breakSpan, 0, 44, 5)
            }
        }

        @Test
        fun `Loop error 1`() {
            testParseWithEnvironment(
                """(print 5)
break
""", arrayListOf(Import("print", "print", 1))
            )
            {
                it.buildError(errorLoopBreakOutside)
            }
        }
    }

private fun testParseWithEnvironment(inp: String, imports: ArrayList<Import>, resultBuilder: (Parser) -> Unit) {
    val lr = Lexer(inp.toByteArray(), FileType.executable)
    lr.lexicallyAnalyze()

    if (lr.wasError) {
        println(lr.errMsg)
        assertEquals(true, false)
    }

    val testSpecimen = Parser(lr)
    val controlParser = Parser(lr)

    testSpecimen.parse(imports)

    controlParser.insertImports(imports, FileType.executable)
    resultBuilder(controlParser)

    val indOfDiff = Parser.diffInd(testSpecimen, controlParser)
    if (indOfDiff != null) {
        println("Lexer:")
        println(lr.toDebugString())
        println()
        val printOut = Parser.printSideBySide(testSpecimen, controlParser, indOfDiff)
        println(printOut)
    }
    assertEquals(indOfDiff, null)
}


}