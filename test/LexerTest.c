#include "../source/utils/Arena.h"
#include "../source/utils/String.h"
#include "../source/utils/Stack.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#define lIntLit 0
#define lFloatLit 1
#define lStringLit 2

typedef struct {
    unsigned int tp : 6;
    unsigned int lenBytes: 26;
    unsigned int startByte;
    unsigned int payload1;
    unsigned int payload2;
} Token;

typedef struct {
    Token* tokens;
    int totalTokens;
} Lexer;

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


Token* allocTokens(int count, Arena *ar, ...) {
    va_list args;
    va_start (args, ar);
    
    Token* result = allocateOnArena(ar, count*sizeof(Token));
    if (result == NULL) return result;
    
    for (int i = 0; i < count; i++) {
        result[i] = va_arg(args, Token);
    }    
    
    va_end(args);
    return result;
}


Lexer* createLexer(int totalTokens, Arena *ar, Token* tokens) {
    Lexer* result = allocateOnArena(ar, sizeof(Lexer));
    result->totalTokens = totalTokens;
    result->tokens = tokens;
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


int main() {
    printf("Hello test\n");
    Arena *ar = mkArena();
    
    String* str = allocateLiteral(ar, "Hello from F!");
    printf("Sizeof LexNode is %d\n", sizeof(Token));
    printString(str);
    
    LexerTestSet* testSet = createTestSet(allocateLiteral(ar, "Number lexing"), 2, ar,
        (LexerTest) { .name = allocateLiteral(ar, "Test 1"), .input = allocateLiteral(ar, "1.234"), 
            .expectedOutput = createLexer(2, ar, allocTokens(2, ar, 
                (Token){ .tp = lStringLit, .lenBytes = 5, .startByte = 0, .payload1 = 7 },
                (Token){ .tp = lFloatLit, .lenBytes = 4, .startByte = 6, .payload1 = 5 }
            )),
        },
        (LexerTest) { .name = allocateLiteral(ar, "Test 2"), .input = allocateLiteral(ar, "asdf"), 
            .expectedOutput = createLexer(1, ar, allocTokens(1, ar, 
                (Token){ .tp = lStringLit, .lenBytes = 5, .startByte = 0, .payload1 = 7 }
            )),
        }
    );
    
    if (testSet == NULL) printf("It's null\n");

    printString(testSet->name);
    
    for (int j = 0; j < testSet->totalTests; j++) {
        printf("I'mhere\n");
        printString(testSet->tests[j].name);
    }
    
    
    deleteArena(ar);
	
    
}
