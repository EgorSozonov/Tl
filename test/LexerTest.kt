import language.*
import lexer.*
import lexer.Lexer
import lexer.PunctuationToken.*
import lexer.RegularToken.*
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals

internal class LexerTest {


@Nested
inner class LexWordTest {
    @Test
    fun `Simple word lexing`() {
        testInpOutp("asdf Abc") {
            it.build(word, 0, 0, 4)
              .build(word, 1, 5, 3)
        }
    }

    @Test
    fun `Word snake case`() {
        testInpOutp("asdf_abc") {
            it.bError(errorWordUnderscoresOnlyAtStart)
        }
    }

    @Test
    fun `Word correct capitalization 1`() {
        testInpOutp("Asdf.abc") {
            it.build(word, 0, 0, 8)
        }
    }

    @Test
    fun `Word correct capitalization 2`() {
        testInpOutp("asdf.abcd.zyui") {
            it.build(word, 0, 0, 14)
        }
    }

    @Test
    fun `Word correct capitalization 3`() {
        testInpOutp("Asdf.Abcd") {
            it.build(word, 1, 0, 9)
        }
    }

    @Test
    fun `Word incorrect capitalization`() {
        testInpOutp("asdf.Abcd") {
            it.bError(errorWordCapitalizationOrder)
        }
    }

    @Test
    fun `Word starts with underscore and lowercase letter`() {
        testInpOutp("_abc") {
            it.build(word, 0, 0, 4)
        }
    }

    @Test
    fun `Word starts with underscore and capital letter`() {
        testInpOutp("_Abc") {
            it.build(word, 1, 0, 4)
        }
    }

    @Test
    fun `Word starts with 2 underscores`() {
        testInpOutp("__abc") {
            it.bError(errorWordChunkStart)
        }
    }

    @Test
    fun `Word starts with underscore and digit`() {
        testInpOutp("_1abc") {
            it.bError(errorWordChunkStart)
        }
    }

    @Test
    fun `Dotword & @-word`() {
        testInpOutp("@a123 .Abc ") {
            it.build(atWord, 0, 0, 5)
              .build(dotWord, 1, 6, 4)
        }
    }

    @Test
    fun `Dot-reserved word error`() {
        testInpOutp(".false") {
            it.bError(errorWordReservedWithDot)
        }
    }

    @Test
    fun `At-reserved word`() {
        testInpOutp("@if") {
            it.build(atWord, 0, 0, 3)
        }
    }
}


@Nested
inner class LexNumericTest {

    @Test
    fun `Binary numeric 64-bit zero`() {
        testInpOutp("0b0") {
            it.build(intTok, 0, 0, 3)
        }
    }

    @Test
    fun `Binary numeric basic`() {
        testInpOutp("0b101") {
            it.build(intTok, 5, 0, 5)
        }
    }

    @Test
    fun `Binary numeric 64-bit positive`() {
        testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111") {
            it.build(intTok, 7, 0, 81)
        }
    }

    @Test
    fun `Binary numeric 64-bit negative`() {
        testInpOutp("0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1001") {
            it.buildLitInt(-7, 0, 81)
        }
    }

    @Test
    fun `Binary numeric 65-bit error`() {
        testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_01110") {
            it.bError(errorNumericBinWidthExceeded)
        }
    }

    @Test
    fun `Hex numeric 1`() {
        testInpOutp("0x15") {
            it.build(intTok, 21, 0, 4)
        }
    }

    @Test
    fun `Hex numeric 2`() {
        testInpOutp("0x05") {
            it.build(intTok, 5, 0, 4)
        }
    }

    @Test
    fun `Hex numeric 3`() {
        testInpOutp("0xFFFFFFFFFFFFFFFF") {
            it.buildLitInt(-1L, 0, 18)
        }
    }

    @Test
    fun `Hex numeric too long`() {
        testInpOutp("0xFFFFFFFFFFFFFFFF0") {
            it.bError(errorNumericBinWidthExceeded)
        }
    }

    @Test
    fun `Float numeric 1`() {
        testInpOutp("1.234") {
            it.buildLitFloat(1.234, 0, 5)
        }
    }

    @Test
    fun `Float numeric 2`() {
        testInpOutp("00001.234") {
            it.buildLitFloat(1.234, 0, 9)
        }
    }

    @Test
    fun `Float numeric 3`() {
        testInpOutp("10500.01") {
            it.buildLitFloat(10500.01, 0, 8)
        }
    }

    @Test
    fun `Float numeric 4`() {
        testInpOutp("0.9") {
            it.buildLitFloat(0.9, 0, 3)
        }
    }

    @Test
    fun `Float numeric 5`() {
        testInpOutp("100500.123456") {
            it.buildLitFloat(100500.123456, 0, 13)
        }
    }

    @Test
    fun `Float numeric big`() {
        testInpOutp("9007199254740992.0") {
            it.buildLitFloat(9007199254740992.0, 0, 18)
        }
    }

    @Test
    fun `Float numeric too big`() {
        testInpOutp("9007199254740993.0") {
            it.bError(errorNumericFloatWidthExceeded)
        }
    }


    @Test
    fun `Float numeric big exponent`() {
        testInpOutp("1005001234560000000000.0") {
            it.buildLitFloat(1005001234560000000000.0, 0, 24)
        }
    }

    @Test
    fun `Float numeric tiny`() {
        testInpOutp("0.0000000000000000000003") {
            it.buildLitFloat(0.0000000000000000000003, 0, 24)
        }
    }

    @Test
    fun `Float numeric negative 1`() {
        testInpOutp("-9.0") {
            it.buildLitFloat(-9.0, 0, 4)
        }
    }

    @Test
    fun `Float numeric negative 2`() {
        testInpOutp("-8.775_807") {
            it.buildLitFloat(-8.775_807, 0, 10)
        }
    }

    @Test
    fun `Float numeric negative 3`() {
        testInpOutp("-1005001234560000000000.0") {
            it.buildLitFloat(-1005001234560000000000.0, 0, 25)
        }
    }

    @Test
    fun `Int numeric 1`() {
        testInpOutp("3") {
            it.build(intTok, 3, 0, 1)
        }
    }

    @Test
    fun `Int numeric 2`() {
        testInpOutp("12") {
            it.build(intTok, 12, 0, 2)
        }
    }

    @Test
    fun `Int numeric 3`() {
        testInpOutp("0987_12") {
            it.build(intTok, 98712, 0, 7)
        }
    }

    @Test
    fun `Int numeric 4`() {
        testInpOutp("9_223_372_036_854_775_807") {
            it.buildLitInt(9_223_372_036_854_775_807L, 0, 25)
        }
    }

    @Test
    fun `Int numeric negative 1`() {
        testInpOutp("-1") {
            it.buildLitInt(-1L, 0, 2)
        }
    }

    @Test
    fun `Int numeric negative 2`() {
        testInpOutp("-775_807") {
            it.buildLitInt(-775_807L, 0, 8)
        }
    }

    @Test
    fun `Int numeric negative 3`() {
        testInpOutp("-9_223_372_036_854_775_807") {
            it.buildLitInt(-9_223_372_036_854_775_807L, 0, 26)
        }
    }

    @Test
    fun `Int numeric error 1`() {
        testInpOutp("3_") {
            it.bError(errorNumericEndUnderscore)
        }
    }

    @Test
    fun `Int numeric error 2`() {
        testInpOutp("9_223_372_036_854_775_808") {
            it.bError(errorNumericIntWidthExceeded)
        }
    }
}


@Nested
inner class LexStringTest {
    @Test
    fun `String simple literal`() {
        testInpOutp("\"asdf\"") {
            it.build(stringTok, 0, 1, 4)
        }
    }

    @Test
    fun `String literal with escaped quote inside`() {
        testInpOutp("\"wasn\\\"t\" so sure") {
            it.build(stringTok, 0, 1, 7)
              .build(word, 0, 10, 2)
              .build(word, 0, 13, 4)
        }
    }

    @Test
    fun `String literal with non-ASCII inside`() {
        testInpOutp("\"hello мир\"") {
            it.build(stringTok, 0, 1, 12) // 12 because each Cyrillic letter = 2 bytes
        }
    }

    @Test
    fun `String literal unclosed`() {
        testInpOutp("\"asdf") {
            it.bError(errorPrematureEndOfInput)
        }
    }

//    @Test
    // TODO bring this test back when verbatim strings are implemented
//    fun `Verbatim string literal`() {
//        testInpOutp(
//            "\"asdf foo\"\"\"",
//            it.buildPunctuation(0, 12, statementFun, 1)
//                .build(verbatimString, 0, 1, 8)
//        )
//    }
}


@Nested
inner class LexCommentTest {
    @Test
    fun `Comment simple`() {
        testInpOutp("; this is a comment") {
            it.buildComment(1, 18)
        }
    }

    @Test
    fun `Comment inline`() {
        testInpOutp("; this is a comment.; and ;this too.; but not this") {
            it.buildComment(1, 18)
              .build(word, 0, 22, 3)
              .buildComment(27, 8)
              .build(word, 0, 38, 3)
              .build(word, 0, 42, 3)
              .build(word, 0, 46, 4)
        }
    }

    @Test
    fun `Doc comment`() {
        testInpOutp("(;; Documentation comment .)") {
            it.build(docCommentTok, 0, 3, 23)
        }
    }

    @Test
    fun `Doc comment before something`() {
        testInpOutp("""(;; Documentation comment .)
("hw" .print) """) {
            it.build(docCommentTok, 0, 3, 23)
                .buildPunctuation(30, 11, parens, 2)
                .build(stringTok, 0, 31, 2)
                .build(dotWord, 0, 35, 6)
        }
    }

    @Test
    fun `Doc comment error`() {
        testInpOutp("( ;; Documentation comment .)") {
            it.buildPunctuation(1, 0, parens, 0).bError(errorDocComment)
        }
    }
}


@Nested
inner class LexPunctuationTest {
    @Test
    fun `Parens simple`() {
        testInpOutp("(car cdr)") {
            it.buildPunctuation(1, 7, parens, 2)
              .build(word, 0, 1, 3)
              .build(word, 0, 5, 3)
        }
    }


    @Test
    fun `Parens nested`() {
        testInpOutp("(car (other car) cdr)") {
            it.buildPunctuation(1, 19, parens, 5)
              .build(word, 0, 1, 3)
              .buildPunctuation(6, 9, parens, 2)
              .build(word, 0, 6, 5)
              .build(word, 0, 12, 3)
              .build(word, 0, 17, 3)
        }
    }


    @Test
    fun `Parens unclosed`() {
        testInpOutp("(car (other car) cdr") {
            it.buildPunctuation(1, 0, parens, 0)
              .build(word, 0, 1, 3)
              .buildPunctuation(6, 9, parens, 2)
              .build(word, 0, 6, 5)
              .build(word, 0, 12, 3)
              .build(word, 0, 17, 3)
              .bError(errorPunctuationExtraOpening)
        }
    }


    @Test
    fun `Brackets simple`() {
        testInpOutp("[car cdr]") {
            it.buildPunctuation(1, 7, brackets, 2)
              .build(word, 0, 1, 3)
              .build(word, 0, 5, 3)
        }
    }


    @Test
    fun `Brackets nested`() {
        testInpOutp("[car [other car] cdr]") {
            it.buildPunctuation(1, 19, brackets, 5)
              .build(word, 0, 1, 3)
              .buildPunctuation(6, 9, brackets, 2)
              .build(word, 0, 6, 5)
              .build(word, 0, 12, 3)
              .build(word, 0, 17, 3)
        }
    }

    @Test
    fun `Brackets mismatched`() {
        testInpOutp("(asdf QWERT]") {
            it.buildPunctuation(1, 0, parens, 0)
              .build(word, 0, 1, 4)
              .build(word, 1, 6, 5)
              .bError(errorPunctuationUnmatched)
        }
    }

    @Test
    fun `Curly brace with statements`() {
        testInpOutp("""{
asdf

bcjk
}""") {
            it.buildPunctuation(1, 12, curlyBraces, 2)
              .build(word, 0, 2, 4)
              .build(word, 0, 8, 4)
        }
    }

    @Test
    fun `Punctuation scope inside statement 1`() {
        testInpOutp("""foo bar { asdf }""") {
            it.build(word, 0, 0, 3)
              .build(word, 0, 4, 3)
              .buildPunctuation(9, 6, curlyBraces, 1)
              .build(word, 0, 10, 4)
        }
    }

    @Test
    fun `Punctuation scope inside statement 2`() {
        testInpOutp(
            """foo bar { 
asdf
bcj
}"""
        ) {
            it.build(word, 0, 0, 3)
              .build(word, 0, 4, 3)
              .buildPunctuation(9, 11, curlyBraces, 2)
              .build(word, 0, 11, 4)
              .build(word, 0, 16, 3)
        }
    }

    @Test
    fun `Punctuation all types`() {
        testInpOutp(
            """{
asdf (b [d ef (y z)] c d[x y])

bcjk ({a b})
}""") {
            it.buildPunctuation(1, 46, curlyBraces, 19)
              .build(word, 0, 2, 4)
              .buildPunctuation(8, 23, parens, 12)
              .build(word, 0, 8, 1)
              .buildPunctuation(11, 10, brackets, 5)
              .build(word, 0, 11, 1)
              .build(word, 0, 13, 2)
              .buildPunctuation(17, 3, parens, 2)
              .build(word, 0, 17, 1)
              .build(word, 0, 19, 1)
              .build(word, 0, 23, 1)
              .build(word, 0, 25, 1)
              .buildPunctuation(27, 3, brackets, 2)
              .build(word, 0, 27, 1)
              .build(word, 0, 29, 1)
              .build(word, 0, 34, 4)
              .buildPunctuation(40, 5, parens, 3)
              .buildPunctuation(41, 3, curlyBraces, 2)
              .build(word, 0, 41, 1)
              .build(word, 0, 43, 1)
        }
    }
}

@Nested
inner class LexOperatorTest {
    @Test
    fun `Operator simple 1`() {
        testInpOutp("+") {
            it.build(operatorTok, OperatorToken(OperatorType.plus, 0, false).toInt(), 0, 1)
        }
    }

    @Test
    fun `Operator extensible 1`() {
        testInpOutp("+.= +:= +. +:") {
            it.build(operatorTok, OperatorToken(OperatorType.plus, 1, true).toInt(), 0, 3)
              .build(operatorTok, OperatorToken(OperatorType.plus, 2, true).toInt(), 4, 3)
              .build(operatorTok, OperatorToken(OperatorType.plus, 1, false).toInt(), 8, 2)
              .build(operatorTok, OperatorToken(OperatorType.plus, 2, false).toInt(), 11, 2)
        }
    }

    @Test
    fun `Operator extensible 2`() {
        testInpOutp("||:= &&. /.=") {
            it.build(operatorTok, OperatorToken(OperatorType.boolOr, 2, true).toInt(), 0, 4)
              .build(operatorTok, OperatorToken(OperatorType.boolAnd, 1, false).toInt(), 5, 3)
              .build(operatorTok, OperatorToken(OperatorType.divBy, 1, true).toInt(), 9, 3)
        }
    }

    @Test
    fun `Operators list`() {
        testInpOutp("+ - / * ** && || ^ <- ? -> >=< >< :") {
            it.build(operatorTok, OperatorToken(OperatorType.plus, 0, false).toInt(), 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.minus, 0, false).toInt(), 2, 1)
              .build(operatorTok, OperatorToken(OperatorType.divBy, 0, false).toInt(), 4, 1)
              .build(operatorTok, OperatorToken(OperatorType.times, 0, false).toInt(), 6, 1)
              .build(operatorTok, OperatorToken(OperatorType.exponentiation, 0, false).toInt(), 8, 2)
              .build(operatorTok, OperatorToken(OperatorType.boolAnd, 0, false).toInt(), 11, 2)
              .build(operatorTok, OperatorToken(OperatorType.boolOr, 0, false).toInt(), 14, 2)
              .build(operatorTok, OperatorToken(OperatorType.dereference, 0, false).toInt(), 17, 1)
              .build(operatorTok, OperatorToken(OperatorType.arrowLeft, 0, false).toInt(), 19, 2)
              .build(operatorTok, OperatorToken(OperatorType.questionMark, 0, false).toInt(), 22, 1)
              .build(operatorTok, OperatorToken(OperatorType.arrowRight, 0, false).toInt(), 24, 2)
              .build(operatorTok, OperatorToken(OperatorType.intervalLeftInclusive, 0, false).toInt(), 27, 3)
              .build(operatorTok, OperatorToken(OperatorType.intervalNonInclusive, 0, false).toInt(), 31, 2)
              .build(operatorTok, OperatorToken(OperatorType.elseBranch, 0, false).toInt(), 34, 1)
        }
    }

    @Test
    fun `Operator expression`() {
        testInpOutp("a - b") {
            it.build(word, 0, 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.minus, 0, false).toInt(), 2, 1)
              .build(word, 0, 4, 1)
        }
    }

    @Test
    fun `Operator assignment 1`() {
        testInpOutp("a += b") {
            it.build(word, 0, 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, true).toInt(), 2, 2)
              .build(word, 0, 5, 1)
        }
    }


    @Test
    fun `Operator assignment 2`() {
        testInpOutp("a ||= b") {
            it.build(word, 0, 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.boolOr, 0, true).toInt(), 2, 3)
              .build(word, 0, 6, 1)
        }
    }

    @Test
    fun `Operator assignment 3`() {
        testInpOutp("a ||.= b") {
            it.build(word, 0, 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.boolOr, 1, true).toInt(), 2, 4)
              .build(word, 0, 7, 1)
        }
    }

    @Test
    fun `Operator assignment 4`() {
        testInpOutp("a ||:= b") {
            it.build(word, 0, 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.boolOr, 2, true).toInt(), 2, 4)
              .build(word, 0, 7, 1)
        }
    }

    @Test
    fun `Operator assignment in parens 1`() {
        testInpOutp("x += y + 5") {
            it.build(word, 0, 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, true).toInt(), 2, 2)
              .build(word, 0, 5, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, false).toInt(), 7, 1)
              .build(intTok, 5, 9, 1)
        }
    }

    @Test
    fun `Operator assignment in parens 2`() {
        testInpOutp("(x += y + 5)") {
            it.buildPunctPayload(stmtAssignment, (1 shl 16) + OperatorToken(OperatorType.plus, 0, true).toInt(), 4, 1, 10)
              .build(word, 0, 1, 1)
              .build(word, 0, 6, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, false).toInt(), 8, 1)
              .build(intTok, 5, 10, 1)
        }
    }

    @Test
    fun `Operator assignment in parens 3`() {
        testInpOutp("((x += y + 5))") {
            it.buildPunctuation(1, 12, lexScope, 5)
              .buildPunctPayload(stmtAssignment, (1 shl 16) + OperatorToken(OperatorType.plus, 0, true).toInt(), 4, 2, 10)
              .build(word, 0, 2, 1)
              .build(word, 0, 7, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, false).toInt(), 9, 1)
              .build(intTok, 5, 11, 1)
        }
    }

    @Test
    fun `Operator assignment in parens 4`() {
        testInpOutp("x += (y + 5)") {
            it.build(word, 0, 0, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, true).toInt(), 2, 2)
              .buildPunctuation(6, 5, parens, 3)
              .build(word, 0, 6, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, false).toInt(), 8, 1)
              .build(intTok, 5, 10, 1)
        }
    }

    @Test
    fun `Operator assignment in parens error 1`() {
        testInpOutp("x (+= y) + 5") {
            it.build(word, 0, 0, 1)
              .buildPunctuation(3, 0, parens, 0)
              .bError(errorOperatorAssignmentPunct)
        }
    }

    @Test
    fun `Operator assignment in parens error but only parser will tell`() {
        testInpOutp("((x += y) + 5)") {
            it.buildPunctuation(1, 12, lexScope, 5)
              .buildPunctPayload(stmtAssignment, (1 shl 16) + OperatorToken(OperatorType.plus, 0, true).toInt(), 2, 2, 6)
              .build(word, 0, 2, 1)
              .build(word, 0, 7, 1)
              .build(operatorTok, OperatorToken(OperatorType.plus, 0, false).toInt(), 10, 1)
              .build(intTok, 5, 12, 1)
        }
    }

    @Test
    fun `Operator assignment multiple error 1`() {
        testInpOutp("(x := y := 7)") {
            it.buildPunctPayload(stmtAssignment, (1 shl 16) + OperatorToken(OperatorType.mutation, 0, false).toInt(), 0, 1, 0)
              .build(word, 0, 1, 1)
              .build(word, 0, 6, 1)
              .bError(errorOperatorMultipleAssignment)
        }
    }
}


    private fun testInpOutp(inp: String, transform: (Lexer) -> Unit) {
        val testSpecimen = Lexer(inp.toByteArray(), FileType.executable)
        val control = Lexer(inp.toByteArray(), FileType.executable)
        testSpecimen.lexicallyAnalyze()
        
        transform(control)
        val doEqual = Lexer.equality(testSpecimen, control)
        if (!doEqual) {
            val printOut = Lexer.printSideBySide(testSpecimen, control)
            println(printOut)
        }
        assertEquals(doEqual, true)
    }
}