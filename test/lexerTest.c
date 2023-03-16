#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/stack.h"
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
    "int", "float", "bool", "string", "_", "docComment", 
    "word", ".word", "@word", "reserved", "operator", ":", 
    "stmt", "()", "[]", "compoundString", "accessor", "assignment", "typeDecl", "lexScope", 
    "alias", "await", "break", "catch", "continue", "embed", "export", "for",
    "generator", "if", "ifEq", "ifPr", "impl", "interface", "lambda", "match", "mut",
    "nodestruct", "return", "struct", "test", "try", "type", "yield"
};

static Lexer* buildLexer(int totalTokens, Arena *ar, /* Tokens */ ...) {
    Lexer* result = createLexer(&empty, ar);
    if (result == NULL) return result;
    
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, ar);
    
    for (int i = 0; i < totalTokens; i++) {
        add(va_arg(tokens, Token), result);
    }
    
    va_end(tokens);
    return result;
}


Lexer* buildLexerWithError(String* errMsg, int totalTokens, Arena *ar, /* Tokens */ ...) {
    Lexer* result = allocateOnArena(sizeof(Lexer), ar);
    result->wasError = true;
    result->errMsg = errMsg;
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, ar);
    
    result->tokens = allocateOnArena(totalTokens*sizeof(Token), ar);
    if (result == NULL) return result;
    
    for (int i = 0; i < totalTokens; i++) {
        result->tokens[i] = va_arg(tokens, Token);
    }
    
    va_end(tokens);
    return result;
}


LexerTestSet* createTestSet(String* name, int count, Arena *ar, ...) {
    LexerTestSet* result = allocateOnArena(sizeof(LexerTestSet), ar);
    result->name = name;
    result->totalTests = count;
    
    va_list tests;
    va_start (tests, ar);
    
    result->tests = allocateOnArena(count*sizeof(LexerTest), ar);
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
    printf("Total tokens: %d \n", a->totalTokens);
    for (int i = 0; i < a->totalTokens; i++) {
        printf("%s [%d; %d]\n", tokNames[a->tokens[i].tp], a->tokens[i].startByte, a->tokens[i].lenBytes);
    }
}

/** Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code */
void runLexerTest(LexerTest test, int* countPassed, int* countTests, LanguageDefinition* lang, Arena *ar) {
    (*countTests)++;
    Lexer* result = lexicallyAnalyze(test.input, lang, ar);

    int equalityStatus = equalityLexer(*result, *test.expectedOutput);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("ERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(result->errMsg);
        printf("\nBut was expected: ");
        printString(test.expectedOutput->errMsg);
        printf("\n\n");
    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On token %d\n\n", equalityStatus);
        printLexer(result);
    }
}


LexerTestSet* wordTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Word lexer test"), 12, ar,
        (LexerTest) { 
            .name = allocLit(ar, "Simple word lexing"),
            .input = allocLit(ar, "asdf abc"),
            .expectedOutput = buildLexer(3, ar, 
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word snake case"),
            .input = allocLit(ar, "asdf_abc"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordUnderscoresOnlyAtStart), 1, ar,
                (Token){ .tp = tokStmt }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word correct capitalization 1"),
            .input = allocLit(ar, "Asdf.abc"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 8  }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word correct capitalization 2"),
            .input = allocLit(ar, "asdf.abcd.zyui"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14  },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 14  }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word correct capitalization 3"),
            .input = allocLit(ar, "asdf.Abcd"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 0, .lenBytes = 9 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word incorrect capitalization"),
            .input = allocLit(ar, "Asdf.Abcd"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordCapitalizationOrder), 1, ar,
                (Token){ .tp = tokStmt }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with underscore and lowercase letter"),
            .input = allocLit(ar, "_abc"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with underscore and capital letter"),
            .input = allocLit(ar, "_Abc"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 0, .lenBytes = 4 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with 2 underscores"),
            .input = allocLit(ar, "__abc"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordChunkStart), 1, ar,
                (Token){ .tp = tokStmt }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with underscore and digit"),
            .input = allocLit(ar, "_1abc"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordChunkStart), 1, ar,
                (Token){ .tp = tokStmt }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Dotword"),
            .input = allocLit(ar, "@a123"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 5 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "At-reserved word"),
            .input = allocLit(ar, "@if"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokAtWord, .startByte = 0, .lenBytes = 3 }
            )
        }
    );
}

LexerTestSet* numericTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Numeric lexer test"), 31, ar,
        (LexerTest) { 
            .name = allocLit(ar, "Binary numeric 64-bit zero"),
            .input = allocLit(ar, "0b0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2=1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokInt, .payload2=0, .startByte = 0, .lenBytes = 3,  }
            )},
        (LexerTest) { .name = allocLit(ar, "Binary numeric basic"),
            .input = allocLit(ar, "0b0101"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 0, .lenBytes = 6 }
            )},        
        (LexerTest) { .name = allocLit(ar, "Binary numeric 64-bit positive"),
            .input = allocLit(ar, "0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 81 },
                (Token){ .tp = tokInt, .startByte = 7, .lenBytes = 0, .payload2 = 81 }
            )},
        (LexerTest) { 
            .name = allocLit(ar, "Binary numeric 64-bit negative"),
            .input = allocLit(ar, "0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1001"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 81 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-7 >> 32), .payload2 = ((int64_t)-7 & LOWER32BITS), 
                            .startByte = 0, .lenBytes = 81 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Binary numeric 65-bit error"),
            .input = allocLit(ar, "0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_01110"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorNumericBinWidthExceeded), 1, ar, 
                (Token){ .tp = tokStmt }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 1"),
            .input = allocLit(ar, "0x15"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 21, .startByte = 0, .lenBytes = 4 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 2"),
            .input = allocLit(ar, "0x05"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 4, .startByte = 0, .lenBytes = 4 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 3"),
            .input = allocLit(ar, "0xFFFFFFFFFFFFFFFF"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 4"),
            .input = allocLit(ar, "0xFFFFFFFFFFFFFFFE"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-2 >> 32), .payload2 = ((int64_t)-2 & LOWER32BITS),
                        .startByte = 0, .lenBytes = 18  }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric too long"),
            .input = allocLit(ar, "0xFFFFFFFFFFFFFFFF0"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorNumericBinWidthExceeded), 1, ar, 
                (Token){ .tp = tokStmt }
            )
        },  
        (LexerTest) { 
            .name = allocLit(ar, "Float numeric 1"),
            .input = allocLit(ar, "1.234"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32, .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, 
                            .startByte = 0, .lenBytes = 5 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Float numeric 2"),
            .input = allocLit(ar, "00001.234"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(1.234) >> 32, .payload2 = longOfDoubleBits(1.234) & LOWER32BITS, 
                            .startByte = 0, .lenBytes = 9 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric 3"),
            .input = allocLit(ar, "10500.01"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(10500.01) >> 32, .payload2 = longOfDoubleBits(10500.01) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 8 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric 4"),
            .input = allocLit(ar, "0.9"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(0.9) >> 32, .payload2 = longOfDoubleBits(0.9) & LOWER32BITS,  
                        .startByte = 0, .lenBytes = 3 }                
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric 5"),
            .input = allocLit(ar, "100500.123456"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(100500.123456) >> 32, .payload2 = longOfDoubleBits(100500.123456) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 13 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric big"),
            .input = allocLit(ar, "9007199254740992.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokWord, 
                    .payload1 = longOfDoubleBits(9007199254740992.0) >> 32, .payload2 = longOfDoubleBits(9007199254740992.0) & LOWER32BITS,
                    .startByte = 0, .lenBytes = 18 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric too big"),
            .input = allocLit(ar, "9007199254740993.0"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorNumericFloatWidthExceeded), 1, ar, 
                (Token){ .tp = tokStmt }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric big exponent"),
            .input = allocLit(ar, "1005001234560000000000.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokWord, 
                        .payload1 = longOfDoubleBits(1005001234560000000000.0) >> 32,  
                        .payload2 = longOfDoubleBits(1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric tiny"),
            .input = allocLit(ar, "0.0000000000000000000003"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokWord, 
                        .payload1 = longOfDoubleBits(0.0000000000000000000003) >> 32,  
                        .payload2 = longOfDoubleBits(0.0000000000000000000003) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 24 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric negative 1"),
            .input = allocLit(ar, "-9.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-9.0) >> 32, .payload2 = longOfDoubleBits(-9.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 4 }
            )
        },              
        (LexerTest) {
            .name = allocLit(ar, "Float numeric negative 2"),
            .input = allocLit(ar, "-8.775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 10 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-8.775807) >> 32, .payload2 = longOfDoubleBits(-8.775807) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 10 }
            )
        },       
        (LexerTest) {
            .name = allocLit(ar, "Float numeric negative 3"),
            .input = allocLit(ar, "-1005001234560000000000.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokFloat, .payload1 = longOfDoubleBits(-1005001234560000000000.0) >> 32, 
                          .payload2 = longOfDoubleBits(-1005001234560000000000.0) & LOWER32BITS, 
                        .startByte = 0, .lenBytes = 25 }
            )
        },        
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 1"),
            .input = allocLit(ar, "3"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .payload2 = 3, .startByte = 5, .lenBytes = 3 }
            )
        },                  
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 2"),
            .input = allocLit(ar, "12"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 12, .startByte = 0, .lenBytes = 2,  }
            )
        },    
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 3"),
            .input = allocLit(ar, "0987_12"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokInt, .payload2 = 98712, .startByte = 0, .lenBytes = 7 }
            )
        },    
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 4"),
            .input = allocLit(ar, "9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 25 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)9223372036854775807 >> 32), 
                        .payload2 = ((int64_t)9223372036854775807 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 25 }
            )
        },           
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric negative 1"),
            .input = allocLit(ar, "-1"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)-1 >> 32), .payload2 = ((int64_t)-1 & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 2 }
            )
        },          
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric negative 2"),
            .input = allocLit(ar, "-775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 8 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-775807) >> 32), .payload2 = ((int64_t)(-775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 8 }
            )
        },   
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric negative 3"),
            .input = allocLit(ar, "-9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 26 },
                (Token){ .tp = tokInt, .payload1 = ((int64_t)(-9223372036854775807) >> 32), 
                        .payload2 = ((int64_t)(-9223372036854775807) & LOWER32BITS), 
                        .startByte = 0, .lenBytes = 26 }
            )
        },      
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric error 1"),
            .input = allocLit(ar, "3_"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorNumericEndUnderscore), 1, ar, 
                (Token){ .tp = tokStmt }
        )},       
        (LexerTest) { .name = allocLit(ar, "Int numeric error 2"),
            .input = allocLit(ar, "9_223_372_036_854_775_808"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorNumericIntWidthExceeded), 1, ar, 
                (Token){ .tp = tokStmt }
        )}                                             
    );
}


LexerTestSet* stringTests(Arena* ar) {
    return createTestSet(allocLit(ar, "String literals lexer tests"), 3, ar,
        (LexerTest) { .name = allocLit(ar, "String simple literal"),
            .input = allocLit(ar, "\"asdfn't\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokString, .startByte = 1, .lenBytes = 7 }
        )},     
        (LexerTest) { .name = allocLit(ar, "String literal with non-ASCII inside"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 14 },
                (Token){ .tp = tokString, .startByte = 1, .lenBytes = 12 }
        )},
        (LexerTest) { .name = allocLit(ar, "String literal unclosed"),
            .input = allocLit(ar, "\"asdf"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorPrematureEndOfInput), 1, ar, 
                (Token){ .tp = tokStmt }
        )}
    );
}


LexerTestSet* commentTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Comments lexer tests"), 5, ar,
        (LexerTest) { .name = allocLit(ar, "Comment simple"),
            .input = allocLit(ar, "# this is a comment"),
            .expectedOutput = buildLexer(0, ar
        )},     
        (LexerTest) { .name = allocLit(ar, "Doc comment"),
            .input = allocLit(ar, "## Documentation comment "),
            .expectedOutput = buildLexer(1, ar, 
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 2, .lenBytes = 23 }
        )},  
        (LexerTest) { .name = allocLit(ar, "Doc comment before something"),
            .input = allocLit(ar, "## Documentation comment\n" 
                                  "print \"hw\" "
            ),
            .expectedOutput = buildLexer(4, ar, 
                (Token){ .tp = tokDocComment, .startByte = 2, .lenBytes = 22 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 25, .lenBytes = 11 },
                (Token){ .tp = tokWord, .startByte = 25, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 32, .lenBytes = 2 }
        )},
        (LexerTest) { .name = allocLit(ar, "Doc comment empty"),
            .input = allocLit(ar, "##\n" "print \"hw\" "),
            .expectedOutput = buildLexer(3, ar,
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 3, .lenBytes = 11 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 3, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 10, .lenBytes = 2 }
        )},     
        (LexerTest) { .name = allocLit(ar, "Doc comment multiline"),
            .input = allocLit(ar, "## First line\n" 
                                  "## Second line\n" 
                                  "## Third line\n" 
                                  "print \"hw\" "
            ),
            .expectedOutput = buildLexer(4, ar, 
                (Token){ .tp = tokDocComment, .payload2 = 0, .startByte = 2, .lenBytes = 40 },
                (Token){ .tp = tokStmt, .payload2 = 2, .startByte = 43, .lenBytes = 11 },
                (Token){ .tp = tokWord, .startByte = 43, .lenBytes = 5 },
                (Token){ .tp = tokString, .startByte = 50, .lenBytes = 2 }
        )}            
    );
}

LexerTestSet* punctuationTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Punctuation lexer tests"), 11, ar,
        (LexerTest) { .name = allocLit(ar, "Parens simple"),
            .input = allocLit(ar, "(car cdr)"),
            .expectedOutput = buildLexer(4, ar,
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 9 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 1, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
        )},             
        (LexerTest) { .name = allocLit(ar, "Parens nested"),
            .input = allocLit(ar, "(car (other car) cdr)"),
            .expectedOutput = buildLexer(7, ar, 
                (Token){ .tp = tokStmt, .payload2 = 6, .startByte = 0, .lenBytes = 21 },
                (Token){ .tp = tokParens, .payload2 = 5, .startByte = 1, .lenBytes = 19 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 12, .lenBytes = 3 },                
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 3 }
        )},
        (LexerTest) { .name = allocLit(ar, "Parens unclosed"),
            .input = allocLit(ar, "(car (other car) cdr"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorPunctuationExtraOpening), 7, ar,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1, .lenBytes = 0 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 12, .lenBytes = 3 },                
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 3 }            
        )},
        (LexerTest) { .name = allocLit(ar, "Brackets simple"),
            .input = allocLit(ar, "[car cdr]"),
            .expectedOutput = buildLexer(4, ar,
                (Token){ .tp = tokStmt, .payload2 = 3, .lenBytes = 9 },
                (Token){ .tp = tokBrackets, .payload2 = 2, .startByte = 1, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },            
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }            
        )},              
        (LexerTest) { .name = allocLit(ar, "Brackets nested"),
            .input = allocLit(ar, "[car [other car] cdr]"),
            .expectedOutput = buildLexer(7, ar,
                (Token){ .tp = tokStmt, .payload2 = 6, .lenBytes = 21 },
                (Token){ .tp = tokBrackets, .payload2 = 5, .startByte = 1, .lenBytes = 19 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 3 },            
                (Token){ .tp = tokBrackets, .payload2 = 2, .startByte = 6, .lenBytes = 9 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 12, .lenBytes = 3 },                            
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 3 }                        
        )},             
        (LexerTest) { .name = allocLit(ar, "Brackets mismatched"),
            .input = allocLit(ar, "(asdf QWERT]"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorPunctuationUnmatched), 4, ar, 
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1 },
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 5 }
        )},
        (LexerTest) { .name = allocLit(ar, "Data accessor"),
            .input = allocLit(ar, "asdf[5]"),
            .expectedOutput = buildLexer(4, ar,
                (Token){ .tp = tokStmt, .payload2 = 3, .lenBytes = 7 },
                (Token){ .tp = tokWord, .lenBytes = 4 },
                (Token){ .tp = tokAccessor, .payload2 = 1, .startByte = 5, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 5, .lenBytes = 1 }            
        )},
        (LexerTest) { .name = allocLit(ar, "Parens inside statement"),
            .input = allocLit(ar, "foo bar ( asdf )"),
            .expectedOutput = buildLexer(5, ar, 
                (Token){ .tp = tokStmt, .payload2 = 4, .lenBytes = 16 },
                (Token){ .tp = tokWord, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 9, .lenBytes = 6 },                
                (Token){ .tp = tokWord, .startByte = 10, .lenBytes = 4 }   
        )}, 
        (LexerTest) { .name = allocLit(ar, "Multi-line statement without dots"),
            .input = allocLit(ar, "foo bar (\n"
                                  " asdf\n"
                                  " bcj\n"
                                  " )"
                             ),
            .expectedOutput = buildLexer(6, ar,
                (Token){ .tp = tokStmt, .payload2 = 5, .lenBytes = 23 },
                (Token){ .tp = tokWord, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 1, .startByte = 9, .lenBytes = 10 },                
                (Token){ .tp = tokWord, .startByte = 10, .lenBytes = 4 },                                
                (Token){ .tp = tokWord, .startByte = 15, .lenBytes = 3 }               
        )},             
        (LexerTest) { .name = allocLit(ar, "Punctuation all types"),
            .input = allocLit(ar, "(\n"
                                  "asdf (b [d Ef (y z)] c f[h i])\n"
                                  "\n"
                                  "bcjk ((m n))\n"
                                  ")"
            ),
            .expectedOutput = buildLexer(21, ar, 
                (Token){ .tp = tokLexScope, .payload2 = 20, .startByte = 0, .lenBytes = 46 },
                (Token){ .tp = tokStmt, .payload2 = 19, .startByte = 2, .lenBytes = 30 },
                (Token){ .tp = tokWord, .startByte = 2, .lenBytes = 4 },                      // asdf                               
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 8, .lenBytes = 23 },                                
                (Token){ .tp = tokWord, .startByte = 8, .lenBytes = 1 },                      // b                
                (Token){ .tp = tokBrackets, .payload2 = 5, .startByte = 11, .lenBytes = 10 },                
                (Token){ .tp = tokWord, .startByte = 11, .lenBytes = 3 },                     // d
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 13, .lenBytes = 2 },      // Ef              
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 17, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 1 },                     // y
                (Token){ .tp = tokWord, .startByte = 19, .lenBytes = 1 },                     // z                
                (Token){ .tp = tokWord, .startByte = 23, .lenBytes = 1 },                     // c      
                (Token){ .tp = tokWord, .startByte = 25, .lenBytes = 1 },                     // f
                (Token){ .tp = tokAccessor, .payload2 = 2, .startByte = 27, .lenBytes = 3 },
                (Token){ .tp = tokWord, .startByte = 27, .lenBytes = 1 },                     // h
                (Token){ .tp = tokWord, .startByte = 29, .lenBytes = 1 },                     // i                                
                (Token){ .tp = tokStmt, .payload2 = 4, .startByte = 34, .lenBytes = 10 },
                (Token){ .tp = tokWord, .startByte = 34, .lenBytes = 4 },                     // bcjk
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 40, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 40, .lenBytes = 1 },                     // m  
                (Token){ .tp = tokWord, .startByte = 42, .lenBytes = 1 }                      // n
        )},
        (LexerTest) { .name = allocLit(ar, "Colon punctuation 1"),
            .input = allocLit(ar, "Foo : Bar 4"),
            .expectedOutput = buildLexer(5, ar,
                (Token){ .tp = tokStmt, .payload2 = 4, .startByte = 0, .lenBytes = 11 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokParens, .payload2 = 2, .startByte = 5, .lenBytes = 6 },                
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 6, .lenBytes = 3 },
                (Token){ .tp = tokInt, .payload2 = 4, .startByte = 10, .lenBytes = 1 }
        )}                        
    );
}


LexerTestSet* operatorTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Operator lexer tests"), 12, ar,
        (LexerTest) { .name = allocLit(ar, "Operator simple 1"),
            .input = allocLit(ar, "+"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = opTPlus << 2, .startByte = 0, .lenBytes = 1 }
        )},             
        (LexerTest) { .name = allocLit(ar, "Operator extensible"),
            .input = allocLit(ar, "+. -. >>. %. *. 5 <<. ^."),
            .expectedOutput = buildLexer(9, ar, 
                (Token){ .tp = tokStmt, .payload2 = 18, .startByte = 0, .lenBytes = 24 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTPlus << 2), .startByte = 0, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTMinus << 2), .startByte = 3, .lenBytes = 2 },                
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTBitshiftRight << 2), .startByte = 6, .lenBytes = 3 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTRemainder << 2), .startByte = 10, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTTimes << 2), .startByte = 13, .lenBytes = 2 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 16, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTBitShiftLeft << 2), .startByte = 18, .lenBytes = 3 }, 
                (Token){ .tp = tokOperator, .payload1 = 2 + (opTExponent << 2), .startByte = 22, .lenBytes = 2 }
        )},
        (LexerTest) { .name = allocLit(ar, "Operators list"),
            .input = allocLit(ar, "+ - / * ^ && || ~ ? <- >=< >< \\"),
            .expectedOutput = buildLexer(14, ar,
                (Token){ .tp = tokStmt, .payload2 = 13, .startByte = 0, .lenBytes = 31 },
                (Token){ .tp = tokOperator, .payload1 = (opTPlus << 2), .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 2), .startByte = 2, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload1 = (opTDivBy << 2), .startByte = 4, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTTimes << 2), .startByte = 6, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTExponent << 2), .startByte = 8, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTBoolAnd << 2), .startByte = 10, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTBoolOr << 2), .startByte = 13, .lenBytes = 2 }, 
                (Token){ .tp = tokOperator, .payload1 = (opTNotEmpty << 2), .startByte = 16, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTQuestionMark << 2), .startByte = 18, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTArrowLeft << 2), .startByte = 20, .lenBytes = 2 },                
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalLeft << 2), .startByte = 23, .lenBytes = 3 },
                (Token){ .tp = tokOperator, .payload1 = (opTIntervalExcl << 2), .startByte = 27, .lenBytes = 2 },
                (Token){ .tp = tokOperator, .payload1 = (opTToString << 2), .startByte = 30, .lenBytes = 1 }                   
        )},
        (LexerTest) { .name = allocLit(ar, "Operator expression"),
            .input = allocLit(ar, "a - b"),
            .expectedOutput = buildLexer(4, ar,
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload1 = (opTMinus << 2), .startByte = 2, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 4, .lenBytes = 1 }                
        )},              
        (LexerTest) { .name = allocLit(ar, "Operator assignment 1"),
            .input = allocLit(ar, "a += b"),
            .expectedOutput = buildLexer(3, ar,
                (Token){ .tp = tokAssignment, .payload1 = 1 + (opTPlus << 2), .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 1 }    
        )},             
        (LexerTest) { .name = allocLit(ar, "Operator assignment 2"),
            .input = allocLit(ar, "a ||= b"),
            .expectedOutput = buildLexer(3, ar, 
                (Token){ .tp = tokAssignment, .payload1 = 1 + (opTBoolOr << 2), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 1 }    
        )},
        (LexerTest) { .name = allocLit(ar, "Operator assignment 3"),
            .input = allocLit(ar, "a *.= b"),
            .expectedOutput = buildLexer(3, ar,
                (Token){ .tp = tokAssignment, .payload1 = 3 + (opTTimes << 2), .payload2 = 2, .startByte = 0, .lenBytes = 7 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 1 }               
        )},
        (LexerTest) { .name = allocLit(ar, "Operator assignment 4"),
            .input = allocLit(ar, "a ^= b"),
            .expectedOutput = buildLexer(3, ar,
                (Token){ .tp = tokAssignment, .payload1 = 1 + (opTExponent << 2), .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 3, .startByte = 5, .lenBytes = 1 }            
        )}, 
        (LexerTest) { .name = allocLit(ar, "Operator assignment in parens error"),
            .input = allocLit(ar, "(x += y + 5)"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorOperatorAssignmentPunct), 3, ar,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokParens, .startByte = 1},
                (Token){ .tp = tokWord, .startByte = 1, .lenBytes = 1 }
        )},             
        (LexerTest) { .name = allocLit(ar, "Operator assignment with parens"),
            .input = allocLit(ar, "x +.= (y + 5)"),
            .expectedOutput = buildLexer(6, ar, 
                (Token){ .tp = tokAssignment, .payload1 = 3 + (opTPlus << 2), .startByte = 0, .lenBytes = 13 },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .payload2 = 3, .startByte = 7, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 7, .lenBytes = 1 },
                (Token){ .tp = tokOperator, .payload2 = opTPlus << 2, .startByte = 9, .lenBytes = 1 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 11, .lenBytes = 1 }
        )},
        (LexerTest) { .name = allocLit(ar, "Operator assignment in parens error 1"),
            .input = allocLit(ar, "x (+= y) + 5"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorOperatorAssignmentPunct),3, ar,
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokParens, .startByte = 3 }
        )},
        (LexerTest) { .name = allocLit(ar, "Operator assignment multiple error 1"),
            .input = allocLit(ar, "x := y := 7"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorOperatorMultipleAssignment), 3, ar,
                (Token){ .tp = tokAssignment, .payload1 = opTDefinition << 2, .payload2 = 9, .startByte = 0, .lenBytes = 19 },
                (Token){ .tp = tokStmt, .startByte = 0, .lenBytes = 1 },
                (Token){ .tp = tokStmt, .startByte = 5, .lenBytes = 1 }
        )}         
    );
}


LexerTestSet* functionTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Function lexer tests"), 1, ar,
        (LexerTest) { .name = allocLit(ar, "Function simple 1"),
            .input = allocLit(ar, "foo = (.x y. x - y)"),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokAssignment, .payload2 = opTDefinition << 2, .startByte = 0, .lenBytes = 19 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 0, .lenBytes = 3 },
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokLambda, .payload2 = 7, .startByte = 7, .lenBytes = 11 },           
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 8, .lenBytes = 1 },
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 10, .lenBytes = 1 },
                (Token){ .tp = tokStmt, .payload2 = 3, .startByte = 13, .lenBytes = 5 },                     
                (Token){ .tp = tokWord, .payload2 = 0, .startByte = 13, .lenBytes = 1 },                
                (Token){ .tp = tokOperator, .payload2 = opTMinus, .startByte = 15, .lenBytes = 1 },                
                (Token){ .tp = tokWord, .startByte = 17, .lenBytes = 1 }
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
    //runATestSet(&operatorTests, &countPassed, &countTests, lang, a);
    
    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
