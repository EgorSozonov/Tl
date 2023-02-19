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


int main() {
    printf("----------------------------\n");
    printf("Lexer test\n");
    printf("----------------------------\n");
    Arena *ar = mkArena();
        
    LexerTestSet* wordSet = createTestSet(allocateLiteral(ar, "Word lexer test"), 13, ar,
        (LexerTest) { 
            .name = allocateLiteral(ar, "Simple word lexing"), 
            .input = allocateLiteral(ar, "asdf Abc"), 
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Token){ .tp = tokWord, .startByte = 5, .lenBytes = 3, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word snake case"),
            .input = allocateLiteral(ar, "asdf_abc"),
            .expectedOutput = buildLexerWithError(allocateLiteral(ar, errorWordUnderscoresOnlyAtStart), 0, ar)
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word correct capitalization 1"),
            .input = allocateLiteral(ar, "Asdf.abc"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 8  }
            )
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word correct capitalization 2"),
            .input = allocateLiteral(ar, "asdf.abcd.zyui"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 14  }
            )
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word correct capitalization 3"),
            .input = allocateLiteral(ar, "Asdf.Abcd"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 9, .payload2 = 1  }
            )
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word incorrect capitalization"),
            .input = allocateLiteral(ar, "Asdf.Abcd"),
            .expectedOutput = buildLexerWithError(allocateLiteral(ar, errorWordCapitalizationOrder), 0, ar)
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word starts with underscore and lowercase letter"),
            .input = allocateLiteral(ar, "_abc"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4 }
            )
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word starts with underscore and capital letter"),
            .input = allocateLiteral(ar, "_Abc"),
            .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 4, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word starts with 2 underscores"),
            .input = allocateLiteral(ar, "__abc"),
            .expectedOutput = buildLexerWithError(allocateLiteral(ar, errorWordChunkStart), 0, ar)
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Word starts with underscore and digit"),
            .input = allocateLiteral(ar, "_1abc"),
            .expectedOutput = buildLexerWithError(allocateLiteral(ar, errorWordChunkStart), 0, ar)
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Dotword & @-word"),
            .input = allocateLiteral(ar, "@a123 .Abc "),
            .expectedOutput = buildLexer(2, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 5 },
                (Token){ .tp = tokWord, .startByte = 6, .lenBytes = 4, .payload2 = 1 }
            )
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "Dot-reserved word error"),
            .input = allocateLiteral(ar, ".false"),
            .expectedOutput = buildLexerWithError(allocateLiteral(ar, errorWordReservedWithDot), 0, ar)
        },
        (LexerTest) {
            .name = allocateLiteral(ar, "At-reserved word"),
            .input = allocateLiteral(ar, "@if"),
                        .expectedOutput = buildLexer(1, ar,
                (Token){ .tp = tokWord, .startByte = 0, .lenBytes = 3 }
            )
        }
    );

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
