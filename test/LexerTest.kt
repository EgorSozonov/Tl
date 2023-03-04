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
            it.build(tokStmt, 2, 0, 8)
              .build(tokWord, 0, 0, 4)
              .build(tokWord, 1, 5, 3)
        }
    }

    @Test
    fun `Word snake case forbidden`() {
        testInpOutp("asdf_abc") {
            it.build(tokStmt, 0, 0, 0).bError(errorWordUnderscoresOnlyAtStart)
        }
    }

    @Test
    fun `Word correct capitalization 1`() {
        testInpOutp("asdf.abc") {
            it.build(tokStmt, 1, 0, 8)
              .build(tokWord, 0, 0, 8)
        }
    }

    @Test
    fun `Word correct capitalization 2`() {
        testInpOutp("asdf.abcd.zyui") {
            it.build(tokStmt, 1, 0, 14)
              .build(tokWord, 0, 0, 14)
        }
    }

    @Test
    fun `Word correct capitalization 3`() {
        testInpOutp("asdf.Abcd") {
            it.build(tokStmt, 1, 0, 9)
              .build(tokWord, 1, 0, 9)
        }
    }

    @Test
    fun `Word incorrect capitalization`() {
        testInpOutp("Asdf.Abcd") {
            it.build(tokStmt, 0, 0, 0).bError(errorWordCapitalizationOrder)
        }
    }

    @Test
    fun `Word starts with underscore and lowercase letter`() {
        testInpOutp("_abc") {
            it.build(tokStmt, 1, 0, 4)
              .build(tokWord, 0, 0, 4)
        }
    }

    @Test
    fun `Word starts with underscore and capital letter`() {
        testInpOutp("_Abc") {
            it.build(tokStmt, 1, 0, 4)
              .build(tokWord, 1, 0, 4)
        }
    }

    @Test
    fun `Word starts with 2 underscores error`() {
        testInpOutp("__abc") {
            it.build(tokStmt, 0, 0, 0).bError(errorWordChunkStart)
        }
    }

    @Test
    fun `Word starts with underscore and digit error`() {
        testInpOutp("_1abc") {
            it.build(tokStmt, 0, 0, 0).bError(errorWordChunkStart)
        }
    }

    @Test
    fun `@-Word`() {
        testInpOutp("@a123 ") {
            it.build(tokStmt, 1, 0, 6).build(tokAtWord, 0, 0, 5)
        }
    }

    @Test
    fun `At-reserved word`() {
        testInpOutp("@if") {
            it.build(tokStmt, 1, 0, 3).build(tokAtWord, 0, 0, 3)
        }
    }
}


@Nested
inner class LexNumericTest {

    @Test
    fun `Binary numeric 64-bit zero`() {
        testInpOutp("0b0") {
            it.build(tokStmt, 1, 0, 3).build(tokInt, 0, 0, 3)
        }
    }

    @Test
    fun `Binary numeric basic`() {
        testInpOutp("0b101") {
            it.build(tokStmt, 1, 0, 5).build(tokInt, 5, 0, 5)
        }
    }

    @Test
    fun `Binary numeric 64-bit positive`() {
        testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111") {
            it.build(tokStmt, 1, 0, 81).build(tokInt, 7, 0, 81)
        }
    }

    @Test
    fun `Binary numeric 64-bit negative`() {
        testInpOutp("0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1001") {
            it.build(tokStmt, 1, 0, 81).buildLitInt(-7, 0, 81)
        }
    }

    @Test
    fun `Binary numeric 65-bit error`() {
        testInpOutp("0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_01110") {
            it.build(tokStmt, 0, 0, 0).bError(errorNumericBinWidthExceeded)
        }
    }

    @Test
    fun `Hex numeric 1`() {
        testInpOutp("0x15") {
            it.build(tokStmt, 1, 0, 4).build(tokInt, 21, 0, 4)
        }
    }

    @Test
    fun `Hex numeric 2`() {
        testInpOutp("0x05") {
            it.build(tokStmt, 1, 0, 4).build(tokInt, 5, 0, 4)
        }
    }

    @Test
    fun `Hex numeric 3`() {
        testInpOutp("0xFFFFFFFFFFFFFFFF") {
            it.build(tokStmt, 1, 0, 18).buildLitInt(-1L, 0, 18)
        }
    }

    @Test
    fun `Hex numeric 4`() {
        testInpOutp("0xFFFFFFFFFFFFFFFE") {
            it.build(tokStmt, 1, 0, 18).buildLitInt(-2L, 0, 18)
        }
    }

    @Test
    fun `Hex numeric too long`() {
        testInpOutp("0xFFFFFFFFFFFFFFFF0") {
            it.build(tokStmt, 0, 0, 0).bError(errorNumericBinWidthExceeded)
        }
    }

    @Test
    fun `Float numeric 1`() {
        testInpOutp("1.234") {
            it.build(tokStmt, 1, 0, 5).buildLitFloat(1.234, 0, 5)
        }
    }

    @Test
    fun `Float numeric 2`() {
        testInpOutp("00001.234") {
            it.build(tokStmt, 1, 0, 9).buildLitFloat(1.234, 0, 9)
        }
    }

    @Test
    fun `Float numeric 3`() {
        testInpOutp("10500.01") {
            it.build(tokStmt, 1, 0, 8).buildLitFloat(10500.01, 0, 8)
        }
    }

    @Test
    fun `Float numeric 4`() {
        testInpOutp("0.9") {
            it.build(tokStmt, 1, 0, 3).buildLitFloat(0.9, 0, 3)
        }
    }

    @Test
    fun `Float numeric 5`() {
        testInpOutp("100500.123456") {
            it.build(tokStmt, 1, 0, 13).buildLitFloat(100500.123456, 0, 13)
        }
    }

    @Test
    fun `Float numeric big`() {
        testInpOutp("9007199254740992.0") {
            it.build(tokStmt, 1, 0, 18).buildLitFloat(9007199254740992.0, 0, 18)
        }
    }

    @Test
    fun `Float numeric too big`() {
        testInpOutp("9007199254740993.0") {
            it.build(tokStmt, 0, 0, 0).bError(errorNumericFloatWidthExceeded)
        }
    }


    @Test
    fun `Float numeric big exponent`() {
        testInpOutp("1005001234560000000000.0") {
            it.build(tokStmt, 1, 0, 24).buildLitFloat(1005001234560000000000.0, 0, 24)
        }
    }

    @Test
    fun `Float numeric tiny`() {
        testInpOutp("0.0000000000000000000003") {
            it.build(tokStmt, 1, 0, 24).buildLitFloat(0.0000000000000000000003, 0, 24)
        }
    }

    @Test
    fun `Float numeric negative 1`() {
        testInpOutp("-9.0") {
            it.build(tokStmt, 1, 0, 4).buildLitFloat(-9.0, 0, 4)
        }
    }

    @Test
    fun `Float numeric negative 2`() {
        testInpOutp("-8.775_807") {
            it.build(tokStmt, 1, 0, 10).buildLitFloat(-8.775_807, 0, 10)
        }
    }

    @Test
    fun `Float numeric negative 3`() {
        testInpOutp("-1005001234560000000000.0") {
            it.build(tokStmt, 1, 0, 25).buildLitFloat(-1005001234560000000000.0, 0, 25)
        }
    }

    @Test
    fun `Int numeric 1`() {
        testInpOutp("3") {
            it.build(tokStmt, 1, 0, 1).build(tokInt, 3, 0, 1)
        }
    }

    @Test
    fun `Int numeric 2`() {
        testInpOutp("12") {
            it.build(tokStmt, 1, 0, 2).build(tokInt, 12, 0, 2)
        }
    }

    @Test
    fun `Int numeric 3`() {
        testInpOutp("0987_12") {
            it.build(tokStmt, 1, 0, 7).build(tokInt, 98712, 0, 7)
        }
    }

    @Test
    fun `Int numeric 4`() {
        testInpOutp("9_223_372_036_854_775_807") {
            it.build(tokStmt, 1, 0, 25).buildLitInt(9_223_372_036_854_775_807L, 0, 25)
        }
    }

    @Test
    fun `Int numeric negative 1`() {
        testInpOutp("-1") {
            it.build(tokStmt, 1, 0, 2).buildLitInt(-1L, 0, 2)
        }
    }

    @Test
    fun `Int numeric negative 2`() {
        testInpOutp("-775_807") {
            it.build(tokStmt, 1, 0, 8).buildLitInt(-775_807L, 0, 8)
        }
    }

    @Test
    fun `Int numeric negative 3`() {
        testInpOutp("-9_223_372_036_854_775_807") {
            it.build(tokStmt, 1, 0, 26).buildLitInt(-9_223_372_036_854_775_807L, 0, 26)
        }
    }

    @Test
    fun `Int numeric error 1`() {
        testInpOutp("3_") {
            it.build(tokStmt, 0, 0, 0).bError(errorNumericEndUnderscore)
        }
    }

    @Test
    fun `Int numeric error 2`() {
        testInpOutp("9_223_372_036_854_775_808") {
            it.build(tokStmt, 0, 0, 0).bError(errorNumericIntWidthExceeded)
        }
    }
}


@Nested
inner class LexStringTest {
    @Test
    fun `String simple literal`() {
        testInpOutp("\"asdf\"") {
            it.build(tokStmt, 1, 0, 6).build(tokString, 0, 1, 4)
        }
    }

    @Test
    fun `String literal with escaped apostrophe inside`() {
        testInpOutp("\"wasn't so sure\"") {
            it.build(tokStmt, 1, 0, 16).build(tokString, 0, 1, 14)
        }
    }

    @Test
    fun `String literal with non-ASCII inside`() {
        testInpOutp("\"hello мир\"") {
            it.build(tokStmt, 1, 0, 14)
              .build(tokString, 0, 1, 12) // 12 because each Cyrillic letter = 2 bytes
        }
    }

    @Test
    fun `String literal unclosed`() {
        testInpOutp("\"asdf") {
            it.build(tokStmt, 0, 0, 0).bError(errorPrematureEndOfInput)
        }
    }
}


@Nested
inner class LexCommentTest {
    @Test
    fun `Comment simple`() {
        testInpOutp("# this is a comment") {

        }
    }


    @Test
    fun `Doc comment`() {
        testInpOutp("## Documentation comment ") {
            it.build(tokDocComment, 0, 2, 23)
        }
    }

    @Test
    fun `Doc comment before something`() {
        testInpOutp("""## Documentation comment
print "hw" """) {
            it.build(tokDocComment, 0, 2, 22)
              .build(tokStmt, 2, 25, 11)
              .build(tokWord, 0, 25, 5)
              .build(tokString, 0, 32, 2)
        }
    }

    @Test
    fun `Doc comment empty`() {
        testInpOutp("""##
print "hw" """) {
            it.build(tokStmt, 2, 3, 11)
              .build(tokWord, 0, 3, 5)
              .build(tokString, 0, 10, 2)
        }
    }

    @Test
    fun `Doc comment multiline`() {
        testInpOutp("""## First line
## Second line
## Third line
print "hw" """) {
            it.build(tokDocComment, 0, 2, 40)
              .build(tokStmt, 2, 43, 11)
              .build(tokWord, 0, 43, 5)
              .build(tokString, 0, 50, 2)
        }
    }
}


@Nested
inner class LexPunctuationTest {
    @Test
    fun `Parens simple`() {
        testInpOutp("(car cdr)") {
            it.build(tokStmt, 3, 0, 9)
              .buildPunctuation(tokParens, 2, 1, 7)
              .build(tokWord, 0, 1, 3)
              .build(tokWord, 0, 5, 3)
        }
    }


    @Test
    fun `Parens nested`() {
        testInpOutp("(car (other car) cdr)") {
            it.build(tokStmt, 6, 0, 21)
              .buildPunctuation(tokParens, 5, 1, 19)
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
            it.build(tokStmt, 0, 0, 0)
              .buildPunctuation(tokParens, 0, 1, 0)
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
            it.build(tokStmt, 3, 0, 9)
              .buildPunctuation(tokBrackets, 2, 1, 7)
              .build(tokWord, 0, 1, 3)
              .build(tokWord, 0, 5, 3)
        }
    }


    @Test
    fun `Brackets nested`() {
        testInpOutp("[car [other car] cdr]") {
            it.build(tokStmt, 6, 0, 21)
              .buildPunctuation(tokBrackets, 5, 1, 19)
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
            it.build(tokStmt, 0, 0, 0)
              .buildPunctuation(tokParens, 0, 1, 0)
              .build(tokWord, 0, 1, 4)
              .build(tokWord, 1, 6, 5)
              .bError(errorPunctuationUnmatched)
        }
    }

    @Test
    fun `Data accessor`() {
        testInpOutp("asdf[5]") {
            it.build(tokStmt, 3, 0, 7)
              .build(tokWord, 0, 0, 4)
              .buildPunctuation(tokAccessor, 1, 5, 1)
              .buildLitInt(5, 5, 1)
        }
    }

    @Test
    fun `Punctuation scope inside statement 1`() {
        testInpOutp("""foo bar ( asdf )""") {
            it.build(tokStmt, 4, 0, 16)
              .build(tokWord, 0, 0, 3)
              .build(tokWord, 0, 4, 3)
              .buildPunctuation(tokParens, 1, 9, 6)
              .build(tokWord, 0, 10, 4)
        }
    }

    @Test
    fun `Punctuation scope inside statement 2`() {
        testInpOutp(
            """foo bar (
asdf
bcj
)"""
        ) {
            it.build(tokStmt, 5, 0, 20)
              .build(tokWord, 0, 0, 3)
              .build(tokWord, 0, 4, 3)
              .buildPunctuation(tokParens, 2, 9, 10)
              .build(tokWord, 0, 10, 4)
              .build(tokWord, 0, 15, 3)
        }
    }

    @Test
    fun `Punctuation all types`() {
        testInpOutp(
            """(
asdf (b [d ef (y z)] c d[x y])

bcjk ((a b))
)""") {
            it.build(tokStmt, 20, 0, 48)
              .buildPunctuation(tokParens, 19, 1, 46)
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
              .buildPunctuation(tokAccessor, 2, 27, 3)
              .build(tokWord, 0, 27, 1)
              .build(tokWord, 0, 29, 1)
              .build(tokWord, 0, 34, 4)
              .buildPunctuation(tokParens, 3, 40, 5)
              .buildPunctuation(tokParens, 2, 41, 3)
              .build(tokWord, 0, 41, 1)
              .build(tokWord, 0, 43, 1)
        }
    }

    @Test
    fun `Colon punctuation 1`() {
        testInpOutp(
            """Foo : Bar 4"""
        ) {
            it.build(tokStmt, 4, 0, 11)
              .build(tokWord, 1, 0, 3)
              .buildPunctuation(tokParens, 2, 5, 6)
              .build(tokWord, 1, 6, 3)
              .buildLitInt(4, 10, 1)
        }
    }
}

@Nested
inner class LexOperatorTest {
    @Test
    fun `Operator simple 1`() {
        testInpOutp("+") {
            it.build(tokStmt, 1, 0, 1).buildOperator(opTPlus, false, false, 0, 1)
        }
    }

    @Test
    fun `Operator extensible`() {
        testInpOutp("+. -. >>. %. *. 5 <<. ^.") {
            it.build(tokStmt, 8, 0, 24)
              .buildOperator(opTPlus, true, false, 0, 2)
              .buildOperator(opTMinus, true, false, 3, 2)
              .buildOperator(opTBitshiftRight, true, false, 6, 3)
              .buildOperator(opTRemainder, true, false, 10, 2)
              .buildOperator(opTTimes, true, false, 13, 2)
              .buildLitInt(5, 16, 1)
              .buildOperator(opTBitShiftLeft, true, false, 18, 3)
              .buildOperator(opTExponentiation, true, false, 22, 2)

        }
    }

    @Test
    fun `Operators list`() {
        testInpOutp("+ - / * ^ && || ~ ? => >=< ><") {
            it.build(tokStmt, 12, 0, 29)
              .buildOperator(opTPlus, false, false, 0, 1)
              .buildOperator(opTMinus, false, false, 2, 1)
              .buildOperator(opTDivBy, false, false, 4, 1)
              .buildOperator(opTTimes, false, false, 6, 1)
              .buildOperator(opTExponentiation, false, false, 8, 1)
              .buildOperator(opTBoolAnd, false, false, 10, 2)
              .buildOperator(opTBoolOr, false, false, 13, 2)
              .buildOperator(opTIsEmpty, false, false, 16, 1)
              .buildOperator(opTQuestionMark, false, false, 18, 1)
              .buildOperator(opTArrowRight, false, false, 20, 2)
              .buildOperator(opTIntervalLeftInclusive, false, false, 23, 3)
              .buildOperator(opTIntervalExclusive, false, false, 27, 2)
        }
    }

    @Test
    fun `Operator expression`() {
        testInpOutp("a - b") {
            it.build(tokStmt, 3, 0, 5)
              .build(tokWord, 0, 0, 1)
              .buildOperator(opTMinus, false, false, 2, 1)
              .build(tokWord, 0, 4, 1)
        }
    }

    @Test
    fun `Operator assignment 1`() {
        testInpOutp("a += b") {
            it.buildAll(tokStmtAssignment shl 26, 6, (opTPlus shl 2) + 1, 2)
              .build(tokWord, 0, 0, 1)
              .build(tokWord, 0, 5, 1)
        }
    }


    @Test
    fun `Operator assignment 2`() {
        testInpOutp("a ||= b") {
            it.buildAll(tokStmtAssignment shl 26, 7, (opTBoolOr shl 2) + 1, 2)
              .build(tokWord, 0, 0, 1)
              .build(tokWord, 0, 6, 1)
        }
    }

    @Test
    fun `Operator assignment 3`() {
        testInpOutp("a *.= b") {
            it.buildAll(tokStmtAssignment shl 26, 7, (opTTimes shl 2) + 3, 2)
              .build(tokWord, 0, 0, 1)
              .build(tokWord, 0, 6, 1)
        }
    }

    @Test
    fun `Operator assignment 4`() {
        testInpOutp("a ^= b") {
            it.buildAll(tokStmtAssignment shl 26, 6, (opTExponentiation shl 2) + 1, 2)
              .build(tokWord, 0, 0, 1)
              .build(tokWord, 0, 5, 1)
        }
    }

    @Test
    fun `Operator assignment in parens error`() {
        testInpOutp("(x += y + 5)") {
            it.build(tokStmt, 0, 0, 0)
              .buildPunctuation(tokParens, 0, 1, 0)
              .build(tokWord, 0, 1, 1)
              .bError(errorOperatorAssignmentPunct)
        }
    }

    @Test
    fun `Operator assignment with parens`() {
        testInpOutp("x +.= (y + 5)") {
            it.buildAll(tokStmtAssignment shl 26, 13, 3 + (opTPlus shl 2), 5) // 3 for isExtended and isAssignment
              .build(tokWord, 0, 0, 1)
              .buildPunctuation(tokParens, 3, 7, 5)
              .build(tokWord, 0, 7, 1)
              .buildOperator(opTPlus, false, false, 9, 1)
              .build(tokInt, 5, 11, 1)
        }
    }

    @Test
    fun `Operator assignment in tokParens error 1`() {
        testInpOutp("x (+= y) + 5") {
            it.build(tokStmt, 0, 0, 0)
              .build(tokWord, 0, 0, 1)
              .buildPunctuation(tokParens, 0, 3, 0)
              .bError(errorOperatorAssignmentPunct)
        }
    }

    @Test
    fun `Operator assignment multiple error 1`() {
        testInpOutp("x := y := 7") {
            it.buildAll(tokStmtAssignment shl 26, 0, opTMutation shl 2, 0)
              .build(tokWord, 0, 0, 1)
              .build(tokWord, 0, 5, 1)
              .bError(errorOperatorMultipleAssignment)
        }
    }
}

@Nested
inner class LexFunctionTest {
    @Test
    fun `Function simple 1`() {
        testInpOutp("foo = (\\x y. x - y)") {
            it.buildAll(tokStmtAssignment shl 26, 19, opTImmDefinition shl 2, 9)
              .build(tokWord, 0, 0, 3)
              .buildPunctuation(tokStmtLambda, 7, 7, 11)
              .buildPunctuation(tokStmt, 2, 8, 3)
              .build(tokWord, 0, 8, 1)
              .build(tokWord, 0, 10, 1)
              .buildPunctuation(tokStmt, 3, 13, 5)
              .build(tokWord, 0, 13, 1)
              .buildOperator(opTMinus, false, false, 15, 1)
              .build(tokWord, 0, 17, 1)
        }
    }
}


// Int Int => Int
// foo = (\x y. x - y)


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