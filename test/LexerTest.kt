import compiler.lexer.*
import compiler.lexer.Lexer
import compiler.lexer.PunctuationToken.*
import compiler.lexer.RegularToken.*
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals

internal class LexerTest {


private fun testInpOutp(inp: String, expected: Lexer) {
    val lr = Lexer()
    lr.setInput(inp.toByteArray())
    lr.lexicallyAnalyze()
    val doEqual = Lexer.equality(lr, expected)
    if (!doEqual) {
        val printOut = Lexer.printSideBySide(lr, expected)
        println(printOut)
    }
    assertEquals(doEqual, true)
}


@Nested
inner class LexWordTest {
    @Test
    fun `Simple word lexing`() {
        testInpOutp(
            "asdf Abc",
            Lexer().buildPunct(0, 8, statementFun, 2)
                  .build(0, 0, 4, word)
                  .build(0, 5, 3, word)
        )
    }

    @Test
    fun `Word snake case`() {
        testInpOutp(
            "asdf_abc",
            Lexer().error(errorWordUnderscoresOnlyAtStart)
        )
    }

    @Test
    fun `Word correct capitalization 1`() {
        testInpOutp(
            "Asdf.abc",
            Lexer().buildPunct(0, 8, statementFun, 1)
                  .build(0, 0, 8, word)
        )
    }

    @Test
    fun `Word correct capitalization 2`() {
        testInpOutp(
            "asdf.abcd.zyui",
            Lexer().buildPunct(0, 14, statementFun, 1)
                  .build(0, 0, 14, word)
        )
    }

    @Test
    fun `Word correct capitalization 3`() {
        testInpOutp(
            "Asdf.Abcd",
            Lexer().buildPunct(0, 9, statementFun, 1)
                   .build(0, 0, 9, word)
        )
    }

    @Test
    fun `Word incorrect capitalization`() {
        testInpOutp(
            "asdf.Abcd",
            Lexer().error(errorWordCapitalizationOrder)
        )
    }

    @Test
    fun `Word starts with underscore and lowercase letter`() {
        testInpOutp(
            "_abc",
            Lexer().buildPunct(0, 4, statementFun, 1)
                   .build(0, 0, 4, word)
        )
    }

    @Test
    fun `Word starts with underscore and capital letter`() {
        testInpOutp(
            "_Abc",
            Lexer().buildPunct(0, 4, statementFun, 1)
                   .build(0, 0, 4, word)
        )
    }

    @Test
    fun `Word starts with 2 underscores`() {
        testInpOutp(
            "__abc",
            Lexer().error(errorWordChunkStart)
        )
    }

    @Test
    fun `Word starts with underscore and digit`() {
        testInpOutp(
            "_1abc",
            Lexer().error(errorWordChunkStart)
        )
    }

    @Test
    fun `Dotword & @-word`() {
        testInpOutp(
            "@a123 .Abc ",
            Lexer().buildPunct(0, 11, statementFun, 2)
                   .build(0, 0, 5, atWord)
                   .build(0, 6, 4, dotWord)
        )
    }
}


@Nested
inner class LexNumericTest {

    @Test
    fun `Binary numeric 64-bit zero`() {
        testInpOutp(
            "0b0",
            Lexer().buildPunct(0, 3, statementFun, 1)
                   .build(0, 0, 3, intTok)
        )
    }

    @Test
    fun `Binary numeric basic`() {
        testInpOutp(
            "0b101",
            Lexer().buildPunct(0, 5, statementFun, 1)
                   .build(5, 0, 5, intTok)
        )
    }

    @Test
    fun `Binary numeric 64-bit positive`() {
        testInpOutp(
            "0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111",
            Lexer().buildPunct(0, 81, statementFun, 1)
                   .build(7, 0, 81, intTok)
        )
    }

    @Test
    fun `Binary numeric 64-bit negative`() {
        testInpOutp(
            "0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1001",
            Lexer().buildPunct(0, 81, statementFun, 1)
                   .build(-7, 0, 81, intTok)
        )
    }

    @Test
    fun `Binary numeric 65-bit error`() {
        testInpOutp(
            "0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_01110",
            Lexer().error(errorNumericBinWidthExceeded)
        )
    }

    @Test
    fun `Hex numeric 1`() {
        testInpOutp(
            "0x15",
            Lexer().buildPunct(0, 4, statementFun, 1)
                   .build(21, 0, 4, intTok)
        )
    }

    @Test
    fun `Hex numeric 2`() {
        testInpOutp(
            "0x05",
            Lexer().buildPunct(0, 4, statementFun, 1)
                   .build(5, 0, 4, intTok)
        )
    }

    @Test
    fun `Hex numeric 3`() {
        testInpOutp(
            "0xFFFFFFFFFFFFFFFF",
            Lexer().buildPunct(0, 18, statementFun, 1)
                   .build(-1L, 0, 18, intTok)
        )
    }

    @Test
    fun `Hex numeric too long`() {
        testInpOutp(
            "0xFFFFFFFFFFFFFFFF0",
            Lexer().error(errorNumericBinWidthExceeded)
        )
    }

    @Test
    fun `Float numeric 1`() {
        testInpOutp(
            "1.234",
            Lexer().buildPunct(0, 5, statementFun, 1)
                   .build(1.234, 0, 5)
        )
    }

    @Test
    fun `Float numeric 2`() {
        testInpOutp(
            "00001.234",
            Lexer().buildPunct(0, 9, statementFun, 1)
                   .build(1.234, 0, 9)
        )
    }

    @Test
    fun `Float numeric 3`() {
        testInpOutp(
            "10500.01",
            Lexer().buildPunct(0, 8, statementFun, 1)
                   .build(10500.01, 0, 8)
        )
    }

    @Test
    fun `Float numeric 4`() {
        testInpOutp(
            "0.9",
            Lexer().buildPunct(0, 3, statementFun, 1)
                   .build(0.9, 0, 3)
        )
    }

    @Test
    fun `Float numeric 5`() {
        testInpOutp(
            "100500.123456",
            Lexer().buildPunct(0, 13, statementFun, 1)
                   .build(100500.123456, 0, 13)
        )
    }

    @Test
    fun `Float numeric big`() {
        testInpOutp(
            "9007199254740992.0",
            Lexer().buildPunct(0, 18, statementFun, 1)
                   .build(9007199254740992.0, 0, 18)
        )
    }

    @Test
    fun `Float numeric too big`() {
        testInpOutp(
            "9007199254740993.0",
            Lexer().error(errorNumericFloatWidthExceeded)
        )
    }


    @Test
    fun `Float numeric big exponent`() {
        testInpOutp(
            "1005001234560000000000.0",
            Lexer().buildPunct(0, 24, statementFun, 1)
                   .build(1005001234560000000000.0, 0, 24)
        )
    }

    @Test
    fun `Float numeric tiny`() {
        testInpOutp(
            "0.0000000000000000000003",
            Lexer().buildPunct(0, 24, statementFun, 1)
                   .build(0.0000000000000000000003, 0, 24)
        )
    }

    @Test
    fun `Int numeric 1`() {
        testInpOutp(
            "3",
            Lexer().buildPunct(0, 1, statementFun, 1)
                   .build(3, 0, 1, intTok)
        )
    }

    @Test
    fun `Int numeric 2`() {
        testInpOutp(
            "12",
            Lexer().buildPunct(0, 2, statementFun, 1)
                   .build(12, 0, 2, intTok)
        )
    }

    @Test
    fun `Int numeric 3`() {
        testInpOutp(
            "0987_12",
            Lexer().buildPunct(0, 7, statementFun, 1)
                   .build(98712, 0, 7, intTok)
        )
    }

    @Test
    fun `Int numeric 4`() {
        testInpOutp(
            "9_223_372_036_854_775_807",
            Lexer().buildPunct(0, 25, statementFun, 1)
                   .build(9_223_372_036_854_775_807L, 0, 25, intTok)
        )
    }

    @Test
    fun `Int numeric error 1`() {
        testInpOutp(
            "3_",
            Lexer().error(errorNumericEndUnderscore)
        )
    }

    @Test
    fun `Int numeric error 2`() {
        testInpOutp(
            "9_223_372_036_854_775_808",
            Lexer().error(errorNumericIntWidthExceeded)
        )
    }
}


@Nested
inner class LexStringTest {
    @Test
    fun `String simple literal`() {
        testInpOutp(
            "\"asdf\"",
            Lexer().buildPunct(0, 6, statementFun, 1)
                   .build(0, 1, 4, stringTok)
        )
    }

    @Test
    fun `String literal with escaped quote inside`() {
        testInpOutp(
            "\"wasn\\\"t\" so sure",
            Lexer().buildPunct(0, 17, statementFun, 3)
                   .build(0, 1, 7, stringTok)
                   .build(0, 10, 2, word)
                   .build(0, 13, 4, word)
        )
    }

    @Test
    fun `String literal with non-ASCII inside`() {
        testInpOutp(
            "\"hello мир\"",
            Lexer().buildPunct(0, 14, statementFun, 1)
                  .build(0, 1, 12, stringTok) // 12 because each Cyrillic letter = 2 bytes
        )
    }

    @Test
    fun `String literal unclosed`() {
        testInpOutp(
            "\"asdf",
            Lexer().error(errorPrematureEndOfInput)
        )
    }

//    @Test
    // TODO bring this test back when verbatim strings are implemented
//    fun `Verbatim string literal`() {
//        testInpOutp(
//            "\"asdf foo\"\"\"",
//            Lexer().buildPunct(0, 12, statementFun, 1)
//                .build(0, 1, 8, verbatimString)
//        )
//    }
}


@Nested
inner class LexCommentTest {
    @Test
    fun `Comment simple`() {
        testInpOutp(
            "# this is a comment",
            Lexer().build(0, 1, 18, comment)
        )
    }

    @Test
    fun `Comment inline`() {
        testInpOutp(
            "# this is a comment.# and #this too.# but not this",
            Lexer().buildPunct(22, 28, statementFun, 4)
                   .build(0, 1, 18, comment)
                   .build(0, 22, 3, word)
                   .build(0, 27, 8, comment)
                   .build(0, 38, 3, word)
                   .build(0, 42, 3, word)
                   .build(0, 46, 4, word)
        )
    }
}


@Nested
inner class LexPunctuationTest {
    @Test
    fun `Parens simple`() {
        testInpOutp(
            "(car cdr)",
            Lexer().buildPunct(0, 9, statementFun, 3)
                   .buildPunct(1, 7, parens, 2)
                   .build(0, 1, 3, word)
                   .build(0, 5, 3, word)
        )
    }


    @Test
    fun `Parens nested`() {
        testInpOutp(
            "(car (other car) cdr)",
            Lexer()
                .buildPunct(0, 21, statementFun, 6)
                .buildPunct(1, 19, parens, 5)
                .build(0, 1, 3, word)
                .buildPunct(6, 9, parens, 2)
                .build(0, 6, 5, word)
                .build(0, 12, 3, word)
                .build(0, 17, 3, word)
        )
    }


    @Test
    fun `Parens unclosed`() {
        testInpOutp(
            "(car (other car) cdr",
            Lexer()
                .buildPunct(0, 0, statementFun, 0)
                .buildPunct(1, 0, parens, 0)
                .build(0, 1, 3, word)
                .buildPunct(6, 9, parens, 2)
                .build(0, 6, 5, word)
                .build(0, 12, 3, word)
                .build(0, 17, 3, word)
                .error(errorPunctuationExtraOpening)
        )
    }


    @Test
    fun `Brackets simple`() {

        testInpOutp(
            "[car cdr]",
            Lexer()
                .buildPunct(0, 9, statementFun, 3)
                .buildPunct(1, 7, brackets, 2)
                .build(0, 1, 3, word)
                .build(0, 5, 3, word)
        )
    }


    @Test
    fun `Brackets nested`() {
        testInpOutp(
            "[car [other car] cdr]",
            Lexer()
                .buildPunct(0, 21, statementFun, 6)
                .buildPunct(1, 19, brackets, 5)
                .build(0, 1, 3, word)
                .buildPunct(6, 9, brackets, 2)
                .build(0, 6, 5, word)
                .build(0, 12, 3, word)
                .build(0, 17, 3, word)
        )
    }


    @Test
    fun `Dot-brackets simple`() {
        testInpOutp(
            "x.[y z]",
            Lexer().buildPunct(0, 7, statementFun, 4)
                   .build(0, 0, 1, word)
                   .buildPunct(3, 3, dotBrackets, 2)
                   .build(0, 3, 1, word)
                   .build(0, 5, 1, word)
        )
    }


    @Test
    fun `Brackets mismatched`() {
        testInpOutp(
            "(true false]",
            Lexer()
                .buildPunct(0, 0, statementFun, 0)
                .buildPunct(1, 0, parens, 0)
                .build(0, 1, 4, word)
                .build(0, 6, 5, word)
                .error(errorPunctuationUnmatched)
        )
    }


    @Test
    fun `Curly brace with statements`() {
        testInpOutp(
            """{
asdf

bcjk
}""",
            Lexer().buildPunct(1, 12, curlyBraces, 4)
                .buildPunct(2, 4, statementFun, 1)
                .build(0, 2, 4, word)
                .buildPunct(8, 4, statementFun, 1)
                .build(0, 8, 4, word)
        )
    }


    @Test
    fun `Dollar 1`() {
        testInpOutp(
            """foo $ a b""",
            Lexer().buildPunct(0, 9, statementFun, 4)
                   .build(0, 0, 3, word)
                   .buildPunct(5, 4, dollar, 2)
                   .build(0, 6, 1, word)
                   .build(0, 8, 1, word)
        )
    }

    @Test
    fun `Dollar 2`() {
        testInpOutp(
            """foo $ a b $ 6 5""",
            Lexer().buildPunct(0, 15, statementFun, 7)
                   .build(0, 0, 3, word)
                   .buildPunct(5, 10, dollar, 5)
                   .build(0, 6, 1, word)
                   .build(0, 8, 1, word)
                   .buildPunct(11, 4, dollar, 2)
                   .build(6, 12, 1, intTok)
                   .build(5, 14, 1, intTok)
        )
    }

    @Test
    fun `Dollar 3`() {
        testInpOutp(
            """foo (bar $ 1 2 3)""",
            Lexer().buildPunct(0, 17, statementFun, 7)
                   .build(0, 0, 3, word)
                   .buildPunct(5, 11, parens, 5)
                   .build(0, 5, 3, word)
                   .buildPunct(10, 6, dollar, 3)
                   .build(1, 11, 1, intTok)
                   .build(2, 13, 1, intTok)
                   .build(3, 15, 1, intTok)
        )
    }

    @Test
    fun `Punctuation scope inside statement 1`() {
        testInpOutp(
            """foo bar { asdf }""",
            Lexer().buildPunct(0, 16, statementFun, 5)
                   .build(0, 0, 3, word)
                   .build(0, 4, 3, word)
                   .buildPunct(9, 6, curlyBraces, 2)
                   .buildPunct(10, 5, statementFun, 1)
                   .build(0, 10, 4, word)
        )

    }

    @Test
    fun `Punctuation scope inside statement 2`() {
        testInpOutp(
            """foo bar { 
asdf
bcj
}""".trimMargin(),
            Lexer().buildPunct(0, 21, statementFun, 7)
                   .build(0, 0, 3, word)
                   .build(0, 4, 3, word)
                   .buildPunct(9, 11, curlyBraces, 4)
                   .buildPunct(11, 4, statementFun, 1)
                   .build(0, 11, 4, word)
                   .buildPunct(16, 3, statementFun, 1)
                   .build(0, 16, 3, word)
        )

    }

    @Test
    fun `Punctuation all types`() {
        testInpOutp(
            """{
asdf (b [d ef (y z)] c d.[x y])

bcjk ({a; b})
}""",
            Lexer().buildPunct(1, 48, curlyBraces, 23)
                   .buildPunct(2, 31, statementFun, 14)
                   .build(0, 2, 4, word)
                   .buildPunct(8, 24, parens, 12)
                   .build(0, 8, 1, word)
                   .buildPunct(11, 10, brackets, 5)
                   .build(0, 11, 1, word)
                   .build(0, 13, 2, word)
                   .buildPunct(17, 3, parens, 2)
                   .build(0, 17, 1, word)
                   .build(0, 19, 1, word)
                   .build(0, 23, 1, word)
                   .build(0, 25, 1, word)
                   .buildPunct(28, 3, dotBrackets, 2)
                   .build(0, 28, 1, word)
                   .build(0, 30, 1, word)
                   .buildPunct(35, 13, statementFun, 7)
                   .build(0, 35, 4, word)
                   .buildPunct(41, 6, parens, 5)
                   .buildPunct(42, 4, curlyBraces, 4)
                   .buildPunct(42, 1, statementFun, 1)
                   .build(0, 42, 1, word)
                   .buildPunct(45, 1, statementFun, 1)
                   .build(0, 45, 1, word)
        )
    }
}

@Nested
inner class LexOperatorTest {
    @Test
    fun `Operator simple 1`() {
        testInpOutp("+",
        Lexer().buildPunct(0, 1, statementFun, 1)
               .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 0, 1, operatorTok))
    }

    @Test
    fun `Operator simple 2`() {
        testInpOutp("+ - / * ** && || ^ <- -> := => >=. <=.",
            Lexer()
                .buildPunct(0, 38, statementAssignment, 14)
                .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 0, 1, operatorTok)
                .build(OperatorToken(OperatorType.minus, 0, false).toInt(), 2, 1, operatorTok)
                .build(OperatorToken(OperatorType.divBy, 0, false).toInt(), 4, 1, operatorTok)
                .build(OperatorToken(OperatorType.times, 0, false).toInt(), 6, 1, operatorTok)
                .build(OperatorToken(OperatorType.exponentiation, 0, false).toInt(), 8, 2, operatorTok)
                .build(OperatorToken(OperatorType.boolAnd, 0, false).toInt(), 11, 2, operatorTok)
                .build(OperatorToken(OperatorType.boolOr, 0, false).toInt(), 14, 2, operatorTok)
                .build(OperatorToken(OperatorType.xor, 0, false).toInt(), 17, 1, operatorTok)
                .build(OperatorToken(OperatorType.arrowLeft, 0, false).toInt(), 19, 2, operatorTok)
                .build(OperatorToken(OperatorType.arrowRight, 0, false).toInt(), 22, 2, operatorTok)
                .build(OperatorToken(OperatorType.mutation, 0, false).toInt(), 25, 2, operatorTok)
                .build(OperatorToken(OperatorType.arrowFat, 0, false).toInt(), 28, 2, operatorTok)
                .build(OperatorToken(OperatorType.greaterThanEqInterv, 0, false).toInt(), 31, 3, operatorTok)
                .build(OperatorToken(OperatorType.lessThanEqInterval, 0, false).toInt(), 35, 3, operatorTok)
        )
    }

    @Test
    fun `Operator expression`() {
        testInpOutp("a - b",
            Lexer().buildPunct(0, 5, statementFun, 3)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.minus, 0, false).toInt(), 2, 1, operatorTok)
                   .build(0, 4, 1, word))
    }

    @Test
    fun `Operator assignment 1`() {
        testInpOutp("a += b",
            Lexer().buildPunct(0, 6, statementAssignment, 3)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, true).toInt(), 2, 2, operatorTok)
                   .build(0, 5, 1, word))
    }


    @Test
    fun `Operator assignment 2`() {
        testInpOutp("a ||= b",
            Lexer().buildPunct(0, 7, statementAssignment, 3)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.boolOr, 0, true).toInt(), 2, 3, operatorTok)
                   .build(0, 6, 1, word))
    }

    @Test
    fun `Operator assignment 3`() {
        testInpOutp("a ||=. b",
            Lexer().buildPunct(0, 8, statementAssignment, 3)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.boolOr, 1, true).toInt(), 2, 4, operatorTok)
                   .build(0, 7, 1, word))
    }

    @Test
    fun `Operator assignment 4`() {
        testInpOutp("a ||=: b",
            Lexer().buildPunct(0, 8, statementAssignment, 3)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.boolOr, 2, true).toInt(), 2, 4, operatorTok)
                   .build(0, 7, 1, word))
    }

    @Test
    fun `Operator assignment in parens 1`() {
        testInpOutp("x += y + 5",
            Lexer().buildPunct(0, 10, statementAssignment, 5)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, true).toInt(), 2, 2, operatorTok)
                   .build(0, 5, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 7, 1, operatorTok)
                   .build(5, 9, 1, intTok))
    }

    @Test
    fun `Operator assignment in parens 2`() {
        testInpOutp("(x += y + 5)",
            Lexer().buildPunct(0, 12, statementAssignment, 6)
                   .buildPunct(1, 10, parens, 5)
                   .build(0, 1, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, true).toInt(), 3, 2, operatorTok)
                   .build(0, 6, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 8, 1, operatorTok)
                   .build(5, 10, 1, intTok))
    }

    @Test
    fun `Operator assignment in parens 3`() {
        testInpOutp("((x += y + 5))",
            Lexer().buildPunct(0, 14, statementAssignment, 7)
                   .buildPunct(1, 12, parens, 6)
                   .buildPunct(2, 10, parens, 5)
                   .build(0, 2, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, true).toInt(), 4, 2, operatorTok)
                   .build(0, 7, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 9, 1, operatorTok)
                   .build(5, 11, 1, intTok))
    }

    @Test
    fun `Operator assignment in parens 4`() {
        testInpOutp("x += (y + 5)",
            Lexer().buildPunct(0, 12, statementAssignment, 6)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, true).toInt(), 2, 2, operatorTok)
                   .buildPunct(6, 5, parens, 3)
                   .build(0, 6, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 8, 1, operatorTok)
                   .build(5, 10, 1, intTok))
    }

    @Test
    fun `Operator assignment in parens error 1`() {
        testInpOutp("x (+= y) + 5",
            Lexer().buildPunct(0, 12, statementAssignment, 6)
                   .build(0, 0, 1, word)
                   .buildPunct(3, 4, parens, 2)
                   .build(OperatorToken(OperatorType.plus, 0, true).toInt(), 3, 2, operatorTok)
                   .build(0, 6, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 9, 1, operatorTok)
                   .build(5, 11, 1, intTok)
                   .error(errorOperatorAssignmentPunct)
        )
    }

    @Test
    fun `Operator assignment in parens error 2`() {
        testInpOutp("((x += y) + 5)",
            Lexer().buildPunct(0, 14, statementAssignment, 7)
                   .buildPunct(1, 12, parens, 6)
                   .buildPunct(2, 6, parens, 3)
                   .build(0, 2, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, true).toInt(), 4, 2, operatorTok)
                   .build(0, 7, 1, word)
                   .build(OperatorToken(OperatorType.plus, 0, false).toInt(), 10, 1, operatorTok)
                   .build(5, 12, 1, intTok)
                   .error(errorOperatorAssignmentPunct)
        )
    }

    @Test
    fun `Operator assignment multiple error 1`() {
        testInpOutp("x := y := 7",
            Lexer().buildPunct(0, 0, statementAssignment, 0)
                   .build(0, 0, 1, word)
                   .build(OperatorToken(OperatorType.mutation, 0, false).toInt(), 2, 2, operatorTok)
                   .build(0, 5, 1, word)
                   .build(OperatorToken(OperatorType.mutation, 0, false).toInt(), 7, 2, operatorTok)
                   .error(errorOperatorMultipleAssignment)
        )
    }
}

}