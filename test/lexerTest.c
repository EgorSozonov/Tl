#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../source/tl.internal.h"
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

#define in(x) prepareInput(x, a)
#define S   70000000 // A constant larger than the largest allowed file size. Separates parsed names from others


private Compiler* buildLexer0(Compiler* proto, Arena *a, int totalTokens, Arr(Token) tokens) {
    Compiler* result = createLexerFromProto(&empty, proto, a);
    if (result == NULL) return result;
    StandardText stText = getStandardTextLength();

    for (int i = 0; i < totalTokens; i++) {
        Token tok = tokens[i];
        // offset nameIds and startBts for the standardText and standard nameIds correspondingly
        tok.startBt += stText.len;
        if (tok.tp == tokMutation) {
            tok.pl1 += stText.len;
        }
        if (tok.tp == tokWord || tok.tp == tokKwArg || tok.tp == tokTypeName || tok.tp == tokStructField
            || (tok.tp == tokAccessor && tok.pl1 == tkAccDot)) {
            if (tok.pl2 < S) {
                tok.pl2 += stText.numNames;
            } else {
                tok.pl2 -= (S + stText.firstNonreserved);
            }
        } else if (tok.tp == tokCall || tok.tp == tokTypeCall) {
            if (tok.pl1 < S) {
                tok.pl1 += stText.numNames;
            } else {
                tok.pl1 -= (S + stText.firstNonreserved);
            }
        }
        pushIntokens(tok, result);
    }

    return result;
}

#define buildLexer(toks) buildLexer0(proto, a, sizeof(toks)/sizeof(Token), toks)


private Compiler* buildLexerWithError0(String* errMsg, Compiler* proto, Arena *a,
                                       Int totalTokens, Arr(Token) tokens) {
    Compiler* result = buildLexer0(proto, a, totalTokens, tokens);
    result->wasError = true;
    result->errMsg = errMsg;
    return result;
}

#define buildLexerWithError(msg, toks) buildLexerWithError0(msg, proto, a, sizeof(toks)/sizeof(Token), toks)


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


/** Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code */
void runLexerTest(LexerTest test, int* countPassed, int* countTests, Compiler* proto, Arena *a) {
    (*countTests)++;
    Compiler* result = lexicallyAnalyze(test.input, proto, a);

    int equalityStatus = equalityLexer(*result, *test.expectedOutput);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(result->errMsg);
        if (test.expectedOutput->wasError) {
            printf("\nBut was expected: ");
            printString(test.expectedOutput->errMsg);
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


LexerTestSet* wordTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Word lexer test"), a, ((LexerTest[]) {
        (LexerTest) {
            .name = s("Simple word lexing"),
            .input = in("asdf abc"),
            .expectedOutput = buildLexer(((Token[]){
                    (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                    (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 4 },
                    (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
            }))
        },
//~        (LexerTest) {
//~            .name = s("Word with array accessor"),
//~            .input = in("asdf_abc"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                    (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
//~                    (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 4 },
//~                    (Token){ .tp = tokAccessor, .pl1 = tkAccAt, .startBt = 5, .lenBts = 1 },
//~                    (Token){ .tp = tokWord, .pl2 = 0, .startBt = 6, .lenBts = 3 }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Word correct capitalization 1"),
//~            .input = in("asdf-Abc"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8  },
//~                (Token){ .tp = tokTypeName, .pl2 = 0, .startBt = 0, .lenBts = 8  }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Word correct capitalization 2"),
//~            .input = in("asdf-abcd-zyui"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 14  },
//~                (Token){ .tp = tokWord, .startBt = 0, .lenBts = 14  }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Word correct capitalization 3"),
//~            .input = in("asdf-Abcd"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
//~                (Token){ .tp = tokTypeName, .startBt = 0, .lenBts = 9 }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Word starts with underscore and lowercase letter"),
//~            .input = in("_abc"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
//~                (Token){ .tp = tokWord, .startBt = 0, .lenBts = 4 }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Word starts with underscore and capital letter"),
//~            .input = in("_Abc"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
//~                (Token){ .tp = tokTypeName, .startBt = 0, .lenBts = 4 }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Word may not start with 2 underscores"),
//~            .input = in("__abc"),
//~            .expectedOutput = buildLexerWithError(s(errWordChunkStart), ((Token[]){0}))
//~        },
//~        (LexerTest) {
//~            .name = s("Word may not start with underscore and digit"),
//~            .input = in("_1abc"),
//~            .expectedOutput = buildLexerWithError(s(errWordChunkStart), ((Token[]){0}))
//~        },
//~        (LexerTest) {
//~            .name = s("Field accessor"),
//~            .input = in("a.field"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 7 },
//~                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
//~                (Token){ .tp = tokAccessor, .pl1 = accField, .pl2 = 1, .startBt = 1, .lenBts = 6 }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Array numeric index"),
//~            .input = in("a_5"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 3 },
//~                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
//~                (Token){ .tp = tokAccessor, .pl1 = accArrayInd, .startBt = 1, .lenBts = 1 },
//~                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 2, .lenBts = 1 }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Array variable index"),
//~            .input = in("a_ind"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt,         .pl2 = 3, .startBt = 0, .lenBts = 5 },
//~                (Token){ .tp = tokWord,         .pl2 = 0, .startBt = 0, .lenBts = 1 },
//~                (Token){ .tp = tokAccessor, .pl1 = tkAccAt, .startBt = 1, .lenBts = 1 },
//~                (Token){ .tp = tokWord,         .pl2 = 1, .startBt = 2, .lenBts = 3 }
//~            }))
//~        },
//~        (LexerTest) {
//~            .name = s("Word starts with reserved word"),
//~            .input = in("ifter"),
//~            .expectedOutput = buildLexer(((Token[]){
//~                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 5 },
//~                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 5 }
//~            }))
//~        },
    }));
}

LexerTestSet* numericTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Numeric lexer test"), a, ((LexerTest[]) {
        (LexerTest) {
            .name = s("Hex numeric 1"),
            .input = in("0x15"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokInt, .pl2 = 21, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric 2"),
            .input = in("0x05"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric 3"),
            .input = in("0xFFFFFFFFFFFFFFFF"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)-1 >> 32), .pl2 = ((int64_t)-1 & LOWER32BITS),
                        .startBt = 0, .lenBts = 18  }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric 4"),
            .input = in("0xFFFFFFFFFFFFFFFE"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)-2 >> 32), .pl2 = ((int64_t)-2 & LOWER32BITS),
                        .startBt = 0, .lenBts = 18  }
            }))
        },
        (LexerTest) {
            .name = s("Hex numeric too long"),
            .input = in("0xFFFFFFFFFFFFFFFF0"),
            .expectedOutput = buildLexerWithError(s(errNumericBinWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 1"),
            .input = in("1.234"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(1.234) >> 32,
                                         .pl2 = longOfDoubleBits(1.234) & LOWER32BITS, .startBt = 0, .lenBts = 5 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 2"),
            .input = in("00001.234"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(1.234) >> 32,
                                         .pl2 = longOfDoubleBits(1.234) & LOWER32BITS, .startBt = 0, .lenBts = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 3"),
            .input = in("10500.01"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(10500.01) >> 32,
                                         .pl2 = longOfDoubleBits(10500.01) & LOWER32BITS, .startBt = 0, .lenBts = 8 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 4"),
            .input = in("0.9"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(0.9) >> 32, .pl2 = longOfDoubleBits(0.9) & LOWER32BITS,
                        .startBt = 0, .lenBts = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 5"),
            .input = in("100500.123456"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 13 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(100500.123456) >> 32,
                         .pl2 = longOfDoubleBits(100500.123456) & LOWER32BITS, .startBt = 0, .lenBts = 13 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric big"),
            .input = in("9007199254740992.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokDouble,
                    .pl1 = longOfDoubleBits(9007199254740992.0) >> 32,
                    .pl2 = longOfDoubleBits(9007199254740992.0) & LOWER32BITS, .startBt = 0, .lenBts = 18 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric too big"),
            .input = in("9007199254740993.0"),
            .expectedOutput = buildLexerWithError(s(errNumericFloatWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric big exponent"),
            .input = in("1005001234560000000000.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 24 },
                (Token){ .tp = tokDouble,
                        .pl1 = longOfDoubleBits(1005001234560000000000.0) >> 32,
                        .pl2 = longOfDoubleBits(1005001234560000000000.0) & LOWER32BITS,
                        .startBt = 0, .lenBts = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric tiny"),
            .input = in("0.0000000000000000000003"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 24 },
                (Token){ .tp = tokDouble,
                        .pl1 = longOfDoubleBits(0.0000000000000000000003) >> 32,
                        .pl2 = longOfDoubleBits(0.0000000000000000000003) & LOWER32BITS,
                        .startBt = 0, .lenBts = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 1"),
            .input = in("-9.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(-9.0) >> 32,
                        .pl2 = longOfDoubleBits(-9.0) & LOWER32BITS, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 2"),
            .input = in("-8.775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 10 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(-8.775807) >> 32,
                    .pl2 = longOfDoubleBits(-8.775807) & LOWER32BITS, .startBt = 0, .lenBts = 10 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 3"),
            .input = in("-1005001234560000000000.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 25 },
                (Token){ .tp = tokDouble, .pl1 = longOfDoubleBits(-1005001234560000000000.0) >> 32,
                          .pl2 = longOfDoubleBits(-1005001234560000000000.0) & LOWER32BITS,
                        .startBt = 0, .lenBts = 25 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 1"),
            .input = in("3"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 3, .startBt = 0, .lenBts = 1 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 2"),
            .input = in("12"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokInt, .pl2 = 12, .startBt = 0, .lenBts = 2,  }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 3"),
            .input = in("0987_12"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokInt, .pl2 = 98712, .startBt = 0, .lenBts = 7 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric 4"),
            .input = in("9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 25 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)9223372036854775807 >> 32),
                        .pl2 = ((int64_t)9223372036854775807 & LOWER32BITS),
                        .startBt = 0, .lenBts = 25 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric negative 1"),
            .input = in("-1"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)-1 >> 32), .pl2 = ((int64_t)-1 & LOWER32BITS),
                        .startBt = 0, .lenBts = 2 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric negative 2"),
            .input = in("-775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)(-775807) >> 32), .pl2 = ((int64_t)(-775807) & LOWER32BITS),
                        .startBt = 0, .lenBts = 8 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric negative 3"),
            .input = in("-9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 26 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)(-9223372036854775807) >> 32),
                        .pl2 = ((int64_t)(-9223372036854775807) & LOWER32BITS),
                        .startBt = 0, .lenBts = 26 }
            }))
        },
        (LexerTest) {
            .name = s("Int numeric error 1"),
            .input = in("3_"),
            .expectedOutput = buildLexerWithError(s(errNumericEndUnderscore), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))},
        (LexerTest) { .name = s("Int numeric error 2"),
            .input = in("9_223_372_036_854_775_808"),
            .expectedOutput = buildLexerWithError(s(errNumericIntWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}
    }));
}


LexerTestSet* stringTests(Compiler* proto, Arena* a) {
    return createTestSet(s("String literals lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("String simple literal"),
            .input = in("`asdfn't`"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokString, .startBt = 0, .lenBts = 9 }
            }))
        },
        (LexerTest) { .name = s("String literal with non-ASCII inside"),
            .input = in("`hello мир`"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 14 },
                (Token){ .tp = tokString, .startBt = 0, .lenBts = 14 }
        }))},
        (LexerTest) { .name = s("String literal unclosed"),
            .input = in("`asdf"),
            .expectedOutput = buildLexerWithError(s(errPrematureEndOfInput), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}
    }));
}


LexerTestSet* commentTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Comments lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Comment simple"),
            .input = in("-- this is a comment"),
            .expectedOutput = buildLexer((Token[]){0}
        )},
    }));
}

LexerTestSet* punctuationTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Punctuation lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Parens simple"),
            .input = in("(car cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokWord, .pl2 = 0,  .startBt = 1, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Parens nested"),
            .input = in("(car (other car) cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt,   .pl2 = 6, .startBt = 0, .lenBts = 21 },
                (Token){ .tp = tokParens, .pl2 = 5, .startBt = 0, .lenBts = 21 },
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 1, .lenBts = 3 },  // car

                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 5, .lenBts = 11 },
                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 6, .lenBts = 5 },  // other
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 12, .lenBts = 3 }, // car

                (Token){ .tp = tokWord,   .pl2 = 2, .startBt = 17, .lenBts = 3 }  // cdr
        }))},
        (LexerTest) { .name = s("Parens unclosed"),
            .input = in("(car (other car) cdr"),
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
            .input = in("car(other car)"),
            .expectedOutput = buildLexer(((Token[]) {
                (Token){ .tp = tokStmt, .pl2 = 3, .lenBts = 14 },
                (Token){ .tp = tokCall, .pl2 = 2, .startBt = 0, .lenBts = 14 },
                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 4, .lenBts = 5 },
                (Token){ .tp = tokWord,   .pl2 = 0, .startBt = 10, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Scope simple"),
            .input = in("do(car cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 3, .startBt = 0, .lenBts = 11 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 3, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 3, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 7, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Scopes nested"),
            .input = in("do(car. do(other car) cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 8, .startBt = 0, .lenBts = 26 },
                (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 3, .lenBts = 3 },
                (Token){ .tp = tokWord,  .pl2 = 0, .startBt = 3, .lenBts = 3 },  // car

                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 3, .startBt = 8, .lenBts = 13 },
                (Token){ .tp = tokStmt,  .pl2 = 2, .startBt = 11, .lenBts = 9 },
                (Token){ .tp = tokWord,  .pl2 = 1, .startBt = 11, .lenBts = 5 },  // other
                (Token){ .tp = tokWord,  .pl2 = 0, .startBt = 17, .lenBts = 3 }, // car

                (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 22, .lenBts = 3 },
                (Token){ .tp = tokWord,  .pl2 = 2, .startBt = 22, .lenBts = 3 }  // cdr
        }))},
        (LexerTest) { .name = s("Parens inside statement"),
            .input = in("foo bar ( asdf )"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 4, .lenBts = 16 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 },
                (Token){ .tp = tokParens, .pl2 = 1, .startBt = 8, .lenBts = 8 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 10, .lenBts = 4 }
        }))},
        (LexerTest) { .name = s("Multi-line statement"),
            .input = in("foo bar (\n"
                       "asdf\n"
                       "bcj\n"
                       ")"
                      ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 5, .lenBts = 20 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },                // foo
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 },  // bar
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 8, .lenBts = 12 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 10, .lenBts = 4 }, // asdf
                (Token){ .tp = tokWord, .pl2 = 3, .startBt = 15, .lenBts = 3 }  // bcj
        }))},
        (LexerTest) { .name = s("Multiple statements"),
            .input = in("foo bar\n"
                       "asdf.\n"
                       "bcj"
                      ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 2, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },               // foo
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 }, // bar

                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 8, .lenBts = 4 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 8, .lenBts = 4 }, // asdf

                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 14, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 3, .startBt = 14, .lenBts = 3 } // bcj
        }))},
        (LexerTest) { .name = s("Punctuation all types"),
            .input = in("do(\n"
                       "    b >asdf (d Ef (:y z))\n"
                       "    do(\n"
                       "        bcjk ( .m b )\n"
                       "    )\n"
                       ")"
            ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 15,  .startBt = 0,  .lenBts = 67 },
                (Token){ .tp = tokStmt,      .pl2 = 8,   .startBt = 8,  .lenBts = 21 },
                (Token){ .tp = tokCall,      .pl2 = 7,   .startBt = 8,  .lenBts = 21 },  //asdf
                (Token){ .tp = tokWord,      .pl2 = 1,   .startBt = 13,  .lenBts = 1 },  //b
                (Token){ .tp = tokParens,    .pl2 = 5,   .startBt = 15, .lenBts = 13 },
                (Token){ .tp = tokWord,      .pl2 = 2,   .startBt = 16, .lenBts = 1 },   // d
                (Token){ .tp = tokTypeName,  .pl2 = 3,   .startBt = 18, .lenBts = 2 },   // Ef
                (Token){ .tp = tokParens,    .pl2 = 2,   .startBt = 21, .lenBts = 6 },
                (Token){ .tp = tokKwArg,     .pl2 = 4,   .startBt = 22, .lenBts = 2 },   // :y
                (Token){ .tp = tokWord,      .pl2 = 5,   .startBt = 25, .lenBts = 1 },   // z
                (Token){ .tp = tokScope, .pl1 = slScope, .pl2 = 5, .startBt = 34, .lenBts = 31 },
                (Token){ .tp = tokStmt,      .pl2 = 4,   .startBt = 46, .lenBts = 13 },
                (Token){ .tp = tokWord,      .pl2 = 6,   .startBt = 46, .lenBts = 4 },   // bcjk
                (Token){ .tp = tokParens,    .pl2 = 2,   .startBt = 51, .lenBts = 8 },
                (Token){ .tp = tokStructField, .pl2 = 7,   .startBt = 53, .lenBts = 2 }, // m
                (Token){ .tp = tokWord,      .pl2 = 1,   .startBt = 56, .lenBts = 1 }    // b
        }))},
        (LexerTest) { .name = s("Dollar punctuation 1"),
            .input = in("Foo $ Bar 4"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 4, .startBt = 0, .lenBts = 11 },
                (Token){ .tp = tokTypeName, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 4, .lenBts = 7 },
                (Token){ .tp = tokTypeName, .pl2 = 1, .startBt = 6, .lenBts = 3 },
                (Token){ .tp = tokInt, .pl2 = 4, .startBt = 10, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Dollar punctuation 2"),
            .input = in("ab (arr (foo $ bar))"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 7,   .startBt = 0, .lenBts = 20 },
                (Token){ .tp = tokWord,  .pl2 = 0,  .startBt = 0, .lenBts = 2 }, // ab
                (Token){ .tp = tokParens, .pl2 = 5, .startBt = 3, .lenBts = 17 },
                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 4, .lenBts = 3 }, // arr
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 8, .lenBts = 11 },
                (Token){ .tp = tokWord,   .pl2 = 2, .startBt = 9, .lenBts = 3 },  // foo
                (Token){ .tp = tokParens, .pl2 = 1, .startBt = 13, .lenBts = 5 },
                (Token){ .tp = tokWord,   .pl2 = 3, .startBt = 15, .lenBts = 3 }  // bar
        }))},
        (LexerTest) { .name = s("Stmt separator"),
            .input = in("foo. bar baz"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 5, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 9, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Dot usage error"),
            .input = in("foo (bar. baz)"),
            .expectedOutput = buildLexerWithError(s(errPunctuationOnlyInMultiline), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokParens, .startBt = 4 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
        }))}
    }));
}


LexerTestSet* operatorTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Operator lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Operator simple 1"),
            .input = in("+"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 0, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operators extended"),
            .input = in("+: *: -: /:"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 7, .lenBts = 21 },
                (Token){ .tp = tokOperator, .pl1 = opPlusExt,      .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opMinusExt,     .startBt = 3, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opBitShiftRight, .startBt = 6, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opTimesExt,     .startBt = 10, .lenBts = 2 },
                (Token){ .tp = tokInt,                          .pl2 = 5, .startBt = 13, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBitShiftLeft, .startBt = 15, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opBitwiseXor,  .startBt = 19, .lenBts = 2 }
        }))},
        (LexerTest) { .name = s("Operators bitwise"),
            .input = in("!. ||. >>. &&. <<. ^."),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 7, .lenBts = 21 },
                (Token){ .tp = tokOperator, .pl1 = opPlusExt,      .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opMinusExt,     .startBt = 3, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opBitShiftRight, .startBt = 6, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opTimesExt,     .startBt = 10, .lenBts = 2 },
                (Token){ .tp = tokInt,                          .pl2 = 5, .startBt = 13, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBitShiftLeft, .startBt = 15, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opBitwiseXor,  .startBt = 19, .lenBts = 2 }
        }))},
        (LexerTest) { .name = s("Operators list"),
            .input = in("+ - / * ^ && || ' ? >=< >< ; , - >> <<"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 14, .startBt = 0, .lenBts = 32 },
                (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opMinus, .startBt = 2, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opDivBy, .startBt = 4, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTimes, .startBt = 6, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opExponent, .startBt = 8, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBoolAnd, .startBt = 10, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opBoolOr, .startBt = 13, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opIsNull, .startBt = 16, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opQuestionMark, .startBt = 18, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opIntervalLeft, .startBt = 20, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opIntervalExcl, .startBt = 24, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opToString, .startBt = 27, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opToFloat, .startBt = 29, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opMinus, .startBt = 31, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator expression"),
            .input = in("a - b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opMinus, .startBt = 2, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 1"),
            .input = in("a += b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = (opPlus << 26) + 2, .pl2 = 2,
                         .startBt = 0, .lenBts = 6 }, // 2 = startBt
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 2"),
            .input = in("a ||= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = (opBoolOr << 26) + 2, .pl2 = 2, 
                         .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 6, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 3"),
            .input = in("a*:= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = (opTimesExt << 26) + 1, .pl2 = 2,
                         .startBt = 0, .lenBts = 6 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 4"),
            .input = in("a ^= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = (opExponent << 26) + 2, .pl2 = 2, .startBt = 0, 
                         .lenBts = 6 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment in parens error"),
            .input = in("(x += y + 5)"),
            .expectedOutput = buildLexerWithError(s(errOperatorAssignmentPunct), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens },
                (Token){ .tp = tokWord, .startBt = 1, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment with parens"),
            .input = in("x +:= (y + 5)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = (opPlusExt << 26) + 2, .pl2 = 5, .startBt = 0, .lenBts = 13 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 7, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 9, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 11, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment in parens error"),
            .input = in("x (+= y) + 5"),
            .expectedOutput = buildLexerWithError(s(errOperatorAssignmentPunct), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokParens, .startBt = 2 }
        }))},
        (LexerTest) { .name = s("Operator assignment multiple error"),
            .input = in("x := y := 7"),
            .expectedOutput = buildLexerWithError(s(errOperatorMultipleAssignment), ((Token[]) {
                (Token){ .tp = tokReassign },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Boolean operators"),
            .input = in("a && b || c"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 6, .lenBts = 14 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 4, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBoolAnd, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 11, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opBoolOr, .startBt = 8, .lenBts = 2 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 13, .lenBts = 1 }
        }))}
    }));
}


LexerTestSet* coreFormTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Core form lexer tests"), a, ((LexerTest[]) {
         (LexerTest) { .name = s("Statement-type core form"),
             .input = in("x = 9. assert (x == 55) `Error!`"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokAssignment,  .pl2 = 2,             .lenBts = 5 },
                 (Token){ .tp = tokWord, .startBt = 0, .lenBts = 1 },                // x
                 (Token){ .tp = tokInt, .pl2 = 9, .startBt = 4,     .lenBts = 1 },

                 (Token){ .tp = tokAssert, .pl2 = 5, .startBt = 7,  .lenBts = 25 },
                 (Token){ .tp = tokParens, .pl2 = 3, .startBt = 14, .lenBts = 9 },
                 (Token){ .tp = tokOperator, .pl1 = opEquality, .startBt = 15, .lenBts = 2 },
                 (Token){ .tp = tokWord,                  .startBt = 18, .lenBts = 1 },
                 (Token){ .tp = tokInt, .pl2 = 55,  .startBt = 20,  .lenBts = 2 },
                 (Token){ .tp = tokString,               .startBt = 24,  .lenBts = 8 }
         }))},
         (LexerTest) { .name = s("Statement-type core form error"),
             .input = in("/(x (assert foo))"),
             .expectedOutput = buildLexerWithError(s(errCoreNotInsideStmt), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokOperCall, .pl1 = opDivBy },
                 (Token){ .tp = tokWord, .pl2 = 0, .startBt = 2, .lenBts = 1 },                // x
                 (Token){ .tp = tokParens, .startBt = 4 }
         }))},
         (LexerTest) { .name = s("Paren-type core form"),
             .input = in("if(x <> 7 > 0 : true)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .pl1 = slParenMulti, .pl2 = 9, .startBt = 0, .lenBts = 23 },

                 (Token){ .tp = tokStmt, .pl2 = 5, .startBt = 3, .lenBts = 12 },
                 (Token){ .tp = tokOperCall, .pl1 = opGreaterThan, .pl2 = 4, .startBt = 3, .lenBts = 12 },
                 (Token){ .tp = tokOperCall, .pl1 = opComparator, .pl2 = 2, .startBt = 5, .lenBts = 7 },
                 (Token){ .tp = tokWord, .startBt = 8, .lenBts = 1 },                // x
                 (Token){ .tp = tokInt, .pl2 = 7, .startBt = 10, .lenBts = 1 },
                 (Token){ .tp = tokInt, .startBt = 13, .lenBts = 1 },

                 (Token){ .tp = tokColon, .startBt = 16, .lenBts = 1 },
                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 18, .lenBts = 4 },
                 (Token){ .tp = tokBool, .pl2 = 1, .startBt = 18, .lenBts = 4 },
         }))},
         (LexerTest) { .name = s("If with else"),
             .input = in("if(x <> 7 > 0 : true else false)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .pl1 = slParenMulti, .pl2 = 12, .startBt = 0, .lenBts = 34 },

                 (Token){ .tp = tokStmt, .pl2 = 5, .startBt = 3, .lenBts = 12 },
                 (Token){ .tp = tokOperCall, .pl1 = opGreaterThan, .pl2 = 4, .startBt = 3, .lenBts = 12 },
                 (Token){ .tp = tokOperCall, .pl1 = opComparator, .pl2 = 2, .startBt = 5, .lenBts = 7 },
                 (Token){ .tp = tokWord, .startBt = 8, .lenBts = 1 },                // x
                 (Token){ .tp = tokInt, .pl2 = 7, .startBt = 10, .lenBts = 1 },
                 (Token){ .tp = tokInt, .startBt = 13, .lenBts = 1 },
                 (Token){ .tp = tokColon, .startBt = 16, .lenBts = 1 },

                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 18, .lenBts = 4 },
                 (Token){ .tp = tokBool, .pl2 = 1, .startBt = 18, .lenBts = 4 },

                 (Token){ .tp = tokElse,                .startBt = 23, .lenBts = 4 },
                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 28, .lenBts = 5 },
                 (Token){ .tp = tokBool,                .startBt = 28, .lenBts = 5 }
         }))},
        (LexerTest) { .name = s("If with elseif and else"),
            .input = in("if( x <> 7 > 0 : 5\n"
                       "    x <> 7 < 0 : 11\n"
                       "    else true)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokIf, .pl1 = slParenMulti, .pl2 = 21, .startBt = 0, .lenBts = 57 },

                (Token){ .tp = tokStmt, .pl2 = 5, .startBt = 4, .lenBts = 12 },
                (Token){ .tp = tokOperCall, .pl1 = opGreaterThan, .pl2 = 4, .startBt = 4, .lenBts = 12 },
                (Token){ .tp = tokOperCall, .pl1 = opComparator, .pl2 = 2, .startBt = 6, .lenBts = 7 },
                (Token){ .tp = tokWord, .startBt = 9, .lenBts = 1 },                // x
                (Token){ .tp = tokInt, .pl2 = 7, .startBt = 11, .lenBts = 1 },
                (Token){ .tp = tokInt, .startBt = 14, .lenBts = 1 },
                (Token){ .tp = tokColon, .startBt = 17, .lenBts = 1 },

                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 19, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 19, .lenBts = 1 },

                (Token){ .tp = tokStmt, .pl2 = 5, .startBt = 25, .lenBts = 12 },
                (Token){ .tp = tokOperCall, .pl1 = opLessThan, .pl2 = 4, .startBt = 25, .lenBts = 12 },
                (Token){ .tp = tokOperCall, .pl1 = opComparator, .pl2 = 2, .startBt = 27, .lenBts = 7 },
                (Token){ .tp = tokWord, .startBt = 30, .lenBts = 1 },                // x
                (Token){ .tp = tokInt, .pl2 = 7, .startBt = 32, .lenBts = 1 },
                (Token){ .tp = tokInt,                .startBt = 35, .lenBts = 1 },

                (Token){ .tp = tokColon, .startBt = 38, .lenBts = 1 },
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 40, .lenBts = 2 },
                (Token){ .tp = tokInt,  .pl2 = 11,.startBt = 40, .lenBts = 2 },

                (Token){ .tp = tokElse,                .startBt = 47, .lenBts = 4 },
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 52, .lenBts = 4 },
                (Token){ .tp = tokBool, .pl2 = 1, .startBt = 52, .lenBts = 4 }
         }))},
         (LexerTest) { .name = s("Paren-type form error 1"),
             .input = in("if x <> 7 > 0"),
             .expectedOutput = buildLexerWithError(s(errCoreMissingParen), ((Token[]) {0})
         )},
         (LexerTest) { .name = s("Function simple 1"),
             .input = in("foo = f(x Int y Int => Int : x - y)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokAssignment,              .pl2 = 14, .startBt = 0, .lenBts = 36 },
                 (Token){ .tp = tokWord,                               .startBt = 0, .lenBts = 3 },

                 (Token){ .tp = tokFn,  .pl1 = slParenMulti, .pl2 = 12, .startBt = 6, .lenBts = 30 },
                 (Token){ .tp = tokStmt, .pl2 = 6, .startBt = 9, .lenBts = 18 },
                 (Token){ .tp = tokWord, .pl2 = 1, .startBt = 9, .lenBts = 1 }, // x
                 (Token){ .tp = tokTypeName, .pl2 = (strInt + S), .startBt = 11, .lenBts = 3 }, // Int
                 (Token){ .tp = tokWord, .pl2 = 2, .startBt = 15, .lenBts = 1 }, // y
                 (Token){ .tp = tokTypeName, .pl2 = (strInt + S), .startBt = 17, .lenBts = 3 }, // Int
                 (Token){ .tp = tokArrow,                               .startBt = 21, .lenBts = 2 },
                 (Token){ .tp = tokTypeName,        .pl2 = (strInt + S), .startBt = 24, .lenBts = 3 },

                 (Token){ .tp = tokColon, .startBt = 28, .lenBts = 1 },

                 (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 30, .lenBts = 5 },
                 (Token){ .tp = tokWord, .pl2 = 1, .startBt = 30, .lenBts = 1 }, // x
                 (Token){ .tp = tokOperator, .pl1 = opMinus, .startBt = 32, .lenBts = 1 },
                 (Token){ .tp = tokWord, .pl2 = 2, .startBt = 34, .lenBts = 1 } // y
         }))},
         (LexerTest) { .name = s("Function simple error"),
             .input = in("x + f(x Int y Int : x - y)"),
             .expectedOutput = buildLexerWithError(s(errPunctuationScope), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokWord, .startBt = 0, .lenBts = 1 },                // x
                 (Token){ .tp = tokOperator, .pl1 = opPlus, .startBt = 2, .lenBts = 1 }
         }))},

         (LexerTest) { .name = s("Loop simple"),
             .input = in("while(x = 1. x < 101: x >print)"),
             .expectedOutput = buildLexer(((Token[]) {
                 (Token){ .tp = tokWhile, .pl1 = slParenMulti, .pl2 = 11, .lenBts = 32 },

                 (Token){ .tp = tokAssignment, .pl2 = 2, .startBt = 6, .lenBts = 5 },
                 (Token){ .tp = tokWord,                 .startBt = 6, .lenBts = 1 }, // print
                 (Token){ .tp = tokInt,        .pl2 = 1, .startBt = 10, .lenBts = 1 },

                 (Token){ .tp = tokStmt,            .pl2 = 3, .startBt = 13, .lenBts = 8 },
                 (Token){ .tp = tokOperCall, .pl1 = opLessThan, .pl2 = 2, .startBt = 13, .lenBts = 8 },
                 (Token){ .tp = tokWord,                  .startBt = 15, .lenBts = 1 }, // x
                 (Token){ .tp = tokInt,             .pl2 = 101, .startBt = 17, .lenBts = 3 },

                 (Token){ .tp = tokColon,           .startBt = 21, .lenBts = 1 },
                 (Token){ .tp = tokStmt,           .pl2 = 2, .startBt = 23, .lenBts = 8 },
                 (Token){ .tp = tokCall, .pl1 = (strPrint + S), .pl2 = 1, .startBt = 23, .lenBts = 8 }, // print
                 (Token){ .tp = tokWord,                .startBt = 29, .lenBts = 1 }  // x
         }))}
    }));
}


LexerTestSet* typeTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Type forms lexer tests"), a, ((LexerTest[]) {
         (LexerTest) { .name = s("Simple type call"),
             .input = in("Foo(Bar Baz)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokStmt,               .pl2 = 3,               .lenBts = 12 },
                 (Token){ .tp = tokTypeCall, .pl1 = 0, .pl2 = 2, .startBt = 0, .lenBts = 12 },
                 (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 4, .lenBts = 3 },
                 (Token){ .tp = tokTypeName,           .pl2 = 2, .startBt = 8, .lenBts = 3 }
         }))},

         (LexerTest) { .name = s("Generic function signature"),
             .input = in("fn([U V] lst L(U) v V)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokFn, .pl1 = slParenMulti, .pl2 = 9,    .lenBts = 22 },
                 (Token){ .tp = tokStmt,         .pl2 = 8, .startBt = 3, .lenBts = 18 },
                 (Token){ .tp = tokBrackets,     .pl2 = 2, .startBt = 3, .lenBts = 5 },
                 (Token){ .tp = tokTypeName,     .pl2 = 0, .startBt = 4, .lenBts = 1 },
                 (Token){ .tp = tokTypeName,     .pl2 = 1, .startBt = 6, .lenBts = 1 },

                 (Token){ .tp = tokWord,         .pl2 = 2, .startBt = 9,     .lenBts = 3 },
                 (Token){ .tp = tokTypeCall, .pl1 = strL + S, .pl2 = 1, .startBt = 13, .lenBts = 4 },
                 (Token){ .tp = tokTypeName,     .pl2 = 0, .startBt = 15,     .lenBts = 1 },

                 (Token){ .tp = tokWord,            .pl2 = 3, .startBt = 18,     .lenBts = 1 },
                 (Token){ .tp = tokTypeName,     .pl2 = 1, .startBt = 20,     .lenBts = 1 },
         }))},

         (LexerTest) { .name = s("Generic function signature with type funcs"),
             .input = in("fn([U/2 V] lst U(Int V))"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokFn, .pl1 = slParenMulti, .pl2 = 9,        .lenBts = 24 },
                 (Token){ .tp = tokStmt,               .pl2 = 8, .startBt = 3, .lenBts = 20 },
                 (Token){ .tp = tokBrackets,           .pl2 = 3, .startBt = 3, .lenBts = 7 },
                 (Token){ .tp = tokTypeName, .pl1 = 1, .pl2 = 0, .startBt = 4,     .lenBts = 1 },
                 (Token){ .tp = tokInt,                .pl2 = 2, .startBt = 6,     .lenBts = 1 },
                 (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 8,     .lenBts = 1 },

                 (Token){ .tp = tokWord,               .pl2 = 2, .startBt = 11, .lenBts = 3 },

                 (Token){ .tp = tokTypeCall, .pl1 = 0, .pl2 = 2, .startBt = 15, .lenBts = 8 },
                 (Token){ .tp = tokTypeName,           .pl2 = strInt + S, .startBt = 17, .lenBts = 3 },
                 (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 21, .lenBts = 1 }
         }))}
    }));
}

void runATestSet(LexerTestSet* (*testGenerator)(Compiler*, Arena*), int* countPassed, int* countTests,
                 Compiler* proto, Arena* a) {
    LexerTestSet* testSet = (testGenerator)(proto, a);
    for (int j = 0; j < testSet->totalTests; j++) {
        LexerTest test = testSet->tests[j];
        runLexerTest(test, countPassed, countTests, proto, a);
    }
}


int main() {
    printf("----------------------------\n");
    printf("--  LEXER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    Compiler* proto = createProtoCompiler(a);

    int countPassed = 0;
    int countTests = 0;
    runATestSet(&wordTests, &countPassed, &countTests, proto, a);
//~    runATestSet(&stringTests, &countPassed, &countTests, proto, a);
//~    runATestSet(&commentTests, &countPassed, &countTests, proto, a);
//~    runATestSet(&operatorTests, &countPassed, &countTests, proto, a);
//~    runATestSet(&punctuationTests, &countPassed, &countTests, proto, a);
//~    runATestSet(&numericTests, &countPassed, &countTests, proto, a);
//~    runATestSet(&coreFormTests, &countPassed, &countTests, proto, a);
//~    runATestSet(&typeTests, &countPassed, &countTests, proto, a);
    if (countTests == 0) {
        print("\nThere were no tests to run!");
    } else if (countPassed == countTests) {
        print("\nAll %d tests passed!", countTests);
    } else {
        print("\nFailed %d tests out of %d!", (countTests - countPassed), countTests);
    }

    deleteArena(a);
}
