import compiler.LexResult
import compiler.Lexer
import compiler.TokenType
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals

internal class LexerTest {
    private fun testInpOutp(inp: String, expected: LexResult) {
        val result = Lexer.lexicallyAnalyze(inp.toByteArray())
        assertEquals(LexResult.equality(result, expected), true)
    }

    @Nested
    inner class LexWordTest {
        @Test
        fun `Simple word lexing`() {
            testInpOutp("asdf Abc",
                        LexResult().add(0, 0, 4, TokenType.word, 0)
                                   .add(0, 5, 3, TokenType.word, 0)
            )
        }

        @Test
        fun `Dotword & @-word`() {
            testInpOutp("@a123 .Abc ",
                        LexResult().add(0, 0, 5, TokenType.atWord, 0)
                                   .add(0, 6, 4, TokenType.dotWord, 0)
            )
        }

        @Test
        fun `Binary numeric 64-bit zero`() {
            testInpOutp("0b0",
                LexResult().add(0, 0, 3, TokenType.litInt, 0)
            )
        }

        @Test
        fun `Binary numeric basic`() {
            testInpOutp("0b101",
                LexResult().add(5, 0, 5, TokenType.litInt, 0)
            )
        }

        @Test
        fun `Binary numeric 64-bit positive`() {
            testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111",
                LexResult().add(7, 0, 81, TokenType.litInt, 0)
            )
        }

        @Test
        fun `Binary numeric 64-bit negative`() {
            testInpOutp("0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1001",
                LexResult().add(-7, 0, 81, TokenType.litInt, 0)
            )
        }
    }
}