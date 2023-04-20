#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/structures/stack.h"
#include "../source/compiler/lexer.h"
#include "../source/compiler/lexerConstants.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>


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
    "word", ".word", "@word", ",func", "operator", "and", "or", "nodispose", ":", 
    "(:", ".", "()", "[]", "accessor", "funcExpr", "assign", "else", ";",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue", 
    "dispose", "embed", "export", "finally", "fn", "if", "ifEq", "ifPr", "impl", "interface", 
    "lambda", "lam1", "lam2", "lam3", "loop", "match", "mut", "return", 
    "struct", "try", "yield"
};

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


Lexer* buildLexerWithError(String* errMsg, int totalTokens, Arena *a, /* Tokens */ ...) {
    Lexer* result = allocateOnArena(sizeof(Lexer), a);
    result->wasError = true;
    result->errMsg = errMsg;
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, a);
    
    result->tokens = allocateOnArena(totalTokens*sizeof(Token), a);
    if (result == NULL) return result;
    
    for (int i = 0; i < totalTokens; i++) {
        result->tokens[i] = va_arg(tokens, Token);
    }
    
    va_end(tokens);
    return result;
}


LexerTestSet* createTestSet(String* name, int count, Arena *a, ...) {
    LexerTestSet* result = allocateOnArena(sizeof(LexerTestSet), a);
    result->name = name;
    result->totalTests = count;
    
    va_list tests;
    va_start (tests, a);
    
    result->tests = allocateOnArena(count*sizeof(LexerTest), a);
    if (result->tests == NULL) return result;
    
    for (int i = 0; i < count; i++) {
        result->tests[i] = va_arg(tests, LexerTest);
    }
    
    va_end(tests);
    return result;
}

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
    return createTestSet(allocLit("Word lexer test", a), 13, a,
        (LexerTest) { 
            .name = allocLit("Simple word lexing", a),
            .input = allocLit("asdf abc", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                    (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                    (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                    (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Word snake case", a),
            .input = allocLit("asdf_abc", a),
            .expectedOutput = buildLexerWithError(allocLit(errorWordUnderscoresOnlyAtStart, a), 0, a)
        },
        (LexerTest) {
            .name = allocLit("Word correct capitalization 1", a),
            .input = allocLit("Asdf.abc", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 8  }
            }))
        },
        (LexerTest) {
            .name = allocLit("Word correct capitalization 2", a),
            .input = allocLit("asdf.abcd.zyui", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 14  }
            }))
        },
        (LexerTest) {
            .name = allocLit("Word correct capitalization 3", a),
            .input = allocLit("asdf.Abcd", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 0, .lenBytes = 9 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Word starts with underscore and lowercase letter", a),
            .input = allocLit("_abc", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Word starts with underscore and capital letter", a),
            .input = allocLit("_Abc", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Word starts with 2 underscores", a),
            .input = allocLit("__abc", a),
            .expectedOutput = buildLexerWithError(allocLit(errorWordChunkStart, a), 0, a)
        },
        (LexerTest) {
            .name = allocLit("Word starts with underscore and digit error", a),
            .input = allocLit("_1abc", a),
            .expectedOutput = buildLexerWithError(allocLit(errorWordChunkStart, a), 0, a)
        },
        (LexerTest) {
            .name = allocLit("Dotword", a),
            .input = allocLit("@a123", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 5 }
            }))
        },
        (LexerTest) {
            .name = allocLit("At-reserved word", a),
            .input = allocLit("@if", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 3 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Function call", a),
            .input = allocLit("a ,func", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokFuncWord, .startByte = 2, .lenBytes = 5 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Function call with no space", a),
            .input = allocLit("b,func", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokFuncWord, .startByte = 1, .lenBytes = 5 }
            }))
        }
    );
}

LexerTestSet* numericTests(Arena* a) {
    return createTestSet(allocLit("Numeric lexer test", a), 26, a,
        (LexerTest) { 
            .name = allocLit("Hex numeric 1", a),
            .input = allocLit("0x15", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 21, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) { 
            .name = allocLit("Hex numeric 2", a),
            .input = allocLit("0x05", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 0, .lenBytes = 4 }
            }))
        },
        (LexerTest) { 
            .name = allocLit("Hex numeric 3", a),
            .input = allocLit("0xFFFFFFFFFFFFFFFF", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            }))
        },
        (LexerTest) { 
            .name = allocLit("Hex numeric 4", a),
            .input = allocLit("0xFFFFFFFFFFFFFFFE", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-2 >> 32), .payload2 = ((int64_t)-2 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            }))
        },
        (LexerTest) { 
            .name = allocLit("Hex numeric too long", a),
            .input = allocLit("0xFFFFFFFFFFFFFFFF0", a),
            .expectedOutput = buildLexerWithError(allocLit(errorNumericBinWidthExceeded, a), 1, a, 
                (Token){ .tp = tokStmt }
            )
        },  
        (LexerTest) { 
            .name = allocLit("Float numeric 1", a),
            .input = allocLit("1.234", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32, .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, 
                            .startByte = 0, .lenBytes = 5 }
            }))
        },
        (LexerTest) { 
            .name = allocLit("Float numeric 2", a),
            .input = allocLit("00001.234", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32, .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, 
                            .startByte = 0, .lenBytes = 9 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Float numeric 3", a),
            .input = allocLit("10500.01", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(10500.01) >> 32, .payload2 = longOfDoubleBits(10500.01) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 8 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Float numeric 4", a),
            .input = allocLit("0.9", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(0.9) >> 32, .payload2 = longOfDoubleBits(0.9) & LOWER32BITS,  
                        .startByte = 0, .lenBytes = 3 }                
            }))
        },
        (LexerTest) {
            .name = allocLit("Float numeric 5", a),
            .input = allocLit("100500.123456", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(100500.123456) >> 32, .payload2 = longOfDoubleBits(100500.123456) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 13 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Float numeric big", a),
            .input = allocLit("9007199254740992.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokFloat, 
                    .payload1 = longOfDoubleBits(9007199254740992.0) >> 32, .payload2 = longOfDoubleBits(9007199254740992.0) & LOWER32BITS,
                    .startByte = 0, .lenBytes = 18 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Float numeric too big", a),
            .input = allocLit("9007199254740993.0", a),
            .expectedOutput = buildLexerWithError(allocLit(errorNumericFloatWidthExceeded, a), 1, a, 
                (Token){ .tp = tokStmt }
            )
        },
        (LexerTest) {
            .name = allocLit("Float numeric big exponent", a),
            .input = allocLit("1005001234560000000000.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokFloat, 
                        .payload1 = longOfDoubleBits(1005001234560000000000.0) >> 32,  
                        .payload2 = longOfDoubleBits(1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Float numeric tiny", a),
            .input = allocLit("0.0000000000000000000003", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokFloat, 
                        .payload1 = longOfDoubleBits(0.0000000000000000000003) >> 32,  
                        .payload2 = longOfDoubleBits(0.0000000000000000000003) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            }))
        },
        (LexerTest) {
            .name = allocLit("Float numeric negative 1", a),
            .input = allocLit("-9.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-9.0) >> 32, .payload2 = longOfDoubleBits(-9.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 4 }
            }))
        },              
        (LexerTest) {
            .name = allocLit("Float numeric negative 2", a),
            .input = allocLit("-8.775_807", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 10 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-8.775807) >> 32, .payload2 = longOfDoubleBits(-8.775807) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 10 }
            }))
        },       
        (LexerTest) {
            .name = allocLit("Float numeric negative 3", a),
            .input = allocLit("-1005001234560000000000.0", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-1005001234560000000000.0) >> 32, 
                          .payload2 = longOfDoubleBits(-1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 25 }
            }))
        },        
        (LexerTest) {
            .name = allocLit("Int numeric 1", a),
            .input = allocLit("3", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 3, .startByte = 0, .lenBytes = 1 }
            }))
        },                  
        (LexerTest) {
            .name = allocLit("Int numeric 2", a),
            .input = allocLit("12", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 12, .startByte = 0, .lenBytes = 2,  }
            }))
        },    
        (LexerTest) {
            .name = allocLit("Int numeric 3", a),
            .input = allocLit("0987_12", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokInt, .payload2 = 98712, .startByte = 0, .lenBytes = 7 }
            }))
        },    
        (LexerTest) {
            .name = allocLit("Int numeric 4", a),
            .input = allocLit("9_223_372_036_854_775_807", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)9223372036854775807 >> 32), 
                        .payload2 = ((int64_t)9223372036854775807 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 25 }
            }))
        },           
        (LexerTest) { 
            .name = allocLit("Int numeric negative 1", a),
            .input = allocLit("-1", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 2 }
            }))
        },          
        (LexerTest) { 
            .name = allocLit("Int numeric negative 2", a),
            .input = allocLit("-775_807", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-775807) >> 32), .payload2 = ((int64_t)(-775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 8 }
            }))
        },   
        (LexerTest) { 
            .name = allocLit("Int numeric negative 3", a),
            .input = allocLit("-9_223_372_036_854_775_807", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 26 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-9223372036854775807) >> 32), 
                        .payload2 = ((int64_t)(-9223372036854775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 26 }
            }))
        },      
        (LexerTest) { 
            .name = allocLit("Int numeric error 1", a),
            .input = allocLit("3_", a),
            .expectedOutput = buildLexerWithError(allocLit(errorNumericEndUnderscore, a), 1, a, 
                (Token){ .tp = tokStmt }
        )},       
        (LexerTest) { .name = allocLit("Int numeric error 2", a),
            .input = allocLit("9_223_372_036_854_775_808", a),
            .expectedOutput = buildLexerWithError(allocLit(errorNumericIntWidthExceeded, a), 1, a, 
                (Token){ .tp = tokStmt }
        )}                                             
    );
}


LexerTestSet* stringTests(Arena* a) {
    return createTestSet(allocLit("String literals lexer tests", a), 3, a,
        (LexerTest) { .name = allocLit("String simple literal", a),
            .input = allocLit("\"asdfn't\"", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 9 }
            }))
        },     
        (LexerTest) { .name = allocLit("String literal with non-ASCII inside", a),
            .input = allocLit("\"hello мир\"", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 14 }
        }))},
        (LexerTest) { .name = allocLit("String literal unclosed", a),
            .input = allocLit("\"asdf", a),
            .expectedOutput = buildLexerWithError(allocLit(errorPrematureEndOfInput, a), 1, a, 
                (Token){ .tp = tokStmt }
        )}
    );
}


LexerTestSet* commentTests(Arena* a) {
    return createTestSet(allocLit("Comments lexer tests", a), 5, a,
        (LexerTest) { .name = allocLit("Comment simple", a),
            .input = allocLit("--this is a comment", a),
            .expectedOutput = buildLexer(a, (Token[]){}
        )},     
        (LexerTest) { .name = allocLit("Doc comment", a),
            .input = allocLit("---Documentation comment ", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 3, .lenBytes = 22 }
        }))},  
        (LexerTest) { .name = allocLit("Doc comment before something", a),
            .input = allocLit("---Documentation comment\nprint \"hw\" ", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokDocComment, .startByte = 3, .lenBytes = 21 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 25, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 25, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 31, .lenBytes = 4 }
        }))},
        (LexerTest) { .name = allocLit("Doc comment empty", a),
            .input = allocLit("---\n" "print \"hw\" ", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 4, .lenBytes = 10 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 4, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 10, .lenBytes = 4 }
        }))},     
        (LexerTest) { .name = allocLit("Doc comment multiline", a),
            .input = allocLit("---First line\n" 
                              "---Second line\n" 
                              "---Third line\n" 
                              "print \"hw\" "
            , a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 3, .lenBytes = 39 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 43, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 43, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 49, .lenBytes = 4 }
        }))}            
    );
}

LexerTestSet* punctuationTests(Arena* a) {
    return createTestSet(allocLit("Punctuation lexer tests", a), 14, a,
        (LexerTest) { .name = allocLit("Parens simple", a),
            .input = allocLit("(car cdr)", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 1, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
        }))},             
        (LexerTest) { .name = allocLit("Parens nested", a),
            .input = allocLit("(car (other car) cdr)", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 6, .startByte = 0, .lenBytes = 21 },
                (Token){ .tp = tokParens, .payload2 = 5, .startByte = 1, .lenBytes = 19 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 12, .lenBytes = 3 },                
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 3 }
        }))},
        (LexerTest) { .name = allocLit("Parens unclosed", a),
            .input = allocLit("(car (other car) cdr", a),
            .expectedOutput = buildLexerWithError(allocLit(errorPunctuationExtraOpening, a), 7, a,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1, .lenBytes = 0 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 12, .lenBytes = 3 },                
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 3 }            
        )},
        (LexerTest) { .name = allocLit("Brackets simple", a),
            .input = allocLit("[car cdr]", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .lenBytes = 9 },
                (Token){ .tp = tokBrackets, .payload2 = 2, .startByte = 1, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },            
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }            
        }))},              
        (LexerTest) { .name = allocLit("Brackets nested", a),
            .input = allocLit("[car [other car] cdr]", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 6, .lenBytes = 21 },
                (Token){ .tp = tokBrackets, .payload2 = 5, .startByte = 1, .lenBytes = 19 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },            
                (Token){ .tp = tokBrackets, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 12, .lenBytes = 3 },                            
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 3 }                        
        }))},             
        (LexerTest) { .name = allocLit("Brackets mismatched", a),
            .input = allocLit("(asdf QWERT]", a),
            .expectedOutput = buildLexerWithError(allocLit(errorPunctuationUnmatched, a), 4, a, 
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 5 }
        )},
        (LexerTest) { .name = allocLit("Data accessor", a),
            .input = allocLit("asdf[5]", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .lenBytes = 7 },
                (Token){ .tp = tokWord, .lenBytes = 4 },
                (Token){ .tp = tokAccessor, .payload2 = 1, .startByte = 5, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 5, .lenBytes = 1 }            
        }))},
        (LexerTest) { .name = allocLit("Parens inside statement", a),
            .input = allocLit("foo bar ( asdf )", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 4, .lenBytes = 16 },
                (Token){ .tp = tokWord, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 9, .lenBytes = 6 },                
                (Token){ .tp = tokWord, .startByte = 10, .lenBytes = 4 }   
        }))}, 
        (LexerTest) { .name = allocLit("Multi-line statement without dots", a),
            .input = allocLit("foo bar (\n"
                                  " asdf\n"
                                  " bcj\n"
                                  " )"
                             , a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 5, .lenBytes = 23 },
                (Token){ .tp = tokWord, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 9, .lenBytes = 13 },                
                (Token){ .tp = tokWord, .startByte = 11, .lenBytes = 4 },                                
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 3 }               
        }))},             
        (LexerTest) { .name = allocLit("Punctuation all types", a),
            .input = allocLit("(:\n"
                              "asdf (b [d Ef (y z)] c f[h i])\n"
                              "\n"
                              ".bcjk (m n)\n"
                              ")"
            , a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokScope, .payload2 = 20,  .startByte = 2, .lenBytes = 45 },
                (Token){ .tp = tokStmt,  .payload2 = 14,  .startByte = 3, .lenBytes = 30 },
                (Token){ .tp = tokWord,                   .startByte = 3, .lenBytes = 4 },     //asdf 
                (Token){ .tp = tokParens, .payload2 = 12, .startByte = 9, .lenBytes = 23 },
                (Token){ .tp = tokWord,                   .startByte = 9, .lenBytes = 1 },     // b                
                (Token){ .tp = tokBrackets, .payload2 = 5,.startByte = 12, .lenBytes = 10 },                
                (Token){ .tp = tokWord,                   .startByte = 12, .lenBytes = 1 },    // d
                (Token){ .tp = tokWord, .payload2 = 1,    .startByte = 14, .lenBytes = 2 },    // Ef              
                (Token){ .tp = tokParens, .payload2 = 2,  .startByte = 18, .lenBytes = 3 },
                (Token){ .tp = tokWord,                   .startByte = 18, .lenBytes = 1 },    // y
                (Token){ .tp = tokWord,                   .startByte = 20, .lenBytes = 1 },    // z                
                (Token){ .tp = tokWord,                   .startByte = 24, .lenBytes = 1 },    // c      
                (Token){ .tp = tokWord,                   .startByte = 26, .lenBytes = 1 },    // f
                (Token){ .tp = tokAccessor, .payload2 = 2,.startByte = 28, .lenBytes = 3 },
                (Token){ .tp = tokWord,                   .startByte = 28, .lenBytes = 1 },    // h
                (Token){ .tp = tokWord,                   .startByte = 30, .lenBytes = 1 },    // i 
                (Token){ .tp = tokStmt, .payload2 = 4,    .startByte = 36, .lenBytes = 10 },
                (Token){ .tp = tokWord,                   .startByte = 36, .lenBytes = 4 },    // bcjk
                (Token){ .tp = tokParens, .payload2 = 2,  .startByte = 42, .lenBytes = 3 },
                (Token){ .tp = tokWord,                   .startByte = 42, .lenBytes = 1 },    // m  
                (Token){ .tp = tokWord,                   .startByte = 44, .lenBytes = 1 }     // n
        }))},
        (LexerTest) { .name = allocLit("Colon punctuation 1", a),
            .input = allocLit("Foo : Bar 4", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 4, .startByte = 0, .lenBytes = 11 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 5, .lenBytes = 6 },                
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 3 },
                (Token){ .tp = tokInt, .payload2 = 4, .startByte = 10, .lenBytes = 1 }
        }))},           
        (LexerTest) { .name = allocLit("Colon punctuation 2", a),
            .input = allocLit("ab (arr[foo : bar])", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 7,   .startByte = 0, .lenBytes = 19 },
                (Token){ .tp = tokWord,                  .startByte = 0, .lenBytes = 2 }, // ab
                (Token){ .tp = tokParens, .payload2 = 5, .startByte = 4, .lenBytes = 14 },                
                (Token){ .tp = tokWord,                  .startByte = 4, .lenBytes = 3 }, // arr
                (Token){ .tp = tokAccessor, .payload2 = 3, .startByte = 8, .lenBytes = 9 },
                (Token){ .tp = tokWord,                  .startByte = 8, .lenBytes = 3 },     // foo
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 13, .lenBytes = 4 },
                (Token){ .tp = tokWord,                  .startByte = 14, .lenBytes = 3 }   // bar
        }))},
        (LexerTest) { .name = allocLit("Dot separator", a),
            .input = allocLit("foo .bar baz", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokWord,                .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 5, .lenBytes = 7 },
                (Token){ .tp = tokWord,                .startByte = 5, .lenBytes = 3 },
                (Token){ .tp = tokWord,                .startByte = 9, .lenBytes = 3 }
        }))},
        (LexerTest) { .name = allocLit("Dot usage error", a),
            .input = allocLit("foo (bar .baz)", a), 
            .expectedOutput = buildLexerWithError(allocLit(errorPunctuationOnlyInMultiline, a), 4, a,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokParens, .startByte = 5 },  
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
        )}        
    );
}


LexerTestSet* operatorTests(Arena* a) {
    return createTestSet(allocLit("Operator lexer tests", a), 13, a,
        (LexerTest) { .name = allocLit("Operator simple 1", a),
            .input = allocLit("+", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = opTPlus << 2, .startByte = 0, .lenBytes = 1 }
        }))},             
        (LexerTest) { .name = allocLit("Operator extensible", a),
            .input = allocLit("+. -. >>. %. *. 5 <<. ^.", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 8, .lenBytes = 24 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTPlus << 2), .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTMinus << 2), .startByte = 3, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTBitshiftRight << 2), .startByte = 6, .lenBytes = 3 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTRemainder << 2), .startByte = 10, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTTimes << 2), .startByte = 13, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 16, .lenBytes = 1 }, 
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTBitShiftLeft << 2), .startByte = 18, .lenBytes = 3 }, 
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTExponent << 2), .startByte = 22, .lenBytes = 2 }
        }))},
        (LexerTest) { .name = allocLit("Operators list", a),
            .input = allocLit("+ - / * ^ && || ' ? <- >=< >< $", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 13, .startByte = 0, .lenBytes = 31 },
                (Token){ .tp = tokOperator, .payload1 = (opTPlus << 2), .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 2), .startByte = 2, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTDivBy << 2), .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTTimes << 2), .startByte = 6, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTExponent << 2), .startByte = 8, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTBinaryAnd << 2), .startByte = 10, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTBoolOr << 2), .startByte = 13, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTNotEmpty << 2), .startByte = 16, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTQuestionMark << 2), .startByte = 18, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTArrowLeft << 2), .startByte = 20, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalLeft << 2), .startByte = 23, .lenBytes = 3 },
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalExcl << 2), .startByte = 27, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = (opTToString << 2), .startByte = 30, .lenBytes = 1 }                   
        }))},
        (LexerTest) { .name = allocLit("Operator expression", a),
            .input = allocLit("a - b", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 2), .startByte = 2, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 }                
        }))},              
        (LexerTest) { .name = allocLit("Operator assignment 1", a),
            .input = allocLit("a += b", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment, .payload1 = 1 + (opTPlus << 2), .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 1 }    
        }))},             
        (LexerTest) { .name = allocLit("Operator assignment 2", a),
            .input = allocLit("a ||= b", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment, .payload1 = 1 + (opTBoolOr << 2), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 1 }    
        }))},
        (LexerTest) { .name = allocLit("Operator assignment 3", a),
            .input = allocLit("a *.= b", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment, .payload1 = 3 + (opTTimes << 2), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 1 }               
        }))},
        (LexerTest) { .name = allocLit("Operator assignment 4", a),
            .input = allocLit("a ^= b", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment, .payload1 = 1 + (opTExponent << 2), .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 1 }            
        }))}, 
        (LexerTest) { .name = allocLit("Operator assignment in parens error", a),
            .input = allocLit("(x += y + 5)", a),
            .expectedOutput = buildLexerWithError(allocLit(errorOperatorAssignmentPunct, a), 3, a,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1},
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 1 }
        )},             
        (LexerTest) { .name = allocLit("Operator assignment with parens", a),
            .input = allocLit("x +.= (y + 5)", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment, .payload1 = 3 + (opTPlus << 2), .payload2 = 5, .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 7, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 7, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = opTPlus << 2, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 11, .lenBytes = 1 }
        }))},
        (LexerTest) { .name = allocLit("Operator assignment in parens error 1", a),
            .input = allocLit("x (+= y) + 5", a),
            .expectedOutput = buildLexerWithError(allocLit(errorOperatorAssignmentPunct, a), 3, a,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .startByte = 3 }
        )},
        (LexerTest) { .name = allocLit("Operator assignment multiple error 1", a),
            .input = allocLit("x := y := 7", a),
            .expectedOutput = buildLexerWithError(allocLit(errorOperatorMultipleAssignment, a), 3, a,
                (Token){ .tp = tokAssignment, .payload1 = 1 + (opTMutation << 2) },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 1 }
        )},
        (LexerTest) { .name = allocLit("Boolean operators", a),
            .input = allocLit("a and b or c", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokStmt, .payload2 = 5, .lenBytes = 12 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokAnd, .startByte = 2, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 1 },
                (Token){ .tp = tokOr, .startByte = 8, .lenBytes = 2 },
                (Token){ .tp = tokWord, .startByte = 11, .lenBytes = 1 }
        }))}   
    );
}


LexerTestSet* coreFormTests(Arena* a) {
    return createTestSet(allocLit("Core form lexer tests", a), 8, a,
        (LexerTest) { .name = allocLit("Statement-type core form", a),
            .input = allocLit("x = 9 .assert (x == 55) \"Error!\"", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokAssignment, .payload1 = (opTDefinition << 2) + 1,  .payload2 = 2, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                (Token){ .tp = tokInt, .payload2 = 9, .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokAssert, .payload2 = 5, .startByte = 7, .lenBytes = 25 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 15, .lenBytes = 7 },                
                (Token){ .tp = tokWord,                  .startByte = 15, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTEquality << 2), .startByte = 17, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 55,  .startByte = 20, .lenBytes = 2 },
                (Token){ .tp = tokString,               .startByte = 24, .lenBytes = 8 }
        }))},
        (LexerTest) { .name = allocLit("Statement-type core form error", a),
            .input = allocLit("x/(await foo)", a),
            .expectedOutput = buildLexerWithError(allocLit(errorCoreNotInsideStmt, a), 4, a,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTDivBy << 2), .startByte = 1, .lenBytes = 1 },
                (Token){ .tp = tokParens, .startByte = 3 }
        )},
        (LexerTest) { .name = allocLit("Paren-type core form", a),
            .input = allocLit("(if x <> 7 > 0)", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokIf, .payload2 = 6, .startByte = 1, .lenBytes = 13 },
                (Token){ .tp = tokStmt, .payload2 = 5, .startByte = 4, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTComparator << 2), .startByte = 6, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 7, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 2), .startByte = 11, .lenBytes = 1 },
                (Token){ .tp = tokInt, .startByte = 13, .lenBytes = 1 }                
        }))},        
        (LexerTest) { .name = allocLit("If with else", a),
            .input = allocLit("(if x <> 7 > 0 ::true)", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokIf, .payload2 = 8, .startByte = 1, .lenBytes = 20 },
                (Token){ .tp = tokStmt, .payload2 = 5, .startByte = 4, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTComparator << 2), .startByte = 6, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 7, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTGreaterThan << 2), .startByte = 11, .lenBytes = 1 },
                (Token){ .tp = tokInt, .startByte = 13, .lenBytes = 1 },         
                (Token){ .tp = tokElse, .payload2 = 1, .startByte = 17, .lenBytes = 4 },
                (Token){ .tp = tokBool, .payload2 = 1, .startByte = 17, .lenBytes = 4 }
        }))},            
        (LexerTest) { .name = allocLit("Paren-type form error 1", a),
            .input = allocLit("if x <> 7 > 0", a),
            .expectedOutput = buildLexerWithError(allocLit(errorCoreMissingParen, a), 0, a
        )},
        (LexerTest) { .name = allocLit("Paren-type form error 2", a),
            .input = allocLit("(brr if x <> 7 > 0)", a),
            .expectedOutput = buildLexerWithError(allocLit(errorCoreNotAtSpanStart, a), 3, a,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1 },
                
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 }
        )},
        (LexerTest) { .name = allocLit("Function simple 1", a),
            .input = allocLit("fn foo Int(x Int y Int)(x - y)", a),
            .expectedOutput = buildLexer(a, ((Token[]){
                (Token){ .tp = tokFnDef, .payload2 = 11, .startByte = 0, .lenBytes = 30 },
                (Token){ .tp = tokWord, .startByte = 3, .lenBytes = 3 },                // foo
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 7, .lenBytes = 3 }, // Int
                (Token){ .tp = tokParens, .payload2 = 4, .startByte = 11, .lenBytes = 11 },
                (Token){ .tp = tokWord, .startByte = 11, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 13, .lenBytes = 3 },                
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 19, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 24, .lenBytes = 5 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 24, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 2), .startByte = 26, .lenBytes = 1 },                
                (Token){ .tp = tokWord, .startByte = 28, .lenBytes = 1 }
        }))},
        (LexerTest) { .name = allocLit("Function simple error", a),
            .input = allocLit("x + (fn foo Int(x Int y Int)(x - y))", a),
            .expectedOutput = buildLexerWithError(allocLit(errorCoreNotInsideStmt, a), 4, a,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },                // x
                (Token){ .tp = tokOperator, .payload1 = (opTPlus << 2), .startByte = 2, .lenBytes = 1 },
                (Token){ .tp = tokParens,  .startByte = 5}
        )} 
    );
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
    printf("Lexer test\n");
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
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
