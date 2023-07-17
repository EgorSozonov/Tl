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
    Lexer* expectedOutput;
} LexerTest;


typedef struct {
    String* name;
    int totalTests;
    LexerTest* tests;
} LexerTestSet;


private Lexer* buildLexer0(Arena *a, int totalTokens, Arr(Token) tokens) {
    Lexer* result = createLexer(&empty, NULL, a);
    if (result == NULL) return result;
    
    for (int i = 0; i < totalTokens; i++) {
        Token tok = tokens[i];
        add(tok, result);
    }
    result->totalTokens = totalTokens;
    
    return result;
}

// Macro wrapper to get array length
#define buildLexer(toks) buildLexer0(a, sizeof(toks)/sizeof(Token), toks)


private Lexer* buildLexerWithError0(String* errMsg, Arena *a, int totalTokens, Arr(Token) tokens) {
    Lexer* result = buildLexer0(a, totalTokens, tokens);
    result->wasError = true;
    result->errMsg = errMsg;
    return result;
}

#define buildLexerWithError(msg, toks) buildLexerWithError0(msg, a, sizeof(toks)/sizeof(Token), toks)


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
void runLexerTest(LexerTest test, int* countPassed, int* countTests, LanguageDefinition* lang, Arena *a) {
    (*countTests)++;
    Lexer* result = lexicallyAnalyze(test.input, lang, a);
        
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


LexerTestSet* wordTests(Arena* a) {
    return createTestSet(s("Word lexer test"), a, ((LexerTest[]) {
        (LexerTest) { 
            .name = s("Simple word lexing"),
            .input = s("asdf abc"),
            .expectedOutput = buildLexer(((Token[]){
                    (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                    (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 4 },
                    (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Word snake case"),
            .input = s("asdf_abc"),
            .expectedOutput = buildLexerWithError(s(errWordUnderscoresOnlyAtStart), ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 1"),
            .input = s("asdf-Abc"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8  },
                (Token){ .tp = tokTypeName, .startBt = 0, .lenBts = 8  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 2"),
            .input = s("asdf-abcd-zyui"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 14  },
                (Token){ .tp = tokWord, .startBt = 0, .lenBts = 14  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 3"),
            .input = s("asdf-Abcd"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokTypeName, .startBt = 0, .lenBts = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and lowercase letter"),
            .input = s("_abc"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokWord, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and capital letter"),
            .input = s("_Abc"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokTypeName, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with 2 underscores"),
            .input = s("__abc"),
            .expectedOutput = buildLexerWithError(s(errWordChunkStart), ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and digit error"),
            .input = s("_1abc"),
            .expectedOutput = buildLexerWithError(s(errWordChunkStart), ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Dot word"),
            .input = s("a .func"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokDotWord, .pl2 = 1, .startBt = 2, .lenBts = 5 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with reserved word"),
            .input = s("ifter"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 5 }
            }))
        },
    }));
}

LexerTestSet* numericTests(Arena* a) {
    return createTestSet(s("Numeric lexer test"), a, ((LexerTest[]) {
        (LexerTest) { 
            .name = s("Hex numeric 1"),
            .input = s("0x15"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokInt, .pl2 = 21, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) { 
            .name = s("Hex numeric 2"),
            .input = s("0x05"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 0, .lenBts = 4 }
            }))
        },
        (LexerTest) { 
            .name = s("Hex numeric 3"),
            .input = s("0xFFFFFFFFFFFFFFFF"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)-1 >> 32), .pl2 = ((int64_t)-1 & LOWER32BITS),
                        .startBt = 0, .lenBts = 18  }
            }))
        },
        (LexerTest) { 
            .name = s("Hex numeric 4"),
            .input = s("0xFFFFFFFFFFFFFFFE"),
            .expectedOutput = buildLexer(((Token[]){
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
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(1.234) >> 32,
                                         .pl2 = longOfDoubleBits(1.234) & LOWER32BITS, .startBt = 0, .lenBts = 5 }
            }))
        },
        (LexerTest) { 
            .name = s("Float numeric 2"),
            .input = s("00001.234"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(1.234) >> 32,
                                         .pl2 = longOfDoubleBits(1.234) & LOWER32BITS, .startBt = 0, .lenBts = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 3"),
            .input = s("10500.01"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(10500.01) >> 32,
                                         .pl2 = longOfDoubleBits(10500.01) & LOWER32BITS, .startBt = 0, .lenBts = 8 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 4"),
            .input = s("0.9"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(0.9) >> 32, .pl2 = longOfDoubleBits(0.9) & LOWER32BITS,  
                        .startBt = 0, .lenBts = 3 }                
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 5"),
            .input = s("100500.123456"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 13 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(100500.123456) >> 32,
                         .pl2 = longOfDoubleBits(100500.123456) & LOWER32BITS, .startBt = 0, .lenBts = 13 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric big"),
            .input = s("9007199254740992.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 18 },
                (Token){ .tp = tokFloat, 
                    .pl1 = longOfDoubleBits(9007199254740992.0) >> 32,
                    .pl2 = longOfDoubleBits(9007199254740992.0) & LOWER32BITS, .startBt = 0, .lenBts = 18 }
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
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 24 },
                (Token){ .tp = tokFloat, 
                        .pl1 = longOfDoubleBits(1005001234560000000000.0) >> 32,  
                        .pl2 = longOfDoubleBits(1005001234560000000000.0) & LOWER32BITS, 
                        .startBt = 0, .lenBts = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric tiny"),
            .input = s("0.0000000000000000000003"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 24 },
                (Token){ .tp = tokFloat, 
                        .pl1 = longOfDoubleBits(0.0000000000000000000003) >> 32,  
                        .pl2 = longOfDoubleBits(0.0000000000000000000003) & LOWER32BITS, 
                        .startBt = 0, .lenBts = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 1"),
            .input = s("-9.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(-9.0) >> 32, 
                        .pl2 = longOfDoubleBits(-9.0) & LOWER32BITS, .startBt = 0, .lenBts = 4 }
            }))
        },              
        (LexerTest) {
            .name = s("Float numeric negative 2"),
            .input = s("-8.775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 10 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(-8.775807) >> 32, 
                    .pl2 = longOfDoubleBits(-8.775807) & LOWER32BITS, .startBt = 0, .lenBts = 10 }
            }))
        },       
        (LexerTest) {
            .name = s("Float numeric negative 3"),
            .input = s("-1005001234560000000000.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 25 },
                (Token){ .tp = tokFloat, .pl1 = longOfDoubleBits(-1005001234560000000000.0) >> 32, 
                          .pl2 = longOfDoubleBits(-1005001234560000000000.0) & LOWER32BITS, 
                        .startBt = 0, .lenBts = 25 }
            }))
        },        
        (LexerTest) {
            .name = s("Int numeric 1"),
            .input = s("3"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 3, .startBt = 0, .lenBts = 1 }
            }))
        },                  
        (LexerTest) {
            .name = s("Int numeric 2"),
            .input = s("12"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokInt, .pl2 = 12, .startBt = 0, .lenBts = 2,  }
            }))
        },    
        (LexerTest) {
            .name = s("Int numeric 3"),
            .input = s("0987_12"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokInt, .pl2 = 98712, .startBt = 0, .lenBts = 7 }
            }))
        },    
        (LexerTest) {
            .name = s("Int numeric 4"),
            .input = s("9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 25 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)9223372036854775807 >> 32), 
                        .pl2 = ((int64_t)9223372036854775807 & LOWER32BITS), 
                        .startBt = 0, .lenBts = 25 }
            }))
        },           
        (LexerTest) { 
            .name = s("Int numeric negative 1"),
            .input = s("-1"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)-1 >> 32), .pl2 = ((int64_t)-1 & LOWER32BITS), 
                        .startBt = 0, .lenBts = 2 }
            }))
        },          
        (LexerTest) { 
            .name = s("Int numeric negative 2"),
            .input = s("-775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 8 },
                (Token){ .tp = tokInt, .pl1 = ((int64_t)(-775807) >> 32), .pl2 = ((int64_t)(-775807) & LOWER32BITS), 
                        .startBt = 0, .lenBts = 8 }
            }))
        },   
        (LexerTest) { 
            .name = s("Int numeric negative 3"),
            .input = s("-9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(((Token[]){
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


LexerTestSet* stringTests(Arena* a) {
    return createTestSet(s("String literals lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("String simple literal"),
            .input = s("\"asdfn't\""),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokString, .startBt = 0, .lenBts = 9 }
            }))
        },     
        (LexerTest) { .name = s("String literal with non-ASCII inside"),
            .input = s("\"hello мир\""),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 14 },
                (Token){ .tp = tokString, .startBt = 0, .lenBts = 14 }
        }))},
        (LexerTest) { .name = s("String literal unclosed"),
            .input = s("\"asdf"),
            .expectedOutput = buildLexerWithError(s(errPrematureEndOfInput), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}
    }));
}


LexerTestSet* commentTests(Arena* a) {
    return createTestSet(s("Comments lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Comment simple"),
            .input = s("; this is a comment"),
            .expectedOutput = buildLexer((Token[]){}
        )},
        (LexerTest) { .name = s("Doc comment"),
            .input = s("{ Documentation comment }"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .pl2 = 0, .startBt = 0, .lenBts = 25 }
        }))},
        (LexerTest) { .name = s("Doc comment before something"),
            .input = s("{ Documentation comment { with nesting } }\n"
                       "print \"hw\" "),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .startBt = 0, .lenBts = 42 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 43, .lenBts = 10 },
                (Token){ .tp = tokWord, .startBt = 43, .lenBts = 5 },
                (Token){ .tp = tokString, .startBt = 49, .lenBts = 4 }
        }))},
        (LexerTest) { .name = s("Doc comment empty"),
            .input = s("{} \n" 
                       "print \"hw\" "),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 4, .lenBts = 10 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 4, .lenBts = 5 },
                (Token){ .tp = tokString, .startBt = 10, .lenBts = 4 }
        }))},
        (LexerTest) { .name = s("Doc comment multiline"),
            .input = s("{ First line\n" 
                       " Second line\n" 
                       " Third line }\n" 
                       "print \"hw\" "
            ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .pl2 = 0, .startBt = 0, .lenBts = 39 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 40, .lenBts = 10 },
                (Token){ .tp = tokWord, .startBt = 40, .lenBts = 5 },
                (Token){ .tp = tokString, .startBt = 46, .lenBts = 4 }
        }))}
    }));
}

LexerTestSet* punctuationTests(Arena* a) {
    return createTestSet(s("Punctuation lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Parens simple"),
            .input = s("(car cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 0, .lenBts = 9 },
                (Token){ .tp = tokWord, .pl2 = 0,  .startBt = 1, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
        }))},             
        (LexerTest) { .name = s("Parens nested"),
            .input = s("(car (other car) cdr)"),
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
        (LexerTest) { .name = s("Scope simple"),
            .input = s("(:car cdr)"),
            .expectedOutput = buildLexer(((Token[]){                
                (Token){ .tp = tokScope, .pl2 = 3, .startBt = 0, .lenBts = 10 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 2, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 2, .lenBts = 3 },            
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 6, .lenBts = 3 }            
        }))},              
        (LexerTest) { .name = s("Scopes nested"),
            .input = s("(:car. (:other car) cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokScope, .pl2 = 8, .startBt = 0, .lenBts = 24 },
                (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 2, .lenBts = 3 },                
                (Token){ .tp = tokWord,  .pl2 = 0, .startBt = 2, .lenBts = 3 },  // car
                
                (Token){ .tp = tokScope, .pl2 = 3, .startBt = 7, .lenBts = 12 },
                (Token){ .tp = tokStmt,  .pl2 = 2, .startBt = 9, .lenBts = 9 },                
                (Token){ .tp = tokWord,  .pl2 = 1, .startBt = 9, .lenBts = 5 },  // other
                (Token){ .tp = tokWord,  .pl2 = 0, .startBt = 15, .lenBts = 3 }, // car
                
                (Token){ .tp = tokStmt,  .pl2 = 1, .startBt = 20, .lenBts = 3 },                
                (Token){ .tp = tokWord,  .pl2 = 2, .startBt = 20, .lenBts = 3 }  // cdr
        }))},             
        (LexerTest) { .name = s("Parens inside statement"),
            .input = s("foo bar ( asdf )"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 4, .lenBts = 16 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 },
                (Token){ .tp = tokParens, .pl2 = 1, .startBt = 8, .lenBts = 8 },  
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 10, .lenBts = 4 }   
        }))}, 
        (LexerTest) { .name = s("Multi-line statement"),
            .input = s("foo bar (\n"
                       "asdf\n"
                       "bcj\n"
                       ")"
                      ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 5, .lenBts = 20 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },                 // foo
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 }, // bar
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 8, .lenBts = 12 },                
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 10, .lenBts = 4 }, // asdf                          
                (Token){ .tp = tokWord, .pl2 = 3, .startBt = 15, .lenBts = 3 }  // bcj      
        }))}, 
		(LexerTest) { .name = s("Multiple statements"),
            .input = s("foo bar\n"
                       "asdf.\n"
                       "bcj"
                      ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 2, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .lenBts = 3 },                 // foo
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 3 }, // bar
                      
			    (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 8, .lenBts = 4 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 8, .lenBts = 4 }, // asdf
                
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 14, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 3, .startBt = 14, .lenBts = 3 }  // bcj      
        }))}, 
        (LexerTest) { .name = s("Punctuation all types"),
            .input = s("(:\n"
                       "    asdf (b (d Ef (y z)))\n"
                       "    (:\n"
                       "        bcjk ( m b )))"
            ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokScope, .pl2 = 16,  .startBt = 0,  .lenBts = 58 },
                (Token){ .tp = tokStmt,  .pl2 = 9,   .startBt = 7,  .lenBts = 21 },
                (Token){ .tp = tokWord,  .pl2 = 0,   .startBt = 7,  .lenBts = 4 },   //asdf 
                (Token){ .tp = tokParens, .pl2 = 7, .startBt = 12, .lenBts = 16 },
                (Token){ .tp = tokWord,  .pl2 = 1,   .startBt = 13, .lenBts = 1 },   // b                
                (Token){ .tp = tokParens, .pl2 = 5,  .startBt = 15, .lenBts = 12 },                
                (Token){ .tp = tokWord,  .pl2 = 2,   .startBt = 16, .lenBts = 1 },   // d
                (Token){ .tp = tokTypeName, .pl2 = 3, .startBt = 18, .lenBts = 2 },  // Ef              
                (Token){ .tp = tokParens, .pl2 = 2,  .startBt = 21, .lenBts = 5 },
                (Token){ .tp = tokWord,  .pl2 = 4,   .startBt = 22, .lenBts = 1 },   // y
                (Token){ .tp = tokWord,  .pl2 = 5,   .startBt = 24, .lenBts = 1 },   // z

                (Token){ .tp = tokScope, .pl2 = 5,  .startBt = 33, .lenBts = 24 },
                (Token){ .tp = tokStmt,  .pl2 = 4,  .startBt = 44, .lenBts = 12 },
                (Token){ .tp = tokWord,  .pl2 = 6,  .startBt = 44, .lenBts = 4 },   // bcjk
                (Token){ .tp = tokParens, .pl2 = 2,   .startBt = 49, .lenBts = 7 },
                (Token){ .tp = tokWord,  .pl2 = 7,  .startBt = 51, .lenBts = 1 },   // m  
                (Token){ .tp = tokWord,  .pl2 = 1,  .startBt = 53, .lenBts = 1 }    // b
        }))},
        (LexerTest) { .name = s("Colon punctuation 1"),
            .input = s("Foo : Bar 4"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 4, .startBt = 0, .lenBts = 11 },
                (Token){ .tp = tokTypeName, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokParens, .pl2 = 2, .startBt = 4, .lenBts = 7 },                
                (Token){ .tp = tokTypeName, .pl2 = 1, .startBt = 6, .lenBts = 3 },
                (Token){ .tp = tokInt, .pl2 = 4, .startBt = 10, .lenBts = 1 }
        }))},           
        (LexerTest) { .name = s("Colon punctuation 2"),
            .input = s("ab (arr(foo : bar))"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 7,   .startBt = 0, .lenBts = 19 },
                (Token){ .tp = tokWord,  .pl2 = 0,  .startBt = 0, .lenBts = 2 }, // ab
                (Token){ .tp = tokParens, .pl2 = 5, .startBt = 3, .lenBts = 16 },                
                (Token){ .tp = tokWord,   .pl2 = 1, .startBt = 4, .lenBts = 3 }, // arr
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 7, .lenBts = 11 },
                (Token){ .tp = tokWord,   .pl2 = 2, .startBt = 8, .lenBts = 3 },     // foo
                (Token){ .tp = tokParens, .pl2 = 1, .startBt = 12, .lenBts = 5 },
                (Token){ .tp = tokWord,   .pl2 = 3, .startBt = 14, .lenBts = 3 }   // bar
        }))},
        (LexerTest) { .name = s("Stmt separator"),
            .input = s("foo. bar baz"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 5, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 9, .lenBts = 3 }
        }))},
        (LexerTest) { .name = s("Dot usage error"),
            .input = s("foo (bar. baz)"), 
            .expectedOutput = buildLexerWithError(s(errPunctuationOnlyInMultiline), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokParens, .startBt = 4 },  
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 3 }
        }))}
    }));
}


LexerTestSet* operatorTests(Arena* a) {
    return createTestSet(s("Operator lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Operator simple 1"),
            .input = s("+"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTPlus, .startBt = 0, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator extensible"),
            .input = s("+. -. >>. *. 5 <<. ^."),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 7, .lenBts = 21 },
                (Token){ .tp = tokOperator, .pl1 = opTPlusExt,      .startBt = 0, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opTMinusExt,     .startBt = 3, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opTBitShiftRightExt, .startBt = 6, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opTTimesExt,     .startBt = 10, .lenBts = 2 },
                (Token){ .tp = tokInt,                          .pl2 = 5, .startBt = 13, .lenBts = 1 }, 
                (Token){ .tp = tokOperator, .pl1 = opTBitShiftLeftExt, .startBt = 15, .lenBts = 3 }, 
                (Token){ .tp = tokOperator, .pl1 = opTExponentExt,  .startBt = 19, .lenBts = 2 }
        }))},
        (LexerTest) { .name = s("Operators list"),
            .input = s("+ - / * ^ && || ' ? ++ >=< >< $ , - @"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 16, .startBt = 0, .lenBts = 37 },
                (Token){ .tp = tokOperator, .pl1 = opTPlus, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTMinus, .startBt = 2, .lenBts = 1 },                
                (Token){ .tp = tokOperator, .pl1 = opTDivBy, .startBt = 4, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTTimes, .startBt = 6, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTExponent, .startBt = 8, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTBinaryAnd, .startBt = 10, .lenBts = 2 }, 
                (Token){ .tp = tokOperator, .pl1 = opTBoolOr, .startBt = 13, .lenBts = 2 }, 
                (Token){ .tp = tokOperator, .pl1 = opTIsNull, .startBt = 16, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTQuestionMark, .startBt = 18, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTIncrement, .startBt = 20, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opTIntervalLeft, .startBt = 23, .lenBts = 3 },
                (Token){ .tp = tokOperator, .pl1 = opTIntervalExcl, .startBt = 27, .lenBts = 2 },
                (Token){ .tp = tokOperator, .pl1 = opTToString, .startBt = 30, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTToFloat, .startBt = 32, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTMinus, .startBt = 34, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTAccessor, .startBt = 36, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator expression"),
            .input = s("a - b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 5 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTMinus, .startBt = 2, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 4, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Negation operator"),
            .input = s("a -b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 0, .lenBts = 4 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTNegation, .startBt = 2, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 3, .lenBts = 1 }
        }))},        
        (LexerTest) { .name = s("Operator assignment 1"),
            .input = s("a += b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = opTPlus, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 2"),
            .input = s("a ||= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = opTBoolOr, .pl2 = 2, .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 6, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment 3"),
            .input = s("a *.= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = opTTimesExt, .pl2 = 2, .startBt = 0, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 6, .lenBts = 1 }               
        }))},
        (LexerTest) { .name = s("Operator assignment 4"),
            .input = s("a ^= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = opTExponent, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
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
            .input = s("x +.= (y + 5)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .pl1 = opTPlusExt, .pl2 = 5, .startBt = 0, .lenBts = 13 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 7 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 7, .lenBts = 1 },
                (Token){ .tp = tokOperator, .pl1 = opTPlus, .startBt = 9, .lenBts = 1 },
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
            .input = s("x := y := 7"),
            .expectedOutput = buildLexerWithError(s(errOperatorMultipleAssignment), ((Token[]) {
                (Token){ .tp = tokReassign },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 5, .lenBts = 1 }
        }))},
        (LexerTest) { .name = s("Boolean operators"),
            .input = s("and a : or b c"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .pl2 = 6, .lenBts = 14 },
                (Token){ .tp = tokOperator, .pl1 = opTAnd, .startBt = 0, .lenBts = 3 },
                (Token){ .tp = tokWord, .pl2 = 0, .startBt = 4, .lenBts = 1 },
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 8 },
                (Token){ .tp = tokOperator, .pl1 = opTOr, .startBt = 8, .lenBts = 2 },                
                (Token){ .tp = tokWord, .pl2 = 1, .startBt = 11, .lenBts = 1 },
                (Token){ .tp = tokWord, .pl2 = 2, .startBt = 13, .lenBts = 1 }
        }))}
    }));
}


LexerTestSet* coreFormTests(Arena* a) {
    return createTestSet(s("Core form lexer tests"), a, ((LexerTest[]) {
         (LexerTest) { .name = s("Statement-type core form"),
             .input = s("x = 9. assert (== x 55) \"Error!\""),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokAssignment,  .pl2 = 2,             .lenBts = 5 },
                 (Token){ .tp = tokWord, .startBt = 0, .lenBts = 1 },                // x
                 (Token){ .tp = tokInt, .pl2 = 9, .startBt = 4,     .lenBts = 1 },
                 
                 (Token){ .tp = tokAssert, .pl2 = 5, .startBt = 7,  .lenBts = 25 },
                 (Token){ .tp = tokParens, .pl2 = 3, .startBt = 14, .lenBts = 9 },
                 (Token){ .tp = tokOperator, .pl1 = opTEquality, .startBt = 15, .lenBts = 2 },                 
                 (Token){ .tp = tokWord,                  .startBt = 18, .lenBts = 1 },                
                 (Token){ .tp = tokInt, .pl2 = 55,  .startBt = 20,  .lenBts = 2 },
                 (Token){ .tp = tokString,               .startBt = 24,  .lenBts = 8 }
         }))},
         (LexerTest) { .name = s("Statement-type core form error"),
             .input = s("x/(await foo)"),
             .expectedOutput = buildLexerWithError(s(errCoreNotInsideStmt), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokWord, .startBt = 0, .lenBts = 1 },                // x
                 (Token){ .tp = tokOperator, .pl1 = opTDivBy, .startBt = 1, .lenBts = 1 },
                 (Token){ .tp = tokParens, .startBt = 2 }
         }))},
         (LexerTest) { .name = s("Paren-type core form"),
             .input = s("(if > (<> x 7) 0 => true)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .pl1 = slParenMulti, .pl2 = 10, .startBt = 0, .lenBts = 25 },
                 
                 (Token){ .tp = tokStmt, .pl2 = 6, .startBt = 4, .lenBts = 12 },
                 (Token){ .tp = tokOperator, .pl1 = opTGreaterThan, .startBt = 4, .lenBts = 1 },
                 (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 8 },
                 (Token){ .tp = tokOperator, .pl1 = opTComparator, .startBt = 7, .lenBts = 2 },
                 (Token){ .tp = tokWord, .startBt = 10, .lenBts = 1 },                // x
                 (Token){ .tp = tokInt, .pl2 = 7, .startBt = 12, .lenBts = 1 },                 
                 (Token){ .tp = tokInt, .startBt = 15, .lenBts = 1 },
                 (Token){ .tp = tokArrow, .startBt = 17, .lenBts = 2 },
                 
                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 20, .lenBts = 4 },
                 (Token){ .tp = tokBool, .pl2 = 1, .startBt = 20, .lenBts = 4 },
         }))},
        (LexerTest) { .name = s("Scope-type core form"),
             .input = s("(:i > (<> x 7) 0 => true)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .pl1 = 1, .pl2 = 10, .startBt = 0, .lenBts = 25 },
                 (Token){ .tp = tokStmt, .pl2 = 6, .startBt = 4, .lenBts = 12 },
                 (Token){ .tp = tokOperator, .pl1 = opTGreaterThan, .startBt = 4, .lenBts = 1 },
                 (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 8 },
                 (Token){ .tp = tokOperator, .pl1 = opTComparator, .startBt = 7, .lenBts = 2 },                 
                 (Token){ .tp = tokWord, .startBt = 10, .lenBts = 1 },                // x
                 (Token){ .tp = tokInt, .pl2 = 7, .startBt = 12, .lenBts = 1 },                 
                 (Token){ .tp = tokInt, .startBt = 15, .lenBts = 1 },
                 (Token){ .tp = tokArrow, .startBt = 17, .lenBts = 2 },
                 
                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 20, .lenBts = 4 },
                 (Token){ .tp = tokBool, .pl2 = 1, .startBt = 20, .lenBts = 4 },   
         }))},         
         (LexerTest) { .name = s("If with else"),
             .input = s("(if > (<> x 7) 0 => true else false)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .pl1 = slParenMulti, .pl2 = 13, .startBt = 0, .lenBts = 36 },

                 (Token){ .tp = tokStmt, .pl2 = 6, .startBt = 4, .lenBts = 12 },
                 (Token){ .tp = tokOperator, .pl1 = opTGreaterThan, .startBt = 4, .lenBts = 1 },
                 (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 8 },
                 (Token){ .tp = tokOperator, .pl1 = opTComparator, .startBt = 7, .lenBts = 2 },                 
                 (Token){ .tp = tokWord, .startBt = 10, .lenBts = 1 },                // x
                 (Token){ .tp = tokInt, .pl2 = 7, .startBt = 12, .lenBts = 1 },                 
                 (Token){ .tp = tokInt, .startBt = 15, .lenBts = 1 },
                 (Token){ .tp = tokArrow, .startBt = 17, .lenBts = 2 },

                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 20, .lenBts = 4 },
                 (Token){ .tp = tokBool, .pl2 = 1, .startBt = 20, .lenBts = 4 },

                 (Token){ .tp = tokElse,                .startBt = 25, .lenBts = 4 },
                 (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 30, .lenBts = 5 },
                 (Token){ .tp = tokBool,                .startBt = 30, .lenBts = 5 }
         }))},
        (LexerTest) { .name = s("If with elseif and else"),
            .input = s("(if > (<> x 7) 0 => 5\n"
                       "    < (<> x 7) 0 => 11\n"
                       "    else true)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokIf, .pl1 = slParenMulti, .pl2 = 23, .startBt = 0, .lenBts = 59 },

                (Token){ .tp = tokStmt, .pl2 = 6, .startBt = 4, .lenBts = 12 },
                (Token){ .tp = tokOperator, .pl1 = opTGreaterThan, .startBt = 4, .lenBts = 1 },
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 6, .lenBts = 8 },
                (Token){ .tp = tokOperator, .pl1 = opTComparator, .startBt = 7, .lenBts = 2 },
                (Token){ .tp = tokWord, .startBt = 10, .lenBts = 1 },                // x                
                (Token){ .tp = tokInt, .pl2 = 7, .startBt = 12, .lenBts = 1 },                
                (Token){ .tp = tokInt, .startBt = 15, .lenBts = 1 },
                (Token){ .tp = tokArrow, .startBt = 17, .lenBts = 2 },
                
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 20, .lenBts = 1 },
                (Token){ .tp = tokInt, .pl2 = 5, .startBt = 20, .lenBts = 1 },

                (Token){ .tp = tokStmt, .pl2 = 6, .startBt = 26, .lenBts = 12 },
                (Token){ .tp = tokOperator, .pl1 = opTLessThan, .startBt = 26, .lenBts = 1 },
                (Token){ .tp = tokParens, .pl2 = 3, .startBt = 28, .lenBts = 8 },
                (Token){ .tp = tokOperator, .pl1 = opTComparator, .startBt = 29, .lenBts = 2 },                
                (Token){ .tp = tokWord, .startBt = 32, .lenBts = 1 },                // x                
                (Token){ .tp = tokInt, .pl2 = 7, .startBt = 34, .lenBts = 1 },
                (Token){ .tp = tokInt,                .startBt = 37, .lenBts = 1 },
                (Token){ .tp = tokArrow, .startBt = 39, .lenBts = 2 },

                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 42, .lenBts = 2 },
                (Token){ .tp = tokInt,  .pl2 = 11,.startBt = 42, .lenBts = 2 },
                
                (Token){ .tp = tokElse,                .startBt = 49, .lenBts = 4 },
                (Token){ .tp = tokStmt, .pl2 = 1, .startBt = 54, .lenBts = 4 },
                (Token){ .tp = tokBool, .pl2 = 1, .startBt = 54, .lenBts = 4 }
         }))},
         (LexerTest) { .name = s("Paren-type form error 1"),
             .input = s("if > (<> x 7) 0"),
             .expectedOutput = buildLexerWithError(s(errCoreMissingParen), ((Token[]) {})
         )},
         (LexerTest) { .name = s("Paren-type form error 2"),
             .input = s("(brr if > (<> x 7) 0)"),
             .expectedOutput = buildLexerWithError(s(errCoreNotAtSpanStart), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokParens, },                
                 (Token){ .tp = tokWord, .startBt = 1, .lenBts = 3 }
         }))},
         (LexerTest) { .name = s("Function simple 1"),
             .input = s("(:f foo Int(x Int y Int) = x - y)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokFnDef, .pl1 = 1, .pl2 = 13, .startBt = 0, .lenBts = 33 },
                 
                 (Token){ .tp = tokStmt, .pl2 = 7, .startBt = 4, .lenBts = 20 },
                 (Token){ .tp = tokWord, .pl2 = 0, .startBt = 4, .lenBts = 3 }, // foo
                 (Token){ .tp = tokTypeName, .pl2 = 1, .startBt = 8, .lenBts = 3 }, // Int                 
                 (Token){ .tp = tokParens, .pl2 = 4, .startBt = 11, .lenBts = 13 },
                 (Token){ .tp = tokWord, .pl2 = 2, .startBt = 12, .lenBts = 1 }, // x
                 (Token){ .tp = tokTypeName, .pl2 = 1, .startBt = 14, .lenBts = 3 }, // Int
                 (Token){ .tp = tokWord, .pl2 = 3, .startBt = 18, .lenBts = 1 }, // y
                 (Token){ .tp = tokTypeName, .pl2 = 1, .startBt = 20, .lenBts = 3 }, // Int
                 
                 (Token){ .tp = tokEqualsSign, .startBt = 25, .lenBts = 1 },
                 
                 (Token){ .tp = tokStmt, .pl2 = 3, .startBt = 27, .lenBts = 5 },
                 (Token){ .tp = tokWord, .pl2 = 2, .startBt = 27, .lenBts = 1 }, // x       
                 (Token){ .tp = tokOperator, .pl1 = opTMinus, .startBt = 29, .lenBts = 1 },      
                 (Token){ .tp = tokWord, .pl2 = 3, .startBt = 31, .lenBts = 1 } // y
         }))},
         (LexerTest) { .name = s("Function simple error"),
             .input = s("x + (:f foo Int(x Int y Int) = x - y)"),
             .expectedOutput = buildLexerWithError(s(errPunctuationScope), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokWord, .startBt = 0, .lenBts = 1 },                // x
                 (Token){ .tp = tokOperator, .pl1 = opTPlus, .startBt = 2, .lenBts = 1 }
         }))},
         (LexerTest) { .name = s("Loop simple"),
             .input = s("(:while (< x 101). x = 1. print x)"),
             .expectedOutput = buildLexer(((Token[]) {
                 (Token){ .tp = tokWhile, .pl1 = slScope, .pl2 = 11, .lenBts = 34 },
                 
                 (Token){ .tp = tokStmt, .pl2 = 4, .startBt = 8, .lenBts = 9 },
                 (Token){ .tp = tokParens, .pl2 = 3, .startBt = 8, .lenBts = 9 },
                 (Token){ .tp = tokOperator, .pl1 = opTLessThan, .startBt = 9, .lenBts = 1 },
                 (Token){ .tp = tokWord,                  .startBt = 11, .lenBts = 1 }, // x
                 (Token){ .tp = tokInt, .pl2 = 101, .startBt = 13, .lenBts = 3 }, 
                 
                 (Token){ .tp = tokAssignment, .pl2 = 2, .startBt = 19, .lenBts = 5 },
                 (Token){ .tp = tokWord,                  .startBt = 19, .lenBts = 1 }, // print
                 (Token){ .tp = tokInt, .pl2 = 1, .startBt = 23, .lenBts = 1 }, 

                 (Token){ .tp = tokStmt, .pl2 = 2, .startBt = 26, .lenBts = 7 },
                 (Token){ .tp = tokWord, .pl2 = 1, .startBt = 26, .lenBts = 5 }, // print
                 (Token){ .tp = tokWord,                .startBt = 32, .lenBts = 1 }  // x
         }))}
    }));
}


void runATestSet(LexerTestSet* (*testGenerator)(Arena*), int* countPassed, int* countTests, LanguageDefinition* lang, Arena* a) {
    LexerTestSet* testSet = (testGenerator)(a);
    for (int j = 0; j < testSet->totalTests; j++) {
        LexerTest test = testSet->tests[j];
        runLexerTest(test, countPassed, countTests, lang, a);
    }
}


int main() {
    printf("----------------------------\n");
    printf("--  LEXER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    LanguageDefinition* lang = buildLanguageDefinitions(a);

    int countPassed = 0;
    int countTests = 0;
    runATestSet(&wordTests, &countPassed, &countTests, lang, a);
    runATestSet(&stringTests, &countPassed, &countTests, lang, a);
    runATestSet(&commentTests, &countPassed, &countTests, lang, a);
    runATestSet(&operatorTests, &countPassed, &countTests, lang, a);
    runATestSet(&punctuationTests, &countPassed, &countTests, lang, a);
    runATestSet(&numericTests, &countPassed, &countTests, lang, a);
    runATestSet(&coreFormTests, &countPassed, &countTests, lang, a);

    if (countTests == 0) {
        print("\nThere were no tests to run!");
    } else if (countPassed == countTests) {
        print("\nAll %d tests passed!", countTests);
    } else {
        print("\nFailed %d tests out of %d!", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
