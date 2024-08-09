#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../src/eyr.internal.h"
#include "../include/eyr.h"
#include "eyrTest.h"

//{{{ Definitions

typedef struct {
    String name;
    String input;
    String expectedOutput;
} CodegenTest;


typedef struct {
    String name;
    Int totalTests;
    Arr(CodegenTest) tests;
} CodegenTestSet;

//}}}
//{{{ Utils

#define S   70000000 // A constant larger than the largest allowed file size. Separates parsed
                     // names from others


private CodegenTestSet* createTestSet0(String name, Arena *a, int count, Arr(CodegenTest) tests) {
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


void runCodegenTest(CodegenTest test, TestContext* ct) {
// Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code
    ct->countTests += 1;
    String result = eyrCompile(test.input);
    printString(test.input);
    printString(result);
    const Int coreSize = getCoreLibSize();

    String interestingPart = result.len > 0 ?
        (String){
            .cont = result.cont + coreSize, // +1 for the newline char
            .len = result.len - coreSize
        }
        : empty ;
    if (equal(interestingPart, test.expectedOutput)) {
        ct->countPassed += 1;
    } else {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nExpected: \n");
        printString(test.expectedOutput);
        printf("\nBut got: \n");
        printString(interestingPart);
    }
}


void runATestSet(CodegenTestSet* (*testGenerator)(Arena*), TestContext* ct) {
    CodegenTestSet* testSet = (testGenerator)(ct->a);
    for (int j = 0; j < testSet->totalTests; j++) {
        CodegenTest test = testSet->tests[j];
        runCodegenTest(test, ct);
    }
}

//}}}
//{{{ Expressions

CodegenTestSet* exprTests(Arena* a) {
    return createTestSet(s("Expression test set"), a, ((CodegenTest[]){
        (CodegenTest){.name = s("Simple assignment"),
            .input = s("main = (( a = 78; print a))"),
            .expectedOutput = s("function main() {\n"
                    "    const a = 78;\n"
                    "    console.log(a);\n"
                    "}")
            }
    }));
}
//}}}

int main(int argc, char** argv) {
    printf("----------------------------\n");
    printf("--  CODEGEN TEST  --\n");
    printf("----------------------------\n");

    auto ct = (TestContext){ .countTests = 0, .countPassed = 0, .a = createArena() };
    runATestSet(&exprTests, &ct);

    if (ct.countTests == 0) {
        print("\nThere were no tests to run!");
    } else if (ct.countPassed == ct.countTests) {
        print("\nAll %d tests passed!", ct.countTests);
    } else {
        print("\nFailed %d tests out of %d!", (ct.countTests - ct.countPassed), ct.countTests);
    }

    deleteArena(ct.a);
}
