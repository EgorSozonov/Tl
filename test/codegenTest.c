#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../src/eyr.internal.h"
#include "../src/eyr.h"
#include "eyrTest.h"

//{{{ Definitions

typedef struct {
    String* name;
    String* input;
    String* expectedOutput;
} CodegenTest;


typedef struct {
    String* name;
    Int totalTests;
    CodegenTest* tests;
} CodegenTestSet;

//}}}
//{{{ Utils

#define S   70000000 // A constant larger than the largest allowed file size. Separates parsed
                     // names from others


private CodegenTestSet* createTestSet0(String* name, Arena *a, int count, Arr(CodegenTest) tests) {
    CodegenTestSet* result = allocateOnArena(sizeof(CodegenTestSet), a);
    result->name = name;
    result->totalTests = count;
    result->tests = allocateOnArena(count*sizeof(CodegenTest), a);
    if (result->tests == NULL) return result;
    for (int i = 0; i < count; i++) {
        result->tests[i] = tests[i];
    }
    return result;
}

#define createTestSet(n, a, tests) createTestSet0(n, a, sizeof(tests)/sizeof(CodegenTest), tests)


void runCodegenTest(CodegenTest test, int* countPassed, int* countTests, Compiler* proto, 
                    Arena *a) {
// Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code
    *countTests += 1;
    Compiler* result = lexicallyAnalyze(test.input, proto, a);

    int equalityStatus = equalityLexer(*result, *test.expectedOutput);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(result->stats.errMsg);
        if (test.expectedOutput->stats.wasError) {
            printf("\nBut was expected: ");
            printString(test.expectedOutput->stats.errMsg);
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

//}}}
//{{{ Expressions

CodegenTestSet* exprTests(Arena* a) {
    return createTestSet(s("Expression test set"), a, ((CodegenTest[]){
        (CodegenTest){.name = s("Simple assignment"),
            .input = s("x = 12"),
            .output = s("function main() {\n"
                    "    const x = 12;\n"
                    "}")
            })
    });
}
//}}}

int main(int argc, char** argc) {
    printf("----------------------------\n");
    printf("--  CODEGEN TEST  --\n");
    printf("----------------------------\n");
    eyrInitCompiler();
    Arena *a = createArena();
    Compiler* proto = createProtoCompiler(a);

    int countPassed = 0;
    int countTests = 0;
    runATestSet(&exprTests, &countPassed, &countTests, proto, a);

    if (countTests == 0) {
        print("\nThere were no tests to run!");
    } else if (countPassed == countTests) {
        print("\nAll %d tests passed!", countTests);
    } else {
        print("\nFailed %d tests out of %d!", (countTests - countPassed), countTests);
    }

    deleteArena(a);
}
