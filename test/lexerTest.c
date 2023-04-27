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

/** Must agree in order with token types in LexerConstants.h */
const char* tokNames[] = {
    "Int", "Float", "Bool", "String", "_", "DocComment", 
    "word", ".word", "@word", ",func", "operator", "and", "or", "dispose", ":",  "mutTemp",
    "(:", ".", "()", "[]", "accessor[]", "funcExpr", "assign", ":=", "mutation", "else", ";",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue", 
    "defer", "embed", "export", "exposePriv", "fn", "interface", 
    "lambda", "lam1", "lam2", "lam3", "package", "return", "struct", "try", "yield",
    "if", "ifEq", "ifPr", "impl", "match", "loop", "mut"
};


#define s(lit) str(lit, a)


private Lexer* buildLexer0(Arena *a, int totalTokens, Arr(Token) tokens) {
    Lexer* result = createLexer(&empty, a);
    if (result == NULL) return result;
    
    result->totalTokens = totalTokens;
        
    for (int i = 0; i < totalTokens; i++) {
        add(tokens[i], result);
    }
    
    return result;
}

// Macro wrapper to get array length
#define buildLexer(a, toks) buildLexer0(a, sizeof(toks)/sizeof(Token), toks)


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

#define buildLexerWithError(msg, a, toks) buildLexerWithError0(msg, a, sizeof(toks)/sizeof(Token), toks)


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

/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
int equalityLexer(Lexer a, Lexer b) {
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.totalTokens < b.totalTokens ? a.totalTokens : b.totalTokens;
    int i = 0;
    for (; i < commonLength; i++) {
        Token tokA = a.tokens[i];
        Token tokB = b.tokens[i];
        if (tokA.tp != tokB.tp || tokA.lenBytes != tokB.lenBytes || tokA.startByte != tokB.startByte 
            || tokA.payload1 != tokB.payload1 || tokA.payload2 != tokB.payload2) {
            printf("\n\nUNEQUAL RESULTS\n", i);
            if (tokA.tp != tokB.tp) {
                printf("Diff in tp, %s but was expected %s\n", tokNames[tokA.tp], tokNames[tokB.tp]);
            }
            if (tokA.lenBytes != tokB.lenBytes) {
                printf("Diff in lenBytes, %d but was expected %d\n", tokA.lenBytes, tokB.lenBytes);
            }
            if (tokA.startByte != tokB.startByte) {
                printf("Diff in startByte, %d but was expected %d\n", tokA.startByte, tokB.startByte);
            }
            if (tokA.payload1 != tokB.payload1) {
                printf("Diff in payload1, %d but was expected %d\n", tokA.payload1, tokB.payload1);
            }
            if (tokA.payload2 != tokB.payload2) {
                printf("Diff in payload2, %d but was expected %d\n", tokA.payload2, tokB.payload2);
            }            
            return i;
        }
    }
    return (a.totalTokens == b.totalTokens) ? -2 : i;        
}


void printLexer(Lexer* a) {
    if (a->wasError) {
        printf("Error: ");
        printString(a->errMsg);
    }
    for (int i = 0; i < a->totalTokens; i++) {
        Token tok = a->tokens[i];
        if (tok.payload1 != 0 || tok.payload2 != 0) {
            printf("%s %d %d [%d; %d]\n", tokNames[tok.tp], tok.payload1, tok.payload2, tok.startByte, tok.lenBytes);
        } else {
            printf("%s [%d; %d]\n", tokNames[tok.tp], tok.startByte, tok.lenBytes);
        }
        
    }
}

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
            .expectedOutput = buildLexer(a, ((Token[]){
                    (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                    (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 4 },
                    (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Word snake case"),
            .input = s("asdf_abc"),
            .expectedOutput = buildLexerWithError(s(errorWordUnderscoresOnlyAtStart), a, ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 1"),
            .input = s("Asdf.abc"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 8  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 2"),
            .input = s("asdf.abcd.zyui"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 14  }
            }))
        },
        (LexerTest) {
            .name = s("Word correct capitalization 3"),
            .input = s("asdf.Abcd"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt,                .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokWord, .payload1 = 1, .startByte = 0, .lenBytes = 9 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and lowercase letter"),
            .input = s("_abc"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and capital letter"),
            .input = s("_Abc"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt,                .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload1 = 1, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) {
            .name = s("Word starts with 2 underscores"),
            .input = s("__abc"),
            .expectedOutput = buildLexerWithError(str(errorWordChunkStart, a), a, ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Word starts with underscore and digit error"),
            .input = s("_1abc"),
            .expectedOutput = buildLexerWithError(str(errorWordChunkStart, a), a, ((Token[]){}))
        },
        (LexerTest) {
            .name = s("Dotword"),
            .input = s("@a123"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 5 }
            }))
        },
        (LexerTest) {
            .name = s("At-reserved word"),
            .input = s("@if"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 3 }
            }))
        },
        (LexerTest) {
            .name = s("Function call"),
            .input = s("a ,func"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokFuncWord, .payload2 = 1, .startByte = 2, .lenBytes = 5 }
            }))
        },
        (LexerTest) {
            .name = s("Function call with no space"),
            .input = s("b,func"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokFuncWord, .payload2 = 1, .startByte = 1, .lenBytes = 5 }
            }))
        }    
    }));
}

LexerTestSet* numericTests(Arena* a) {
    return createTestSet(str("Numeric lexer test", a), a, ((LexerTest[]) {
        (LexerTest) { 
            .name = str("Hex numeric 1", a),
            .input = str("0x15", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 21, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) { 
            .name = str("Hex numeric 2", a),
            .input = str("0x05", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) { 
            .name = str("Hex numeric 3", a),
            .input = str("0xFFFFFFFFFFFFFFFF", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            }))
        },
        (LexerTest) { 
            .name = str("Hex numeric 4", a),
            .input = str("0xFFFFFFFFFFFFFFFE", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-2 >> 32), .payload2 = ((int64_t)-2 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            }))
        },
        (LexerTest) { 
            .name = str("Hex numeric too long", a),
            .input = str("0xFFFFFFFFFFFFFFFF0", a),
            .expectedOutput = buildLexerWithError(str(errorNumericBinWidthExceeded, a), a, ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },  
        (LexerTest) { 
            .name = str("Float numeric 1", a),
            .input = str("1.234", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32, .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, 
                            .startByte = 0, .lenBytes = 5 }
            }))
        },
        (LexerTest) { 
            .name = str("Float numeric 2", a),
            .input = str("00001.234", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32, .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, 
                            .startByte = 0, .lenBytes = 9 }
            }))
        },
        (LexerTest) {
            .name = str("Float numeric 3", a),
            .input = str("10500.01", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(10500.01) >> 32, .payload2 = longOfDoubleBits(10500.01) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 8 }
            }))
        },
        (LexerTest) {
            .name = str("Float numeric 4", a),
            .input = str("0.9", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(0.9) >> 32, .payload2 = longOfDoubleBits(0.9) & LOWER32BITS,  
                        .startByte = 0, .lenBytes = 3 }                
            }))
        },
        (LexerTest) {
            .name = str("Float numeric 5", a),
            .input = str("100500.123456", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(100500.123456) >> 32, .payload2 = longOfDoubleBits(100500.123456) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 13 }
            }))
        },
        (LexerTest) {
            .name = str("Float numeric big", a),
            .input = str("9007199254740992.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokFloat, 
                    .payload1 = longOfDoubleBits(9007199254740992.0) >> 32, .payload2 = longOfDoubleBits(9007199254740992.0) & LOWER32BITS,
                    .startByte = 0, .lenBytes = 18 }
            }))
        },
        (LexerTest) {
            .name = str("Float numeric too big", a),
            .input = str("9007199254740993.0", a),
            .expectedOutput = buildLexerWithError(str(errorNumericFloatWidthExceeded, a), a, ((Token[]) {
                (Token){ .tp = tokStmt }
            }))
        },
        (LexerTest) {
            .name = str("Float numeric big exponent", a),
            .input = str("1005001234560000000000.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokFloat, 
                        .payload1 = longOfDoubleBits(1005001234560000000000.0) >> 32,  
                        .payload2 = longOfDoubleBits(1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            }))
        },
        (LexerTest) {
            .name = str("Float numeric tiny", a),
            .input = str("0.0000000000000000000003", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokFloat, 
                        .payload1 = longOfDoubleBits(0.0000000000000000000003) >> 32,  
                        .payload2 = longOfDoubleBits(0.0000000000000000000003) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            }))
        },
        (LexerTest) {
            .name = str("Float numeric negative 1", a),
            .input = str("-9.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-9.0) >> 32, 
                        .payload2 = longOfDoubleBits(-9.0) & LOWER32BITS, .startByte = 0, .lenBytes = 4 }
            }))
        },              
        (LexerTest) {
            .name = str("Float numeric negative 2", a),
            .input = str("-8.775_807", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 10 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-8.775807) >> 32, 
                    .payload2 = longOfDoubleBits(-8.775807) & LOWER32BITS, .startByte = 0, .lenBytes = 10 }
            }))
        },       
        (LexerTest) {
            .name = str("Float numeric negative 3", a),
            .input = str("-1005001234560000000000.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-1005001234560000000000.0) >> 32, 
                          .payload2 = longOfDoubleBits(-1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 25 }
            }))
        },        
        (LexerTest) {
            .name = str("Int numeric 1", a),
            .input = str("3", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 3, .startByte = 0, .lenBytes = 1 }
            }))
        },                  
        (LexerTest) {
            .name = str("Int numeric 2", a),
            .input = str("12", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 12, .startByte = 0, .lenBytes = 2,  }
            }))
        },    
        (LexerTest) {
            .name = str("Int numeric 3", a),
            .input = str("0987_12", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokInt, .payload2 = 98712, .startByte = 0, .lenBytes = 7 }
            }))
        },    
        (LexerTest) {
            .name = str("Int numeric 4", a),
            .input = str("9_223_372_036_854_775_807", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)9223372036854775807 >> 32), 
                        .payload2 = ((int64_t)9223372036854775807 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 25 }
            }))
        },           
        (LexerTest) { 
            .name = s("Int numeric negative 1"),
            .input = s("-1"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 2 }
            }))
        },          
        (LexerTest) { 
            .name = s("Int numeric negative 2"),
            .input = s("-775_807"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-775807) >> 32), .payload2 = ((int64_t)(-775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 8 }
            }))
        },   
        (LexerTest) { 
            .name = s("Int numeric negative 3"),
            .input = s("-9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 26 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-9223372036854775807) >> 32), 
                        .payload2 = ((int64_t)(-9223372036854775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 26 }
            }))
        },      
        (LexerTest) { 
            .name = s("Int numeric error 1"),
            .input = s("3_"),
            .expectedOutput = buildLexerWithError(str(errorNumericEndUnderscore, a), a, ((Token[]) {
                (Token){ .tp = tokStmt }
        }))},       
        (LexerTest) { .name = s("Int numeric error 2"),
            .input = s("9_223_372_036_854_775_808"),
            .expectedOutput = buildLexerWithError(str(errorNumericIntWidthExceeded, a), a, ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}                                             
    }));
}


LexerTestSet* stringTests(Arena* a) {
    return createTestSet(s("String literals lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("String simple literal"),
            .input = s("\"asdfn't\""),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 9 }
            }))
        },     
        (LexerTest) { .name = s("String literal with non-ASCII inside"),
            .input = s("\"hello мир\""),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 14 }
        }))},
        (LexerTest) { .name = s("String literal unclosed"),
            .input = s("\"asdf"),
            .expectedOutput = buildLexerWithError(str(errorPrematureEndOfInput, a), a, ((Token[]) {
                (Token){ .tp = tokStmt }
        }))}
    }));
}


LexerTestSet* commentTests(Arena* a) {
    return createTestSet(s("Comments lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Comment simple"),
            .input = s("--this is a comment"),
            .expectedOutput = buildLexer(a, (Token[]){}
        )},     
        (LexerTest) { .name = s("Doc comment"),
            .input = s("---Documentation comment "),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 3, .lenBytes = 22 }
        }))},  
        (LexerTest) { .name = s("Doc comment before something"),
            .input = s("---Documentation comment\nprint \"hw\" "),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokDocComment, .startByte = 3, .lenBytes = 21 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 25, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 25, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 31, .lenBytes = 4 }
        }))},
        (LexerTest) { .name = s("Doc comment empty"),
            .input = s("---\n" "print \"hw\" "),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 4, .lenBytes = 10 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 4, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 10, .lenBytes = 4 }
        }))},     
        (LexerTest) { .name = s("Doc comment multiline"),
            .input = s("---First line\n" 
                      "---Second line\n" 
                      "---Third line\n" 
                      "print \"hw\" "
            ),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 3, .lenBytes = 39 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 43, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 43, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 49, .lenBytes = 4 }
        }))}            
    }));
}

LexerTestSet* punctuationTests(Arena* a) {
    return createTestSet(s("Punctuation lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Parens simple"),
            .input = s("(car cdr)"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 1, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0,  .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
        }))},             
        (LexerTest) { .name = s("Parens nested"),
            .input = s("(car (other car) cdr)"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt,   .payload2 = 6, .startByte = 0, .lenBytes = 21 },
                (Token){ .tp = tokParens, .payload2 = 5, .startByte = 1, .lenBytes = 19 },
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord,   .payload2 = 1, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 12, .lenBytes = 3 },                
                (Token){ .tp = tokWord,   .payload2 = 2, .startByte = 17, .lenBytes = 3 }
        }))},
        (LexerTest) { .name = s("Parens unclosed"),
            .input = s("(car (other car) cdr"),
            .expectedOutput = buildLexerWithError(str(errorPunctuationExtraOpening, a), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1, .lenBytes = 0 },
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord,   .payload2 = 1, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord,   .payload2 = 0, .startByte = 12, .lenBytes = 3 },                
                (Token){ .tp = tokWord,   .payload2 = 2, .startByte = 17, .lenBytes = 3 }            
        }))},
        (LexerTest) { .name = s("Brackets simple"),
            .input = s("[car cdr]"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .lenBytes = 9 },
                (Token){ .tp = tokBrackets, .payload2 = 2, .startByte = 1, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 1, .lenBytes = 3 },            
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }            
        }))},              
        (LexerTest) { .name = s("Brackets nested"),
            .input = s("[car [other car] cdr]"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt,     .payload2 = 6, .lenBytes = 21 },
                (Token){ .tp = tokBrackets, .payload2 = 5, .startByte = 1, .lenBytes = 19 },
                (Token){ .tp = tokWord,     .payload2 = 0, .startByte = 1, .lenBytes = 3 },            
                (Token){ .tp = tokBrackets, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord,     .payload2 = 1, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord,     .payload2 = 0, .startByte = 12, .lenBytes = 3 },                            
                (Token){ .tp = tokWord,     .payload2 = 2, .startByte = 17, .lenBytes = 3 }                        
        }))},             
        (LexerTest) { .name = s("Brackets mismatched"),
            .input = s("(asdf QWERT]"),
            .expectedOutput = buildLexerWithError(s(errorPunctuationUnmatched), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 1, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 6, .lenBytes = 5 }
        }))},
        (LexerTest) { .name = s("Data accessor"),
            .input = s("asdf[5]"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .lenBytes = 7 },
                (Token){ .tp = tokWord, .lenBytes = 4 },
                (Token){ .tp = tokAccessor, .payload2 = 1, .startByte = 5, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 5, .lenBytes = 1 }            
        }))},
        (LexerTest) { .name = s("Parens inside statement"),
            .input = s("foo bar ( asdf )"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 4, .lenBytes = 16 },
                (Token){ .tp = tokWord, .payload2 = 0, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 4, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 9, .lenBytes = 6 },                
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 10, .lenBytes = 4 }   
        }))}, 
        (LexerTest) { .name = s("Multi-line statement without dots"),
            .input = s("foo bar (\n"
                       " asdf\n"
                       " bcj\n"
                       " )"
                      ),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 5, .lenBytes = 23 },
                (Token){ .tp = tokWord, .payload2 = 0, .lenBytes = 3 },                 // foo
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 4, .lenBytes = 3 }, // bar
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 9, .lenBytes = 13 },                
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 11, .lenBytes = 4 }, // asdf                          
                (Token){ .tp = tokWord, .payload2 = 3, .startByte = 17, .lenBytes = 3 }  // bcj      
        }))},             
        (LexerTest) { .name = s("Punctuation all types"),
            .input = s("(:\n"
                       "asdf (b [d Ef (y z)] c f[h i])\n"
                       "\n"
                       ".bcjk (m n)\n"
                       ")"
            ),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokScope, .payload2 = 20,  .startByte = 2, .lenBytes = 45 },
                (Token){ .tp = tokStmt,  .payload2 = 14,  .startByte = 3, .lenBytes = 30 },
                (Token){ .tp = tokWord,  .payload2 = 0,   .startByte = 3, .lenBytes = 4 },     //asdf 
                (Token){ .tp = tokParens, .payload2 = 12, .startByte = 9, .lenBytes = 23 },
                (Token){ .tp = tokWord,  .payload2 = 1,   .startByte = 9, .lenBytes = 1 },     // b                
                (Token){ .tp = tokBrackets, .payload2 = 5,.startByte = 12, .lenBytes = 10 },                
                (Token){ .tp = tokWord,  .payload2 = 2,   .startByte = 12, .lenBytes = 1 },    // d
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 3, .startByte = 14, .lenBytes = 2 },  // Ef              
                (Token){ .tp = tokParens, .payload2 = 2,  .startByte = 18, .lenBytes = 3 },
                (Token){ .tp = tokWord,  .payload2 = 4,   .startByte = 18, .lenBytes = 1 },    // y
                (Token){ .tp = tokWord,  .payload2 = 5,   .startByte = 20, .lenBytes = 1 },    // z                
                (Token){ .tp = tokWord,  .payload2 = 6,   .startByte = 24, .lenBytes = 1 },    // c      
                (Token){ .tp = tokWord,  .payload2 = 7,   .startByte = 26, .lenBytes = 1 },    // f
                (Token){ .tp = tokAccessor, .payload2 = 2,.startByte = 28, .lenBytes = 3 },
                (Token){ .tp = tokWord,  .payload2 = 8,   .startByte = 28, .lenBytes = 1 },    // h
                (Token){ .tp = tokWord,  .payload2 = 9,   .startByte = 30, .lenBytes = 1 },    // i 
                (Token){ .tp = tokStmt, .payload2 = 4,    .startByte = 36, .lenBytes = 10 },
                (Token){ .tp = tokWord,  .payload2 = 10,  .startByte = 36, .lenBytes = 4 },    // bcjk
                (Token){ .tp = tokParens, .payload2 = 2,  .startByte = 42, .lenBytes = 3 },
                (Token){ .tp = tokWord,   .payload2 = 11, .startByte = 42, .lenBytes = 1 },    // m  
                (Token){ .tp = tokWord,  .payload2 = 12,  .startByte = 44, .lenBytes = 1 }     // n
        }))},
        (LexerTest) { .name = s("Colon punctuation 1"),
            .input = s("Foo : Bar 4"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 4, .startByte = 0, .lenBytes = 11 },
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 0, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 5, .lenBytes = 6 },                
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 6, .lenBytes = 3 },
                (Token){ .tp = tokInt, .payload2 = 4, .startByte = 10, .lenBytes = 1 }
        }))},           
        (LexerTest) { .name = s("Colon punctuation 2"),
            .input = s("ab (arr[foo : bar])"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 7,   .startByte = 0, .lenBytes = 19 },
                (Token){ .tp = tokWord,  .payload2 = 0,  .startByte = 0, .lenBytes = 2 }, // ab
                (Token){ .tp = tokParens, .payload2 = 5, .startByte = 4, .lenBytes = 14 },                
                (Token){ .tp = tokWord,   .payload2 = 1, .startByte = 4, .lenBytes = 3 }, // arr
                (Token){ .tp = tokAccessor, .payload2 = 3, .startByte = 8, .lenBytes = 9 },
                (Token){ .tp = tokWord,   .payload2 = 2, .startByte = 8, .lenBytes = 3 },     // foo
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 13, .lenBytes = 4 },
                (Token){ .tp = tokWord,   .payload2 = 3, .startByte = 14, .lenBytes = 3 }   // bar
        }))},
        (LexerTest) { .name = s("Dot separator"),
            .input = s("foo .bar baz"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 5, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 9, .lenBytes = 3 }
        }))},
        (LexerTest) { .name = s("Dot usage error"),
            .input = s("foo (bar .baz)"), 
            .expectedOutput = buildLexerWithError(str(errorPunctuationOnlyInMultiline, a), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokParens, .startByte = 5 },  
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
        }))}        
    }));
}


LexerTestSet* operatorTests(Arena* a) {
    return createTestSet(s("Operator lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = str("Operator simple 1", a),
            .input = s("+"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = opTPlus << 1, .startByte = 0, .lenBytes = 1 }
        }))},             
        (LexerTest) { .name = s("Operator extensible"),
            .input = s("+. -. >>. %. *. 5 <<. ^."),
            .expectedOutput = buildLexer(a, ((Token[]){
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
        (LexerTest) { .name = str("Operators list", a),
            .input = str("+ - / * ^ && || ' ? <- >=< >< $", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 13, .startByte = 0, .lenBytes = 31 },
                (Token){ .tp = tokOperator, .payload1 = (opTPlus << 1), .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 1), .startByte = 2, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTDivBy << 1), .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTTimes << 1), .startByte = 6, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTExponent << 1), .startByte = 8, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTBinaryAnd << 1), .startByte = 10, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTBoolOr << 1), .startByte = 13, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTNotEmpty << 1), .startByte = 16, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTQuestionMark << 1), .startByte = 18, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTArrowLeft << 1), .startByte = 20, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalLeft << 1), .startByte = 23, .lenBytes = 3 },
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalExcl << 1), .startByte = 27, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = (opTToString << 1), .startByte = 30, .lenBytes = 1 }                   
        }))},
        (LexerTest) { .name = str("Operator expression", a),
            .input = s("a - b"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 1), .startByte = 2, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 4, .lenBytes = 1 }                
        }))},              
        (LexerTest) { .name = str("Operator assignment 1", a),
            .input = s("a += b"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokMutation, .payload1 = (opTPlus << 1), .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 1 }    
        }))},             
        (LexerTest) { .name = s("Operator assignment 2"),
            .input = s("a ||= b"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokMutation, .payload1 = (opTBoolOr << 1), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 1 }    
        }))},
        (LexerTest) { .name = s("Operator assignment 3"),
            .input = s("a *.= b"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokMutation, .payload1 = 1 + (opTTimes << 1), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 1 }               
        }))},
        (LexerTest) { .name = s("Operator assignment 4"),
            .input = s("a ^= b"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokMutation, .payload1 = opTExponent << 1, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 1 }            
        }))}, 
        (LexerTest) { .name = s("Operator assignment in parens error"),
            .input = str("(x += y + 5)", a),
            .expectedOutput = buildLexerWithError(str(errorOperatorAssignmentPunct, a), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1},
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 1 }
        }))},             
        (LexerTest) { .name = s("Operator assignment with parens"),
            .input = s("x +.= (y + 5)"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokMutation, .payload1 = 1 + (opTPlus << 1), .payload2 = 5, .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 7, .lenBytes = 5 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 7, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = opTPlus << 1, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 11, .lenBytes = 1 }
        }))},
        (LexerTest) { .name = s("Operator assignment in parens error 1"),
            .input = s("x (+= y) + 5"),
            .expectedOutput = buildLexerWithError(str(errorOperatorAssignmentPunct, a), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .startByte = 3 }
        }))},
        (LexerTest) { .name = s("Operator assignment multiple error 1"),
            .input = s("x := y := 7"),
            .expectedOutput = buildLexerWithError(str(errorOperatorMultipleAssignment, a), a, ((Token[]) {
                (Token){ .tp = tokReassign },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 1 }
        }))},
        (LexerTest) { .name = s("Boolean operators"),
            .input = s("a and b or c"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 5, .lenBytes = 12 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokAnd, .startByte = 2, .lenBytes = 3 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 1 },
                (Token){ .tp = tokOr,                  .startByte = 8, .lenBytes = 2 },
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 11, .lenBytes = 1 }
        }))},
        (LexerTest) { .name = s("Definition of mutable var"),
            .input = s("mut x = 10"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment, .payload1 = 1, .payload2 = 2, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 10, .startByte = 8, .lenBytes = 2 },
        }))},
        (LexerTest) { .name = s("Definition of mutable var error"),
            .input = s("mut x := 10"),
            .expectedOutput = buildLexerWithError(str(errorOperatorMutableDef, a), a, ((Token[]){
                (Token){ .tp = tokMutTemp },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 }
        }))}  
    }));
}


LexerTestSet* coreFormTests(Arena* a) {
    return createTestSet(s("Core form lexer tests"), a, ((LexerTest[]) {
        (LexerTest) { .name = s("Statement-type core form"),
            .input = s("x = 9 .assert (x == 55) \"Error!\""),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment,  .payload2 = 2,             .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                (Token){ .tp = tokInt, .payload2 = 9, .startByte = 4,     .lenBytes = 1 },
                (Token){ .tp = tokAssert, .payload2 = 5, .startByte = 7,  .lenBytes = 25 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 15, .lenBytes = 7 },                
                (Token){ .tp = tokWord,                  .startByte = 15, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTEquality << 1), .startByte = 17, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 55,  .startByte = 20,  .lenBytes = 2 },
                (Token){ .tp = tokString,               .startByte = 24,  .lenBytes = 8 }
        }))},
        (LexerTest) { .name = s("Statement-type core form error"),
            .input = s("x/(await foo)"),
            .expectedOutput = buildLexerWithError(s(errorCoreNotInsideStmt), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTDivBy << 1), .startByte = 1, .lenBytes = 1 },
                (Token){ .tp = tokParens, .startByte = 3 }
        }))},
        (LexerTest) { .name = s("Paren-type core form"),
            .input = s("(if x <> 7 > 0)"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokIf, .payload2 = 6, .startByte = 1, .lenBytes = 13 },
                (Token){ .tp = tokStmt, .payload2 = 5, .startByte = 4, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTComparator << 1), .startByte = 6, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 7, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 1), .startByte = 11, .lenBytes = 1 },
                (Token){ .tp = tokInt, .startByte = 13, .lenBytes = 1 }                
        }))},        
        (LexerTest) { .name = s("If with else"),
            .input = s("(if x <> 7 > 0 ::true)"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokIf, .payload2 = 8, .startByte = 1, .lenBytes = 20 },
                (Token){ .tp = tokStmt, .payload2 = 5, .startByte = 4, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTComparator << 1), .startByte = 6, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 7, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 1), .startByte = 11, .lenBytes = 1 },
                (Token){ .tp = tokInt, .startByte = 13, .lenBytes = 1 },         
                (Token){ .tp = tokElse, .payload2 = 1, .startByte = 17, .lenBytes = 4 },
                (Token){ .tp = tokBool, .payload2 = 1, .startByte = 17, .lenBytes = 4 }
        }))},            
        (LexerTest) { .name = s("Paren-type form error 1"),
            .input = s("if x <> 7 > 0"),
            .expectedOutput = buildLexerWithError(s(errorCoreMissingParen), a, ((Token[]) {})
        )},
        (LexerTest) { .name = str("Paren-type form error 2", a),
            .input = s("(brr if x <> 7 > 0)"),
            .expectedOutput = buildLexerWithError(s(errorCoreNotAtSpanStart), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1 },                
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 }
        }))},
        (LexerTest) { .name = s("Function simple 1"),
            .input = s("fn foo Int(x Int y Int)(x - y)"),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokFnDef, .payload2 = 11, .startByte = 0, .lenBytes = 30 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 3, .lenBytes = 3 },                // foo
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 7, .lenBytes = 3 }, // Int
                (Token){ .tp = tokParens, .payload2 = 4, .startByte = 11, .lenBytes = 11 },
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 11, .lenBytes = 1 }, // x
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 13, .lenBytes = 3 }, // Int
                (Token){ .tp = tokWord, .payload2 = 3, .startByte = 17, .lenBytes = 1 }, // y
                (Token){ .tp = tokWord, .payload1 = 1, .payload2 = 1, .startByte = 19, .lenBytes = 3 }, // Int
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 24, .lenBytes = 5 },
                (Token){ .tp = tokWord, .payload2 = 2, .startByte = 24, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 1), .startByte = 26, .lenBytes = 1 },                
                (Token){ .tp = tokWord, .payload2 = 3, .startByte = 28, .lenBytes = 1 } // y
        }))},
        (LexerTest) { .name = s("Function simple error"),
            .input = s("x + (fn foo Int(x Int y Int)(x - y))"),
            .expectedOutput = buildLexerWithError(s(errorCoreNotInsideStmt), a, ((Token[]) {
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTPlus << 1), .startByte = 2, .lenBytes = 1 },
                (Token){ .tp = tokParens,  .startByte = 5}
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
