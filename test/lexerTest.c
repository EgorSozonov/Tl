#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/structures/stack.h"
#include "../source/compiler/lexer.h"
#include "../source/compiler/lexerConstants.h"


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


private Lexer* buildLexer0(Arena *a, LanguageDefinition* langDef, int totalTokens, Arr(Token) tokens) {
    Lexer* result = createLexer(&empty, langDef, a);
    if (result == NULL) return result;
    
    result->totalTokens = totalTokens;
        
    for (int i = 0; i < totalTokens; i++) {
        add(tokens[i], result);
    }
    
    return result;
}

// Macro wrapper to get array length
#define buildLexer(toks) buildLexer0(a, NULL, sizeof(toks)/sizeof(Token), toks)


private Lexer* buildLexerWithError0(String* errMsg, Arena *a, int totalTokens, Arr(Token) tokens) {
    Lexer* result = allocateOnArena(sizeof(Lexer), a);
    result->wasError = true;
    result->errMsg = errMsg;
    result->totalTokens = totalTokens;    
    
    result->tokens = allocateOnArena(totalTokens*sizeof(Token), a);
    if (result == NULL) return result;
    
    for (int i = 0; i < totalTokens; i++) {
        result->tokens[i] = tokens[i];
    }
    
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
                    (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                    (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 4 },
                    (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Word snake case"),
            .input = s("asdf_abc"),
            .expectedOutput = buildLexerWithError(s(errorWordUnderscoresOnlyAtStart), ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 1"),
            .input = s("Asdf.abc"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 8  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 2"),
            .input = s("asdf.abcd.zyui"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 14  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 3"),
            .input = s("asdf.Abcd"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt,                .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokWord, .payload1 = 1, .startByte = 0, .lenBytes = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and lowercase letter"),
            .input = s("_abc"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and capital letter"),
            .input = s("_Abc"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt,                .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload1 = 1, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with 2 underscores"),
            .input = s("__abc"),
            .expectedOutput = buildLexerWithError(s(errorWordChunkStart), ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and digit error"),
            .input = s("_1abc"),
            .expectedOutput = buildLexerWithError(s(errorWordChunkStart), ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Atword"),
            .input = s("@a123"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 5 }
            }))
        },
        (LexerTest) {
            .name = s("At-reserved word"),
            .input = s("@if"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Function call"),
            .input = s("a .func"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokFuncWord, .payload2 = 1, .startByte = 2, .lenBytes = 5 }
            }))
        }  
    }));
}

LexerTestSet* numericTests(Arena* a) {
    return createTestSet(s("Numeric lexer test"), a, ((LexerTest[]) {
        (LexerTest) { 
            .name = s("Hex numeric 1"),
            .input = s("0x15"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 21, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) { 
            .name = s("Hex numeric 2"),
            .input = s("0x05"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) { 
            .name = s("Hex numeric 3"),
            .input = s("0xFFFFFFFFFFFFFFFF"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            }))
        },
        (LexerTest) { 
            .name = s("Hex numeric 4"),
            .input = s("0xFFFFFFFFFFFFFFFE"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-2 >> 32), .payload2 = ((int64_t)-2 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            }))
        },
        (LexerTest) { 
            .name = s("Hex numeric too long"),
            .input = s("0xFFFFFFFFFFFFFFFF0"),
            .expectedOutput = buildLexerWithError(s(errorNumericBinWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },  
        (LexerTest) { 
            .name = s("Float numeric 1"),
            .input = s("1.234"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32,
                                         .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, .startByte = 0, .lenBytes = 5 }
            }))
        },
        (LexerTest) { 
            .name = s("Float numeric 2"),
            .input = s("00001.234"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32,
                                         .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, .startByte = 0, .lenBytes = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 3"),
            .input = s("10500.01"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(10500.01) >> 32,
                                         .payload2 = longOfDoubleBits(10500.01) & LOWER32BITS, .startByte = 0, .lenBytes = 8 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 4"),
            .input = s("0.9"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(0.9) >> 32, .payload2 = longOfDoubleBits(0.9) & LOWER32BITS,  
                        .startByte = 0, .lenBytes = 3 }                
            }))
        },
        (LexerTest) {
            .name = s("Float numeric 5"),
            .input = s("100500.123456"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(100500.123456) >> 32,
                         .payload2 = longOfDoubleBits(100500.123456) & LOWER32BITS, .startByte = 0, .lenBytes = 13 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric big"),
            .input = s("9007199254740992.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokFloat, 
                    .payload1 = longOfDoubleBits(9007199254740992.0) >> 32,
                    .payload2 = longOfDoubleBits(9007199254740992.0) & LOWER32BITS, .startByte = 0, .lenBytes = 18 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric too big"),
            .input = s("9007199254740993.0"),
            .expectedOutput = buildLexerWithError(s(errorNumericFloatWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric big exponent"),
            .input = s("1005001234560000000000.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokFloat, 
                        .payload1 = longOfDoubleBits(1005001234560000000000.0) >> 32,  
                        .payload2 = longOfDoubleBits(1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric tiny"),
            .input = s("0.0000000000000000000003"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokFloat, 
                        .payload1 = longOfDoubleBits(0.0000000000000000000003) >> 32,  
                        .payload2 = longOfDoubleBits(0.0000000000000000000003) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            }))
        },
        (LexerTest) {
            .name = s("Float numeric negative 1"),
            .input = s("-9.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-9.0) >> 32, 
                        .payload2 = longOfDoubleBits(-9.0) & LOWER32BITS, .startByte = 0, .lenBytes = 4 }
            }))
        },              
        (LexerTest) {
            .name = s("Float numeric negative 2"),
            .input = s("-8.775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 10 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-8.775807) >> 32, 
                    .payload2 = longOfDoubleBits(-8.775807) & LOWER32BITS, .startByte = 0, .lenBytes = 10 }
            }))
        },       
        (LexerTest) {
            .name = s("Float numeric negative 3"),
            .input = s("-1005001234560000000000.0"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-1005001234560000000000.0) >> 32, 
                          .payload2 = longOfDoubleBits(-1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 25 }
            }))
        },        
        (LexerTest) {
            .name = s("Int numeric 1"),
            .input = s("3"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 3, .startByte = 0, .lenBytes = 1 }
            }))
        },                  
        (LexerTest) {
            .name = s("Int numeric 2"),
            .input = s("12"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 12, .startByte = 0, .lenBytes = 2,  }
            }))
        },    
        (LexerTest) {
            .name = s("Int numeric 3"),
            .input = s("0987_12"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokInt, .payload2 = 98712, .startByte = 0, .lenBytes = 7 }
            }))
        },    
        (LexerTest) {
            .name = s("Int numeric 4"),
            .input = s("9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)9223372036854775807 >> 32), 
                        .payload2 = ((int64_t)9223372036854775807 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 25 }
            }))
        },           
        (LexerTest) { 
            .name = s("Int numeric negative 1"),
            .input = s("-1"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 2 }
            }))
        },          
        (LexerTest) { 
            .name = s("Int numeric negative 2"),
            .input = s("-775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-775807) >> 32), .payload2 = ((int64_t)(-775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 8 }
            }))
        },   
        (LexerTest) { 
            .name = s("Int numeric negative 3"),
            .input = s("-9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 26 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-9223372036854775807) >> 32), 
                        .payload2 = ((int64_t)(-9223372036854775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 26 }
            }))
        },      
        (LexerTest) { 
            .name = s("Int numeric error 1"),
            .input = s("3_"),
            .expectedOutput = buildLexerWithError(s(errorNumericEndUnderscore), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))},       
        (LexerTest) { .name = s("Int numeric error 2"),
            .input = s("9_223_372_036_854_775_808"),
            .expectedOutput = buildLexerWithError(s(errorNumericIntWidthExceeded), ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}                                             
    }));
}


LexerTestSet* stringTests(Arena* a) {
    return createTestSet(s("String literals lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("String simple literal"),
            .input = s("\"asdfn't\""),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 9 }
            }))
        },     
        (LexerTest) { .name = s("String literal with non-ASCII inside"),
            .input = s("\"hello мир\""),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 14 }
        }))},
        (LexerTest) { .name = s("String literal unclosed"),
            .input = s("\"asdf"),
            .expectedOutput = buildLexerWithError(s(errorPrematureEndOfInput), ((Token[]) {
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
            .input = s("(* Documentation comment)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 0, .lenBytes = 25 }
        }))},  
        (LexerTest) { .name = s("Doc comment before something"),
            .input = s("(* Documentation comment)\nprint \"hw\" "),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 26, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 26, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 32, .lenBytes = 4 }
        }))},
        (LexerTest) { .name = s("Doc comment empty"),
            .input = s("(*) \n" 
                       "print \"hw\" "),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 5, .lenBytes = 10 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 5, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 11, .lenBytes = 4 }
        }))},     
        (LexerTest) { .name = s("Doc comment multiline"),
            .input = s("(* First line\n" 
                       " Second line\n" 
                       " Third line)\n" 
                       "print \"hw\" "
            ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 0, .lenBytes = 39 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 40, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 40, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 46, .lenBytes = 4 }
        }))}            
    }));
}

LexerTestSet* punctuationTests(Arena* a) {
    return createTestSet(s("Punctuation lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Parens simple"),
            .input = s("(car cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokWord, .payload2 = 0,  .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
        }))},             
        (LexerTest) { .name = s("Parens nested"),
            .input = s("(car (other car) cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt,   .payload2 = 6, .startByte = 0, .lenBytes = 21 },
                (Token){ .tp = tokParens, .payload2 = 5, .startByte = 0, .lenBytes = 21 },
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 1, .lenBytes = 3 },  // car
                
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 5, .lenBytes = 11 },
                (Token){ .tp = tokWord,   .payload2 = 1, .startByte = 6, .lenBytes = 5 },  // other
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 12, .lenBytes = 3 }, // car
                
                (Token){ .tp = tokWord,   .payload2 = 2, .startByte = 17, .lenBytes = 3 }  // cdr
        }))},
        (LexerTest) { .name = s("Parens unclosed"),
            .input = s("(car (other car) cdr"),
            .expectedOutput = buildLexerWithError(s(errorPunctuationExtraOpening), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 0, .lenBytes = 0 },
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 5, .lenBytes = 11 },
                (Token){ .tp = tokWord,   .payload2 = 1, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 12, .lenBytes = 3 },                
                (Token){ .tp = tokWord,   .payload2 = 2, .startByte = 17, .lenBytes = 3 }            
        }))},
        (LexerTest) { .name = s("Scope simple"),
            .input = s("(-car cdr)"),
            .expectedOutput = buildLexer(((Token[]){                
                (Token){ .tp = tokScope, .payload2 = 3, .startByte = 0, .lenBytes = 10 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 2, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 2, .lenBytes = 3 },            
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 3 }            
        }))},              
        (LexerTest) { .name = s("Scopes nested"),
            .input = s("(-car. (-other car) cdr)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokScope, .payload2 = 8, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokStmt,  .payload2 = 1, .startByte = 2, .lenBytes = 3 },                
                (Token){ .tp = tokWord,  .payload2 = 0, .startByte = 2, .lenBytes = 3 },  // car
                
                (Token){ .tp = tokScope, .payload2 = 3, .startByte = 7, .lenBytes = 12 },
                (Token){ .tp = tokStmt,  .payload2 = 2, .startByte = 9, .lenBytes = 9 },                
                (Token){ .tp = tokWord,  .payload2 = 1, .startByte = 9, .lenBytes = 5 },  // other
                (Token){ .tp = tokWord,  .payload2 = 0, .startByte = 15, .lenBytes = 3 }, // car
                
                (Token){ .tp = tokStmt,  .payload2 = 1, .startByte = 20, .lenBytes = 3 },                
                (Token){ .tp = tokWord,  .payload2 = 2, .startByte = 20, .lenBytes = 3 }  // cdr
        }))},             
        (LexerTest) { .name = s("Data accessor"),
            .input = s("asdf(5)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .lenBytes = 7 },
                (Token){ .tp = tokWord, .lenBytes = 4 },
                (Token){ .tp = tokAccessor, .payload2 = 1, .startByte = 4, .lenBytes = 3 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 5, .lenBytes = 1 }            
        }))},
        (LexerTest) { .name = s("Parens inside statement"),
            .input = s("foo bar ( asdf )"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 4, .lenBytes = 16 },
                (Token){ .tp = tokWord, .payload2 = 0, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 4, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 8, .lenBytes = 8 },  
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 10, .lenBytes = 4 }   
        }))}, 
        (LexerTest) { .name = s("Multi-line statement"),
            .input = s("foo bar (\n"
                       "asdf\n"
                       "bcj\n"
                       ")"
                      ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 5, .lenBytes = 20 },
                (Token){ .tp = tokWord, .payload2 = 0, .lenBytes = 3 },                 // foo
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 4, .lenBytes = 3 }, // bar
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 8, .lenBytes = 12 },                
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 10, .lenBytes = 4 }, // asdf                          
                (Token){ .tp = tokWord, .payload2 = 3, .startByte = 15, .lenBytes = 3 }  // bcj      
        }))}, 
		(LexerTest) { .name = s("Multiple statements"),
            .input = s("foo bar\n"
                       "asdf.\n"
                       "bcj"
                      ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .lenBytes = 3 },                 // foo
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 4, .lenBytes = 3 }, // bar
                      
			    (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 8, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 8, .lenBytes = 4 }, // asdf
                
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 14, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 3, .startByte = 14, .lenBytes = 3 }  // bcj      
        }))}, 
        (LexerTest) { .name = s("Punctuation all types"),
            .input = s("(-\n"
                       "    asdf (b (d Ef (y z)))\n"
                       "    (-\n"
                       "        bcjk (: m b )))"
            ),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokScope, .payload2 = 16,  .startByte = 0,  .lenBytes = 59 },
                (Token){ .tp = tokStmt,  .payload2 = 9,   .startByte = 7,  .lenBytes = 21 },
                (Token){ .tp = tokWord,  .payload2 = 0,   .startByte = 7,  .lenBytes = 4 },     //asdf 
                (Token){ .tp = tokParens,.payload2 = 7,  .startByte = 12, .lenBytes = 16 },
                (Token){ .tp = tokWord,  .payload2 = 1,   .startByte = 13, .lenBytes = 1 },     // b                
                (Token){ .tp = tokParens,.payload2 = 5,   .startByte = 15, .lenBytes = 12 },                
                (Token){ .tp = tokWord,  .payload2 = 2,   .startByte = 16, .lenBytes = 1 },    // d
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 3, .startByte = 18, .lenBytes = 2 },  // Ef              
                (Token){ .tp = tokParens, .payload2 = 2,  .startByte = 21, .lenBytes = 5 },
                (Token){ .tp = tokWord,  .payload2 = 4,   .startByte = 22, .lenBytes = 1 },    // y
                (Token){ .tp = tokWord,  .payload2 = 5,   .startByte = 24, .lenBytes = 1 },    // z

                (Token){ .tp = tokScope, .payload2 = 5,  .startByte = 33, .lenBytes = 25 },
                (Token){ .tp = tokStmt,  .payload2 = 4,  .startByte = 44, .lenBytes = 13 },
                (Token){ .tp = tokWord,  .payload2 = 6,  .startByte = 44, .lenBytes = 4 },    // bcjk
                (Token){ .tp = tokData, .payload2 = 2,   .startByte = 49, .lenBytes = 8 },
                (Token){ .tp = tokWord,  .payload2 = 7,  .startByte = 52, .lenBytes = 1 },    // m  
                (Token){ .tp = tokWord,  .payload2 = 1,  .startByte = 54, .lenBytes = 1 }     // b
        }))},
        (LexerTest) { .name = s("Colon punctuation 1"),
            .input = s("Foo : Bar 4"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 4, .startByte = 0, .lenBytes = 11 },
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 0, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 4, .lenBytes = 7 },                
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 6, .lenBytes = 3 },
                (Token){ .tp = tokInt, .payload2 = 4, .startByte = 10, .lenBytes = 1 }
        }))},           
        (LexerTest) { .name = s("Colon punctuation 2"),
            .input = s("ab (arr(foo : bar))"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 7,   .startByte = 0, .lenBytes = 19 },
                (Token){ .tp = tokWord,  .payload2 = 0,  .startByte = 0, .lenBytes = 2 }, // ab
                (Token){ .tp = tokParens, .payload2 = 5, .startByte = 3, .lenBytes = 16 },                
                (Token){ .tp = tokWord,   .payload2 = 1, .startByte = 4, .lenBytes = 3 }, // arr
                (Token){ .tp = tokAccessor, .payload2 = 3, .startByte = 7, .lenBytes = 11 },
                (Token){ .tp = tokWord,   .payload2 = 2, .startByte = 8, .lenBytes = 3 },     // foo
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 12, .lenBytes = 5 },
                (Token){ .tp = tokWord,   .payload2 = 3, .startByte = 14, .lenBytes = 3 }   // bar
        }))},
        (LexerTest) { .name = s("Stmt separator"),
            .input = s("foo. bar baz"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 5, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 9, .lenBytes = 3 }
        }))},
        (LexerTest) { .name = s("Dot usage error"),
            .input = s("foo (bar. baz)"), 
            .expectedOutput = buildLexerWithError(s(errorPunctuationOnlyInMultiline), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokParens, .startByte = 4 },  
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
        }))}
    }));
}


LexerTestSet* operatorTests(Arena* a) {
    return createTestSet(s("Operator lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Operator simple 1"),
            .input = s("+"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = opTPlus << 1, .startByte = 0, .lenBytes = 1 }
        }))},             
        (LexerTest) { .name = s("Operator extensible"),
            .input = s("+. -. >>. %. *. 5 <<. ^."),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 8, .lenBytes = 24 },
                (Token){ .tp = tokOperator, .payload1 = 1 + (opTPlus << 1), .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 1 + (opTMinus << 1), .startByte = 3, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 1 + (opTBitshiftRight << 1), .startByte = 6, .lenBytes = 3 },
                (Token){ .tp = tokOperator, .payload1 = 1 + (opTRemainder << 1), .startByte = 10, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 1 + (opTTimes << 1), .startByte = 13, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 16, .lenBytes = 1 }, 
                (Token){ .tp = tokOperator, .payload1 = 1 + (opTBitShiftLeft << 1), .startByte = 18, .lenBytes = 3 }, 
                (Token){ .tp = tokOperator, .payload1 = 1 + (opTExponent << 1), .startByte = 22, .lenBytes = 2 }
        }))},
        (LexerTest) { .name = s("Operators list"),
            .input = s("+ - / * ^ && || ' ? ++ >=< >< $"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 13, .startByte = 0, .lenBytes = 31 },
                (Token){ .tp = tokOperator, .payload1 = (opTPlus << 1), .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 1), .startByte = 2, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTDivBy << 1), .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTTimes << 1), .startByte = 6, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTExponent << 1), .startByte = 8, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTBinaryAnd << 1), .startByte = 10, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTBoolOr << 1), .startByte = 13, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTIsNull << 1), .startByte = 16, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTQuestionMark << 1), .startByte = 18, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTIncrement << 1), .startByte = 20, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalLeft << 1), .startByte = 23, .lenBytes = 3 },
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalExcl << 1), .startByte = 27, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = (opTToString << 1), .startByte = 30, .lenBytes = 1 }                   
        }))},
        (LexerTest) { .name = s("Operator expression"),
            .input = s("a - b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 1), .startByte = 2, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 4, .lenBytes = 1 }                
        }))},              
        (LexerTest) { .name = s("Operator assignment 1"),
            .input = s("a += b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .payload1 = (opTPlus << 1), .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 1 }    
        }))},             
        (LexerTest) { .name = s("Operator assignment 2"),
            .input = s("a ||= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .payload1 = (opTBoolOr << 1), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 1 }    
        }))},
        (LexerTest) { .name = s("Operator assignment 3"),
            .input = s("a *.= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .payload1 = 1 + (opTTimes << 1), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 1 }               
        }))},
        (LexerTest) { .name = s("Operator assignment 4"),
            .input = s("a ^= b"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .payload1 = opTExponent << 1, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 1 }            
        }))}, 
        (LexerTest) { .name = s("Operator assignment in parens error"),
            .input = s("(x += y + 5)"),
            .expectedOutput = buildLexerWithError(s(errorOperatorAssignmentPunct), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 1 }
        }))},             
        (LexerTest) { .name = s("Operator assignment with parens"),
            .input = s("x +.= (y + 5)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokMutation, .payload1 = 1 + (opTPlus << 1), .payload2 = 5, .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 6, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 7, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = opTPlus << 1, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 11, .lenBytes = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment in parens error"),
            .input = s("x (+= y) + 5"),
            .expectedOutput = buildLexerWithError(s(errorOperatorAssignmentPunct), ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .startByte = 2 }
        }))},
        (LexerTest) { .name = s("Operator assignment multiple error"),
            .input = s("x := y := 7"),
            .expectedOutput = buildLexerWithError(s(errorOperatorMultipleAssignment), ((Token[]) {
                (Token){ .tp = tokReassign },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 1 }
        }))},
        (LexerTest) { .name = s("Boolean operators"),
            .input = s("and a : or b c"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 6, .lenBytes = 14 },
                (Token){ .tp = tokAnd, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 6, .lenBytes = 8 },
                (Token){ .tp = tokOr,                  .startByte = 8, .lenBytes = 2 },                
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 11, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 13, .lenBytes = 1 }
        }))}
    }));
}


LexerTestSet* coreFormTests(Arena* a) {
    return createTestSet(s("Core form lexer tests"), a, ((LexerTest[]) {
         (LexerTest) { .name = s("Statement-type core form"),
             .input = s("x = 9. assert (== x 55) \"Error!\""),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokAssignment,  .payload2 = 2,             .lenBytes = 5 },
                 (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                 (Token){ .tp = tokInt, .payload2 = 9, .startByte = 4,     .lenBytes = 1 },
                 
                 (Token){ .tp = tokAssert, .payload2 = 5, .startByte = 7,  .lenBytes = 25 },
                 (Token){ .tp = tokParens, .payload2 = 3, .startByte = 14, .lenBytes = 9 },
                 (Token){ .tp = tokOperator, .payload1 = (opTEquality << 1), .startByte = 15, .lenBytes = 2 },                 
                 (Token){ .tp = tokWord,                  .startByte = 18, .lenBytes = 1 },                
                 (Token){ .tp = tokInt, .payload2 = 55,  .startByte = 20,  .lenBytes = 2 },
                 (Token){ .tp = tokString,               .startByte = 24,  .lenBytes = 8 }
         }))},
         (LexerTest) { .name = s("Statement-type core form error"),
             .input = s("x/(await foo)"),
             .expectedOutput = buildLexerWithError(s(errorCoreNotInsideStmt), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                 (Token){ .tp = tokOperator, .payload1 = (opTDivBy << 1), .startByte = 1, .lenBytes = 1 },
                 (Token){ .tp = tokParens, .startByte = 2 }
         }))},
         (LexerTest) { .name = s("Paren-type core form"),
             .input = s("(if > (<> x 7) 0 => true)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .payload2 = 10, .startByte = 0, .lenBytes = 25 },
                 
                 (Token){ .tp = tokStmt, .payload2 = 6, .startByte = 4, .lenBytes = 12 },
                 (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 1), .startByte = 4, .lenBytes = 1 },
                 (Token){ .tp = tokParens, .payload2 = 3, .startByte = 6, .lenBytes = 8 },
                 (Token){ .tp = tokOperator, .payload1 = (opTComparator << 1), .startByte = 7, .lenBytes = 2 },
                 (Token){ .tp = tokWord, .startByte = 10, .lenBytes = 1 },                // x
                 (Token){ .tp = tokInt, .payload2 = 7, .startByte = 12, .lenBytes = 1 },                 
                 (Token){ .tp = tokInt, .startByte = 15, .lenBytes = 1 },
                 (Token){ .tp = tokArrow, .startByte = 17, .lenBytes = 2 },
                 
                 (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 20, .lenBytes = 4 },
                 (Token){ .tp = tokBool, .payload2 = 1, .startByte = 20, .lenBytes = 4 },
         }))},
        (LexerTest) { .name = s("Scope-type core form"),
             .input = s("(-i > (<> x 7) 0 => true)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .payload2 = 10, .startByte = 0, .lenBytes = 25 },
                 (Token){ .tp = tokStmt, .payload2 = 6, .startByte = 4, .lenBytes = 12 },
                 (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 1), .startByte = 4, .lenBytes = 1 },
                 (Token){ .tp = tokParens, .payload2 = 3, .startByte = 6, .lenBytes = 8 },
                 (Token){ .tp = tokOperator, .payload1 = (opTComparator << 1), .startByte = 7, .lenBytes = 2 },                 
                 (Token){ .tp = tokWord, .startByte = 10, .lenBytes = 1 },                // x
                 (Token){ .tp = tokInt, .payload2 = 7, .startByte = 12, .lenBytes = 1 },                 
                 (Token){ .tp = tokInt, .startByte = 15, .lenBytes = 1 },
                 (Token){ .tp = tokArrow, .startByte = 17, .lenBytes = 2 },
                 
                 (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 20, .lenBytes = 4 },
                 (Token){ .tp = tokBool, .payload2 = 1, .startByte = 20, .lenBytes = 4 },   
         }))},         
         (LexerTest) { .name = s("If with else"),
             .input = s("(if > (<> x 7) 0 => true else false)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokIf, .payload2 = 13, .startByte = 0, .lenBytes = 36 },

                 (Token){ .tp = tokStmt, .payload2 = 6, .startByte = 4, .lenBytes = 12 },
                 (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 1), .startByte = 4, .lenBytes = 1 },
                 (Token){ .tp = tokParens, .payload2 = 3, .startByte = 6, .lenBytes = 8 },
                 (Token){ .tp = tokOperator, .payload1 = (opTComparator << 1), .startByte = 7, .lenBytes = 2 },                 
                 (Token){ .tp = tokWord, .startByte = 10, .lenBytes = 1 },                // x
                 (Token){ .tp = tokInt, .payload2 = 7, .startByte = 12, .lenBytes = 1 },                 
                 (Token){ .tp = tokInt, .startByte = 15, .lenBytes = 1 },
                 (Token){ .tp = tokArrow, .startByte = 17, .lenBytes = 2 },

                 (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 20, .lenBytes = 4 },
                 (Token){ .tp = tokBool, .payload2 = 1, .startByte = 20, .lenBytes = 4 },

                 (Token){ .tp = tokElse,                .startByte = 25, .lenBytes = 4 },
                 (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 30, .lenBytes = 5 },
                 (Token){ .tp = tokBool,                .startByte = 30, .lenBytes = 5 }
         }))},
        (LexerTest) { .name = s("If with elseif and else"),
            .input = s("(if > (<> x 7) 0 => 5\n"
                       "    < (<> x 7) 0 => 11\n"
                       "    else true)"),
            .expectedOutput = buildLexer(((Token[]){
                (Token){ .tp = tokIf, .payload2 = 23, .startByte = 0, .lenBytes = 59 },

                (Token){ .tp = tokStmt, .payload2 = 6, .startByte = 4, .lenBytes = 12 },
                (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 1), .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 6, .lenBytes = 8 },
                (Token){ .tp = tokOperator, .payload1 = (opTComparator << 1), .startByte = 7, .lenBytes = 2 },
                (Token){ .tp = tokWord, .startByte = 10, .lenBytes = 1 },                // x                
                (Token){ .tp = tokInt, .payload2 = 7, .startByte = 12, .lenBytes = 1 },                
                (Token){ .tp = tokInt, .startByte = 15, .lenBytes = 1 },
                (Token){ .tp = tokArrow, .startByte = 17, .lenBytes = 2 },
                
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 20, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 20, .lenBytes = 1 },

                (Token){ .tp = tokStmt, .payload2 = 6, .startByte = 26, .lenBytes = 12 },
                (Token){ .tp = tokOperator, .payload1 = (opTLessThan << 1), .startByte = 26, .lenBytes = 1 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 28, .lenBytes = 8 },
                (Token){ .tp = tokOperator, .payload1 = (opTComparator << 1), .startByte = 29, .lenBytes = 2 },                
                (Token){ .tp = tokWord, .startByte = 32, .lenBytes = 1 },                // x                
                (Token){ .tp = tokInt, .payload2 = 7, .startByte = 34, .lenBytes = 1 },
                (Token){ .tp = tokInt,                .startByte = 37, .lenBytes = 1 },
                (Token){ .tp = tokArrow, .startByte = 39, .lenBytes = 2 },

                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 42, .lenBytes = 2 },
                (Token){ .tp = tokInt,  .payload2 = 11,.startByte = 42, .lenBytes = 2 },
                
                (Token){ .tp = tokElse,                .startByte = 49, .lenBytes = 4 },
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 54, .lenBytes = 4 },
                (Token){ .tp = tokBool, .payload2 = 1, .startByte = 54, .lenBytes = 4 }
        }))},
         (LexerTest) { .name = s("Paren-type form error 1"),
             .input = s("if > (<> x 7) 0"),
             .expectedOutput = buildLexerWithError(s(errorCoreMissingParen), ((Token[]) {})
         )},
         (LexerTest) { .name = s("Paren-type form error 2"),
             .input = s("(brr if > (<> x 7) 0)"),
             .expectedOutput = buildLexerWithError(s(errorCoreNotAtSpanStart), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokParens, },                
                 (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 }
         }))},
         (LexerTest) { .name = s("Function simple 1"),
             .input = s("(-f (foo Int : x Int y Int). x - y)"),
             .expectedOutput = buildLexer(((Token[]){
                 (Token){ .tp = tokFnDef, .payload2 = 13, .startByte = 0, .lenBytes = 35 },
                 
                 (Token){ .tp = tokStmt, .payload2 = 8, .startByte = 4, .lenBytes = 23 },
                 (Token){ .tp = tokParens, .payload2 = 7, .startByte = 4, .lenBytes = 23 },
                 (Token){ .tp = tokWord, .payload2 = 0, .startByte = 5, .lenBytes = 3 },                // foo
                 (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 9, .lenBytes = 3 }, // Int
                 
                 (Token){ .tp = tokParens, .payload2 = 4, .startByte = 13, .lenBytes = 13 },
                 (Token){ .tp = tokWord, .payload2 = 2, .startByte = 15, .lenBytes = 1 }, // x
                 (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 17, .lenBytes = 3 }, // Int
                 (Token){ .tp = tokWord, .payload2 = 3, .startByte = 21, .lenBytes = 1 }, // y
                 (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 23, .lenBytes = 3 }, // Int
                 
                 (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 29, .lenBytes = 5 },
                 (Token){ .tp = tokWord, .payload2 = 2, .startByte = 29, .lenBytes = 1 },                
                 (Token){ .tp = tokOperator, .payload1 = (opTMinus << 1), .startByte = 31, .lenBytes = 1 },                
                 (Token){ .tp = tokWord, .payload2 = 3, .startByte = 33, .lenBytes = 1 } // y
         }))},
         (LexerTest) { .name = s("Function simple error"),
             .input = s("x + (-f (foo Int x Int y Int) x - y)"),
             .expectedOutput = buildLexerWithError(s(errorPunctuationScope), ((Token[]) {
                 (Token){ .tp = tokStmt },
                 (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                 (Token){ .tp = tokOperator, .payload1 = (opTPlus << 1), .startByte = 2, .lenBytes = 1 }
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
