#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../src/tl.internal.h"
#include "tlTest.h"


typedef struct {
    String* name;
    String* input;
    Compiler* expectedOutput;
} LexerTest;


typedef struct {
    String* name;
    int totalTests;
    LexerTest* tests;
} LexerTestSet;

//{{{ Utils

#define S   70000000 // A constant larger than the largest allowed file size. Separates parsed
                     // names from others

private Compiler* buildExpectedLexer(Compiler* proto, Arena *a, int totalTokens, Arr(Token) tokens) {
    Compiler* result = createLexerFromProto(&empty, proto, a);
    if (result == NULL) return result;
    StandardText stText = getStandardTextLength();
    if (tokens == NULL) {
        return result;
    }
    for (int i = 0; i < totalTokens; i++) {
        Token tok = tokens[i];
        // offset nameIds and startBts for the standardText and standard nameIds correspondingly
        tok.startBt += stText.len;
        if (tok.tp == tokWord || tok.tp == tokKwArg || tok.tp == tokTypeName
            || tok.tp == tokFieldAcc) {
            if (tok.pl2 < S) { /* parsed words */
                tok.pl2 += stText.firstParsed;
            } else { /* built-in words */
                tok.pl2 -= S;
                tok.pl2 += countOperators;
            }
        } else if (tok.tp == tokAssignLeft && tok.pl2 == 0) { // the short form of an assignment
            tok.pl1 += stText.firstParsed;
        }
        pushIntokens(tok, result);
    }

    return result;
}

#define expect(toks) buildExpectedLexer(proto, a, sizeof(toks)/sizeof(Token), toks)
#define expectEmpty(toks) buildExpectedLexer(proto, a, 0, NULL)


private Compiler* buildLexerWithError0(String* errMsg, Compiler* proto, Arena *a,
                                       Int totalTokens, Arr(Token) tokens) {
    Compiler* result = buildExpectedLexer(proto, a, totalTokens, tokens);
    result->stats.wasLexerError = true;
    result->stats.errMsg = errMsg;
    return result;
}

#define buildLexerWithError(msg, toks) buildLexerWithError0(msg, proto, a, sizeof(toks)/sizeof(Token), toks)
#define expectEmptyWithError(msg) buildLexerWithError0(msg, proto, a, 0, NULL)


private LexerTestSet* createTestSet0(String* name, Arena *a, int count, Arr(LexerTest) tests) {
    LexerTestSet* result = allocateOnArena(sizeof(LexerTestSet), a);
    result->name = name;
    result->totalTests = count;
    result->tests = allocateOnArena(count*sizeof(LexerTest), a);
    if (result->tests == NULL) return result;
    for (int i = 0; i < count; i++) {
        result->tests[i] = tests[i];
    }
    return result;
}

#define createTestSet(n, a, tests) createTestSet0(n, a, sizeof(tests)/sizeof(LexerTest), tests)


void runLexerTest(LexerTest test, int* countPassed, int* coUntests, Compiler* proto, Arena *a) {
// Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code
    (*coUntests)++;
    Compiler* result = lexicallyAnalyze(test.input, proto, a);

    int equalityStatus = equalityLexer(*result, *test.expectedOutput);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(result->stats.errMsg);
        if (test.expectedOutput->stats.wasError) {
            printf("\nBut was expected: ");
            printString(test.expectedOutput->stats.errMsg);
        } else {
            printf("\nBut was expected to be error-free\n");
        }
        printLexer(result);
    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On token %d\n", equalityStatus);
        printLexer(result);
    }
}
//}}}
//{{{ Word

LexerTestSet* wordTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Word lexer test"), a, ((LexerTest[]) {
        (LexerTest) {
            .name = s("Simple word lexing"),
            .input = s("asdf abc"),
            .expectedOutput = expect(((Token[]){
                    (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                    (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 4 },
                    (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Word with array accessor"),
            .input = s("asdf@abc"),
            .expectedOutput = expect(((Token[]){
                    (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 8 },
                    (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 4 },
                    (Token){ .tp = tokOperator, .pl1 = opAccessor, .startBt = 4, .lenBts = 1 },
                    (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 1"),
            .input = s("asdf:Abc"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8  },
                (Token){ .tp = tokTypeName, .pl2 = 0, .startBt = 0, .lenBts = 8  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 2"),
            .input = s("asdf:abcd:zyui"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 14  },
                (Token){ .tp = tokWord, .startBt = 0, .lenBts = 14  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 3"),
            .input = s("asdf:Abcd"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokTypeName, .startBt = 0, .lenBts = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Field accessor"),
            .input = s("a.field"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokFieldAcc, .pl2 = 1, .startBt = 1, .lenBts = 6 }
            }))
        },
        (LexerTest) {
            .name = s("Array numeric index"),
            .input = s("a@5"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opAccessor, .startBt = 1, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 2, .lenBts = 1 }
            }))
        },
        (LexerTest) {
            .name = s("Array variable index"),
            .input = s("a@ind"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt,         .pl2 = 3, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokWord,         .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opAccessor, .startBt = 1, .lenBts = 1 },
                (Token){ .tp = tokWord,         .pl2 = 1, .startBt = 2, .lenBts = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Array complex index"),
            .input = s("a@(i + 1)"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt,         .pl2 = 6, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokWord,         .pl2 = 0, .startBt = 0, .lenBts = 1 }, /*a*/
                (Token){ .tp = tokOperator, .pl1 = opAccessor, .startBt = 1, .lenBts = 1 },
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 2, .lenBts = 7 },
                (Token){ .tp = tokWord,         .pl2 = 1, .startBt = 3, .lenBts = 1 }, /*i*/
                (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 5, .lenBts = 1 },
                (Token){ .tp = tokInt,         .pl2 = 1, .startBt = 7, .lenBts = 1 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with reserved word"),
            .input = s("ifter"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 5 }
            }))
        },
    }));
}

//}}}
//{{{ Numeric

LexerTestSet* numericTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Numeric lexer test"), a, ((LexerTest[]) {
        (LexerTest) {
            .name = s("Hex numeric 1"),
            .input = s("0x15"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokInt, .pl2 = 21, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric 2"),
            .input = s("0x05"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric 3"),
            .input = s("0xFFFFFFFFFFFFFFFF"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)-1 >> 32), .pl2 = ((int64_t)-1 & LOWER32BITS),
                        .startBt = 0, .lenBts = 18  }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric 4"),
            .input = s("0xFFFFFFFFFFFFFFFE"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)-2 >> 32), .pl2 = ((int64_t)-2 & LOWER32BITS),
                        .startBt = 0, .lenBts = 18  }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric too long"),
            .input = s("0xFFFFFFFFFFFFFFFF0"),
            .expectedOutput = buildLexerWithError(s(errNumericBinWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 1"),
            .input = s("1.234"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(1.234) >> 32,
                         .pl2 = longOfDoubleBits(1.234) & LOWER32BITS, .startBt = 0, .lenBts = 5 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 2"),
            .input = s("00001.234"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(1.234) >> 32,
                         .pl2 = longOfDoubleBits(1.234) & LOWER32BITS, .startBt = 0, .lenBts = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 3"),
            .input = s("10500.01"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(10500.01) >> 32,
                         .pl2 = longOfDoubleBits(10500.01) & LOWER32BITS, .startBt = 0, .lenBts = 8 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 4"),
            .input = s("0.9"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(0.9) >> 32,
                         .pl2 = longOfDoubleBits(0.9) & LOWER32BITS, .startBt = 0, .lenBts = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 5"),
            .input = s("100500.123456"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 13 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(100500.123456) >> 32,
                         .pl2 = longOfDoubleBits(100500.123456) & LOWER32BITS,
                         .startBt = 0, .lenBts = 13 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric big"),
            .input = s("9007199254740992.0"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokDouble,
                    .pl1 = longOfDoubleBits(9007199254740992.0) >> 32,
                    .pl2 = longOfDoubleBits(9007199254740992.0) & LOWER32BITS,
                    .startBt = 0, .lenBts = 18 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric too big"),
            .input = s("9007199254740993.0"),
            .expectedOutput = buildLexerWithError(s(errNumericFloatWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric big exponent"),
            .input = s("1005001234560000000000.0"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 24 },
                (Token){ .tp = tokDouble,
                        .pl1 = longOfDoubleBits(1005001234560000000000.0) >> 32,
                        .pl2 = longOfDoubleBits(1005001234560000000000.0) & LOWER32BITS,
                        .startBt = 0, .lenBts = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric tiny"),
            .input = s("0.0000000000000000000003"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 24 },
                (Token){ .tp = tokDouble,
                        .pl1 = longOfDoubleBits(0.0000000000000000000003) >> 32,
                        .pl2 = longOfDoubleBits(0.0000000000000000000003) & LOWER32BITS,
                        .startBt = 0, .lenBts = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 1"),
            .input = s("-9.0"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(-9.0) >> 32,
                        .pl2 = longOfDoubleBits(-9.0) & LOWER32BITS, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 2"),
            .input = s("-8.775_807"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 10 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(-8.775807) >> 32,
                    .pl2 = longOfDoubleBits(-8.775807) & LOWER32BITS, .startBt = 0, .lenBts = 10 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 3"),
            .input = s("-1005001234560000000000.0"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 25 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(-1005001234560000000000.0) >> 32,
                          .pl2 = longOfDoubleBits(-1005001234560000000000.0) & LOWER32BITS,
                        .startBt = 0, .lenBts = 25 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 1"),
            .input = s("3"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 3, .startBt = 0, .lenBts = 1 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 2"),
            .input = s("12"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokInt, .pl2 = 12, .startBt = 0, .lenBts = 2,  }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 3"),
            .input = s("0987_12"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokInt, .pl2 = 98712, .startBt = 0, .lenBts = 7 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 4"),
            .input = s("9_223_372_036_854_775_807"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 25 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)9223372036854775807 >> 32),
                        .pl2 = ((int64_t)9223372036854775807 & LOWER32BITS),
                        .startBt = 0, .lenBts = 25 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric negative 1"),
            .input = s("-1"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokInt, .pl1 = (((int64_t)-1) >> 32),
                        .pl2 = (((int64_t)-1) & LOWER32BITS),
                        .startBt = 0, .lenBts = 2 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric negative 2"),
            .input = s("-775_807"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)(-775807) >> 32),
                         .pl2 = ((int64_t)(-775807) & LOWER32BITS), .startBt = 0, .lenBts = 8 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric negative 3"),
            .input = s("-9_223_372_036_854_775_807"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 26 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)(-9223372036854775807) >> 32),
                        .pl2 = ((int64_t)(-9223372036854775807) & LOWER32BITS),
                        .startBt = 0, .lenBts = 26 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric error 1"),
            .input = s("3_"),
            .expectedOutput = buildLexerWithError(s(errNumericEndUnderscore), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))},
        (LexerTest) { .name = s("Int numeric error 2"),
            .input = s("9_223_372_036_854_775_808"),
            .expectedOutput = buildLexerWithError(s(errNumericIntWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}
    }));
}
//}}}
//{{{ String

LexerTestSet* stringTests(Compiler* proto, Arena* a) {
    return createTestSet(s("String literals lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("String simple literal"),
            .input = s("`asdfn't`"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokString, .startBt = 0, .lenBts = 9 }
            }))
        },
        (LexerTest) { .name = s("String literal with non-ASCII inside"),
            .input = s("`hello мир`"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 14 },
                (Token){ .tp = tokString, .startBt = 0, .lenBts = 14 }
        }))},
        (LexerTest) { .name = s("String literal unclosed"),
            .input = s("`asdf"),
            .expectedOutput = buildLexerWithError(s(errPrematureEndOfInput), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}
    }));
}

//}}}
//{{{ Meta
/*
LexerTestSet* metaTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Metaexpressions lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Comment simple"),
            .input = s("[`this is a comment`]"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 21 },
                (Token){ .tp = tokMeta, .pl1 = slSubexpr, .pl2 = 1, .startBt = 0, .lenBts = 21 },
                (Token){ .tp = tokString, .startBt = 1, .lenBts = 19 }
            }))
            }
    }));
}
*/

//}}}
//{{{ Punctuation

LexerTestSet* punctuationTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Punctuation lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Parens simple"),
            .input = s("(car cdr)"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokWord, .pl2 = 0,  .startBt = 1, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Parens nested"),
            .input = s("(car (other car) cdr)"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt,   .pl2 = 6, .startBt = 0, .lenBts = 21 },
                (Token){ .tp = tokParens, .pl2 = 5, .startBt = 0, .lenBts = 21 },
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 1, .lenBts = 3 },  // car

                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 5, .lenBts = 11 },
                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 6, .lenBts = 5 },  // other
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 12, .lenBts = 3 }, // car

                (Token){ .tp = tokWord,   .pl2 = 2, .startBt = 17, .lenBts = 3 }  // cdr
        }))},
        (LexerTest) { .name = s("Parens unclosed"),
            .input = s("(car (other car) cdr"),
            .expectedOutput = buildLexerWithError(s(errPunctuationExtraOpening), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startBt = 0, .lenBts = 0 },
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 1, .lenBts = 3 },
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 5, .lenBts = 11 },
                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 6, .lenBts = 5 },
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 12, .lenBts = 3 },
                (Token){ .tp = tokWord,   .pl2 = 2, .startBt = 17, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Call simple"),
            .input = s("call var otherVar"),
            .expectedOutput = expect(((Token[]) {
                (Token){ .tp = tokStmt,   .pl2 = 3, .lenBts = 17 },
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 5, .lenBts = 3 },
                (Token){ .tp = tokWord,   .pl2 = 2, .startBt = 9, .lenBts = 8 }
        }))},
        (LexerTest) { .name = s("Call with parens"),
            .input = s("var (call otherVar)"),
            .expectedOutput = expect(((Token[]) {
                (Token){ .tp = tokStmt,   .pl2 = 4, .lenBts = 19 },
                (Token){ .tp = tokWord, .pl1 = 0, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokParens, .pl1 = 0, .pl2 = 2, .startBt = 4, .lenBts = 15 },

                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 5, .lenBts = 4 },
                (Token){ .tp = tokWord,   .pl2 = 2, .startBt = 10, .lenBts = 8 }
        }))},
        (LexerTest) { .name = s("Scope simple"),
            .input = s("(do car;\n"
                       "    cdr)"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 4, .startBt = 0, .lenBts = 17 },
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 4, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 4, .lenBts = 3 },
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 13, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 13, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Scopes nested"),
            .input = s("(do car;\n"
                       "(do other car)\n"
                       "cdr)"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 8, .startBt = 0, .lenBts = 28 },

                (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 4, .lenBts = 3 },
                (Token){ .tp = tokWord,  .pl2 = 0, .startBt = 4, .lenBts = 3 },  // car

                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 3, .startBt = 9, .lenBts = 14 },
                (Token){ .tp = tokStmt,  .pl2 = 2, .startBt = 13, .lenBts = 9 },
                (Token){ .tp = tokWord,  .pl2 = 1, .startBt = 13, .lenBts = 5 },  // other
                (Token){ .tp = tokWord,  .pl2 = 0, .startBt = 19, .lenBts = 3 }, // car

                (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 24, .lenBts = 3 },
                (Token){ .tp = tokWord,  .pl2 = 2, .startBt = 24, .lenBts = 3 }  // cdr
        }))},
        (LexerTest) { .name = s("Parens inside statement"),
            .input = s("loo aa ( asdf )"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 4, .lenBts = 15 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 2 },
                (Token){ .tp = tokParens, .pl2 = 1, .startBt = 7, .lenBts = 8 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 9, .lenBts = 4 }
        }))},
        (LexerTest) { .name = s("Multi-line statement"),
            .input = s("owl car (\n"
                       "asdf\n"
                       "bcj\n"
                       ")"
                      ),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 5, .lenBts = 20 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },                // foo
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 },  // bar
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 8, .lenBts = 12 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 10, .lenBts = 4 }, // asdf
                (Token){ .tp = tokWord, .pl2 = 3, .startBt = 15, .lenBts = 3 }  // bcj
        }))},
        (LexerTest) { .name = s("Multiple statements"),
            .input = s("moo car;\n"
                       "asdf;\n"
                       "bcj"
                      ),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 2,               .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0,               .lenBts = 3 }, // moo
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 }, // car

                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 9, .lenBts = 4 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 9, .lenBts = 4 }, // asdf

                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 15, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 3, .startBt = 15, .lenBts = 3 } // bcj
        }))},
        (LexerTest) { .name = s("Stmt separator"),
            .input = s("awu; arn baz"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 5, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 9, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Semicolon usage error"),
            .input = s("asdf (zoogle; baz)"),
            .expectedOutput = buildLexerWithError(s(errPunctuationOnlyInMultiline), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokParens, .startBt = 5 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 6, .lenBts = 6 }
        }))},
        (LexerTest) { .name = s("Accessors"),
            .input = s("arr[i] arr[i - 1] brr[j][k]"),
            .expectedOutput = expect(((Token[]) {
                (Token){ .tp = tokStmt,  .pl2 = 13, .startBt = 0, .lenBts = 27 },

                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokAccessor, .pl2 = 1, .startBt = 3, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 1 },

                (Token){ .tp = tokWord,      .pl2 = 0, .startBt = 7, .lenBts = 3 },
                (Token){ .tp = tokAccessor,   .pl2 = 3, .startBt = 10, .lenBts = 7 },
                (Token){ .tp = tokWord,       .pl2 = 1, .startBt = 11, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opMinus,  .startBt = 13, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 1, .startBt = 15, .lenBts = 1 },

                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 18, .lenBts = 3 },
                (Token){ .tp = tokAccessor, .pl2 = 1, .startBt = 21, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 3, .startBt = 22, .lenBts = 1 },
                (Token){ .tp = tokAccessor, .pl2 = 1, .startBt = 24, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 4, .startBt = 25, .lenBts = 1 },
        }))}
    }));
}

//}}}
//{{{ Operator

LexerTestSet* operatorTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Operator lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Operator simple 1"),
            .input = s("+"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 0, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operators extended"),
            .input = s("+: *: -: /:"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 4, .lenBts = 11 },
                (Token){ .tp = tokOperator, .pl1 = opPlusExt,  .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opTimesExt, .startBt = 3, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opMinusExt, .startBt = 6, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opDivByExt, .startBt = 9, .lenBts = 2 }
        }))},
        (LexerTest) { .name = s("Operators bitwise"),
            .input = s("!. ||. >>. &&. <<. ^."),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 6, .lenBts = 21 },
                (Token){ .tp = tokOperator, .pl1 = opBitwiseNeg, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opBitwiseOr,     .startBt = 3, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opBitShiftR, .startBt = 7, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opBitwiseAnd,     .startBt = 11, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opBitShiftL, .startBt = 15, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opBitwiseXor,  .startBt = 19, .lenBts = 2 }
        }))},
        (LexerTest) { .name = s("Operators list"),
            .input = s("+ - / * ^ && || ' ? >=< >< >=<= $ >> << ##"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt,             .pl2 = 16, .startBt = 0, .lenBts = 42 },
                (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opMinus, .startBt = 2, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opDivBy, .startBt = 4, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTimes, .startBt = 6, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opExponent, .startBt = 8, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBoolAnd, .startBt = 10, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opBoolOr, .startBt = 13, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opIsNull, .startBt = 16, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opQuestionMark, .startBt = 18, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opIntervalL, .startBt = 20, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opIntervalEx, .startBt = 24, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opIntervalBo, .startBt = 27, .lenBts = 4 },
                (Token){ .tp = tokOperator, .pl1 = opToString, .startBt = 32, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opShiftR, .startBt = 34, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opShiftL, .startBt = 37, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opSize, .startBt = 40, .lenBts = 2 }
        }))},
        (LexerTest) { .name = s("Operator expression"),
            .input = s("a - b"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opMinus, .startBt = 2, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 1"),
            .input = s("a += b"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokAssignLeft, .pl1 = opPlus + S, .pl2 = 1,
                    .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokAssignRight, .pl2 = 1, .startBt = 2, .lenBts = 4 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 2"),
            .input = s("a ||= b"),
            .expectedOutput = expect(((Token[]) {
                (Token){ .tp = tokAssignLeft, .pl1 = opBoolOr + S, .pl2 = 1,
                         .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokAssignRight, .pl2 = 1, .startBt = 2, .lenBts = 5 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 6, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 3"),
            .input = s("a*:= b"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokAssignLeft, .pl1 = opTimesExt + S, .pl2 = 1,
                         .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokAssignRight, .pl2 = 1, .startBt = 1, .lenBts = 5 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 4"),
            .input = s("a ^= b"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokAssignLeft, .pl1 = opExponent + S, .pl2 = 1, .startBt = 0,
                         .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokAssignRight, .pl2 = 1, .startBt = 2, .lenBts = 4 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment in parens error"),
            .input = s("(x += y + 5)"),
            .expectedOutput = buildLexerWithError(s(errOperatorAssignmentPunct), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens },
                (Token){ .tp = tokWord, .startBt = 1, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment with parens"),
            .input = s("x +:= (y + 5)"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokAssignLeft, .pl1 = opPlusExt + S, .pl2 = 1, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokAssignRight, .pl2 = 4, .startBt = 2, .lenBts = 11 },
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 7, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 9, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 11, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment in parens error"),
            .input = s("x (+= y) + 5"),
            .expectedOutput = buildLexerWithError(s(errOperatorAssignmentPunct), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokParens, .startBt = 2 }
        }))},
        (LexerTest) { .name = s("Operator assignment multiple error"),
            .input = s("x = y = 7"),
            .expectedOutput = buildLexerWithError(s(errOperatorAssignmentPunct), ((Token[]) {
                (Token){ .tp = tokAssignLeft, .pl1 = 0, .pl2 = 0, .lenBts = 1 },
                (Token){ .tp = tokAssignRight, .pl2 = 0, .startBt = 2 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Boolean operators"),
            .input = s("a && b || c"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 5, .lenBts = 11 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBoolAnd, .startBt = 2, .lenBts = 2 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBoolOr, .startBt = 7, .lenBts = 2 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 10, .lenBts = 1 }
        }))}
    }));
}

//}}}
//{{{ Core forms

LexerTestSet* coreFormTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Core form lexer tests"), a, ((LexerTest[]) {
         (LexerTest) { .name = s("Statement-type core form"),
             .input = s("x = 9; assert (x == 55) `Error!`"),
             .expectedOutput = expect(((Token[]){
                 (Token){ .tp = tokAssignLeft,  .pl2 = 0,             .lenBts = 1 }, // x
                 (Token){ .tp = tokAssignRight,  .pl2 = 1,   .startBt = 2, .lenBts = 3 },
                 (Token){ .tp = tokInt, .pl2 = 9, .startBt = 4,     .lenBts = 1 },

                 (Token){ .tp = tokAssert, .pl2 = 5, .startBt = 7,  .lenBts = 25 },
                 (Token){ .tp = tokParens, .pl2 = 3, .startBt = 14, .lenBts = 9 },
                 (Token){ .tp = tokWord,                  .startBt = 15, .lenBts = 1 },
                 (Token){ .tp = tokOperator, .pl1 = opEquality, .startBt = 17, .lenBts = 2 },
                 (Token){ .tp = tokInt, .pl2 = 55,  .startBt = 20,  .lenBts = 2 },
                 (Token){ .tp = tokString,               .startBt = 24,  .lenBts = 8 }
         }))},
         (LexerTest) { .name = s("Statement-type core form error"),
             .input = s("x / (assert foo)"),
             .expectedOutput = buildLexerWithError(s(errCoreNotInsideStmt), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },                // x
                 (Token){ .tp = tokOperator, .pl1 = opDivBy, .startBt = 2, .lenBts = 1 },
                 (Token){ .tp = tokParens, .startBt = 4 }
         }))},
         (LexerTest) { .name = s("Definition of a mutable var"),
             .input = s("w~ = 9"),
             .expectedOutput = expect(((Token[]) {
                 (Token){ .tp = tokAssignLeft,  .pl2 = 1,             .lenBts = 2 },
                 (Token){ .tp = tokWord,    .pl1 = 1,          .startBt = 0, .lenBts = 2 }, // w~
                 (Token){ .tp = tokAssignRight,  .pl2 = 1,     .startBt = 3, .lenBts = 3 },
                 (Token){ .tp = tokInt,      .pl2 = 9, .startBt = 5, .lenBts = 1 },
         }))},
         (LexerTest) { .name = s("Assignment with complex left side"),
             .input = s("a[i][5] = 9"),
             .expectedOutput = expect(((Token[]) {
                 (Token){ .tp = tokAssignLeft,  .pl2 = 5,             .lenBts = 7 },
                 (Token){ .tp = tokWord,                .startBt = 0, .lenBts = 1 }, // x
                 (Token){ .tp = tokAccessor,    .pl2 = 1, .startBt = 1, .lenBts = 3 }, // x
                 (Token){ .tp = tokWord,      .pl2 = 1, .startBt = 2, .lenBts = 1 }, // i
                 (Token){ .tp = tokAccessor,  .pl2 = 1, .startBt = 4, .lenBts = 3 },
                 (Token){ .tp = tokInt,      .pl2 = 5, .startBt = 5, .lenBts = 1 },

                 (Token){ .tp = tokAssignRight,  .pl2 = 1, .startBt = 8, .lenBts = 3 },
                 (Token){ .tp = tokInt, .pl2 = 9,  .startBt = 10,  .lenBts = 1 },
         }))},
         (LexerTest) { .name = s("Paren-type core form"),
             .input = s("(if x <> 7 > 0: true )"),
             .expectedOutput = expect(((Token[]){
                 (Token){ .tp = tokIf, .pl1 = slScope, .pl2 = 8, .startBt = 0, .lenBts = 22 },

                 (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 5, .startBt = 4, .lenBts = 10 },
                 (Token){ .tp = tokWord,            .startBt = 4, .lenBts = 1 }, // x
                 (Token){ .tp = tokOperator, .pl1 = opComparator, .startBt = 6, .lenBts = 2 },
                 (Token){ .tp = tokInt, .pl2 = 7, .startBt = 9, .lenBts = 1 },
                 (Token){ .tp = tokOperator, .pl1 = opGreaterTh, .startBt = 11, .lenBts = 1 },
                 (Token){ .tp = tokInt, .startBt = 13, .lenBts = 1 },

                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 16, .lenBts = 4 },
                 (Token){ .tp = tokBool, .pl2 = 1, .startBt = 16, .lenBts = 4 },
         }))},
         (LexerTest) { .name = s("If with else"),
             .input = s("(if x <> 7 > 0: true else false )"),
             .expectedOutput = expect(((Token[]){
                 (Token){ .tp = tokIf, .pl1 = slScope, .pl2 = 11, .startBt = 0, .lenBts = 33 },
                 (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 5, .startBt = 4, .lenBts = 10 },
                 (Token){ .tp = tokWord,            .startBt = 4, .lenBts = 1 },                // x
                 (Token){ .tp = tokOperator, .pl1 = opComparator, .startBt = 6, .lenBts = 2 },
                 (Token){ .tp = tokInt,   .pl2 = 7, .startBt = 9, .lenBts = 1 },
                 (Token){ .tp = tokOperator, .pl1 = opGreaterTh, .startBt = 11, .lenBts = 1 },
                 (Token){ .tp = tokInt,             .startBt = 13, .lenBts = 1 },

                 (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 16, .lenBts = 4 },
                 (Token){ .tp = tokBool,  .pl2 = 1, .startBt = 16, .lenBts = 4 },

                 (Token){ .tp = tokElse, .pl1 = slScope, .pl2 = 2,
                          .startBt = 21, .lenBts = 10 },
                 (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 26, .lenBts = 5 },
                 (Token){ .tp = tokBool,            .startBt = 26, .lenBts = 5 }
         }))},
        (LexerTest) { .name = s("If with elseif and else"),
            .input = s("(if x <> 7 > 0: 5\n"
                       "eif x <> 7 < 0: 11\n"
                       "else true )"),
            .expectedOutput = expect(((Token[]){
                (Token){ .tp = tokIf, .pl1 = slScope, .pl2 = 20, .startBt = 0, .lenBts = 48 },
                (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 5, .startBt = 4, .lenBts = 10 },
                (Token){ .tp = tokWord,            .startBt = 4, .lenBts = 1 }, // x
                (Token){ .tp = tokOperator, .pl1 = opComparator, .startBt = 6, .lenBts = 2 },
                (Token){ .tp = tokInt,          .pl2 = 7, .startBt = 9, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opGreaterTh, .startBt = 11, .lenBts = 1 },
                (Token){ .tp = tokInt,   .pl2 = 0, .startBt = 13, .lenBts = 1 },
                (Token){ .tp = tokStmt,      .pl2 = 1, .startBt = 16, .lenBts = 1 },
                (Token){ .tp = tokInt,       .pl2 = 5, .startBt = 16, .lenBts = 1 },

                (Token){ .tp = tokElseIf, .pl1 = slScope, .pl2 = 8,
                         .startBt = 18, .lenBts = 18 },
                (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 5, .startBt = 22, .lenBts = 10 },
                (Token){ .tp = tokWord,                .startBt = 22, .lenBts = 1 }, // x
                (Token){ .tp = tokOperator, .pl1 = opComparator, .startBt = 24, .lenBts = 2 },
                (Token){ .tp = tokInt,       .pl2 = 7, .startBt = 27, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opLessTh, .startBt = 29, .lenBts = 1 },
                (Token){ .tp = tokInt,       .pl2 = 0, .startBt = 31, .lenBts = 1 },

                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 34, .lenBts = 2 },
                (Token){ .tp = tokInt,  .pl2 = 11,.startBt = 34, .lenBts = 2 },

                (Token){ .tp = tokElse, .pl1 = slScope, .pl2 = 2,
                         .startBt = 37, .lenBts = 9 },
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 42, .lenBts = 4 },
                (Token){ .tp = tokBool, .pl2 = 1, .startBt = 42, .lenBts = 4 }
         }))},
         (LexerTest) { .name = s("Function simple 1"),
             .input = s("noo = (\\ x Int y Int -> Int: x - y)"),
             .expectedOutput = expect(((Token[]){
                 (Token){ .tp = tokAssignLeft,         .pl2 = 0,
                          .startBt = 0, .lenBts = 3 }, // foo
                 (Token){ .tp = tokAssignRight,              .pl2 = 12,
                          .startBt = 4, .lenBts = 31 },
                 (Token){ .tp = tokFn, .pl1 = slScope,       .pl2 = 11,
                          .startBt = 6, .lenBts = 29 },
                 (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 6,
                          .startBt = 9, .lenBts = 18 },
                 (Token){ .tp = tokWord,                 .pl2 = 1, .startBt = 9, .lenBts = 1 }, // x
                 (Token){ .tp = tokTypeName,  .pl2 = (strInt + S),
                            .startBt = 11, .lenBts = 3 }, // Int
                 (Token){ .tp = tokWord,       .pl2 = 2,        .startBt = 15, .lenBts = 1 }, // y
                 (Token){ .tp = tokTypeName, .pl2 = (strInt + S),
                          .startBt = 17, .lenBts = 3 }, // Int
                 (Token){ .tp = tokMisc, .pl1 = miscArrow,          .startBt = 21, .lenBts = 2 },
                 (Token){ .tp = tokTypeName,   .pl2 = (strInt + S), .startBt = 24, .lenBts = 3 },

                 (Token){ .tp = tokStmt,       .pl2 = 3, .startBt = 29, .lenBts = 5 },
                 (Token){ .tp = tokWord,       .pl2 = 1, .startBt = 29, .lenBts = 1 }, // x
                 (Token){ .tp = tokOperator, .pl1 = opMinus, .startBt = 31, .lenBts = 1 },
                 (Token){ .tp = tokWord,       .pl2 = 2, .startBt = 33, .lenBts = 1 } // y
         }))},
         (LexerTest) { .name = s("Loop simple"),
             .input = s("(for x = 1; x < 101; x = x + 1: print x )"),
             .expectedOutput = expect(((Token[]) {
                 (Token){ .tp = tokFor, .pl1 = slScope, .pl2 = 15, .lenBts = 41 },

                 (Token){ .tp = tokAssignLeft,   .pl2 = 0, .startBt = 5, .lenBts = 1 }, // print
                 (Token){ .tp = tokAssignRight,  .pl2 = 1, .startBt = 7, .lenBts = 3 },
                 (Token){ .tp = tokInt,          .pl2 = 1, .startBt = 9, .lenBts = 1 },

                 (Token){ .tp = tokStmt,         .pl2 = 3, .startBt = 12, .lenBts = 7 },
                 (Token){ .tp = tokWord,                   .startBt = 12, .lenBts = 1 }, // x
                 (Token){ .tp = tokOperator, .pl1 = opLessTh, .startBt = 14, .lenBts = 1 },
                 (Token){ .tp = tokInt,             .pl2 = 101, .startBt = 16, .lenBts = 3 },


                 (Token){ .tp = tokAssignLeft, .pl1 = 0, .pl2 = 0,
                             .startBt = 21, .lenBts = 1 },
                 (Token){ .tp = tokIntro, .pl1 = tokAssignRight, .pl2 = 3,
                             .startBt = 23, .lenBts = 7 },
                 (Token){ .tp = tokWord,                 .startBt = 25, .lenBts = 1 },  // x
                 (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 27, .lenBts = 1 },
                 (Token){ .tp = tokInt,             .pl2 = 1, .startBt = 29, .lenBts = 1 },

                 (Token){ .tp = tokStmt,           .pl2 = 2, .startBt = 32, .lenBts = 7 },
                 (Token){ .tp = tokWord,  .pl2 = (strPrint + S), .startBt = 32, .lenBts=5 },//print
                 (Token){ .tp = tokWord,                 .startBt = 38, .lenBts = 1 }  // x
         }))}
    }));
}

//}}}
//{{{ Types

LexerTestSet* typeTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Type forms lexer tests"), a, ((LexerTest[]) {
         (LexerTest) { .name = s("Simple type call"),
             .input = s("(Foo Bar Baz)"),
             .expectedOutput = expect(((Token[]){
                 (Token){ .tp = tokStmt,               .pl2 = 4, .startBt = 0, .lenBts = 13 },
                 (Token){ .tp = tokTypeCall,  .pl2 = 3,
                             .startBt = 0, .lenBts = 13 },
                 (Token){ .tp = tokTypeName,           .pl2 = 0, .startBt = 1, .lenBts = 3 },
                 (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 5, .lenBts = 3 },
                 (Token){ .tp = tokTypeName,           .pl2 = 2, .startBt = 9, .lenBts = 3 }
         }))},
         (LexerTest) { .name = s("Generic function signature"),
             .input = s("(\\V W: lst (L V) w W: print w)"),
             .expectedOutput = expect(((Token[]){
                 (Token){ .tp = tokFn, .pl1 = slScope, .pl2 = 13,    .lenBts = 30 },

                 (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 2, .startBt = 2, .lenBts = 3 },
                 (Token){ .tp = tokTypeName,     .pl2 = 0, .startBt = 2, .lenBts = 1 }, //V
                 (Token){ .tp = tokTypeName,     .pl2 = 1, .startBt = 4, .lenBts = 1 }, //W

                 (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 6, .startBt = 7, .lenBts = 13 },
                 (Token){ .tp = tokWord,              .pl2 = 2, .startBt = 7, .lenBts = 3 }, //lst
                 (Token){ .tp = tokTypeCall,     .pl2 = 2, .startBt = 11, .lenBts = 5 },
                 (Token){ .tp = tokTypeName,         .pl2 = strL + S, .startBt = 12, .lenBts = 1 },
                 (Token){ .tp = tokTypeName,         .pl2 = 0, .startBt = 14, .lenBts = 1 }, //V
                 (Token){ .tp = tokWord,         .pl2 = 3,     .startBt = 17, .lenBts = 1 }, //w
                 (Token){ .tp = tokTypeName,     .pl2 = 1,     .startBt = 19, .lenBts = 1 },

                 (Token){ .tp = tokStmt,         .pl2 = 2,     .startBt = 22, .lenBts = 7 },
                 (Token){ .tp = tokWord,  .pl2 = strPrint + S, .startBt = 22, .lenBts = 5 },
                 (Token){ .tp = tokWord,     .pl2 = 3, .startBt = 28, .lenBts = 1 },
         }))},
         (LexerTest) { .name = s("Generic function signature with type funcs"),
             .input = s("(\\(G 2) H: lst (G Int H))"),
             .expectedOutput = expect(((Token[]){
                 (Token){ .tp = tokFn, .pl1 = slScope, .pl2 = 11,              .lenBts = 25 },
                 (Token){ .tp = tokIntro, .pl1 = tokStmt, .pl2 = 4, .startBt = 2, .lenBts = 7 },
                 (Token){ .tp = tokTypeCall,       .pl2 = 2, .startBt = 2, .lenBts = 5},
                 (Token){ .tp = tokTypeName,           .pl2 = 0, .startBt = 3, .lenBts = 1 }, // G
                 (Token){ .tp = tokInt,                .pl2 = 2, .startBt = 5, .lenBts = 1 },
                 (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 8, .lenBts = 1 }, // D

                 (Token){ .tp = tokStmt,               .pl2 = 5, .startBt = 11, .lenBts = 13 },
                 (Token){ .tp = tokWord,               .pl2 = 2, .startBt = 11, .lenBts = 3 },
                 (Token){ .tp = tokTypeCall,           .pl2 = 3,
                            .startBt = 15, .lenBts = 9 },
                 (Token){ .tp = tokTypeName,           .pl2 = 0, .startBt = 16, .lenBts = 1 },
                 (Token){ .tp = tokTypeName,           .pl2 = strInt + S,
                            .startBt = 18, .lenBts = 3 },
                 (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 22, .lenBts = 1 }
         }))},
         (LexerTest) { .name = s("Data allocations"),
             .input = s("[[1 2 3] [-3 4 5]]"),
             .expectedOutput = expect(((Token[]) {
                 (Token){ .tp = tokStmt, .pl2 = 9, .lenBts = 18 },
                 (Token){ .tp = tokDataList, .pl1 = 0, .pl2 = 8, .startBt = 0, .lenBts=18 },

                 (Token){ .tp = tokDataList, .pl2 = 3,
                            .startBt = 1, .lenBts = 7 },
                 (Token){ .tp = tokInt,          .pl2 = 1, .startBt = 2, .lenBts = 1 },
                 (Token){ .tp = tokInt,          .pl2 = 2, .startBt = 4, .lenBts = 1 },
                 (Token){ .tp = tokInt,          .pl2 = 3, .startBt = 6, .lenBts = 1 },

                 (Token){ .tp = tokDataList,     .pl2 = 3, .startBt = 9, .lenBts = 8 },
                 (Token){ .tp = tokInt, .pl1 = (((int64_t)-3) >> 32),
                       .pl2 = (((int64_t)-3) & LOWER32BITS), .startBt = 10, .lenBts = 2 },
                 (Token){ .tp = tokInt,                .pl2 = 4, .startBt = 13, .lenBts = 1 },
                 (Token){ .tp = tokInt,                .pl2 = 5, .startBt = 15, .lenBts = 1 },
         }))}
    }));
}

//}}}


void runATestSet(LexerTestSet* (*testGenerator)(Compiler*, Arena*), int* countPassed, int* coUntests,
                 Compiler* proto, Arena* a) {
    LexerTestSet* testSet = (testGenerator)(proto, a);
    for (int j = 0; j < testSet->totalTests; j++) {
        LexerTest test = testSet->tests[j];
        runLexerTest(test, countPassed, coUntests, proto, a);
    }
}


int main(int argc, char** argv) {
    printf("----------------------------\n");
    printf("--  LEXER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    Compiler* proto = createProtoCompiler(a);

    int countPassed = 0;
    int coUntests = 0;
    runATestSet(&wordTests, &countPassed, &coUntests, proto, a);
    runATestSet(&stringTests, &countPassed, &coUntests, proto, a);
    runATestSet(&operatorTests, &countPassed, &coUntests, proto, a);
    runATestSet(&punctuationTests, &countPassed, &coUntests, proto, a);
    runATestSet(&numericTests, &countPassed, &coUntests, proto, a);
    runATestSet(&coreFormTests, &countPassed, &coUntests, proto, a);
    runATestSet(&typeTests, &countPassed, &coUntests, proto, a);

    //runATestSet(&metaTests, &countPassed, &coUntests, proto, a);
    if (coUntests == 0) {
        print("\nThere were no tests to run!");
    } else if (countPassed == coUntests) {
        print("\nAll %d tests passed!", coUntests);
    } else {
        print("\nFailed %d tests out of %d!", (coUntests - countPassed), coUntests);
    }

    deleteArena(a);
}
