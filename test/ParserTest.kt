import lexer.FileType
import lexer.Lexer
import org.junit.jupiter.api.Nested
import parser.*
import kotlin.test.Test
import kotlin.test.assertEquals

class ParserTest {


@Nested
inner class ExprTest {
    @Test
    fun `Simple function call`() {
        testParseWithEnvironment(
            "foo 10 2 3", arrayListOf(Import("foo", "foo", 3)
        )) {
            it.buildSpan(nodFunctionDef, 6, 0, 12)
              .buildSpan(nodScope, 5, 0, 12)
              .buildSpan(nodExpr, 4, 1, 10)
              .buildNode(nodInt, 0, 10, 5, 2)
              .buildNode(nodInt, 0, 2, 8, 1)
              .buildNode(nodInt, 0, 3, 10, 1)
              .buildNode(nodFunc, 3, 0, 1, 3)
        }
    }

    @Test
    fun `Double function call`() {
        testParseWithEnvironment(
            "foo a (buzz b c d)", arrayListOf(
                Import("foo", "foo", 2),
                Import("buzz", "buzz", 3),
                Import("a", "", -1),
                Import("b", "", -1),
                Import("c", "", -1),
                Import("d", "", -1),
            )
        ) {
            it.buildSpan(nodFunctionDef, 8, 0, 20)
              .buildSpan(nodScope, 7, 0, 20)
              .buildSpan(nodExpr, 6, 1, 18)
              .buildNode(nodId, 0, 2, 5, 1)
              .buildNode(nodId, 0, 3, 13, 1)
              .buildNode(nodId, 0, 4, 15, 1)
              .buildNode(nodId, 0, 5, 17, 1)
              .buildNode(nodFunc, 3, 1, 8, 4) // buzz
              .buildNode(nodFunc, 2, 0, 1, 3) // foo
        }
    }

    @Test
    fun `Infix function calls`() {
        testParseWithEnvironment("(baz : bar 5 : foo c b a)", arrayListOf(
            Import("a", "", -1),
            Import("b", "", -1),
            Import("c", "", -1),
            Import("foo", "foo", 3),
            Import("bar", "bar", 2),
            Import("baz", "baz", 1),
        )) {
            it.buildSpan(nodFunctionDef, 9, 0, 23)
              .buildSpan(nodScope, 8, 0, 23)
              .buildSpan(nodExpr, 7, 1, 21)
              .buildNode(nodId, 0, 2, 5, 1)
              .buildNode(nodId, 0, 1, 7, 1)
              .buildNode(nodId, 0, 0, 9, 1)
              .buildNode(nodFunc, 3, 3, 1, 3) // foo
              .buildIntNode(5, 16, 1)
              .buildNode(nodFunc, 2, 4, 11, 4) // bar
              .buildNode(nodFunc, 1, 5, 18, 4) // baz
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
                it.buildSpan(nodFunctionDef, 6, 0, 18)
                  .buildSpan(nodScope, 5, 0, 18)
                  .buildSpan(nodExpr, 4, 1, 16)
                  .buildNode(nodId, 0, 0, 13, 1) // a
                  .buildNode(nodFunc, 1, 2, 9, 3) // foo
                  .buildNode(nodId, 0, 1, 16, 1) // b
                  .buildNode(nodFunc, 2, 3, 1, 6) // barbar
            }
    }

    @Test
    fun `Operator precedence simple`() {
        testParseWithEnvironment(
            "(a + b * c)", arrayListOf(
                Import("a", "", -1), Import("b", "", -1), Import("c", "", -1)
            )) {
                it.buildSpan(nodFunctionDef, 7, 0, 11)
                  .buildSpan(nodScope, 6, 0, 11)
                  .buildSpan(nodExpr, 5, 1, 9)
                  .buildNode(nodId, 0, 0, 1, 1)
                  .buildNode(nodId, 0, 1, 5, 1)
                  .buildNode(nodId, 0, 2, 9, 1)
                  .buildNode(nodFunc, -2, 8, 7, 1)
                  .buildNode(nodFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Operator precedence simple 2`() {
        testParseWithEnvironment("((a + b) * c)", arrayListOf(
                Import("a", "", -1), Import("b", "", -1), Import("c", "", -1)
        )) {
            it.buildSpan(nodFunctionDef, 7, 0, 13)
              .buildSpan(nodScope, 6, 0, 13)
              .buildSpan(nodExpr, 5, 1, 11)
              .buildNode(nodId, 0, 0, 2, 1)
              .buildNode(nodId, 0, 1, 6, 1)
              .buildNode(nodFunc, -2, 10, 4, 1)
              .buildNode(nodId, 0, 2, 11, 1)
              .buildNode(nodFunc, -2, 8, 9, 1)

        }

    }

    @Test
    fun `Operator precedence simple 3`() {
        testParseWithEnvironment(
            "(a != 7 && a != 8 && a != 9)", arrayListOf(
                Import("a", "", -1),
            )) {
                it.buildSpan(nodFunctionDef, 13, 0, 28)
                  .buildSpan(nodScope, 12, 0, 28)
                  .buildSpan(nodExpr, 11, 1, 26)
                  .buildNode(nodId, 0, 0, 1, 1)
                  .buildNode(nodInt, 0, 7, 6, 1)
                  .buildNode(nodFunc, -2, 0, 3, 2)
                  .buildNode(nodId, 0, 0, 11, 1)
                  .buildNode(nodInt, 0, 8, 16, 1)
                  .buildNode(nodFunc, -2, 0, 13, 2)
                  .buildNode(nodFunc, -2, 4, 8, 2)
                  .buildNode(nodId, 0, 0, 21, 1)
                  .buildNode(nodInt, 0, 9, 26, 1)
                  .buildNode(nodFunc, -2, 0, 23, 2)
                  .buildNode(nodFunc, -2, 4, 18, 2)
        }
    }

    @Test
    fun `Operator prefix 1`() {
        testParseWithEnvironment(
            "(foo !a)", arrayListOf(
                Import("a", "", -1),
                Import("foo", "foo", 1)
            )) {
                it.buildSpan(nodFunctionDef, 5, 0, 8)
                  .buildSpan(nodScope, 4, 0, 8)
                  .buildSpan(nodExpr, 3, 1, 6)
                  .buildNode(nodId, 0, 0, 6, 1)
                  .buildNode(nodFunc, -1, 1, 5, 1)
                  .buildNode(nodFunc, 1, 1, 1, 3)
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
                it.buildSpan(nodFunctionDef, 8, 0, 13)
                  .buildSpan(nodScope, 7, 0, 13)
                  .buildSpan(nodExpr, 6, 1, 11)
                  .buildNode(nodId, 0, 0, 6, 1) // a
                  .buildNode(nodFunc, -1, 1, 5, 1)
                  .buildNode(nodId, 0, 1, 8, 1) // b
                  .buildNode(nodId, 0, 2, 11, 1) // c
                  .buildNode(nodFunc, -1, 1, 10, 1)
                  .buildNode(nodFunc, 3, 3, 1, 3)
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
                it.buildSpan(nodFunctionDef, 7, 0, 15)
                  .buildSpan(nodScope, 6, 0, 15)
                  .buildSpan(nodExpr, 5, 1, 13)
                  .buildNode(nodId, 0, 0, 7, 1) // a
                  .buildNode(nodId, 0, 1, 12, 1) // b
                  .buildNode(nodFunc, -2, 35, 9, 2) // ||
                  .buildNode(nodFunc, -1, 1, 5, 1) // !
                  .buildNode(nodFunc, 1, 2, 1, 3)
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
                it.buildSpan(nodFunctionDef, 11, 0, 21)
                  .buildSpan(nodScope, 10, 0, 21)
                  .buildSpan(nodExpr, 9, 1, 19)
                  .buildNode(nodId, 0, 0, 1, 1)
                  .buildNode(nodId, 0, 1, 6, 1)
                  .buildNode(nodId, 0, 2, 10, 1)
                  .buildNode(nodInt, 0, 2, 14, 1)
                  .buildNode(nodFunc, -2, 3, 12, 1)
                  .buildNode(nodFunc, -2, 13, 8, 1)
                  .buildNode(nodInt, 0, 11, 18, 2)
                  .buildNode(nodFunc, -2, 7, 16, 2)
                  .buildNode(nodFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Operator arithmetic 2`() {
        testParseWithEnvironment(
            "(a + -5)", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(nodFunctionDef, 5, 0, 8)
                  .buildSpan(nodScope, 4, 0, 8)
                  .buildSpan(nodExpr, 3, 1, 6)
                  .buildNode(nodId, 0, 0, 1, 1)
                  .buildIntNode(-5, 5, 2)
                  .buildNode(nodFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Operator arithmetic 3`() {
        testParseWithEnvironment(
            "(a + !(-5))", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(nodFunctionDef, 6, 0, 11)
                  .buildSpan(nodScope, 5, 0, 11)
                  .buildSpan(nodExpr, 4, 1, 9)
                  .buildNode(nodId, 0, 0, 1, 1)
                  .buildIntNode(-5, 7, 2)
                  .buildNode(nodFunc, -1, 1, 5, 1)
                  .buildNode(nodFunc, -2, 10, 3, 1)
            }

    }

    @Test
    fun `Operator arithmetic 4`() {
        testParseWithEnvironment(
            "(a + !!(-3))", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(nodFunctionDef, 7, 0, 12)
                  .buildSpan(nodScope, 6, 0, 12)
                  .buildSpan(nodExpr, 5, 1, 10)
                  .buildNode(nodId, 0, 0, 1, 1)
                  .buildIntNode(-3, 8, 2)
                  .buildNode(nodFunc, -1, 1, 6, 1)
                  .buildNode(nodFunc, -1, 1, 5, 1)
                  .buildNode(nodFunc, -2, 10, 3, 1)
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
    fun `Single-item nodExpr 1`() {
        testParseWithEnvironment(
            "(a + (5))", arrayListOf(
                Import("a", "", -1)
            )) {
                it.buildSpan(nodFunctionDef, 5, 0, 9)
                  .buildSpan(nodScope, 4, 0, 9)
                  .buildSpan(nodExpr, 3, 1, 7)
                  .buildNode(nodId, 0, 0, 1, 1)
                  .buildNode(nodInt, 0, 5, 6, 1)
                  .buildNode(nodFunc, -2, 10, 3, 1)
            }
    }

    @Test
    fun `Single-item nodExpr 2`() {
        testParseWithEnvironment(
            "(foo 5 !!!(a))", arrayListOf(
                Import("a", "", -1),
                Import("foo", "foo", 2)
            )) {
                it.buildSpan(nodFunctionDef, 8, 0, 14)
                  .buildSpan(nodScope, 7, 0, 14)
                  .buildSpan(nodExpr, 6, 1, 12)
                  .buildNode(nodInt, 0, 5, 5, 1)
                  .buildNode(nodId, 0, 0, 11, 1)
                  .buildNode(nodFunc, -1, 1, 9, 1)
                  .buildNode(nodFunc, -1, 1, 8, 1)
                  .buildNode(nodFunc, -1, 1, 7, 1)
                  .buildNode(nodFunc, 2, 1, 1, 4)
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
    fun `Head-function nodExpr 1`() {
        testParseWithEnvironment(
            "((f a) 5)", arrayListOf(
                Import("f", "f", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(nodFunctionDef, 5, 0, 9)
              .buildSpan(nodScope, 4, 0, 9)
              .buildSpan(nodExpr, 3, 1, 7)
              .buildNode(nodId, 0, 1, 4, 1)
              .buildIntNode(5, 7, 1)
              .buildNode(nodFunc, 2, 0, 2, 1)
        }
    }

    @Test
    fun `Head-function nodExpr 2`() {
        testParseWithEnvironment(
            "(f ((g a) 4))", arrayListOf(
                Import("f", "f", 1),
                Import("g", "g", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(nodFunctionDef, 6, 0, 13)
              .buildSpan(nodScope, 5, 0, 13)
              .buildSpan(nodExpr, 4, 1, 11)
              .buildNode(nodId, 0, 2, 7, 1)
              .buildIntNode(4, 10, 1)
              .buildNode(nodFunc, 2, 1, 5, 1)
              .buildNode(nodFunc, 1, 0, 1, 1)
        }
    }

    @Test
    fun `Head-function nodExpr 3`() {
        testParseWithEnvironment(
            "((1 +) 5)", arrayListOf(
                Import("f", "f", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(nodFunctionDef, 5, 0, 9)
                .buildSpan(nodScope, 4, 0, 9)
                .buildSpan(nodExpr, 3, 1, 7)
                .buildIntNode(1, 2, 1)
                .buildIntNode(5, 7, 1)
                .buildNode(nodFunc, -2, 10, 4, 1)
        }
    }

    @Test
    fun `Head-function nodExpr 4`() {
        testParseWithEnvironment(
            "((3*a +) 5)", arrayListOf(
                Import("f", "f", 2),
                Import("a", "", -1),
            )) {
            it.buildSpan(nodFunctionDef, 7, 0, 11)
                .buildSpan(nodScope, 6, 0, 11)
                .buildSpan(nodExpr, 5, 1, 9)
                .buildIntNode(3, 2, 1)
                .buildNode(nodId, 0, 1, 4, 1)
                .buildNode(nodFunc, -2, 8, 3, 1) // *
                .buildIntNode(5, 9, 1)
                .buildNode(nodFunc, -2, 10, 6, 1)// +
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
//            it.buildSpan(nodFunctionDef, 5, 0, 9)
//                .buildSpan(nodScope, 4, 0, 9)
//                .buildSpan(nodExpr, 3, 1, 7)
//                .buildIntNode(1, 2, 1)
//                .buildIntNode(5, 7, 1)
//                .buildNode(nodFunc, -2, 10, 4, 1)
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
                  .buildSpan(nodFunctionDef, 7, 0, 11)
                  .buildSpan(nodScope, 6, 0, 11)
                  .buildSpan(nodStmtAssignment, 5, 1, 9)
                  .buildNode(nodBinding, 0, it.indFirstName, 1, 1)
                  .buildSpan(nodExpr, 3, 5, 5)
                  .buildNode(nodInt, 0, 1, 5, 1)
                  .buildNode(nodInt, 0, 5, 9, 1)
                  .buildNode(nodFunc, -2, 10, 7, 1)
            }
    }

    @Test
    fun `Simple assignment 2`() {
        testParseWithEnvironment(
            "(a = 9)", arrayListOf())
            {
                it.buildInsertString("a")
                  .buildSpan(nodFunctionDef, 4, 0, 7)
                  .buildSpan(nodScope, 3, 0, 7)
                  .buildSpan(nodStmtAssignment, 2, 1, 5)
                  .buildNode(nodBinding, 0, it.indFirstName, 1, 1)
                  .buildNode(nodInt, 0, 9, 5, 1)
            }
    }
}

@Nested
inner class ScopeTest {
    @Test
    fun `Simple nodScope 1`() {
        testParseWithEnvironment(
            """(
    (x = 5)

    (print x)
)""", arrayListOf(Import("print", "console.log", 1)))
            {
                it.buildInsertString("x")
                  .buildSpan(nodFunctionDef, 8, 0, 30)
                  .buildSpan(nodScope, 7, 0, 30)
                  .buildSpan(nodScope, 6, 1, 28)
                  .buildSpan(nodStmtAssignment, 2, 7, 5)
                  .buildNode(nodBinding, 0, it.indFirstName, 7, 1)
                  .buildNode(nodInt, 0, 5, 11, 1)
                  .buildSpan(nodExpr, 2, 20, 7)
                  .buildNode(nodId, 0, it.indFirstName, 26, 1)
                  .buildNode(nodFunc, 1, 0, 20, 5)
            }
    }

    @Test
    fun `Simple nodScope 2`() {
        testParseWithEnvironment(
            """(
    (x = 123)  (yy = x * 10)
    (print yy)
)""",
            arrayListOf(Import("print", "console.log", 1)))
            {
                it.buildInsertString("x")
                  .buildInsertString("yy")
                  .buildSpan(nodFunctionDef, 14, 0, 47)
                  .buildSpan(nodScope, 13, 0, 47)
                  .buildSpan(nodScope, 12, 1, 45)
                  .buildSpan(nodStmtAssignment, 2, 7, 7)
                  .buildNode(nodBinding, 0, it.indFirstName, 7, 1)
                  .buildNode(nodInt, 0, 123, 11, 3)
                  .buildSpan(nodStmtAssignment, 5, 18, 11)
                  .buildNode(nodBinding, 0, it.indFirstName + 1, 18, 2)
                  .buildSpan(nodExpr, 3, 23, 6)
                  .buildNode(nodId, 0, it.indFirstName, 23, 1)
                  .buildNode(nodInt, 0, 10, 27, 2)
                  .buildNode(nodFunc, -2, 8, 25, 1)
                  .buildSpan(nodExpr, 2, 36, 8)
                  .buildNode(nodId, 0, it.indFirstName + 1, 42, 2)
                  .buildNode(nodFunc, 1, 0, 36, 5)
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
            it.buildSpan(nodFunctionDef, 3, 0, 8)
              .buildSpan(nodScope, 2, 0, 8)
              .buildSpan(nodTypeDecl, 1, 1, 6)
              .buildNode(nodTypeId, 1, 0, 4, 3)
        }
    }

    @Test
    fun `Simple type declaration 2`() {
        testParseWithEnvironment(
            "(:: List Int)", arrayListOf(Import("List", "List", 1)))
        {
            it.buildSpan(nodFunctionDef, 4, 0, 13)
              .buildSpan(nodScope, 3, 0, 13)
              .buildSpan(nodTypeDecl, 2, 1, 11)
              .buildNode(nodTypeId, 1, 0, 9, 3)
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
        }
    }

    @Test
    fun `Simple type declaration 3`() {
        testParseWithEnvironment(
            "(:: &Int)", arrayListOf())
        {
            it.buildSpan(nodFunctionDef, 4, 0, 9)
              .buildSpan(nodScope, 3, 0, 9)
              .buildSpan(nodTypeDecl, 2, 1, 7)
              .buildNode(nodTypeId, 1, 0, 5, 3)
              .buildNode(nodTypeFunc, 1 + (1 shl 31), 5, 4, 1)
        }
    }

    @Test
    fun `Simple type declaration 4`() {
        testParseWithEnvironment(
            "(:: List a)", arrayListOf(Import("List", "List", 1)))
        {
            it.buildInsertString("a")
              .buildSpan(nodFunctionDef, 4, 0, 11)
              .buildSpan(nodScope, 3, 0, 11)
              .buildSpan(nodTypeDecl, 2, 1, 9)
              .buildNode(nodTypeId, 0, it.indFirstName, 9, 1)
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
        }
    }

    @Test
    fun `Simple type declaration 5`() {
        testParseWithEnvironment(
            "(:: a Int)", arrayListOf())
        {
            it.buildInsertString("a")
              .buildSpan(nodFunctionDef, 4, 0, 10)
              .buildSpan(nodScope, 3, 0, 10)
              .buildSpan(nodTypeDecl, 2, 1, 8)
              .buildNode(nodTypeId, 1, 0, 6, 3)
              .buildNode(nodTypeFunc, 1, it.indFirstName, 4, 1)
        }
    }

    @Test
    fun `Complex type declaration 1`() {
        testParseWithEnvironment(
            "(:: List Int .Maybe)", arrayListOf(Import("List", "List", 1), Import("Maybe", "Maybe", 1)))
        {
            it.buildSpan(nodFunctionDef, 5, 0, 20)
              .buildSpan(nodScope, 4, 0, 20)
              .buildSpan(nodTypeDecl, 3, 1, 18)
              .buildNode(nodTypeId, 1, 0, 9, 3)
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 2, 13, 6)
        }
    }

    @Test
    fun `Complex type declaration 2`() {
        testParseWithEnvironment(
            "(:: Pair Float (List Int))", arrayListOf(Import("List", "List", 1), Import("Pair", "Pair", 2)))
        {
            it.buildSpan(nodFunctionDef, 6, 0, 26)
              .buildSpan(nodScope, 5, 0, 26)
              .buildSpan(nodTypeDecl, 4, 1, 24)
              .buildNode(nodTypeId, 1, 1, 9, 5) // Float
              .buildNode(nodTypeId, 1, 0, 21, 3) // Int
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 16, 4)
              .buildNode(nodTypeFunc, 2 + (1 shl 30), it.indFirstFunction + 2, 4, 4)
        }
    }

    @Test
    fun `Complex type declaration 3`() {
        testParseWithEnvironment(
            "(:: List &Int)", arrayListOf(Import("List", "List", 1), Import("Pair", "Pair", 2)))
        {
            it.buildSpan(nodFunctionDef, 5, 0, 14)
              .buildSpan(nodScope, 4, 0, 14)
              .buildSpan(nodTypeDecl, 3, 1, 12)
              .buildNode(nodTypeId, 1, 0, 10, 3) // Int
              .buildNode(nodTypeFunc, 1 + (1 shl 31), 5, 9, 1) // &
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 4, 4)
        }
    }

    @Test
    fun `Complex type declaration 4`() {
        testParseWithEnvironment(
            "(:: &Int * Float)", arrayListOf())
        {
            it.buildSpan(nodFunctionDef, 6, 0, 17)
              .buildSpan(nodScope, 5, 0, 17)
              .buildSpan(nodTypeDecl, 4, 1, 15)
              .buildNode(nodTypeId, 1, 0, 5, 3) // Int
              .buildNode(nodTypeFunc, 1 + (1 shl 31), 5, 4, 1) // &
              .buildNode(nodTypeId, 1, 1, 11, 5) // Float
              .buildNode(nodTypeFunc, 2 + (1 shl 31), 8, 9, 1) // *
        }
    }

    @Test
    fun `Nested type declaration 1`() {
        testParseWithEnvironment(
            "(:: Pair &(List Int) (List Bool))", arrayListOf(Import("List", "List", 1), Import("Pair", "Pair", 2)))
        {
            it.buildSpan(nodFunctionDef, 8, 0, 33)
              .buildSpan(nodScope, 7, 0, 33)
              .buildSpan(nodTypeDecl, 6, 1, 31)
              .buildNode(nodTypeId, 1, 0, 16, 3) // Int
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 11, 4) // List
              .buildNode(nodTypeFunc, 1 + (1 shl 31), 5, 9, 1) // &
              .buildNode(nodTypeId, 1, 2, 27, 4) // Float
              .buildNode(nodTypeFunc, 1 + (1 shl 30), it.indFirstFunction + 1, 22, 4) // List
              .buildNode(nodTypeFunc, 2 + (1 shl 30), it.indFirstFunction + 2, 4, 4) // Pair
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

              .buildSpan(nodFunctionDef, 6, 1, 33)
              .buildNode(nodId, 0, it.indFirstName, 10, 1)
              .buildSpan(nodScope, 4, 13, 20)
              .buildSpan(nodReturn, 3, 19, 12)
              .buildNode(nodId, 0, it.indFirstName, 26, 1)
              .buildNode(nodInt, 0, 3, 30, 1)
              .buildNode(nodFunc, -2, 10, 28, 1)

              .buildSpan(nodFunctionDef, 2, 0, 35)
              .buildSpan(nodScope, 1, 0, 35)
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

              .buildSpan(nodFunctionDef, 14, 66, 53) // inner
              .buildNode(nodId, 0, it.indFirstName + 3, 75, 1)
              .buildNode(nodId, 0, it.indFirstName + 4, 77, 1)
              .buildNode(nodId, 0, it.indFirstName + 5, 79, 1)
              .buildSpan(nodScope, 10, 82, 36)
              .buildSpan(nodReturn, 9, 92, 20)
              .buildNode(nodId, 0, it.indFirstName + 3, 99, 1) // a
              .buildNode(nodInt, 0, 2, 103, 1)
              .buildNode(nodId, 0, it.indFirstName + 4, 105, 1) // b
              .buildNode(nodFunc, -2, 8, 104, 1) // *
              .buildNode(nodFunc, -2, 13, 101, 1) // -
              .buildNode(nodInt, 0, 3, 109, 1)
              .buildNode(nodId, 0, it.indFirstName + 5, 111, 1) // c
              .buildNode(nodFunc, -2, 8, 110, 1) // *
              .buildNode(nodFunc, -2, 10, 107, 1) // +

              .buildSpan(nodFunctionDef, 17, 1, 121) // foo
              .buildNode(nodId, 0, it.indFirstName, 8, 1) // param x
              .buildNode(nodId, 0, it.indFirstName + 1, 10, 1) // param y
              .buildSpan(nodScope, 14, 13, 108)
              .buildSpan(nodStmtAssignment, 5, 19, 7)
              .buildNode(nodBinding, 0, it.indFirstName + 7, 19, 1) // z
              .buildSpan(nodExpr, 3, 23, 3)
              .buildNode(nodId, 0, it.indFirstName, 23, 1)     // x
              .buildNode(nodId, 0, it.indFirstName + 1, 25, 1) // y
              .buildNode(nodFunc, -2, 8, 24, 1) // *
              .buildSpan(nodReturn, 6, 33, 25)
              .buildNode(nodId, 0, it.indFirstName + 7, 47, 1) // z
              .buildNode(nodId, 0, it.indFirstName, 51, 1) // x
              .buildNode(nodFunc, -2, 10, 49, 1) // +
              .buildIntNode(5L, 54, 1)
              .buildIntNode(-2L, 56, 2)
              .buildNode(nodFunc, 3, it.indFirstName + 6, 40, 5) // inner
              .buildFnDefPlaceholder(it.indFirstFunction + 2) // definition of inner

              .buildSpan(nodFunctionDef, 2, 0, 123) // entrypoint
              .buildSpan(nodScope, 1, 0, 123)
              .buildFnDefPlaceholder(it.indFirstFunction + 1) // definition of foo
            it.ast.functions.c[it.indFirstFunction*4 + 3] = 33

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
            "(if true. 6)", arrayListOf()
        )
        {
            it.buildSpan(nodFunctionDef, 5, 0, 13)
              .buildSpan(nodScope, 4, 0, 13)
              .buildSpan(nodIfSpan, 3, 1, 11)
              .buildSpan(nodIfClause, 2, 3, 9)
              .buildNode(nodBool, 0, 1, 3, 4)
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
              .buildSpan(nodFunctionDef, 8, 0, 23)
              .buildSpan(nodScope, 7, 0, 23)
              .buildSpan(nodStmtAssignment, 2, 1, 9)
              .buildNode(nodBinding, 0, it.indFirstName, 1, 1)
              .buildNode(nodBool, 0, 0, 5, 5)
              .buildSpan(nodIfSpan, 3, 13, 9)
              .buildSpan(nodIfClause, 2, 15, 7)
              .buildNode(nodId, 0, it.indFirstName, 15, 1)
              .buildIntNode(88, 20, 2)
        }
    }

    @Test
    fun `Simple if 3`() {
        testParseWithEnvironment(
            """x = false.
(if !x. 88)""", arrayListOf()
        )
        {
            it.buildInsertString("x")
              .buildSpan(nodFunctionDef, 10, 0, 24)
              .buildSpan(nodScope, 9, 0, 24)
              .buildSpan(nodStmtAssignment, 2, 1, 9)
              .buildNode(nodBinding, 0, it.indFirstName, 1, 1)
              .buildNode(nodBool, 0, 0, 5, 5)
              .buildSpan(nodIfSpan, 5, 13, 10)
              .buildSpan(nodIfClause, 4, 15, 8)
              .buildSpan(nodExpr, 2, 15, 2)
              .buildNode(nodId, 0, it.indFirstName, 16, 1)
              .buildNode(nodFunc, -1, 1, 15, 1)
              .buildIntNode(88, 21, 2)
        }
    }

    @Test
    fun `If-else 1`() {
        testParseWithEnvironment(
            """x = false.
(if !x. 88. 1)""", arrayListOf()
        )
        {
            it.buildInsertString("x")
              .buildSpan(nodFunctionDef, 13, 0, 28)
              .buildSpan(nodScope, 12, 0, 28)
              .buildSpan(nodStmtAssignment, 2, 1, 9)
              .buildNode(nodBinding, 0, it.indFirstName, 1, 1)
              .buildNode(nodBool, 0, 0, 5, 5)
              .buildSpan(nodIfSpan, 8, 13, 14)
              .buildSpan(nodIfClause, 4, 15, 8)
              .buildSpan(nodExpr, 2, 15, 2)
              .buildNode(nodId, 0, it.indFirstName, 16, 1)
              .buildNode(nodFunc, -1, 1, 15, 1)
              .buildIntNode(88, 21, 2)
              .buildSpan(nodIfClause, 2, 24, 3)
              .buildNode(nodUnderscore, 0, 0, 24, 1)
              .buildIntNode(1, 26, 1)
        }
    }

    @Test
    fun `If-else 2`() {
        testParseWithEnvironment(
            """x = 10.
(if x > 8. 88.
    x > 7. 77.
            1
)""", arrayListOf()
        )
        {
            it.buildInsertString("x")
              .buildSpan(nodFunctionDef, 20, 0, 60)
              .buildSpan(nodScope, 19, 0, 60)
              .buildSpan(nodStmtAssignment, 2, 1, 6)
              .buildNode(nodBinding, 0, it.indFirstName, 1, 1)
              .buildIntNode(10, 5, 2)

              .buildSpan(nodIfSpan, 15, 10, 49)

              .buildSpan(nodIfClause, 5, 13, 12)
              .buildSpan(nodExpr, 3, 13, 5)
              .buildNode(nodId, 0, it.indFirstName, 13, 1)
              .buildIntNode(8, 17, 1)
              .buildNode(nodFunc, -2, 31, 15, 1) // >
              .buildIntNode(88, 23, 2)

              .buildSpan(nodIfClause, 5, 31, 12)
              .buildSpan(nodExpr, 3, 31, 5)
              .buildNode(nodId, 0, it.indFirstName, 31, 1)
              .buildIntNode(7, 35, 1)
              .buildNode(nodFunc, -2, 31, 33, 1) // >
              .buildIntNode(77, 41, 2)


              .buildSpan(nodIfClause, 2, 56, 3)
              .buildNode(nodUnderscore, 0, 0, 56, 1)
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
                it.buildSpan(nodFunctionDef, 4, 0, 12)
                  .buildSpan(nodScope, 3, 0, 12)
                  .buildSpan(nodFor, 2, 1, 10)
                  .buildSpan(nodScope, 1, 6, 5)
                  .buildSpan(nodBreak, 0, 6, 5)
            }
        }

        @Test
        fun `Simple loop 2`() {
            testParseWithEnvironment(
                """(for ...
    print "hw".
    break)""", arrayListOf(Import("print", "print", 1))
            )
            {
                it.buildSpan(nodFunctionDef, 7, 0, 50)
                  .buildSpan(nodScope, 6, 0, 50)
                  .buildSpan(nodFor, 5, 1, 48)
                  .buildSpan(nodScope, 4, 6, 43)
                  .buildSpan(nodExpr, 2, 12, 10)
                  .buildNode(nodString, 0, 0, 19, 2)
                  .buildNode(nodFunc, 1, 0, 12, 5)
                  .buildSpan(nodBreak, 0, 44, 5)
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