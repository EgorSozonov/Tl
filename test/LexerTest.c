#include "../source/utils/Arena.h"
#include "../source/utils/String.h"
#include "../source/utils/Stack.h"
#include "../source/compiler/lexer/Lexer.h"
#include "../source/compiler/lexer/LexerConstants.h"
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


Lexer* buildLexer(int totalTokens, Arena *ar, /* Tokens */ ...) {
    Lexer* result = createLexer(ar);
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

bool equalityLexer(Lexer a, Lexer b) {
    if (a.wasError != b.wasError || a.totalTokens != b.totalTokens || !endsWith(a.errMsg, b.errMsg)) {
        return false;
    }
    
}


int main() {
    printf("Hello test\n");
    Arena *ar = mkArena();
    
    String* str = allocateLiteral(ar, "Hello from F!");
    printf("Sizeof LexNode is %d\n", sizeof(Token));
    printString(str);
    
    LexerTestSet* testSet = createTestSet(allocateLiteral(ar, "Word lexer test"), 2, ar,
        (LexerTest) { .name = allocateLiteral(ar, "Simple word lexing"), .input = allocateLiteral(ar, "asdf Abc"), 
            .expectedOutput = buildLexer(2, ar, 
                (Token){ .tp = tokWord, .lenBytes = 4, .startByte = 0 },
                (Token){ .tp = tokWord, .lenBytes = 3, .startByte = 5, .payload2 = 1 }
            )
        },
        (LexerTest) { .name = allocateLiteral(ar, "Word snake case"), .input = allocateLiteral(ar, "asdf_abc"), 
            .expectedOutput = buildLexerWithError(allocateLiteral(ar, "Simple word lexing"), 0, ar)
        }
    );
    
    if (testSet == NULL) printf("It's null\n");

    printString(testSet->name);
    
    for (int j = 0; j < testSet->totalTests; j++) {
        printString(testSet->tests[j].name);
    }
    
    
    deleteArena(ar);
	
    
}
