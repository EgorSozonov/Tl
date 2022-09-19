import compiler.lexer.*
import compiler.Lexer
import compiler.PunctuationToken
import compiler.RegularToken
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals

internal class LexerTest {
    private val lr: Lexer = Lexer()

    private fun testInpOutp(inp: String, expected: Lexer) {
        val result = lr.lexicallyAnalyze(inp.toByteArray())
        val doEqual = Lexer.equality(result, expected)
        if (!doEqual) {
            val printOut = Lexer.printSideBySide(result, expected)
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
                Lexer().buildPunct(1, 20, PunctuationToken.curlyBraces, 4)
                           .buildPunct(6, 4, PunctuationToken.statement, 1)
                           .build(0, 6, 4, RegularToken.word)
                           .buildPunct(16, 4, PunctuationToken.statement, 1)
                           .build(0, 16, 4, RegularToken.word)
            )
        }


        @Test
        fun `Punctuation all types`() {
            testInpOutp(
                """{
    asdf (b [d ef (y z)] c d.[x y])

    bcjk ({a; b})
}""",
                Lexer().buildPunct(1, 56, PunctuationToken.curlyBraces, 23)
                    .buildPunct(6, 31, PunctuationToken.statement, 14)
                    .build(0, 6, 4, RegularToken.word)
                    .buildPunct(12, 24, PunctuationToken.parens, 12)
                    .build(0, 12, 1, RegularToken.word)
                    .buildPunct(15, 10, PunctuationToken.brackets, 5)
                    .build(0, 15, 1, RegularToken.word)
                    .build(0, 17, 2, RegularToken.word)
                    .buildPunct(21, 3, PunctuationToken.parens, 2)
                    .build(0, 21, 1, RegularToken.word)
                    .build(0, 23, 1, RegularToken.word)
                    .build(0, 27, 1, RegularToken.word)
                    .build(0, 29, 1, RegularToken.word)
                    .buildPunct(32, 3, PunctuationToken.dotBrackets, 2)
                    .build(0, 32, 1, RegularToken.word)
                    .build(0, 34, 1, RegularToken.word)
                    .buildPunct(43, 13, PunctuationToken.statement, 7)
                    .build(0, 43, 4, RegularToken.word)
                    .buildPunct(49, 6, PunctuationToken.parens, 5)
                    .buildPunct(50, 4, PunctuationToken.curlyBraces, 4)
                    .buildPunct(50, 1, PunctuationToken.statement, 1)
                    .build(0, 50, 1, RegularToken.word)
                    .buildPunct(53, 1, PunctuationToken.statement, 1)
                    .build(0, 53, 1, RegularToken.word)
            )
        }
    }
}