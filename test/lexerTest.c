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

/** Must agree in order with token types in Lexer.h */
const char* tokNames[] = {
    "int", "float", "bool", "string", "_", "docComment", "compoundString", "word", ".word", "@word", "reserved", "operator"
};

static Lexer* buildLexer(int totalTokens, Arena *ar, /* Tokens */ ...) {
    Lexer* result = createLexer(&empty, ar);
    if (result == NULL) return result;
    
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, ar);
    
    for (int i = 0; i < totalTokens; i++) {
        addToken(va_arg(tokens, Token), result);
    }
    
    va_end(tokens);
    return result;
}


Lexer* buildLexerWithError(String* errMsg, int totalTokens, Arena *ar, /* Tokens */ ...) {
    Lexer* result = allocateOnArena(ar, sizeof(Lexer));
    result->wasError = true;
    result->errMsg = errMsg;
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, ar);
    
    result->tokens = allocateOnArena(ar, totalTokens*sizeof(Token));
    if (result == NULL) return result;
    
    for (int i = 0; i < totalTokens; i++) {
        result->tokens[i] = va_arg(tokens, Token);
    }
    
    va_end(tokens);
    return result;
}


LexerTestSet* createTestSet(String* name, int count, Arena *ar, ...) {
    LexerTestSet* result = allocateOnArena(ar, sizeof(LexerTestSet));
    result->name = name;
    result->totalTests = count;
    
    va_list tests;
    va_start (tests, ar);
    
    result->tests = allocateOnArena(ar, count*sizeof(LexerTest));
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
    }
}


LexerTestSet* wordTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Word lexer test"), 13, ar,
        (LexerTest) { 
            .name = allocLit(ar, "Simple word lexing"),
            .input = allocLit(ar, "asdf Abc"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word snake case"),
            .input = allocLit(ar, "asdf_abc"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordUnderscoresOnlyAtStart), 0, ar)
        },
        (LexerTest) {
            .name = allocLit(ar, "Word correct capitalization 1"),
            .input = allocLit(ar, "Asdf.abc"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 8  }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word correct capitalization 2"),
            .input = allocLit(ar, "asdf.abcd.zyui"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 14  }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word correct capitalization 3"),
            .input = allocLit(ar, "Asdf.Abcd"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 9, .payload2 = 1  }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word incorrect capitalization"),
            .input = allocLit(ar, "Asdf.Abcd"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordCapitalizationOrder), 0, ar)
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with underscore and lowercase letter"),
            .input = allocLit(ar, "_abc"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with underscore and capital letter"),
            .input = allocLit(ar, "_Abc"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with 2 underscores"),
            .input = allocLit(ar, "__abc"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordChunkStart), 0, ar)
        },
        (LexerTest) {
            .name = allocLit(ar, "Word starts with underscore and digit"),
            .input = allocLit(ar, "_1abc"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordChunkStart), 0, ar)
        },
        (LexerTest) {
            .name = allocLit(ar, "Dotword & @-word"),
            .input = allocLit(ar, "@a123 .Abc "),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 4, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Dot-reserved word error"),
            .input = allocLit(ar, ".false"),
            .expectedOutput = buildLexerWithError(allocLit(ar, errorWordReservedWithDot), 0, ar)
        },
        (LexerTest) {
            .name = allocLit(ar, "At-reserved word"),
            .input = allocLit(ar, "@if"),
                        .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 3 }
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
                (Token){ .tp = tokStmt, .payload2 = 1, startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokInt, .payload2 = 5, .startByte = 5, .lenBytes = 5 }
            )},        
        (LexerTest) { .name = allocLit(ar, "Binary numeric 64-bit positive"),
            .input = allocLit(ar, "0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 81 },
                (Token){ .tp = tokInt, .startByte = 7, .lenBytes = 0, .payload2 = 81 }
            )},
        (LexerTest) { 
            .name = allocLit(ar, "Binary numeric 64-bit negative"),
            .input = allocLit(ar, "0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0111"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 81 },
                (Token){ .tp = tokInt, .payload2 = -7, .startByte = 0, .lenBytes = 81 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Binary numeric 65-bit error"),
            .input = allocLit(ar, "0b0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_01110"),
            .expectedOutput = buildLexerWithError(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 1"),
            .input = allocLit(ar, "0x15"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 21, .lenBytes = 0, .payload2 = 4 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 2"),
            .input = allocLit(ar, "0x05"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 0, .payload2 = 4 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 3"),
            .input = allocLit(ar, "0xFFFFFFFFFFFFFFFF"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 18 },
                (Token){ .tp = tokInt, .payload2 = -1, .startByte = 0, .lenBytes = 18  }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric 4"),
            .input = allocLit(ar, "0xFFFFFFFFFFFFFFFE"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Hex numeric too long"),
            .input = allocLit(ar, "0xFFFFFFFFFFFFFFFF0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },  
        (LexerTest) { 
            .name = allocLit(ar, "Float numeric 1"),
            .input = allocLit(ar, "1.234"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
            )
        },
        (LexerTest) { 
            .name = allocLit(ar, "Float numeric 2"),
            .input = allocLit(ar, "00001.234"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric 3"),
            .input = allocLit(ar, "10500.01"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, , .payload2 = 1.startByte = 5, .lenBytes = 3 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric 4"),
            .input = allocLit(ar, "0.9"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric 5"),
            .input = allocLit(ar, "100500.123456"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .payload2 = 1, .startByte = 5, .lenBytes = 3 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric big"),
            .input = allocLit(ar, "9007199254740992.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric too big"),
            .input = allocLit(ar, "9007199254740993.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric big exponent"),
            .input = allocLit(ar, "1005001234560000000000.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric tiny"),
            .input = allocLit(ar, "0.0000000000000000000003"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocLit(ar, "Float numeric negative 1"),
            .input = allocLit(ar, "-9.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },              
        (LexerTest) {
            .name = allocLit(ar, "Float numeric negative 2"),
            .input = allocLit(ar, "-8.775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },       
        (LexerTest) {
            .name = allocLit(ar, "Float numeric negative 3"),
            .input = allocLit(ar, "-1005001234560000000000.0"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },        
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 1"),
            .input = allocLit(ar, "3"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },                  
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 2"),
            .input = allocLit(ar, "12"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },    
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 3"),
            .input = allocLit(ar, "0987_12"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },    
        (LexerTest) {
            .name = allocLit(ar, "Int numeric 4"),
            .input = allocLit(ar, "9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },           
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric negative 1"),
            .input = allocLit(ar, "-1"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokInt, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },          
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric negative 2"),
            .input = allocLit(ar, "-775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },   
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric negative 3"),
            .input = allocLit(ar, "-9_223_372_036_854_775_807"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },      
        (LexerTest) { 
            .name = allocLit(ar, "Int numeric error 1"),
            .input = allocLit(ar, "3_"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
        )},       
        (LexerTest) { .name = allocLit(ar, "Int numeric error 2"),
            .input = allocLit(ar, "9_223_372_036_854_775_808"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
        )}                                             
    );
}


LexerTestSet* stringTests(Arena* ar) {
    return createTestSet(allocLit(ar, "String literals lexer tests"), 3, ar,
        (LexerTest) { .name = allocLit(ar, "String simple literal"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},     
        (LexerTest) { .name = allocLit(ar, "String literal with non-ASCII inside"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},  
        (LexerTest) { .name = allocLit(ar, "String literal unclosed"),
            .input = allocLit(ar, "\"asdf"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )}            
    );
}


LexerTestSet* commentTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Comments lexer tests"), 5, ar,
        (LexerTest) { .name = allocLit(ar, "Comment simple"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},     
        (LexerTest) { .name = allocLit(ar, "Doc comment"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},  
        (LexerTest) { .name = allocLit(ar, "Doc comment before something"),
            .input = allocLit(ar, "\"asdf"),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},
        (LexerTest) { .name = allocLit(ar, "Doc comment empty"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},     
        (LexerTest) { .name = allocLit(ar, "Doc comment multiline"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )}            
    );
}

LexerTestSet* punctuationTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Punctuation lexer tests"), 5, ar,
        (LexerTest) { .name = allocLit(ar, "Parens simple"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},             
        (LexerTest) { .name = allocLit(ar, "Parens nested"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},
        (LexerTest) { .name = allocLit(ar, "Parens unclosed"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},
        (LexerTest) { .name = allocLit(ar, "Brackets simple"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},              
        (LexerTest) { .name = allocLit(ar, "Brackets nested"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},             
        (LexerTest) { .name = allocLit(ar, "Brackets mismatched"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},
        (LexerTest) { .name = allocLit(ar, "Data accessor"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},
        (LexerTest) { .name = allocLit(ar, "Punctuation scope inside statement 1"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )}, 
        (LexerTest) { .name = allocLit(ar, "Punctuation scope inside statement 2"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},             
        (LexerTest) { .name = allocLit(ar, "Punctuation all types"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},
        (LexerTest) { .name = allocLit(ar, "Colon punctuation 1"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )}                        
    );
}


LexerTestSet* operatorTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Operator lexer tests"), 5, ar,
        (LexerTest) { .name = allocLit(ar, "Operator simple 1"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},             
        (LexerTest) { .name = allocLit(ar, "Parens nested"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},
        (LexerTest) { .name = allocLit(ar, "Parens unclosed"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},
        (LexerTest) { .name = allocLit(ar, "Brackets simple"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},              
        (LexerTest) { .name = allocLit(ar, "Brackets nested"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},             
        (LexerTest) { .name = allocLit(ar, "Brackets mismatched"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},
        (LexerTest) { .name = allocLit(ar, "Data accessor"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},
        (LexerTest) { .name = allocLit(ar, "Punctuation scope inside statement 1"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )}, 
        (LexerTest) { .name = allocLit(ar, "Punctuation scope inside statement 2"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )},             
        (LexerTest) { .name = allocLit(ar, "Punctuation all types"),
            .input = allocLit(ar, "\"hello мир\""),
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokStmt, .payload2 = 1, .startByte = 0, .lenBytes = 6 },
                (Token){ .tp = tokString, .startByte = 0, .lenBytes = 6 }
        )},
        (LexerTest) { .name = allocLit(ar, "Colon punctuation 1"),
            .input = allocLit(ar, "\"asdf\""),
            .expectedOutput = buildLexer(2, ar
        )}                        
    );
}


LexerTestSet* functionTests(Arena* ar) {
    return createTestSet(allocLit(ar, "Function lexer tests"), 1, ar,
        (LexerTest) { .name = allocLit(ar, "Function simple 1"),
            .input = allocLit(ar, "foo = (\x y. x - y)"),
            .expectedOutput = buildLexer(2, ar
        )},  
    );
}


int main() {
    printf("----------------------------\n");
    printf("Lexer test\n");
    printf("----------------------------\n");
    Arena *ar = mkArena();
        
    LexerTestSet* wordSet = wordTests(ar);

    LanguageDefinition* lang = buildLanguageDefinitions(ar);
    int countPassed = 0;
    int countTests = 0;

    for (int j = 0; j < wordSet->totalTests; j++) {
        LexerTest test = wordSet->tests[j];
        runLexerTest(test, &countPassed, &countTests, lang, ar);
    }
    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(ar);
}
