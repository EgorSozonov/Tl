import language.*
import lexer.*
import lexer.Lexer
import org.junit.jupiter.api.Nested
import kotlin.test.Test
import kotlin.test.assertEquals

class LexerTest {


@Nested
inner class LexWordTest {
    @Test
    fun `Simple word lexing`() {
        testInpOutp("asdf Abc") {
            it.build(tokWord, 0, 0, 4)
              .build(tokWord, 1, 5, 3)
        }
    }

    @Test
    fun `Word snake case forbidden`() {
        testInpOutp("asdf_abc") {
            it.bError(errorWordUnderscoresOnlyAtStart)
        }
    }

    @Test
    fun `Word correct capitalization 1`() {
        testInpOutp("asdf.abc") {
            it.build(tokWord, 0, 0, 8)
        }
    }

    @Test
    fun `Word correct capitalization 2`() {
        testInpOutp("asdf.abcd.zyui") {
            it.build(tokWord, 0, 0, 14)
        }
    }

    @Test
    fun `Word correct capitalization 3`() {
        testInpOutp("asdf.Abcd") {
            it.build(tokWord, 1, 0, 9)
        }
    }

    @Test
    fun `Word incorrect capitalization`() {
        testInpOutp("Asdf.Abcd") {
            it.bError(errorWordCapitalizationOrder)
        }
    }

    @Test
    fun `Word starts with underscore and lowercase letter`() {
        testInpOutp("_abc") {
            it.build(tokWord, 0, 0, 4)
        }
    }

    @Test
    fun `Word starts with underscore and capital letter`() {
        testInpOutp("_Abc") {
            it.build(tokWord, 1, 0, 4)
        }
    }

    @Test
    fun `Word starts with 2 underscores error`() {
        testInpOutp("__abc") {
            it.bError(errorWordChunkStart)
        }
    }

    @Test
    fun `Word starts with underscore and digit error`() {
        testInpOutp("_1abc") {
            it.bError(errorWordChunkStart)
        }
    }

    @Test
    fun `@-Word`() {
        testInpOutp("@a123 ") {
            it.build(tokAtWord, 0, 0, 5)
        }
    }

    @Test
    fun `At-reserved word`() {
        testInpOutp("@if") {
            it.build(tokAtWord, 0, 0, 3)
        }
    }
}


@Nested
inner class LexNumericTest {

    @Test
    fun `Binary numeric 64-bit zero`() {
        testInpOutp("0b0") {
            it.build(tokInt, 0, 0, 3)
        }
    }

    @Test
    fun `Binary numeric basic`() {
        testInpOutp("0b101") {
            it.build(tokInt, 5, 0, 5)
        }
    }

    @Test
    fun `Binary numeric 64-bit positive`() {
        testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111") {
            it.build(tokInt, 7, 0, 81)
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
            it.build(tokInt, 21, 0, 4)
        }
    }

    @Test
    fun `Hex numeric 2`() {
        testInpOutp("0x05") {
            it.build(tokInt, 5, 0, 4)
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
            it.build(tokInt, 3, 0, 1)
        }
    }

    @Test
    fun `Int numeric 2`() {
        testInpOutp("12") {
            it.build(tokInt, 12, 0, 2)
        }
    }

    @Test
    fun `Int numeric 3`() {
        testInpOutp("0987_12") {
            it.build(tokInt, 98712, 0, 7)
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
        testInpOutp("'asdf'") {
            it.build(tokString, 0, 1, 4)
        }
    }

    @Test
    fun `String literal with escaped apostrophe inside`() {
        testInpOutp("'wasn''t so sure'") {
            it.build(tokString, 0, 1, 15)
        }
    }

    @Test
    fun `String literal with non-ASCII inside`() {
        testInpOutp("'hello мир'") {
            it.build(tokString, 0, 1, 12) // 12 because each Cyrillic letter = 2 bytes
        }
    }

    @Test
    fun `String literal unclosed`() {
        testInpOutp("'asdf") {
            it.bError(errorPrematureEndOfInput)
        }
    }
}


@Nested
inner class LexCommentTest {
    @Test
    fun `Comment simple`() {
        testInpOutp("; this is a comment") {
            
        }
    }


    @Test
    fun `Doc comment`() {
        testInpOutp(";; Documentation comment ") {
            it.build(tokDocComment, 0, 2, 23)
        }
    }

    @Test
    fun `Doc comment before something`() {
        testInpOutp(""";; Documentation comment
print 'hw' """) {
            it.build(tokDocComment, 0, 2, 22)
              .build(tokWord, 0, 25, 5)
              .build(tokString, 0, 32, 2)
        }
    }

    @Test
    fun `Doc comment empty`() {
        testInpOutp(""";;
print 'hw' """) {
            it.build(tokWord, 0, 3, 5)
              .build(tokString, 0, 10, 2)
        }
    }
}


@Nested
inner class LexPunctuationTest {
    @Test
    fun `Parens simple`() {
        testInpOutp("(car cdr)") {
            it.buildPunctuation(tokParens, 2, 1, 7)
              .build(tokWord, 0, 1, 3)
              .build(tokWord, 0, 5, 3)
        }
    }


    @Test
    fun `Parens nested`() {
        testInpOutp("(car (other car) cdr)") {
            it.buildPunctuation(tokParens, 5, 1, 19)
              .build(tokWord, 0, 1, 3)
              .buildPunctuation(tokParens, 2, 6, 9)
              .build(tokWord, 0, 6, 5)
              .build(tokWord, 0, 12, 3)
              .build(tokWord, 0, 17, 3)
        }
    }


    @Test
    fun `Parens unclosed`() {
        testInpOutp("(car (other car) cdr") {
            it.buildPunctuation(tokParens, 0, 1, 0)
              .build(tokWord, 0, 1, 3)
              .buildPunctuation(tokParens, 2, 6, 9)
              .build(tokWord, 0, 6, 5)
              .build(tokWord, 0, 12, 3)
              .build(tokWord, 0, 17, 3)
              .bError(errorPunctuationExtraOpening)
        }
    }


    @Test
    fun `Brackets simple`() {
        testInpOutp("[car cdr]") {
            it.buildPunctuation(tokBrackets, 2, 1, 7)
              .build(tokWord, 0, 1, 3)
              .build(tokWord, 0, 5, 3)
        }
    }


    @Test
    fun `Brackets nested`() {
        testInpOutp("[car [other car] cdr]") {
            it.buildPunctuation(tokBrackets, 5, 1, 19)
              .build(tokWord, 0, 1, 3)
              .buildPunctuation(tokBrackets, 2, 6, 9)
              .build(tokWord, 0, 6, 5)
              .build(tokWord, 0, 12, 3)
              .build(tokWord, 0, 17, 3)
        }
    }

    @Test
    fun `Brackets mismatched`() {
        testInpOutp("(asdf QWERT]") {
            it.buildPunctuation(tokParens, 0, 1, 0)
              .build(tokWord, 0, 1, 4)
              .build(tokWord, 1, 6, 5)
              .bError(errorPunctuationUnmatched)
        }
    }

    @Test
    fun `Data accessor`() {
        testInpOutp("asdf[5]") {
            it.build(tokWord, 0, 0, 4)
              .buildPunctuation(tokAccessor, 1, 5, 1)
              .buildLitInt(5, 5, 1)
        }
    }

    @Test
    fun `Punctuation scope inside statement 1`() {
        testInpOutp("""foo bar { asdf }""") {
            it.build(tokWord, 0, 0, 3)
              .build(tokWord, 0, 4, 3)
              .buildPunctuation(tokCurlyBraces, 1, 9, 6)
              .build(tokWord, 0, 10, 4)
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
            it.build(tokWord, 0, 0, 3)
              .build(tokWord, 0, 4, 3)
              .buildPunctuation(tokCurlyBraces, 2, 9, 11)
              .build(tokWord, 0, 11, 4)
              .build(tokWord, 0, 16, 3)
        }
    }

    @Test
    fun `Punctuation all types`() {
        testInpOutp(
            """{
asdf (b [d ef (y z)] c d[x y])

bcjk ({a b})
}""") {
            it.buildPunctuation(tokCurlyBraces, 19, 1, 46)
              .build(tokWord, 0, 2, 4)
              .buildPunctuation(tokParens, 12, 8, 23)
              .build(tokWord, 0, 8, 1)
              .buildPunctuation(tokBrackets, 5, 11, 10)
              .build(tokWord, 0, 11, 1)
              .build(tokWord, 0, 13, 2)
              .buildPunctuation(tokParens, 2, 17, 3)
              .build(tokWord, 0, 17, 1)
              .build(tokWord, 0, 19, 1)
              .build(tokWord, 0, 23, 1)
              .build(tokWord, 0, 25, 1)
              .buildPunctuation(tokBrackets, 2, 27, 3)
              .build(tokWord, 0, 27, 1)
              .build(tokWord, 0, 29, 1)
              .build(tokWord, 0, 34, 4)
              .buildPunctuation(tokParens, 3, 40, 5)
              .buildPunctuation(tokCurlyBraces, 2, 41, 3)
              .build(tokWord, 0, 41, 1)
              .build(tokWord, 0, 43, 1)
        }
    }
}

@Nested
inner class LexOperatorTest {
    @Test
    fun `Operator simple 1`() {
        testInpOutp("+") {
            it.buildOperator(opTPlus, false, false, 0, 1)
        }
    }

    @Test
    fun `Operator extensible 1`() {
        testInpOutp("+.= +. +") {
            it.buildOperator(opTPlus, true, true, 0, 3)
              .buildOperator(opTPlus, true, false, 4, 2)
              .buildOperator(opTPlus, false, false, 7, 1)
        }
    }

    @Test
    fun `Operator extensible 2`() {
        testInpOutp("||.= &&. /.=") {
            it.buildOperator(opTBoolOr, true, true, 0, 4)
              .buildOperator(opTBoolAnd, true, false, 5, 3)
              .buildOperator(opTDivBy, true, true, 9, 3)
        }
    }

    @Test
    fun `Operators list`() {
        testInpOutp("+ - / * ^ && || <- ? => >=< ><") {
            it.buildOperator(opTPlus, false, false, 0, 1)
              .buildOperator(opTMinus, false, false, 2, 1)
              .buildOperator(opTDivBy, false, false, 4, 1)
              .buildOperator(opTTimes, false, false, 6, 1)
              .buildOperator(opTExponentiation, false, false, 8, 2)
              .buildOperator(opTBoolAnd, false, false, 11, 2)
              .buildOperator(opTBoolOr, false, false, 14, 2)
              .buildOperator(opTArrowLeft, false, false, 17, 1)
              .buildOperator(opTQuestionMark, false, false, 19, 2)
              .buildOperator(opTQuestionMark, false, false, 22, 1)
              .buildOperator(opTArrowRight, false, false, 24, 2)
              .buildOperator(opTIntervalLeftInclusive, false, false, 27, 3)
              .buildOperator(opTIntervalExclusive, false, false, 31, 2)
        }
    }

    @Test
    fun `Operator expression`() {
        testInpOutp("a - b") {
            it.build(tokWord, 0, 0, 1)
              .buildOperator(opTMinus, false, false, 2, 1)
              .build(tokWord, 0, 4, 1)
        }
    }

    @Test
    fun `Operator assignment 1`() {
        testInpOutp("a += b") {
            it.build(tokWord, 0, 0, 1)
              .buildOperator(opTPlus, false, true, 2, 2)
              .build(tokWord, 0, 5, 1)
        }
    }


    @Test
    fun `Operator assignment 2`() {
        testInpOutp("a ||= b") {
            it.build(tokWord, 0, 0, 1)
              .buildOperator(opTBoolOr, false, true, 2, 3)
              .build(tokWord, 0, 6, 1)
        }
    }

    @Test
    fun `Operator assignment 3`() {
        testInpOutp("a ||.= b") {
            it.build(tokWord, 0, 0, 1)
              .buildOperator(opTBoolOr, false, true, 2, 4)
              .build(tokWord, 0, 7, 1)
        }
    }

    @Test
    fun `Operator assignment 4`() {
        testInpOutp("a ||.= b") {
            it.build(tokWord, 0, 0, 1)
              .buildOperator(opTBoolOr, true, true, 2, 4)
              .build(tokWord, 0, 7, 1)
        }
    }


    @Test
    fun `Operator assignment in parens error`() {
        testInpOutp("(x += y + 5)") {
            it.bError(errorOperatorAssignmentPunct)
        }
    }

    @Test
    fun `Operator assignment with parens`() {
        testInpOutp("x +.= (y + 5)") {
            it.build(tokWord, 0, 0, 1)
              .buildOperator(opTPlus, true, true, 2, 3)
              .buildPunctuation(tokParens, 3, 6, 7)
              .build(tokWord, 0, 7, 1)
              .buildOperator(opTPlus, false, false, 9, 1)
              .build(tokInt, 5, 11, 1)
        }
    }

    @Test
    fun `Operator assignment in tokParens error 1`() {
        testInpOutp("x (+= y) + 5") {
            it.build(tokWord, 0, 0, 1)
              .buildPunctuation(tokParens, 0, 3, 0)
              .bError(errorOperatorAssignmentPunct)
        }
    }

    @Test
    fun `Operator assignment multiple error 1`() {
        testInpOutp("x := y := 7") {
            it.buildPunctPayload(tokStmtAssignment, (1 shl 16) + (opTMutation shl 2), 0, 1, 0)
              .build(tokWord, 0, 1, 1)
              .build(tokWord, 0, 6, 1)
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