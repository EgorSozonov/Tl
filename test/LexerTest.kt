import compiler.LexResult
import compiler.Lexer
import compiler.PunctuationToken
import compiler.RegularToken
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals

internal class LexerTest {


    private fun testInpOutp(inp: String, expected: LexResult) {
        val result = Lexer.lexicallyAnalyze(inp.toByteArray())
        val doEqual = LexResult.equality(result, expected)
        if (!doEqual) {
            val printOut = LexResult.printSideBySide(result, expected)
            println(printOut)
        }
        assertEquals(doEqual, true)
    }

//
//    @Nested
//    inner class LexWordTest {
//        @Test
//        fun `Simple word lexing`() {
//            testInpOutp("asdf Abc",
//                        LexResult().add(0, 0, 4, RegularToken.word)
//                                   .add(0, 5, 3, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Word snake case`() {
//            testInpOutp("asdf_abc",
//                LexResult().error(Lexer.errorWordUnderscoresOnlyAtStart)
//            )
//        }
//
//        @Test
//        fun `Word correct capitalization 1`() {
//            testInpOutp("Asdf.abc",
//                LexResult().add(0, 0, 8, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Word correct capitalization 2`() {
//            testInpOutp("asdf.abcd.zyui",
//                LexResult().add(0, 0, 14, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Word correct capitalization 3`() {
//            testInpOutp("Asdf.Abcd",
//                LexResult().add(0, 0, 9, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Word incorrect capitalization`() {
//            testInpOutp("asdf.Abcd",
//                LexResult().error(Lexer.errorWordCapitalizationOrder)
//            )
//        }
//
//        @Test
//        fun `Word starts with underscore and lowercase letter`() {
//            testInpOutp("_abc",
//                LexResult().add(0, 0, 4, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Word starts with underscore and capital letter`() {
//            testInpOutp("_Abc",
//                LexResult().add(0, 0, 4, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Word starts with 2 underscores`() {
//            testInpOutp("__abc",
//                LexResult().error(Lexer.errorWordChunkStart)
//            )
//        }
//
//        @Test
//        fun `Word starts with underscore and digit`() {
//            testInpOutp("_1abc",
//                LexResult().error(Lexer.errorWordChunkStart)
//            )
//        }
//
//        @Test
//        fun `Dotword & @-word`() {
//            testInpOutp("@a123 .Abc ",
//                        LexResult().add(0, 0, 5, RegularToken.atWord)
//                                   .add(0, 6, 4, RegularToken.dotWord)
//            )
//        }
//
//
//
//    }
//
//    @Nested
//    inner class LexNumericTest {
//
//        @Test
//        fun `Binary numeric 64-bit zero`() {
//            testInpOutp("0b0",
//                LexResult().add(0, 0, 3, RegularToken.litInt)
//            )
//        }
//
//        @Test
//        fun `Binary numeric basic`() {
//            testInpOutp("0b101",
//                LexResult().add(5, 0, 5, RegularToken.litInt)
//            )
//        }
//
//        @Test
//        fun `Binary numeric 64-bit positive`() {
//            testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111",
//                LexResult().add(7, 0, 81, RegularToken.litInt)
//            )
//        }
//
//        @Test
//        fun `Binary numeric 64-bit negative`() {
//            testInpOutp("0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1001",
//                LexResult().add(-7, 0, 81, RegularToken.litInt)
//            )
//        }
//
//        @Test
//        fun `Binary numeric 65-bit error`() {
//            testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_01110",
//                LexResult().error(Lexer.errorNumericBinWidthExceeded)
//            )
//        }
//
//    }
//
//    @Nested
//    inner class LexStringTest {
//        @Test
//        fun `String simple literal`() {
//            testInpOutp("'asdf'",
//                LexResult().add(0, 1, 4, RegularToken.litString)
//            )
//        }
//
//        @Test
//        fun `String literal with escaped apostrophe inside`() {
//            testInpOutp("""'wasn\'t' so sure""",
//                LexResult().add(0, 1, 7, RegularToken.litString)
//                    .add(0, 10, 2, RegularToken.word)
//                    .add(0, 13, 4, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `String literal with non-ASCII inside`() {
//            testInpOutp("""'hello мир'""",
//                LexResult().add(0, 1, 12, RegularToken.litString) // 12 because each Cyrillic letter = 2 bytes
//            )
//        }
//
//        @Test
//        fun `String literal unclosed`() {
//            testInpOutp("'asdf",
//                LexResult().error(Lexer.errorPrematureEndOfInput)
//            )
//        }
//
//        @Test
//        fun `Verbatim string literal`() {
//            testInpOutp("\"asdf foo\"\"\"",
//                LexResult().add(0, 1, 8, RegularToken.verbatimString)
//            )
//        }
//    }

    @Nested
    inner class LexCommentTest {
        @Test
        fun `Comment simple`() {
            testInpOutp("# this is a comment",
                LexResult().add(0, 1, 18, RegularToken.comment)
            )
        }

        @Test
        fun `Comment inline`() {
            testInpOutp("# this is a comment.# and #this too.# but not this",
                LexResult().add(0, 1, 18, RegularToken.comment)
                           .add(0, 22, 3, RegularToken.word)
                           .add(0, 27, 8, RegularToken.comment)
                           .add(0, 38, 3, RegularToken.word)
                           .add(0, 42, 3, RegularToken.word)
                           .add(0, 46, 4, RegularToken.word)
            )
        }
    }

    @Nested
    inner class LexPunctuationTest {
        @Test
        fun `Parens simple`() {
            testInpOutp(
                "(car cdr)",
                LexResult().addPunctuation(1, 7, PunctuationToken.parens, 2)
                           .add(0, 1, 3, RegularToken.word)
                           .add(0, 5, 3, RegularToken.word)
            )
        }

        @Test
        fun `Parens nested`() {
            testInpOutp(
                "(car (other car) cdr)",
                LexResult()
                    .addPunctuation(1, 19, PunctuationToken.parens, 5)
                    .add(0, 1, 3, RegularToken.word)
                    .addPunctuation(6, 9, PunctuationToken.parens, 2)
                    .add(0, 6, 5, RegularToken.word)
                    .add(0, 12, 3, RegularToken.word)
                    .add(0, 17, 3, RegularToken.word)
            )
        }

        @Test
        fun `Parens unclosed`() {
            testInpOutp(
                "(car (other car) cdr",
                LexResult()
                    .addPunctuation(1, 0, PunctuationToken.parens, 0)
                    .add(0, 1, 3, RegularToken.word)
                    .addPunctuation(6, 9, PunctuationToken.parens, 2)
                    .add(0, 6, 5, RegularToken.word)
                    .add(0, 12, 3, RegularToken.word)
                    .add(0, 17, 3, RegularToken.word)
                    .error(Lexer.errorPunctuationExtraOpening)
            )
        }
//
//        @Test
//        fun `Brackets simple`() {
//            testInpOutp(
//                "[car cdr]",
//                LexResult().addPunctuation(1, 7, PunctuationToken.brackets, 2)
//                    .add(0, 1, 3, RegularToken.word)
//                    .add(0, 5, 3, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Brackets nested`() {
//            testInpOutp(
//                "[car [other car] cdr]",
//                LexResult()
//                    .addPunctuation(1, 19, PunctuationToken.brackets, 5)
//                    .add(0, 1, 3, RegularToken.word)
//                    .addPunctuation(6, 9, PunctuationToken.brackets, 2)
//                    .add(0, 6, 5, RegularToken.word)
//                    .add(0, 12, 3, RegularToken.word)
//                    .add(0, 17, 3, RegularToken.word)
//            )
//        }
//
//        @Test
//        fun `Curly brace with statements`() {
//            testInpOutp(
//                """{
//    asdf
//
//    bcjk
//}""",
//                LexResult().addPunctuation(1, 24, PunctuationToken.curlyBraces, 4)
//                           .addPunctuation(6, 4, PunctuationToken.statement, 1)
//                           .add(0, 6, 4, RegularToken.word)
//                           .addPunctuation(20, 4, PunctuationToken.statement, 1)
//                           .add(0, 20, 4, RegularToken.word)
//            )
//        }
    }
}