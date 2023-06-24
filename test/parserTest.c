#include "../source/utils/aliases.h"
#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/structures/stack.h"
#include "../source/compiler/compiler.h"
#include "../source/compiler/compilerConstants.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    String* name;
    Lexer* input;
    Compiler* initParser;
    Compiler* expectedOutput;
} ParserTest;


typedef struct {
    String* name;
    Int totalTests;
    ParserTest* tests;
} ParserTestSet;

#define B  70000000 // A constant larger than the largest allowed file size to distinguish unshifted binding ids
#define M 140000000 // The "zero" for construction of bindings and overloads
#define F (M - countOperators - 2) // The base index for imported (but not built-in) overloads

/** Must agree in order with node types in ParserConstants.h */
const char* nodeNames[] = {
    "Int", "Long", "Float", "Bool", "String", "_", "DocComment", 
    "id", "call", "binding", "type", "and", "or", 
    "(-", "expr", "assign", "reAssign", "mutate",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue",
    "defer", "each", "embed", "export", "exposePriv", "fnDef", "interface",
    "lambda", "meta", "package", "return", "struct", "try", "yield",
    "ifClause", "else", "loop", "loopCond", "if", "ifPr", "impl", "match"
};


private Compiler* buildParserWithError0(String* errMsg, Lexer* lx, Arena *a, int nextInd, Arr(Node) nodes) {
    Compiler* result = createCompiler(lx, a);
    (*result) = (Compiler) { .wasError = true, .errMsg = errMsg, .nextInd = nextInd,
        .nodes = allocateOnArena(nextInd*sizeof(Token), a)};

    if (result == NULL) return result;
    
    for (int i = 0; i < nextInd; i++) {
        result->nodes[i] = nodes[i];
    }    
    return result;
}

#define buildParserWithError(msg, lx, a, nodes) buildParserWithError0(msg, lx, a, sizeof(nodes)/sizeof(Node), nodes)


private ParserTestSet* createTestSet0(String* name, Arena *a, int count, Arr(ParserTest) tests) {
    ParserTestSet* result = allocateOnArena(sizeof(ParserTestSet), a);
    result->name = name;
    result->totalTests = count;
    result->tests = allocateOnArena(count*sizeof(ParserTest), a);
    if (result->tests == NULL) return result;
    
    for (int i = 0; i < count; i++) {
        result->tests[i] = tests[i];
    }
    return result;
}

#define createTestSet(n, a, tests) createTestSet0(n, a, sizeof(tests)/sizeof(ParserTest), tests)

private Int transformBindingEntityId(Int inputFromTestData, Compiler* pr) {
    if (inputFromTestData < B) {
        return (inputFromTestData < 0) ? (inputFromTestData - pr->entOverloadZero)
                                       : (inputFromTestData + pr->entBindingZero);
    } else {
        return inputFromTestData - M;
    }
}

/** Creates a test with two parser: one is the init parser (contains all the "imported" bindings)
 *  and one is the output parser (with the bindings and the expected nodes). When the test is run,
 *  the init parser will parse the tokens and then will be compared to the expected output parser.
 *  Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
 *  it will be inserted as 1 + (the number of built-in bindings)
 */
private ParserTest createTest0(String* name, String* input, Arr(Node) nodes, Int countNodes, 
                               Arr(EntityImport) bindings, Int countBindings,
                               Arr(OverloadImport) overloadCounts, Int countOverloadCounts,
                               LanguageDefinition* langDef, Arena* a) {
    Lexer* lx = lexicallyAnalyze(input, langDef, a);
    Compiler* initParser     = createCompiler(lx, a);
    Compiler* expectedParser = createCompiler(lx, a);
    if (expectedParser->wasError) {
        return (ParserTest){ .name = name, .input = lx, .initParser = initParser, .expectedOutput = expectedParser };
    }
    
    importEntities(bindings, countBindings, initParser);
    importEntities(bindings, countBindings, expectedParser);
    importOverloads(overloadCounts, countOverloadCounts, initParser);
    importOverloads(overloadCounts, countOverloadCounts, expectedParser);
    initParser->entBindingZero = initParser->entNext;
    initParser->entOverloadZero = initParser->overlCNext;
    expectedParser->entBindingZero = initParser->entBindingZero;
    expectedParser->entOverloadZero = initParser->entOverloadZero;

    for (Int i = 0; i < countNodes; i++) {
        untt nodeType = nodes[i].tp;
        // All the node types which contain bindingIds
        if (nodeType == nodId || nodeType == nodCall || nodeType == nodBinding || nodeType == nodBinding
         || nodeType == nodFnDef) {
            addNode((Node){ .tp = nodeType, .pl1 = transformBindingEntityId(nodes[i].pl1, expectedParser), .pl2 = nodes[i].pl2, 
                            .startBt = nodes[i].startBt, .lenBts = nodes[i].lenBts }, 
                    expectedParser);
        } else {
            addNode(nodes[i], expectedParser);
        }
    }
    return (ParserTest){ .name = name, .input = lx, .initParser = initParser, .expectedOutput = expectedParser };
}

#define createTest(name, input, nodes, bindings, overloadCounts) createTest0((name), (input), \
    (nodes), sizeof(nodes)/sizeof(Node), bindings, sizeof(bindings)/sizeof(EntityImport), \
    overloadCounts, sizeof(overloadCounts)/sizeof(OverloadImport), langDef, a)

/** Creates a test with two parser: one is the init parser (contains all the "imported" bindings)
 *  and one is the output parser (with the bindings and the expected nodes). When the test is run,
 *  the init parser will parse the tokens and then will be compared to the expected output parser.
 *  Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
 *  it will be inserted as 1 + (the number of built-in bindings)
 */
private ParserTest createTestWithError0(String* name, String* message, String* input, Arr(Node) nodes, Int countNodes, 
                               Arr(EntityImport) bindings, Int countBindings,
                               Arr(OverloadImport) overloadCounts, Int countOverloadCounts,
                               LanguageDefinition* langDef, Arena* a) {
    ParserTest theTest = createTest0(name, input, nodes, countNodes, bindings, countBindings, overloadCounts, countOverloadCounts,
                                     langDef, a);
    theTest.expectedOutput->wasError = true;
    theTest.expectedOutput->errMsg = message;
    return theTest;
}

#define createTestWithError(name, errorMessage, input, nodes, bindings, overloadCounts) createTestWithError0((name), \
    errorMessage, (input), (nodes), sizeof(nodes)/sizeof(Node), bindings, sizeof(bindings)/sizeof(EntityImport), \
    overloadCounts, sizeof(overloadCounts)/sizeof(OverloadImport), langDef, a)    


/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
testable int equalityParser(Compiler a, Compiler b) {
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.nextInd < b.nextInd ? a.nextInd : b.nextInd;
    int i = 0;
    for (; i < commonLength; i++) {
        Node nodA = a.nodes[i];
        Node nodB = b.nodes[i];
        if (nodA.tp != nodB.tp || nodA.lenBts != nodB.lenBts || nodA.startBt != nodB.startBt 
            || nodA.pl1 != nodB.pl1 || nodA.pl2 != nodB.pl2) {
            printf("\n\nUNEQUAL RESULTS\n", i);
            if (nodA.tp != nodB.tp) {
                printf("Diff in tp, %s but was expected %s\n", nodeNames[nodA.tp], nodeNames[nodB.tp]);
            }
            if (nodA.lenBts != nodB.lenBts) {
                printf("Diff in lenBts, %d but was expected %d\n", nodA.lenBts, nodB.lenBts);
            }
            if (nodA.startBt != nodB.startBt) {
                printf("Diff in startBt, %d but was expected %d\n", nodA.startBt, nodB.startBt);
            }
            if (nodA.pl1 != nodB.pl1) {
                printf("Diff in pl1, %d but was expected %d\n", nodA.pl1, nodB.pl1);
            }
            if (nodA.pl2 != nodB.pl2) {
                printf("Diff in pl2, %d but was expected %d\n", nodA.pl2, nodB.pl2);
            }            
            return i;
        }
    }
    return (a.nextInd == b.nextInd) ? -2 : i;        
}


void printParser(Compiler* a, Arena* ar) {
    if (a->wasError) {
        printf("Error: ");
        printString(a->errMsg);
    }
    Int indent = 0;
    Stackint32_t* sentinels = createStackint32_t(16, ar);
    for (int i = 0; i < a->nextInd; i++) {
        Node nod = a->nodes[i];
        for (int m = sentinels->length - 1; m > -1 && (*sentinels->content)[m] == i; m--) {
            popint32_t(sentinels);
            indent--;
        }
        if (i < 10) printf(" ");
        printf("%d: ", i);
        for (int j = 0; j < indent; j++) {
            printf("  ");
        }
        if (nod.pl1 != 0 || nod.pl2 != 0) {            
            printf("%s %d %d [%d; %d]\n", nodeNames[nod.tp], nod.pl1, nod.pl2, nod.startBt, nod.lenBts);
        } else {
            printf("%s [%d; %d]\n", nodeNames[nod.tp], nod.startBt, nod.lenBts);
        }
        if (nod.tp >= nodScope && nod.pl2 > 0) {   
            pushint32_t(i + nod.pl2 + 1, sentinels);
            indent++;
        }
    }
}

/** Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code */
void runParserTest(ParserTest test, int* countPassed, int* countTests, Arena *a) {    
    (*countTests)++;
    if (test.input->wasError) {
        print("Lexer was not without error");
        printLexer(test.input);
        return;
    } else if (test.input->nextInd == 0) {
        print("Lexer result empty");
        return;
    }

    //printLexer(test.input);printf("\n");

    Compiler* resultParser = parseWithCompiler(test.input, test.initParser, a);
        
    int equalityStatus = equalityParser(*resultParser, *test.expectedOutput);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(resultParser->errMsg);
        printf("\nBut was expected: ");
        printString(test.expectedOutput->errMsg);
        printf("\n");
        print("    LEXER:")
        printLexer(test.input);
        print("    PARSER:")
        printParser(resultParser, a);

    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On node %d\n", equalityStatus);
        print("    LEXER:")
        printLexer(test.input);
        print("    PARSER:")
        printParser(resultParser, a);
    }
}


ParserTestSet* assignmentTests(LanguageDefinition* langDef, Arena* a) {
    return createTestSet(s("Assignment test set"), a, ((ParserTest[]){
        createTest(
            s("Simple assignment"), 
            s("x = 12"),            
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                    (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 }
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),
        createTest(
            s("Double assignment"), 
            s("x = 12\n"
              "second = x"  
            ),
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                    (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 },     // x
                    (Node){ .tp = nodInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 },
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 7, .lenBts = 10 },
                    (Node){ .tp = nodBinding, .pl1 = 1, .startBt = 7, .lenBts = 6 }, // second
                    (Node){ .tp = nodId, .pl1 = 0, .pl2 = 0, .startBt = 16, .lenBts = 1 }
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),        
        createTestWithError(
            s("Assignment shadowing error"), 
            s(errorAssignmentShadowing),
            s("x = 12\n"
              "x = 7"  
            ),
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                    (Node){ .tp = nodBinding, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                    (Node){ .tp = nodInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 },
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        )          
    }));
}


ParserTestSet* expressionTests(LanguageDefinition* langDef, Arena* a) {
    return createTestSet(s("Expression test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function call"), 
            s("x = foo 10 2 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 6, .startBt = 0, .lenBts = 14 },
                    // " + 1" because the first binding is taken up by the "imported" function, "foo"
                    (Node){ .tp = nodBinding, .pl1 = 0,        .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodExpr,  .pl2 = 4,          .startBt = 4, .lenBts = 10 },
                    (Node){ .tp = nodCall, .pl1 = F, .pl2 = 3, .startBt = 4, .lenBts = 3 }, // the 0th overload
                    (Node){ .tp = nodInt, .pl2 = 10,           .startBt = 8, .lenBts = 2 },
                    (Node){ .tp = nodInt, .pl2 = 2,            .startBt = 11, .lenBts = 1 },
                    (Node){ .tp = nodInt, .pl2 = 3,            .startBt = 13, .lenBts = 1 }                    
            })),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("foo"), .count = 1}})
        ),
        createTest(
            s("Unary operator as first-class value"), 
            s("x = map (--) coll"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 5,        .startBt = 0, .lenBts = 17 },
                    (Node){ .tp = nodBinding, .pl1 = 0,           .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodExpr,  .pl2 = 3,             .startBt = 4, .lenBts = 13 },
                    (Node){ .tp = nodCall, .pl1 = F, .pl2 = 2,    .startBt = 4, .lenBts = 3 }, // map
                    (Node){ .tp = nodId, .pl1 = opTDecrement + M, .startBt = 9, .lenBts = 2 }, // --
                    (Node){ .tp = nodId, .pl1 = M, .pl2 = 2,      .startBt = 13, .lenBts = 4 } // coll
            })),
            ((EntityImport[]) {(EntityImport){ .name = s("coll"), .entity = (Entity){ }}}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("map"), .count = 1}})
        ),
        createTest(
            s("Non-unary operator as first-class value"), 
            s("x = bimap / dividends divisors"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment,            .pl2 = 6, .startBt = 0, .lenBts = 30 },
                    // "3" because the first binding is taken up by the "imported" function, "foo"
                    (Node){ .tp = nodBinding, .pl1 = 0,               .startBt = 0, .lenBts = 1 },    // x
                    (Node){ .tp = nodExpr,                  .pl2 = 4, .startBt = 4, .lenBts = 26 },
                    (Node){ .tp = nodCall, .pl1 = F,        .pl2 = 3, .startBt = 4, .lenBts = 5 },    // bimap
                    (Node){ .tp = nodId, .pl1 = -opTDivBy - 2 + M,    .startBt = 10, .lenBts = 1 },   // /
                    (Node){ .tp = nodId, .pl1 = M,      .pl2 = 2,     .startBt = 12, .lenBts = 9 },   // dividends
                    (Node){ .tp = nodId, .pl1 = M + 1,      .pl2 = 3, .startBt = 22, .lenBts = 8 }    // divisors
            })),
            ((EntityImport[]) {
                               (EntityImport){ .name = s("dividends"), .entity = (Entity){}},
                               (EntityImport){ .name = s("divisors"), .entity = (Entity){}}
            }),
            ((OverloadImport[]) {(OverloadImport){ .name = s("bimap"), .count = 1}})
        ), 
        createTest(
            s("Nested function call 1"), 
            s("x = foo 10 (bar) 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 6, .startBt = 0, .lenBts = 18 },                    
                    (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodExpr,  .pl2 = 4, .startBt = 4, .lenBts = 14 },
                    (Node){ .tp = nodCall, .pl1 = F, .pl2 = 3, .startBt = 4, .lenBts = 3 }, // foo
                    (Node){ .tp = nodInt, .pl2 = 10,  .startBt = 8, .lenBts = 2 },                    
                    (Node){ .tp = nodCall, .pl1 = F - 1, .pl2 = 0, .startBt = 12, .lenBts = 3 }, // bar
                    (Node){ .tp = nodInt, .pl2 = 3,   .startBt = 17, .lenBts = 1 }               
            })),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("foo"), .count = 1},
                                 (OverloadImport){ .name = s("bar"), .count = 1}})
        ),
        createTest(
            s("Nested function call 2"), 
            s("x =  foo 10 (barr 3)"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 6, .startBt = 0, .lenBts = 20 },                    
                    (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodExpr,  .pl2 = 4, .startBt = 5, .lenBts = 15 },
                    (Node){ .tp = nodCall, .pl1 = F, .pl2 = 2, .startBt = 5, .lenBts = 3 },   // foo
                    (Node){ .tp = nodInt, .pl2 = 10,  .startBt = 9, .lenBts = 2 },
                                       
                    (Node){ .tp = nodCall, .pl1 = F - 1, .pl2 = 1, .startBt = 13, .lenBts = 4 },  // barr
                    (Node){ .tp = nodInt,   .pl2 = 3, .startBt = 18, .lenBts = 1 }
            })),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("foo"), .count = 1},
                                 (OverloadImport){ .name = s("barr"), .count = 1}})
        ),
        createTest(
            s("Triple function call"), 
            s("x = buzz 2 3 4 (foo : triple 5)"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 9, .startBt = 0, .lenBts = 31 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodExpr, .pl2 = 7, .startBt = 4, .lenBts = 27 },
                (Node){ .tp = nodCall, .pl1 = F - 1, .pl2 = 4, .startBt = 4, .lenBts = 4 }, // buzz
                (Node){ .tp = nodInt, .pl2 = 2, .startBt = 9, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 3, .startBt = 11, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 4, .startBt = 13, .lenBts = 1 },

                (Node){ .tp = nodCall, .pl1 = F, .pl2 = 1, .startBt = 16, .lenBts = 3 }, // foo
                (Node){ .tp = nodCall, .pl1 = F - 2, .pl2 = 1, .startBt = 22, .lenBts = 6 },  // triple
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 29, .lenBts = 1 }
            })),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("foo"), .count = 1},
                                 (OverloadImport){ .name = s("buzz"), .count = 1},
                                 (OverloadImport){ .name = s("triple"), .count = 1}})
        ),
        createTest(
            s("Operators simple"), 
            s("x = + 1 : * 2 3"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 7, .startBt = 0, .lenBts = 15 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodExpr,  .pl2 = 5, .startBt = 4, .lenBts = 11 },
                (Node){ .tp = nodCall, .pl1 = - opTPlus - 2 + M, .pl2 = 2, .startBt = 4, .lenBts = 1 },   // +
                (Node){ .tp = nodInt, .pl2 = 1, .startBt = 6, .lenBts = 1 },
                
                (Node){ .tp = nodCall, .pl1 = - opTTimes - 2 + M, .pl2 = 2, .startBt = 10, .lenBts = 1 }, // *
                (Node){ .tp = nodInt, .pl2 = 2, .startBt = 12, .lenBts = 1 },   
                (Node){ .tp = nodInt, .pl2 = 3, .startBt = 14, .lenBts = 1 }
            })),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),
        createTestWithError(
            s("Operator arity error"),
            s(errorOperatorWrongArity),
            s("x = + 1 20 100"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .startBt = 0, .lenBts = 14 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodExpr,  .startBt = 4, .lenBts = 10 }
            })),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        )    
    }));
}

        //~ (ParserTest) { 
            //~ .name = str("Triple function call", a),
            //~ .input = str("c:foo b a :bar 5 :baz 7.2", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) {
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ }))
        //~ },

        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 1", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 2", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ }))
        //~ },       
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 3", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator arithmetic 1", a),
            //~ .input = str("(a + (b - c % 2)^11)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 2", a),
            //~ .input = str("a + -5", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },       
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 3", a),
            //~ .input = str("a + !(-5)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 4", a),
            //~ .input = str("a + !!(-3)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },             
        //~ (ParserTest) { 
            //~ .name = str("Operator arity error", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Single-item expr 1", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Single-item expr 2", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Unknown function arity", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },        
        //~ (ParserTest) { 
            //~ .name = str("Func-expr 1", a),
            //~ .input = str("(4:(a:g)):f", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },            
        //~ (ParserTest) { 
            //~ .name = str("Partial application?", a),
            //~ .input = str("5:(1 + )", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },          
        //~ (ParserTest) { 
            //~ .name = str("Partial application 2?", a),
            //~ .input = str("5:(3*a +)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Partial application error?", a),
            //~ .input = str("5:(3+ a*)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ }
        
        
        
        


//~ ParserTestSet* assignmentTests(Arena* a) {
    //~ return createTestSet(str("Assignment test set", a), 2, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple assignment 1", a),
            //~ .input = str("a = 1 + 5", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Simple assignment 2", a),
            //~ .input = str("a += 9", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ }
    //~ );
//~ }


//~ ParserTestSet* scopeTests(Arena* a) {
    //~ return createTestSet(str("Scopes test set", a), 4, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple scope 1", a),
            //~ .input = str("x = 5"
                              //~ "x:print"
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Simple scope 2", a),
            //~ .input = str("x = 123"
                              //~ "yy = x * 10"  
                              //~ "yy:print"
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Scope with curly braces 1", a),
            //~ .input = str("x = 123\n"
                              //~ "{\n"
                              //~ "yy = x * 10\n"  
                              //~ ".yy:print\n"
                              //~ "}"
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },   
        //~ (ParserTest) { 
            //~ .name = str("Scope inside statement error", a),
            //~ .input = str("x = 123 +(\n"
                              //~ "{\n"
                              //~ "yy = x * 10\n"  
                              //~ ".yy:print\n"
                              //~ "})"
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ }             
    //~ );
//~ }


//~ ParserTestSet* typesTest(Arena* a) {
    //~ return createTestSet(str("Types test set", a), 2, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple type 1", a),
            //~ .input = str("Int"
                              //~ "x:print"
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Simple type 2", a),
            //~ .input = str("List Int"
                              //~ "yy = x * 10"  
                              //~ "yy:print"
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ }           
    //~ );
//~ }
    
ParserTestSet* functionTests(LanguageDefinition* langDef, Arena* a) {
    return createTestSet(s("Functions test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function definition 1"),
            s("(.f newFn Int (x Int y Float). a = x)"),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl1 = -2, .pl2 = 6, .startBt = 0, .lenBts = 37 },
                
                (Node){ .tp = nodScope, .pl2 = 5,             .startBt = 14, .lenBts = 23 },
                (Node){ .tp = nodBinding, .pl1 = 0,           .startBt = 15, .lenBts = 1 },  // param x
                (Node){ .tp = nodBinding, .pl1 = 1,           .startBt = 21, .lenBts = 1 },  // param y
                (Node){ .tp = nodAssignment,        .pl2 = 2, .startBt = 31, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 2,           .startBt = 31, .lenBts = 1 },  // local a
                (Node){ .tp = nodId, .pl1 = 0,      .pl2 = 2, .startBt = 35, .lenBts = 1 }   // x                
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),
        createTest(
            s("Simple function definition 2"),
            s("(.f newFn Int (x Int y Float)\n"
              "    a = x\n"
              "    return a\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl1 = -2, .pl2 = 8, .startBt = 0, .lenBts = 54 },
                (Node){ .tp = nodScope,            .pl2 = 7, .startBt = 14, .lenBts = 40 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 15, .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 21, .lenBts = 1 },  // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2, .startBt = 34, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 2,          .startBt = 34, .lenBts = 1 },  // local a
                (Node){ .tp = nodId, .pl1 = 0,     .pl2 = 2, .startBt = 38, .lenBts = 1 },  // x
                (Node){ .tp = nodReturn,           .pl2 = 1, .startBt = 44, .lenBts = 8 },
                (Node){ .tp = nodId, .pl1 = 2,     .pl2 = 5, .startBt = 51, .lenBts = 1 } // a
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),
        createTest(
            s("Mutually recursive function definitions"),
            s("(.f foo Int (x Int y Float)\n"
              "    a = x\n"
              "    return bar a y\n"
              ")\n"
              "(.f bar Int (x Int y Float)\n"
              "    return foo x y"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl1 = -2, .pl2 = 11, .startBt = 0, .lenBts = 58 }, // foo
                (Node){ .tp = nodScope,           .pl2 = 10, .startBt = 12, .lenBts = 46 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 13, .lenBts = 1 },  // param x. First 2 bindings = types
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 19, .lenBts = 1 },  // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2, .startBt = 32, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 2,          .startBt = 32, .lenBts = 1 },  // local a
                (Node){ .tp = nodId,     .pl1 = 0, .pl2 = 2, .startBt = 36, .lenBts = 1 },  // x
                (Node){ .tp = nodReturn,           .pl2 = 4, .startBt = 42, .lenBts = 14 },
                (Node){ .tp = nodExpr,             .pl2 = 3, .startBt = 49, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = -3,  .pl2 = 2, .startBt = 49, .lenBts = 3 }, // bar call
                (Node){ .tp = nodId, .pl1 = 2,     .pl2 = 5, .startBt = 53, .lenBts = 1 }, // a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 3, .startBt = 55, .lenBts = 1 }, // y
                
                (Node){ .tp = nodFnDef, .pl1 = -3, .pl2 = 8, .startBt = 59,  .lenBts = 47 }, // bar
                (Node){ .tp = nodScope,            .pl2 = 7, .startBt = 71, .lenBts = 35 },
                (Node){ .tp = nodBinding, .pl1 = 3,          .startBt = 72, .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 4,          .startBt = 78, .lenBts = 1 }, // param y
                (Node){ .tp = nodReturn,           .pl2 = 4, .startBt = 91, .lenBts = 14 },
                (Node){ .tp = nodExpr,             .pl2 = 3, .startBt = 98, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = -2,  .pl2 = 2, .startBt = 98, .lenBts = 3 }, // foo call
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 2, .startBt = 102, .lenBts = 1 }, // x
                (Node){ .tp = nodId, .pl1 = 4,     .pl2 = 3, .startBt = 104, .lenBts = 1 }  // y
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        )
        //~ (ParserTest) { 
            //~ .name = str("Nested function def", a),
            //~ .input = str("fn foo Int(x Int y Int) {\n"
                              //~ "z = x*y\n"
                              //~ ".return (z + x):inner 5 2\n"
                              //~ "fn inner Int(a Int b Int c Int)(return a - 2*b + 3*c)"
                              //~ "}\n"
                              
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Function def error 1", a),
            //~ .input = str("fn newFn Int(x Int newFn Float)(return x + y)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Function def error 3", a),
            //~ .input = str("fn foo Int(x Int x Int) {\n"
                              //~ "z = x*y\n"
                              //~ ".return (z + x):inner 5 2\n"
                              //~ "fn inner Int(a Int b Int c Int)(return a - 2*b + 3*c)"
                              //~ "}\n"
                              
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ },        
        //~ (ParserTest) { 
            //~ .name = str("Function def error 3", a),
            //~ .input = str("fn foo Int(x Int y Int)"
                              
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .pl2 = 2, .startBt = 0, .lenBts = 8 },
                //~ (Node){ .tp = tokWord, .startBt = 0, .lenBts = 4 },
                //~ (Node){ .tp = tokWord, .startBt = 5, .lenBts = 3 }
            //~ )
        //~ } 
    }));
}


ParserTestSet* ifTests(LanguageDefinition* langDef, Arena* a) {
    return createTestSet(s("If test set"), a, ((ParserTest[]){
        createTest(
            s("Simple if"),
            s("x = (if == 5 5 => print \"5\")"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 10, .startBt = 0, .lenBts = 28 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 8, .startBt = 4, .lenBts = 24 },
                (Node){ .tp = nodIfClause, .pl1 = 4, .pl2 = 7, .startBt = 8, .lenBts = 19 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = -opTEquality - 2 + M, .pl2 = 2, .startBt = 8, .lenBts = 2 }, // ==
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 11, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 13, .lenBts = 1 },
                
                (Node){ .tp = nodExpr, .pl2 = 2, .startBt = 18, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = F, .pl2 = 1, .startBt = 18, .lenBts = 5 }, // print
                (Node){ .tp = nodString, .startBt = 24, .lenBts = 3 }
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("print"), .count = 1}})
        ),
        createTest(
            s("If with else"),
            s("x = (if > 5 3 => \"5\" else \"=)\")"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 10, .startBt = 0, .lenBts = 31 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 8, .startBt = 4, .lenBts = 27 },
                
                (Node){ .tp = nodIfClause, .pl1 = 4, .pl2 = 5, .startBt = 8, .lenBts = 12 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = -opTGreaterThan - 2 + M, .pl2 = 2, .startBt = 8, .lenBts = 1 }, // >
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 10, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 3, .startBt = 12, .lenBts = 1 },                
                (Node){ .tp = nodString, .startBt = 17, .lenBts = 3 },
                
                (Node){ .tp = nodElse, .pl2 = 1, .startBt = 26, .lenBts = 4 },
                (Node){ .tp = nodString, .startBt = 26, .lenBts = 4 },
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),
        createTest(
            s("If with elseif"),
            s("x = (if > 5 3  => 11\n"
              "       == 5 3 => 4)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 14, .startBt = 0, .lenBts = 40 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 12, .startBt = 4, .lenBts = 36 },
                
                (Node){ .tp = nodIfClause, .pl1 = 4, .pl2 = 5, .startBt = 8, .lenBts = 12 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = -opTGreaterThan - 2 + M, .pl2 = 2, .startBt = 8, .lenBts = 1 }, // >
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 10, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 3, .startBt = 12, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 11, .startBt = 18, .lenBts = 2 },
                
                (Node){ .tp = nodIfClause, .pl1 = 4, .pl2 = 5, .startBt = 28, .lenBts = 11 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 28, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = -opTEquality - 2 + M, .pl2 = 2, .startBt = 28, .lenBts = 2 }, // ==
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 31, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 3, .startBt = 33, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 4, .startBt = 38, .lenBts = 1 }
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),
        createTest(
            s("If with elseif and else"),
            s("x = (if > 5 3  => 11\n"
              "        == 5 3 =>  4\n"
              "       else      100)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 16, .startBt = 0, .lenBts = 63 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 14, .startBt = 4, .lenBts = 59 },
                
                (Node){ .tp = nodIfClause, .pl1 = 4, .pl2 = 5, .startBt = 8, .lenBts = 12 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = -opTGreaterThan - 2 + M, .pl2 = 2, .startBt = 8, .lenBts = 1 }, // >
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 10, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 3, .startBt = 12, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 11, .startBt = 18, .lenBts = 2 },
                
                (Node){ .tp = nodIfClause, .pl1 = 4, .pl2 = 5, .startBt = 29, .lenBts = 12 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 29, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = -opTEquality - 2 + M, .pl2 = 2, .startBt = 29, .lenBts = 2 }, // ==
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 32, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 3, .startBt = 34, .lenBts = 1 },
                (Node){ .tp = nodInt, .pl2 = 4, .startBt = 40, .lenBts = 1 },
                
                (Node){ .tp = nodElse, .pl2 = 1, .startBt = 59, .lenBts = 3 },
                (Node){ .tp = nodInt, .pl2 = 100, .startBt = 59, .lenBts = 3 }
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),    
    }));
}


ParserTestSet* loopTests(LanguageDefinition* langDef, Arena* a) {
    return createTestSet(s("Loops test set"), a, ((ParserTest[]){
        createTest(
            s("Simple loop 1"),
            s("(.f f Int(). (.loop (< x 101) (x 1). print x))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl1 = -2, .pl2 = 14, .lenBts = 46 },
                (Node){ .tp = nodScope, .pl2 = 13, .startBt = 9, .lenBts = 37 }, // function body
                
                (Node){ .tp = nodLoop, .pl1 = slScope, .pl2 = 12, .startBt = 13, .lenBts = 32 },
                
                (Node){ .tp = nodScope, .pl2 = 11, .startBt = 30, .lenBts = 15 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 31, .lenBts = 3 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 31, .lenBts = 1 }, // x
                (Node){ .tp = nodInt, .pl2 = 1, .startBt = 33, .lenBts = 1 },
                
                (Node){ .tp = nodLoopCond, .pl1 = slStmt, .pl2 = 4, .startBt = 20, .lenBts = 9 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 20, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = -opTLessThan - 2 + M, .pl2 = 2, .startBt = 21, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2, .startBt = 23, .lenBts = 1 }, // x
                (Node){ .tp = nodInt, .pl2 = 101, .startBt = 25, .lenBts = 3 },
                
                (Node){ .tp = nodExpr, .pl2 = 2, .startBt = 37, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = F, .pl2 = 1, .startBt = 37, .lenBts = 5 }, // print
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2, .startBt = 43, .lenBts = 1 }      // x
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("print"), .count = 1}})
        ),
        createTest(
            s("Loop with two complex initializers"),
            s("(.f f Int()\n"
              "    (.loop (< x 101) (x 7 y (>> 5 x))\n"
              "        print x))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl1 = -2, .pl2 = 20, .lenBts = 67 },
                (Node){ .tp = nodScope, .pl2 = 19, .startBt = 9, .lenBts = 58 }, // function body
                
                (Node){ .tp = nodLoop, .pl1 = slScope, .pl2 = 18, .startBt = 16, .lenBts = 50 },                
                (Node){ .tp = nodScope, .pl2 = 17, .startBt = 33, .lenBts = 33 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 34, .lenBts = 3 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 34, .lenBts = 1 },  // x
                (Node){ .tp = nodInt, .pl2 = 7, .startBt = 36, .lenBts = 1 },
                (Node){ .tp = nodAssignment, .pl2 = 5, .startBt = 38, .lenBts = 10 },
                (Node){ .tp = nodBinding, .pl1 = 1, .startBt = 38, .lenBts = 1 },  // y
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 40, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = -opTBitshiftRight - 2 + M, .pl2 = 2, .startBt = 41, .lenBts = 2 },
                (Node){ .tp = nodInt, .pl2 = 5, .startBt = 44, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2, .startBt = 46, .lenBts = 1 }, // x                
                
                (Node){ .tp = nodLoopCond, .pl1 = slStmt, .pl2 = 4, .startBt = 23, .lenBts = 9 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 23, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = -opTLessThan - 2 + M, .pl2 = 2, .startBt = 24, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2, .startBt = 26, .lenBts = 1 }, // x
                (Node){ .tp = nodInt, .pl2 = 101, .startBt = 28, .lenBts = 3 },
                
                (Node){ .tp = nodExpr, .pl2 = 2, .startBt = 58, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = F, .pl2 = 1, .startBt = 58, .lenBts = 5 }, // print
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2, .startBt = 64, .lenBts = 1 }      // x

            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("print"), .count = 1}})
        ),
        createTest(
            s("Loop without initializers"),
            s("(.f f Int()\n"
              "    x = 4\n"
              "    (.loop (< x 101) \n"
              "        print x))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl1 = -2, .pl2 = 14,               .lenBts = 61 },
                (Node){ .tp = nodScope, .pl2 = 13,           .startBt = 9, .lenBts = 52 }, // function body

                (Node){ .tp = nodAssignment, .pl2 = 2,       .startBt = 16, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 16, .lenBts = 1 },  // x
                (Node){ .tp = nodInt, .pl2 = 4,              .startBt = 20, .lenBts = 1 },
                
                (Node){ .tp = nodLoop, .pl1 = slScope, .pl2 = 9, .startBt = 26, .lenBts = 34 },                
                (Node){ .tp = nodScope, .pl2 = 8,           .startBt = 52, .lenBts = 8 },
                
                (Node){ .tp = nodLoopCond, .pl1 = slStmt, .pl2 = 4, .startBt = 33, .lenBts = 9 },
                (Node){ .tp = nodExpr, .pl2 = 3,            .startBt = 33, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = -opTLessThan - 2 + M, .pl2 = 2, .startBt = 34, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2,    .startBt = 36, .lenBts = 1 }, // x
                (Node){ .tp = nodInt, .pl2 = 101,           .startBt = 38, .lenBts = 3 },
                
                (Node){ .tp = nodExpr, .pl2 = 2,            .startBt = 52, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = F, .pl2 = 1,  .startBt = 52, .lenBts = 5 }, // print
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2,    .startBt = 58, .lenBts = 1 }  // x

            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {(OverloadImport){ .name = s("print"), .count = 1}})
        ),
        createTest(
            s("Loop with break and continue"),
            s("(.f f Int()\n"
              "    (.loop (< x 101) (x 0)\n"
              "        break\n"
              "        continue 3))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl1 = -2, .pl2 = 13, .lenBts = 73 },
                (Node){ .tp = nodScope, .pl2 = 12, .startBt = 9, .lenBts = 64 }, // function body
                
                (Node){ .tp = nodLoop, .pl1 = slScope, .pl2 = 11, .startBt = 16, .lenBts = 56 },
                
                (Node){ .tp = nodScope, .pl2 = 10, .startBt = 33, .lenBts = 39 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 34, .lenBts = 3 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 34, .lenBts = 1 }, // x
                (Node){ .tp = nodInt, .pl2 = 0, .startBt = 36, .lenBts = 1 },
                
                (Node){ .tp = nodLoopCond, .pl1 = slStmt, .pl2 = 4, .startBt = 23, .lenBts = 9 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 23, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = -opTLessThan - 2 + M, .pl2 = 2, .startBt = 24, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 0, .pl2 = 2, .startBt = 26, .lenBts = 1 }, // x
                (Node){ .tp = nodInt, .pl2 = 101, .startBt = 28, .lenBts = 3 },
                
                (Node){ .tp = nodBreak, .pl1 = 1, .startBt = 47, .lenBts = 5 },
                (Node){ .tp = nodContinue, .pl1 = 3, .startBt = 61, .lenBts = 10 }
            }),
            ((EntityImport[]) {}),
            ((OverloadImport[]) {})
        ),
    }));
}

void runATestSet(ParserTestSet* (*testGenerator)(LanguageDefinition*, Arena*), int* countPassed, int* countTests, 
        LanguageDefinition* lang, ParserDefinition* parsDef, Arena* a) {
    ParserTestSet* testSet = (testGenerator)(lang, a);
    for (int j = 0; j < testSet->totalTests; j++) {
        ParserTest test = testSet->tests[j];
        runParserTest(test, countPassed, countTests, a);
    }
}


int main() {
    printf("----------------------------\n");
    printf("--  PARSER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    LanguageDefinition* langDef = buildLanguageDefinitions(a);
    ParserDefinition* parsDef = buildParserDefinitions(langDef, a);

    int countPassed = 0;
    int countTests = 0;
    runATestSet(&assignmentTests, &countPassed, &countTests, langDef, parsDef, a);
    runATestSet(&expressionTests, &countPassed, &countTests, langDef, parsDef, a);
    runATestSet(&functionTests, &countPassed, &countTests, langDef, parsDef, a);
    runATestSet(&ifTests, &countPassed, &countTests, langDef, parsDef, a);
    //~ runATestSet(&loopTests, &countPassed, &countTests, langDef, parsDef, a);

    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
