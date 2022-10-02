import compiler.lexer.*
import compiler.lexer.Lexer
import compiler.lexer.PunctuationToken
import compiler.lexer.RegularToken
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals

internal class LexerTest {


private fun testInpOutp(inp: String, expected: Lexer) {
    val lr = Lexer()
    lr.lexicallyAnalyze(inp.toByteArray())
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
        testInpOutp("asdf Abc",
                    Lexer().buildPunct(0, 8, PunctuationToken.statement, 2)
                               .build(0, 0, 4, RegularToken.word)
                               .build(0, 5, 3, RegularToken.word)
        )
    }

    @Test
    fun `Word snake case`() {
        testInpOutp("asdf_abc",
            Lexer().error(errorWordUnderscoresOnlyAtStart)
        )
    }

    @Test
    fun `Word correct capitalization 1`() {
        testInpOutp("Asdf.abc",
            Lexer().buildPunct(0, 8, PunctuationToken.statement, 1)
                       .build(0, 0, 8, RegularToken.word)
        )
    }

    @Test
    fun `Word correct capitalization 2`() {
        testInpOutp("asdf.abcd.zyui",
            Lexer().buildPunct(0, 14, PunctuationToken.statement, 1)
                       .build(0, 0, 14, RegularToken.word)
        )
    }

    @Test
    fun `Word correct capitalization 3`() {
        testInpOutp("Asdf.Abcd",
            Lexer().buildPunct(0, 9, PunctuationToken.statement, 1)
                       .build(0, 0, 9, RegularToken.word)
        )
    }

    @Test
    fun `Word incorrect capitalization`() {
        testInpOutp("asdf.Abcd",
            Lexer().error(errorWordCapitalizationOrder)
        )
    }

    @Test
    fun `Word starts with underscore and lowercase letter`() {
        testInpOutp("_abc",
            Lexer().buildPunct(0, 4, PunctuationToken.statement, 1)
                       .build(0, 0, 4, RegularToken.word)
        )
    }

    @Test
    fun `Word starts with underscore and capital letter`() {
        testInpOutp("_Abc",
            Lexer().buildPunct(0, 4, PunctuationToken.statement, 1)
                       .build(0, 0, 4, RegularToken.word)
        )
    }

    @Test
    fun `Word starts with 2 underscores`() {
        testInpOutp("__abc",
            Lexer().error(errorWordChunkStart)
        )
    }

    @Test
    fun `Word starts with underscore and digit`() {
        testInpOutp("_1abc",
            Lexer().error(errorWordChunkStart)
        )
    }

    @Test
    fun `Dotword & @-word`() {
        testInpOutp("@a123 .Abc ",
                    Lexer().buildPunct(0, 11, PunctuationToken.statement, 2)
                               .build(0, 0, 5, RegularToken.atWord)
                               .build(0, 6, 4, RegularToken.dotWord)
        )
    }
}

@Nested
inner class LexNumericTest {

    @Test
    fun `Binary numeric 64-bit zero`() {
        testInpOutp("0b0",
            Lexer().buildPunct(0, 3, PunctuationToken.statement, 1)
                       .build(0, 0, 3, RegularToken.litInt)
        )
    }

    @Test
    fun `Binary numeric basic`() {
        testInpOutp("0b101",
            Lexer().buildPunct(0, 5, PunctuationToken.statement, 1)
                       .build(5, 0, 5, RegularToken.litInt)
        )
    }

    @Test
    fun `Binary numeric 64-bit positive`() {
        testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111",
            Lexer().buildPunct(0, 81, PunctuationToken.statement, 1)
                       .build(7, 0, 81, RegularToken.litInt)
        )
    }

    @Test
    fun `Binary numeric 64-bit negative`() {
        testInpOutp("0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1001",
            Lexer().buildPunct(0, 81, PunctuationToken.statement, 1)
                       .build(-7, 0, 81, RegularToken.litInt)
        )
    }

    @Test
    fun `Binary numeric 65-bit error`() {
        testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_01110",
            Lexer().error(errorNumericBinWidthExceeded)
        )
    }

    @Test
    fun `Hex numeric 1`() {
        testInpOutp("0x15",
            Lexer().buildPunct(0, 4, PunctuationToken.statement, 1)
                .build(21, 0, 4, RegularToken.litInt)
        )
    }

    @Test
    fun `Hex numeric 2`() {
        testInpOutp("0x05",
            Lexer().buildPunct(0, 4, PunctuationToken.statement, 1)
                .build(5, 0, 4, RegularToken.litInt)
        )
    }

    @Test
    fun `Hex numeric 3`() {
        testInpOutp("0xFFFFFFFFFFFFFFFF",
            Lexer().buildPunct(0, 18, PunctuationToken.statement, 1)
                .build(-1L, 0, 18, RegularToken.litInt)
        )
    }

    @Test
    fun `Hex numeric too long`() {
        testInpOutp("0xFFFFFFFFFFFFFFFF0",
            Lexer().error(errorNumericBinWidthExceeded)
        )
    }

    @Test
    fun `Float numeric 1`() {
        testInpOutp("1.234",
            Lexer().buildPunct(0, 5, PunctuationToken.statement, 1)
                   .build(1.234, 0, 5)
        )
    }

    @Test
    fun `Float numeric 2`() {
        testInpOutp("00001.234",
            Lexer().buildPunct(0, 9, PunctuationToken.statement, 1)
                .build(1.234, 0, 9)
        )
    }

    @Test
    fun `Float numeric 3`() {
        testInpOutp("10500.01",
            Lexer().buildPunct(0, 8, PunctuationToken.statement, 1)
                .build(10500.01, 0, 8)
        )
    }

    @Test
    fun `Float numeric 4`() {
        testInpOutp("0.9",
            Lexer().buildPunct(0, 3, PunctuationToken.statement, 1)
                .build(0.9, 0, 3)
        )
    }

    @Test
    fun `Float numeric 5`() {
        testInpOutp("100500.123456",
            Lexer().buildPunct(0, 13, PunctuationToken.statement, 1)
                .build(100500.123456, 0, 13)
        )
    }

    @Test
    fun `Float numeric big`() {
        testInpOutp("9007199254740992.0",
            Lexer().buildPunct(0, 18, PunctuationToken.statement, 1)
                .build(9007199254740992.0, 0, 18)
        )
    }

    @Test
    fun `Float numeric too big`() {
        testInpOutp("9007199254740993.0",
            Lexer().error(errorNumericFloatWidthExceeded)
        )
    }


    @Test
    fun `Float numeric big exponent`() {
        testInpOutp("1005001234560000000000.0",
            Lexer().buildPunct(0, 24, PunctuationToken.statement, 1)
                .build(1005001234560000000000.0, 0, 24)
        )
    }

    @Test
    fun `Float numeric tiny`() {
        testInpOutp("0.0000000000000000000003",
            Lexer().buildPunct(0, 24, PunctuationToken.statement, 1)
                .build(0.0000000000000000000003, 0, 24)
        )
    }

    @Test
    fun `Int numeric 1`() {
        testInpOutp("3",
            Lexer().buildPunct(0, 1, PunctuationToken.statement, 1)
                .build(3, 0, 1, RegularToken.litInt)
        )
    }

    @Test
    fun `Int numeric 2`() {
        testInpOutp("12",
            Lexer().buildPunct(0, 2, PunctuationToken.statement, 1)
                .build(12, 0, 2, RegularToken.litInt)
        )
    }

    @Test
    fun `Int numeric 3`() {
        testInpOutp("0987_12",
            Lexer().buildPunct(0, 7, PunctuationToken.statement, 1)
                .build(98712, 0, 7, RegularToken.litInt)
        )
    }

    @Test
    fun `Int numeric 4`() {
        testInpOutp("9_223_372_036_854_775_807",
            Lexer().buildPunct(0, 25, PunctuationToken.statement, 1)
                .build(9_223_372_036_854_775_807L, 0, 25, RegularToken.litInt)
        )
    }

    @Test
    fun `Int numeric error 1`() {
        testInpOutp("3_",
            Lexer().error(errorNumericEndUnderscore)
        )
    }

    @Test
    fun `Int numeric error 2`() {
        testInpOutp("9_223_372_036_854_775_808",
            Lexer().error(errorNumericIntWidthExceeded)
        )
    }
}


@Nested
inner class LexStringTest {
    @Test
    fun `String simple literal`() {
        testInpOutp("'asdf'",
            Lexer().buildPunct(0, 6, PunctuationToken.statement, 1)
                       .build(0, 1, 4, RegularToken.litString)
        )
    }

    @Test
    fun `String literal with escaped apostrophe inside`() {
        testInpOutp("""'wasn\'t' so sure""",
            Lexer().buildPunct(0, 17, PunctuationToken.statement, 3)
                       .build(0, 1, 7, RegularToken.litString)
                       .build(0, 10, 2, RegularToken.word)
                       .build(0, 13, 4, RegularToken.word)
        )
    }

    @Test
    fun `String literal with non-ASCII inside`() {
        testInpOutp("""'hello мир'""",
            Lexer().buildPunct(0, 14, PunctuationToken.statement, 1)
                       .build(0, 1, 12, RegularToken.litString) // 12 because each Cyrillic letter = 2 bytes
        )
    }

    @Test
    fun `String literal unclosed`() {
        testInpOutp("'asdf",
            Lexer().error(errorPrematureEndOfInput)
        )
    }

    @Test
    fun `Verbatim string literal`() {
        testInpOutp("\"asdf foo\"\"\"",
            Lexer().buildPunct(0, 12, PunctuationToken.statement, 1)
                       .build(0, 1, 8, RegularToken.verbatimString)
        )
    }
}

@Nested
inner class LexCommentTest {
    @Test
    fun `Comment simple`() {
        testInpOutp("# this is a comment",
            Lexer().build(0, 1, 18, RegularToken.comment)
        )
    }

    @Test
    fun `Comment inline`() {
        testInpOutp("# this is a comment.# and #this too.# but not this",
            Lexer().buildPunct(22, 28, PunctuationToken.statement, 4)
                       .build(0, 1, 18, RegularToken.comment)
                       .build(0, 22, 3, RegularToken.word)
                       .build(0, 27, 8, RegularToken.comment)
                       .build(0, 38, 3, RegularToken.word)
                       .build(0, 42, 3, RegularToken.word)
                       .build(0, 46, 4, RegularToken.word)
        )
    }
}

@Nested
inner class LexPunctuationTest {
    @Test
    fun `Parens simple`() {
        testInpOutp(
            "(car cdr)",
            Lexer().buildPunct(0, 9, PunctuationToken.statement, 3)
                       .buildPunct(1, 7, PunctuationToken.parens, 2)
                       .build(0, 1, 3, RegularToken.word)
                       .build(0, 5, 3, RegularToken.word)
        )
    }


    @Test
    fun `Parens nested`() {
        testInpOutp(
            "(car (other car) cdr)",
            Lexer()
                .buildPunct(0, 21, PunctuationToken.statement, 6)
                .buildPunct(1, 19, PunctuationToken.parens, 5)
                .build(0, 1, 3, RegularToken.word)
                .buildPunct(6, 9, PunctuationToken.parens, 2)
                .build(0, 6, 5, RegularToken.word)
                .build(0, 12, 3, RegularToken.word)
                .build(0, 17, 3, RegularToken.word)
        )
    }


    @Test
    fun `Parens unclosed`() {
        testInpOutp(
            "(car (other car) cdr",
            Lexer()
                .buildPunct(0, 0, PunctuationToken.statement, 0)
                .buildPunct(1, 0, PunctuationToken.parens, 0)
                .build(0, 1, 3, RegularToken.word)
                .buildPunct(6, 9, PunctuationToken.parens, 2)
                .build(0, 6, 5, RegularToken.word)
                .build(0, 12, 3, RegularToken.word)
                .build(0, 17, 3, RegularToken.word)
                .error(errorPunctuationExtraOpening)
        )
    }


    @Test
    fun `Brackets simple`() {
        testInpOutp(
            "[car cdr]",
            Lexer()
                .buildPunct(0, 9, PunctuationToken.statement, 3)
                .buildPunct(1, 7, PunctuationToken.brackets, 2)
                .build(0, 1, 3, RegularToken.word)
                .build(0, 5, 3, RegularToken.word)
        )
    }


    @Test
    fun `Brackets nested`() {
        testInpOutp(
            "[car [other car] cdr]",
            Lexer()
                .buildPunct(0, 21, PunctuationToken.statement, 6)
                .buildPunct(1, 19, PunctuationToken.brackets, 5)
                .build(0, 1, 3, RegularToken.word)
                .buildPunct(6, 9, PunctuationToken.brackets, 2)
                .build(0, 6, 5, RegularToken.word)
                .build(0, 12, 3, RegularToken.word)
                .build(0, 17, 3, RegularToken.word)
        )
    }


    @Test
    fun `Dot-brackets simple`() {
        testInpOutp(
            "x.[y z]",
            Lexer().buildPunct(0, 7, PunctuationToken.statement, 4)
                       .build(0, 0, 1, RegularToken.word)
                       .buildPunct(3, 3, PunctuationToken.dotBrackets, 2)
                       .build(0, 3, 1, RegularToken.word)
                       .build(0, 5, 1, RegularToken.word)
        )
    }


    @Test
    fun `Brackets mismatched`() {
        testInpOutp(
            "(true false]",
            Lexer()
                .buildPunct(0, 0, PunctuationToken.statement, 0)
                .buildPunct(1, 0, PunctuationToken.parens, 0)
                .build(0, 1, 4, RegularToken.word)
                .build(0, 6, 5, RegularToken.word)
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
            Lexer().buildPunct(1, 12, PunctuationToken.curlyBraces, 4)
                       .buildPunct(2, 4, PunctuationToken.statement, 1)
                       .build(0, 2, 4, RegularToken.word)
                       .buildPunct(8, 4, PunctuationToken.statement, 1)
                       .build(0, 8, 4, RegularToken.word)
        )
    }


    @Test
    fun `Dollar 1`() {
        testInpOutp(
            """foo $ a b""",
            Lexer().buildPunct(0, 9, PunctuationToken.statement, 4)
                .build(0, 0, 3, RegularToken.word)
                .buildPunct(5, 4, PunctuationToken.dollar, 2)
                .build(0, 6, 1, RegularToken.word)
                .build(0, 8, 1, RegularToken.word)
        )
    }

    @Test
    fun `Dollar 2`() {
        testInpOutp(
            """foo $ a b $ 6 5""",
            Lexer().buildPunct(0, 15, PunctuationToken.statement, 7)
                .build(0, 0, 3, RegularToken.word)
                .buildPunct(5, 10, PunctuationToken.dollar, 5)
                .build(0, 6, 1, RegularToken.word)
                .build(0, 8, 1, RegularToken.word)
                .buildPunct(11, 4, PunctuationToken.dollar, 2)
                .build(6, 12, 1, RegularToken.litInt)
                .build(5, 14, 1, RegularToken.litInt)
        )
    }

    @Test
    fun `Dollar 3`() {
        testInpOutp(
            """foo (bar $ 1 2 3)""",
            Lexer().buildPunct(0, 17, PunctuationToken.statement, 7)
                .build(0, 0, 3, RegularToken.word)
                .buildPunct(5, 11, PunctuationToken.parens, 5)
                .build(0, 5, 3, RegularToken.word)
                .buildPunct(10, 6, PunctuationToken.dollar, 3)
                .build(1, 11, 1, RegularToken.litInt)
                .build(2, 13, 1, RegularToken.litInt)
                .build(3, 15, 1, RegularToken.litInt)
        )
    }


    @Test
    fun `Punctuation all types`() {
        testInpOutp(
            """{
asdf (b [d ef (y z)] c d.[x y])

bcjk ({a; b})
}""",
            Lexer().buildPunct(1, 48, PunctuationToken.curlyBraces, 23)
                .buildPunct(2, 31, PunctuationToken.statement, 14)
                .build(0, 2, 4, RegularToken.word)
                .buildPunct(8, 24, PunctuationToken.parens, 12)
                .build(0, 8, 1, RegularToken.word)
                .buildPunct(11, 10, PunctuationToken.brackets, 5)
                .build(0, 11, 1, RegularToken.word)
                .build(0, 13, 2, RegularToken.word)
                .buildPunct(17, 3, PunctuationToken.parens, 2)
                .build(0, 17, 1, RegularToken.word)
                .build(0, 19, 1, RegularToken.word)
                .build(0, 23, 1, RegularToken.word)
                .build(0, 25, 1, RegularToken.word)
                .buildPunct(28, 3, PunctuationToken.dotBrackets, 2)
                .build(0, 28, 1, RegularToken.word)
                .build(0, 30, 1, RegularToken.word)
                .buildPunct(35, 13, PunctuationToken.statement, 7)
                .build(0, 35, 4, RegularToken.word)
                .buildPunct(41, 6, PunctuationToken.parens, 5)
                .buildPunct(42, 4, PunctuationToken.curlyBraces, 4)
                .buildPunct(42, 1, PunctuationToken.statement, 1)
                .build(0, 42, 1, RegularToken.word)
                .buildPunct(45, 1, PunctuationToken.statement, 1)
                .build(0, 45, 1, RegularToken.word)
        )
    }
}


}