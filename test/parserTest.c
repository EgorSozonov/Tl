#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../src/tl.internal.h"
#include "tlTest.h"

//{{{ Utils

typedef struct {
    String* name;
    Compiler* test;
    Compiler* control;
} ParserTest;


typedef struct {
    String* name;
    Int totalTests;
    ParserTest* tests;
} ParserTestSet;

typedef struct { // :TestEntityImport
    String* name;
    Int typeInd; // index in the intermediary array of types that is imported alongside
} TestEntityImport;

#define S   70000000 // A constant larger than the largest allowed file size.
                     // Separates parsed entities from others
#define I  140000000 // The base index for imported entities/overloads
#define S2 210000000 // A constant larger than the largest allowed file size.
                     //  Separates parsed entities from others
#define O  280000000 // The base index for operators


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


private Int tryGetOper0(Int opType, Int typeId, Compiler* protoOvs) {
// Try and convert test value to operator entityId
    Int entityId;
    Int ovInd = -protoOvs->activeBindings[opType] - 2;
    bool foundOv = findOverload(typeId, ovInd, &entityId, protoOvs);
    if (foundOv)  {
        return entityId;
    } else {
        return -1;
    }
}

#define oper(opType, typeId) tryGetOper0(opType, typeId, protoOvs)


private Int transformBindingEntityId(Int inp, Compiler* pr) {
    if (inp < S) { // parsed stuff
        return inp + pr->countNonparsedEntities;
    } else {
        return inp - O;
    }
}


private Arr(Int) importTypes(Arr(Int) types, Int countTypes, Compiler* cm) {
    Int countImportedTypes = 0;
    Int j = 0;
    while (j < countTypes) {
        if (types[j] == 0) {
            return NULL;
        }
        ++(countImportedTypes);
        j += types[j] + 1;
    }
    Arr(Int) typeIds = allocateOnArena(countImportedTypes*4, cm->aTmp);
    j = 0;
    Int t = 0;
    while (j < countTypes) {
        Int sentinel = j + types[j] + 1;
        Int initTypeId = cm->types.len;

        for (Int k = j; k < sentinel; k++) {
            pushIntypes(types[k], cm);
        }
        //printIntArrayOff(initTypeId, sentinel - j, cm->types.content);
        Int mergedTypeId = mergeType(initTypeId, sentinel - j, cm);
        typeIds[t] = mergedTypeId;
        t++;
        j = sentinel;
    }
    return typeIds;
}


private ParserTest createTest0(String* name, String* sourceCode, Arr(Node) nodes, Int countNodes,
                               Arr(Int) types, Int countTypes, Arr(TestEntityImport) imports,
                               Int countImports, Compiler* proto, Arena* a) {
/** Creates a test with two parsers: one is the init parser (contains all the "imported" bindings and
pre-defined nodes), and the other is the output parser (with all the stuff parsed from source code).
When the test is run, the init parser will parse the tokens and then will be compared to the
expected output parser.
Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
it will be inserted as 1 + (the number of built-in bindings) etc */
    Compiler* test = lexicallyAnalyze(sourceCode, proto, a);
    Compiler* control = lexicallyAnalyze(sourceCode, proto, a);
    print("p1")
    initializeParser(control, proto, a);
    print("p2")
    initializeParser(test, proto, a);
    print("p3")
    Arr(Int) typeIds = importTypes(types, countTypes, control);
    importTypes(types, countTypes, test);

    StandardText stText = getStandardTextLength();
    if (countImports > 0)  {
        Arr(EntityImport) importsForControl = allocateOnArena(countImports*sizeof(EntityImport), a);
        Arr(EntityImport) importsForTest = allocateOnArena(countImports*sizeof(EntityImport), a);

        for (Int j = 0; j < countImports; j++)  {
            String* nameImp = imports[j].name;
            untt nameId = addStringDict(nameImp->cont, 0, nameImp->len,
                      control->stringTable, control->stringDict);
            importsForControl[j] = (EntityImport) { .name = nameId, .typeInd = imports[j].typeInd };
            nameId = addStringDict(nameImp->cont, 0, nameImp->len,
                      test->stringTable, test->stringDict);
            importsForTest[j] = (EntityImport) { .name = nameId, .typeInd = imports[j].typeInd };
        }
        importEntities(importsForControl, countImports, typeIds, control);
        importEntities(importsForTest, countImports, typeIds, test);
    }

    Bool withByteChecks = false;
    for (Int i = 0; i < countNodes; i++) {
        if (nodes[i].startBt > 0) {
            withByteChecks = true;
            break;
        }
    }

    // The control compiler
    for (Int i = 0; i < countNodes; i++) {
        Node nd = nodes[i];
        if (withByteChecks) { nd.startBt += stText.len; }
        untt nodeType = nd.tp;
        // All the node types which contain bindingIds
        if (nodeType == nodId || nodeType == nodCall || nodeType == nodBinding
                || (nodeType == nodAssignLeft && nd.pl2 == 0)) {
            nd.pl1 = transformBindingEntityId(nd.pl1, control);
        }
        if (nodeType == nodId)  {
            nd.pl2 += stText.firstParsed;
        }
        pushInnodes(nd, control);
    }
    return (ParserTest){ .name = name, .test = test, .control = control };
}


#define createTest(name, input, nodes, types, entities) createTest0((name), (input), \
    (nodes), sizeof(nodes)/sizeof(Node), types, sizeof(types)/4, entities, sizeof(entities)/sizeof(EntityImport), \
    proto, a)

private ParserTest createTestWithError0(String* name, String* message, String* input, Arr(Node) nodes,
                            Int countNodes, Arr(Int) types, Int countTypes, Arr(TestEntityImport) entities,
                            Int countEntities, Compiler* proto, Arena* a) {
/* Creates a test with two parsers: one is the test parser (contains just the "imported" bindings)
and one is the control parser (with the bindings and the expected nodes). When the test is run,
the test parser will parse the tokens and then will be compared to the control parser.
Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
it will be inserted as 1 + (the number of built-in bindings) */
    ParserTest theTest = createTest0(name, input, nodes, countNodes, types, countTypes, entities,
                                     countEntities, proto, a);
    theTest.control->wasError = true;
    theTest.control->errMsg = message;
    return theTest;
}

#define createTestWithError(name, errorMessage, input, nodes, types, entities) createTestWithError0((name), \
    errorMessage, (input), (nodes), sizeof(nodes)/sizeof(Node), types, sizeof(types)/4, \
    entities, sizeof(entities)/sizeof(EntityImport), proto, a)


int equalityParser(/* test specimen */Compiler a, /* expected */Compiler b) {
/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first
differing token otherwise */
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.nodes.len < b.nodes.len ? a.nodes.len : b.nodes.len;
    int i = 0;
    for (; i < commonLength; i++) {
        Node nodA = a.nodes.cont[i];
        Node nodB = b.nodes.cont[i];
        if (nodA.tp != nodB.tp
                || (nodB.lenBts > 0 && nodA.lenBts != nodB.lenBts)
                || (nodB.startBt > 0 && nodA.startBt != nodB.startBt)
            || nodA.pl1 != nodB.pl1 || nodA.pl2 != nodB.pl2) {
            printf("\n\nUNEQUAL RESULTS on %d\n", i);
            if (nodA.tp != nodB.tp) {
                printf("Diff in tp, %d but was expected %d\n", nodA.tp, nodB.tp);
            }
            /* In a parser test, we don't always care about the bytes, which is marked by 0 vals */
            if (nodB.lenBts > 0 && nodA.lenBts != nodB.lenBts) {
                printf("Diff in lenBts, %d but was expected %d\n", nodA.lenBts, nodB.lenBts);
            }
            if (nodB.startBt > 0 && nodA.startBt != nodB.startBt) {
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
    return (a.nodes.len == b.nodes.len) ? -2 : i;
}


void runParserTest(ParserTest test, int* countPassed, int* countTests, Arena *a) {
// Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code
    (*countTests)++;
    if (test.test->tokens.len == 0) {
        print("Lexer result empty");
        return;
    }
#ifdef TRACE
    printLexer(test.control);
    printf("\n");
#endif
    parseMain(test.test, a);

    int equalityStatus = equalityParser(*test.test, *test.control);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(test.test->errMsg);
        printf("\nBut was expected: ");
        printString(test.control->errMsg);
        printf("\n");
        print("    LEXER:")
        printLexer(test.control);
        print("    PARSER:")
        printParser(test.test, a);
    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On node %d\n", equalityStatus);
        print("    LEXER:")
        printLexer(test.control);
        print("    PARSER:")
        printParser(test.test, a);
    }
}
//}}}
//{{{ Assignment tests

ParserTestSet* assignmentTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Assignment test set"), a, ((ParserTest[]){
   /*
        createTest(
            s("Simple assignment"),
            s("x = 12"),
            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0, .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodAssignRight, .pl2 = 1, .startBt = 2, .lenBts = 4 },
                (Node){ .tp = tokInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Double assignment"),
            s("x = 12\n"
              "second = x"
            ),
            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0, .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodAssignRight, .pl2 = 1, .startBt = 2, .lenBts = 4 },
                (Node){ .tp = tokInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 },
                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 0,
                        .startBt = 7, .lenBts = 6 }, // second
                (Node){ .tp = nodAssignRight, .pl2 = 1, .startBt = 14, .lenBts = 3 },
                (Node){ .tp = nodId,          .pl2 = 0, .startBt = 16, .lenBts = 1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Assignment shadowing error"),
            s(errAssignmentShadowing),
            s("x = 12\n"
              "x = 7"
            ),
            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0 },
                (Node){ .tp = nodAssignRight, .pl2 = 1 },
                (Node){ .tp = tokInt,  .pl2 = 12 },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Assignment type declaration error"),
            s(errTypeMismatch),
            s("x = 12 String"),
            ((Node[]) {
                (Node){ .tp = nodAssignLeft },
                (Node){ .tp = tokInt,       .pl2 = 12 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Reassignment"),
            s("main = ^{\n"
              "x = `foo`\n"
              "x <- `bar`}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 9 },
                (Node){ .tp = nodBinding, .pl1 = 0         },
                (Node){ .tp = nodScope,           .pl2 = 7 },
                (Node){ .tp = nodAssignLeft, .pl1 = 1,  .pl2 = 0 },   // x
                (Node){ .tp = nodAssignRight, .pl2 = 1         },
                (Node){ .tp = tokString,                   },
                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = nodBinding, .pl1 = 1         }, // second
                (Node){ .tp = nodAssignRight, .pl2 = 1 },
                (Node){ .tp = tokString,                   }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
       */
        createTest(
            s("Mutation simple"),
            s("main = ^{\n"
              "    x = 1\n"
              "    x += 3\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 11 },
                (Node){ .tp = nodBinding, .pl1 = 0,         },
                (Node){ .tp = nodScope,           .pl2 = 9 },
                (Node){ .tp = nodAssignLeft,      .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1,        },     // x
                (Node){ .tp = tokInt,             .pl2 = 1 },
                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,        },
                (Node){ .tp = nodAssignRight, .pl1 = 1, .pl2 = 5 },
                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId,   .pl1 = 1,  .pl2 = 1 },
                (Node){ .tp = tokInt,              .pl2 = 3  }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
       /*
        createTest(
            s("Mutation complex"),
            s("main = ^{ \n"
                   "x = 2\n"
                   "x ^= 15 - 8}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 13 },
                (Node){ .tp = nodBinding, .pl1 = 0,         },
                (Node){ .tp = nodScope,           .pl2 = 11 },
                (Node){ .tp = nodAssignLeft,      .pl2 = 2  },
                (Node){ .tp = nodBinding, .pl1 = 1,         },     // x
                (Node){ .tp = tokInt,             .pl2 = 2  },
                (Node){ .tp = nodAssignLeft,        .pl2 = 7  },
                (Node){ .tp = nodBinding, .pl1 = 1,         },
                (Node){ .tp = nodExpr,            .pl2 = 5  },

                (Node){ .tp = nodCall, .pl1 = oper(opExponent, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId,      .pl1 = 1, .pl2 = 1 },

                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokInt), .pl2 = 2 },
                (Node){ .tp = tokInt,             .pl2 = 15, .startBt = 28, .lenBts = 2 },
                (Node){ .tp = tokInt,             .pl2 = 8, .startBt = 31, .lenBts = 1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Complex left side"),
            s("arr_i <- 5"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 13, .startBt = 0, .lenBts = 34 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 4 },
                (Node){ .tp = nodScope,           .pl2 = 11, .startBt = 8, .lenBts = 26 },
                (Node){ .tp = nodAssignLeft,      .pl2 = 2, .startBt = 14, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 14, .lenBts = 1 },     // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
       */
    }));
}
//}}}
//{{{ Other tests
ParserTestSet* expressionTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Expression test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function call"),
            s("x = 10 .foo 2 3"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 6, .startBt = 0, .lenBts = 14 },
                // " + 1" because the first binding is taken up by the "imported" function, "foo"
                (Node){ .tp = nodBinding, .pl1 = 0,        .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodExpr,  .pl2 = 4,          .startBt = 4, .lenBts = 10 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 3, .startBt = 4, .lenBts = 3 }, // foo
                (Node){ .tp = tokInt, .pl2 = 10,           .startBt = 8, .lenBts = 2 },
                (Node){ .tp = tokInt, .pl2 = 2,            .startBt = 11, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 3,            .startBt = 13, .lenBts = 1 }
            })),
            ((Int[]) {4, tokDouble, tokInt, tokInt, tokInt}),
            ((TestEntityImport[]) {(TestEntityImport){ .name = s("foo"), .typeInd = 0}})
        ),
        createTest(
            s("Nested function call 1"),
            s("x = 10 .foo bar() 3"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 6, .startBt = 0, .lenBts = 18 },
                (Node){ .tp = nodAssignRight, .pl2 = 6, .startBt = 0, .lenBts = 18 },
                (Node){ .tp = nodExpr,  .pl2 = 4, .startBt = 4, .lenBts = 14 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 3, .startBt = 4, .lenBts = 3 }, // foo
                (Node){ .tp = tokInt, .pl2 = 10,  .startBt = 8, .lenBts = 2 },
                (Node){ .tp = nodCall, .pl1 = I + 1, .pl2 = 0, .startBt = 12, .lenBts = 3 }, // bar
                (Node){ .tp = tokInt, .pl2 = 3,   .startBt = 17, .lenBts = 1 }
            })),
            ((Int[]) {4, tokDouble, tokInt, tokDouble, tokInt, 1, tokDouble}),
            ((TestEntityImport[]) {(TestEntityImport){ .name = s("foo"), .typeInd = 0},
                               (TestEntityImport){ .name = s("bar"), .typeInd = 1}})
        ),
        createTest(
            s("Nested function call 2"),
            s("x =  10 .foo (3.4 .barr)"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft,     .pl2 = 6,  .startBt = 0, .lenBts = 22 },
                (Node){ .tp = nodBinding, .pl1 = 0,         .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodExpr,           .pl2 = 4,  .startBt = 5, .lenBts = 17 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 2,  .startBt = 5, .lenBts = 3 },   // foo
                (Node){ .tp = tokInt,            .pl2 = 10, .startBt = 9, .lenBts = 2 },

                (Node){ .tp = nodCall, .pl1 = I + 1, .pl2 = 1,
                        .startBt = 13, .lenBts = 4 },  // barr
                (Node){ .tp = tokDouble, .pl1 = longOfDoubleBits(3.4) >> 32,
                                        .pl2 = longOfDoubleBits(3.4) & LOWER32BITS,
                                        .startBt = 18, .lenBts = 3 }
            })),
            ((Int[]) {3, tokDouble, tokInt, tokBool, 2, tokBool, tokDouble}),
            ((TestEntityImport[]) {(TestEntityImport){ .name = s("foo"), .typeInd = 0},
                               (TestEntityImport){ .name = s("barr"), .typeInd = 1}})
        ),
        createTest(
            s("Triple function call"),
            s("x = 2 .buzz foo(inner(7 `hw`)) 4"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft,     .pl2 = 9 },
                (Node){ .tp = nodBinding, .pl1 = 0 }, // x

                (Node){ .tp = nodExpr,           .pl2 = 7 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 3 }, // buzz
                (Node){ .tp = tokInt,            .pl2 = 2 },

                (Node){ .tp = nodCall, .pl1 = I + 1, .pl2 = 1 }, // foo
                (Node){ .tp = nodCall, .pl1 = I + 2, .pl2 = 2 }, // inner
                (Node){ .tp = tokInt,                .pl2 = 7 },
                (Node){ .tp = tokString,                      },
                (Node){ .tp = tokInt,                .pl2 = 4 },
            })),
            ((Int[]) {4, tokString, tokInt, tokBool, tokInt, // buzz (String <- Int Bool Int)
                      2, tokBool, tokDouble, // foo (Bool <- Float)
                      3, tokDouble, tokInt, tokString}), // inner (Float <- Int String)
            ((TestEntityImport[]) {(TestEntityImport){ .name = s("buzz"), .typeInd = 0},
                               (TestEntityImport){ .name = s("foo"), .typeInd = 1},
                               (TestEntityImport){ .name = s("inner"), .typeInd = 2}})
        ),
        createTest(
            s("Operators simple"),
            s("x = 1 + 9/3"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 7 },
                (Node){ .tp = nodBinding, .pl1 = 0 }, // x
                (Node){ .tp = nodExpr,  .pl2 = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 }, // +
                (Node){ .tp = tokInt, .pl2 = 1 },

                (Node){ .tp = nodCall, .pl1 = oper(opDivBy, tokInt), .pl2 = 2 }, // *
                (Node){ .tp = tokInt, .pl2 = 9 },
                (Node){ .tp = tokInt, .pl2 = 3 }
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Operator arity error"),
            s(errOperatorWrongArity),
            s("x = 1 + 20 100"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .startBt = 0, .lenBts = 14 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodExpr,  .startBt = 4, .lenBts = 10 }
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Unary operator precedence"),
            s("x = 123 + ##$-3"),

            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 7 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodExpr,              .pl2 = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 },
                (Node){ .tp = tokInt,               .pl2 = 123 },
                (Node){ .tp = nodCall, .pl1 = oper(opSize, tokString), .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 },
                (Node){ .tp = tokInt, .pl1 = -1,    .pl2 = -3 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),

        // //~ createTest(
        //     //~ s("Unary operator as first-class value"),
        //     //~ s("x = map (--) coll"),
        //     //~ (((Node[]) {
        //             //~ (Node){ .tp = nodAssignLeft, .pl2 = 5,        .startBt = 0, .lenBts = 17 },
        //             //~ (Node){ .tp = nodBinding, .pl1 = 0,           .startBt = 0, .lenBts = 1 }, // x
        //             //~ (Node){ .tp = nodExpr,  .pl2 = 3,             .startBt = 4, .lenBts = 13 },
        //             //~ (Node){ .tp = nodCall, .pl1 = -2 + I, .pl2 = 2,    .startBt = 4, .lenBts = 3 }, // map
        //             //~ (Node){ .tp = nodId, .pl1 = oper(opDecrement, tokInt), .startBt = 9, .lenBts = 2 },
        //             //~ (Node){ .tp = nodId, .pl1 = I + 1, .pl2 = 2,      .startBt = 13, .lenBts = 4 } // coll
        //     //~ })),
        //     //~ ((TestEntityImport[]) {(TestEntityImport){ .name = s("map"), .entity = (Entity){  }},
        //                        //~ (TestEntityImport){ .name = s("coll"), .entity = (Entity){ }},
        //         //~ })
        // //~ ),
        // //~ createTest(
        //     //~ s("Non-unary operator as first-class value"),
        //     //~ s("x = bimap / dividends divisors"),
        //     //~ (((Node[]) {
        //             //~ (Node){ .tp = nodAssignLeft,            .pl2 = 6, .startBt = 0, .lenBts = 30 },
        //             //~ // "3" because the first binding is taken up by the "imported" function, "foo"
        //             //~ (Node){ .tp = nodBinding, .pl1 = 0,               .startBt = 0, .lenBts = 1 },    // x
        //             //~ (Node){ .tp = nodExpr,                  .pl2 = 4, .startBt = 4, .lenBts = 26 },
        //             //~ (Node){ .tp = nodCall, .pl1 = F,        .pl2 = 3, .startBt = 4, .lenBts = 5 },    // bimap
        //             //~ (Node){ .tp = nodId, .pl1 = oper(opDivBy, tokInt), .startBt = 10, .lenBts = 1 },   // /
        //             //~ (Node){ .tp = nodId, .pl1 = M,      .pl2 = 2,     .startBt = 12, .lenBts = 9 },   // dividends
        //             //~ (Node){ .tp = nodId, .pl1 = M + 1,      .pl2 = 3, .startBt = 22, .lenBts = 8 }    // divisors
        //     //~ })),
        //     //~ ((TestEntityImport[]) {
        //                        //~ (TestEntityImport){ .name = s("dividends"), .entity = (Entity){}},
        //                        //~ (TestEntityImport){ .name = s("divisors"), .entity = (Entity){}}
        //     //~ }),
        //     //~ ((TestEntityImport[]) {(TestEntityImport){ .name = s("bimap"), .count = 1}})
        // //~ ),
    }));
}




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

ParserTestSet* functionTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Functions test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function definition 1"),
            s("newFn = ^{x Int, y Int | a = x)"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,             .pl2 = 7, .startBt = 0, .lenBts = 37 },
                (Node){ .tp = nodBinding, .pl1 = 0,           .startBt = 4, .lenBts = 5 },
                (Node){ .tp = nodScope, .pl2 = 5,             .startBt = 13, .lenBts = 24 },
                (Node){ .tp = nodBinding, .pl1 = 1,           .startBt = 14, .lenBts = 1 },  // param x
                (Node){ .tp = nodBinding, .pl1 = 2,           .startBt = 20, .lenBts = 1 },  // param y
                (Node){ .tp = nodAssignLeft,        .pl2 = 2, .startBt = 31, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 3,           .startBt = 31, .lenBts = 1 },  // local a
                (Node){ .tp = nodId, .pl1 = 1,      .pl2 = 2, .startBt = 35, .lenBts = 1 }   // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Simple function definition 2"),
            s("newFn = ^{x String, y Float => String |\n"
              "    a = x\n"
              "    return a\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 9, .startBt = 0,  .lenBts = 61 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 5 },
                (Node){ .tp = nodScope,            .pl2 = 7, .startBt = 16, .lenBts = 45 },
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 17, .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2,          .startBt = 26, .lenBts = 1 }, // param y
                (Node){ .tp = nodAssignLeft,       .pl2 = 2, .startBt = 41, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 3,          .startBt = 41, .lenBts = 1 }, // local a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 2, .startBt = 45, .lenBts = 1 }, // x
                (Node){ .tp = nodReturn,           .pl2 = 1, .startBt = 51, .lenBts = 8 },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 5, .startBt = 58, .lenBts = 1 }  // a
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Function definition wrong return type"),
            s(errTypeWrongReturnType),
            s("newFn = ^{x Float, y Float => String|\n"
              "    a = x\n"
              "    return a\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef                       },
                (Node){ .tp = nodBinding, .pl1 = 0           },
                (Node){ .tp = nodScope                       },
                (Node){ .tp = nodBinding, .pl1 = 1           }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2           }, // param y
                (Node){ .tp = nodAssignLeft,       .pl2 = 2  },
                (Node){ .tp = nodBinding, .pl1 = 3,          }, // local a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 2  }, // x
                (Node){ .tp = nodReturn,                     },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 5  }  // a
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Function definition with complex return"),
            s("newFn = {x Int, y Float => String | \n"
              "    return $(x.float() - y)}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 11 },
                (Node){ .tp = nodBinding, .pl1 = 0,          },
                (Node){ .tp = nodScope,            .pl2 = 9  },
                (Node){ .tp = nodBinding, .pl1 = 1           }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2           }, // param y
                (Node){ .tp = nodReturn,           .pl2 = 6  },
                (Node){ .tp = nodExpr,           .pl2 = 5    },
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokDouble), .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokDouble), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 2 },  // x
                (Node){ .tp = nodId, .pl1 = 2,     .pl2 = 4 }  // y
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Mutually recursive function definitions"),
            s("foo = {x Int, y Float => Int |\n"
              "    a = x\n"
              "    return y.bar(a)\n"
              "}\n"
              "bar = {x Float, y Int => Int | \n"
              "    return y.foo(x)\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 12  }, // foo
                (Node){ .tp = nodBinding, .pl1 = 0           },
                (Node){ .tp = nodScope,            .pl2 = 10  },
                (Node){ .tp = nodBinding, .pl1 = 2            },
                        // param x. First 2 bindings = types
                (Node){ .tp = nodBinding, .pl1 = 3           },  // param y
                (Node){ .tp = nodAssignLeft,       .pl2 = 2  },
                (Node){ .tp = nodBinding, .pl1 = 4           },  // local a
                (Node){ .tp = nodId,     .pl1 = 2, .pl2 = 2  },  // x
                (Node){ .tp = nodReturn,           .pl2 = 4  },
                (Node){ .tp = nodExpr,             .pl2 = 3  },
                (Node){ .tp = nodCall, .pl1 = 1,   .pl2 = 2  }, // bar call
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 3  }, // y
                (Node){ .tp = nodId, .pl1 = 4,     .pl2 = 5  }, // a

                (Node){ .tp = nodFnDef,            .pl2 = 9   }, // bar
                (Node){ .tp = nodBinding, .pl1 = 1            },
                (Node){ .tp = nodScope,            .pl2 = 7   },
                (Node){ .tp = nodBinding, .pl1 = 5            }, // param x
                (Node){ .tp = nodBinding, .pl1 = 6            }, // param y
                (Node){ .tp = nodReturn,           .pl2 = 4   },
                (Node){ .tp = nodExpr,             .pl2 = 3   },
                (Node){ .tp = nodCall, .pl1 = 0,  .pl2 = 2    }, // foo call
                (Node){ .tp = nodId, .pl1 = 6,     .pl2 = 3   }, // y
                (Node){ .tp = nodId, .pl1 = 5,     .pl2 = 2   }  // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Function definition with nested scope"),
            s("main = f(x Int y Float:\n"
              "    do(\n"
              "        a = 5\n"
              "    )\n"
              "    a = ,x - y\n"
              "    a .print\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 19 },
                (Node){ .tp = nodBinding, .pl1 = 0,         },
                (Node){ .tp = nodScope,            .pl2 = 17 },
                (Node){ .tp = nodBinding, .pl1 = 1,          }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2,          }, // param y

                (Node){ .tp = nodScope,            .pl2 = 3 },
                (Node){ .tp = nodAssignLeft,       .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 3,         }, // first a =
                (Node){ .tp = tokInt,              .pl2 = 5 },

                (Node){ .tp = nodAssignLeft,       .pl2 = 6 },
                (Node){ .tp = nodBinding, .pl1 = 4,         }, // second a =
                (Node){ .tp = nodExpr,            .pl2 = 4 },
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokDouble), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 }, // x
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = 3 }, // y

                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = I,  .pl2 = 1 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokDouble), .pl2 = 1 }, // $
                (Node){ .tp = nodId, .pl1 = 4,  .pl2 = 5 } // a
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
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


ParserTestSet* ifTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("If test set"), a, ((ParserTest[]){
        createTest(
            s("Simple if"),
            s("f = ^{\n"
              "    if 5 == 5 { `5`.print() }\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl2 = 11, .startBt = 0, .lenBts = 38 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 4, .lenBts = 1 }, // f
                (Node){ .tp = nodScope, .pl2 = 9, .startBt = 5, .lenBts = 33 },

                (Node){ .tp = nodIf, .pl1 = slScope, .pl2 = 8, .startBt = 13, .lenBts = 24 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 17, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = oper(opEquality, tokInt), .pl2 = 2, .startBt = 17, .lenBts = 2 }, // ==
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 20, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 22, .lenBts = 1 },

                (Node){ .tp = nodIf,    .pl2 = 3, .startBt = 27, .lenBts = 9 },
                (Node){ .tp = nodExpr, .pl2 = 2, .startBt = 27, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1, .startBt = 27, .lenBts = 5 }, // print
                (Node){ .tp = tokString, .startBt = 33, .lenBts = 3 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("If with else"),
            s("f = fn(=> String:\n"
              "    if( >(5 3): `5` else `=)`\n"
              ")"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl2 = 11, .startBt = 0, .lenBts = 48 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 4, .lenBts = 1 }, // f
                (Node){ .tp = nodScope, .pl2 = 9, .startBt = 12, .lenBts = 36 },

                (Node){ .tp = nodIf, .pl1 = slScope, .pl2 = 8, .startBt = 20, .lenBts = 27 },

                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 24, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opGreaterTh, tokInt), .pl2 = 2,
                        .startBt = 24, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 26, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 3, .startBt = 28, .lenBts = 1 },
                (Node){ .tp = nodIf,  .pl2 = 1, .startBt = 33, .lenBts = 3 },
                (Node){ .tp = tokString, .startBt = 33, .lenBts = 3 },

                (Node){ .tp = nodIf, .pl2 = 1, .startBt = 42, .lenBts = 4 },
                (Node){ .tp = tokString,         .startBt = 42, .lenBts = 4 },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("If with elseif"),
            s("f = f(\n"
              "    if( 5 > 3: 11\n"
              "       5 == 3: 4)\n"
              ")"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,     .pl2 = 15 },
                (Node){ .tp = nodBinding, .pl1 = 0 }, // f
                (Node){ .tp = nodScope,     .pl2 = 13 },

                (Node){ .tp = nodIf, .pl1 = slScope, .pl2 = 12 },

                (Node){ .tp = nodExpr,      .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opGreaterTh, tokInt), .pl2 = 2 },
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodIf,  .pl2 = 1       },
                (Node){ .tp = tokInt,       .pl2 = 11 },

                (Node){ .tp = nodExpr,      .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opEquality, tokInt), .pl2 = 2 },
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodIf,  .pl2 = 1 },
                (Node){ .tp = tokInt,       .pl2 = 4 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("If with elseif and else"),
            s("f = f(\n"
              "    if(5 > 3 : 11\n"
              "       5 == 3:  4\n"
              "       else    100\n"
              "))"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,     .pl2 = 17 },
                (Node){ .tp = nodBinding, .pl1 = 0,   }, // f
                (Node){ .tp = nodScope,     .pl2 = 15 },

                (Node){ .tp = nodIf, .pl1 = slScope, .pl2 = 14 },

                (Node){ .tp = nodExpr,      .pl2 = 3},
                (Node){ .tp = nodCall, .pl1 = oper(opGreaterTh, tokInt), .pl2 = 2 },
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodIf,  .pl2 = 1       },
                (Node){ .tp = tokInt,       .pl2 = 11 },

                (Node){ .tp = nodExpr,      .pl2 = 3},
                (Node){ .tp = nodCall, .pl1 = oper(opEquality, tokInt), .pl2 = 2 },
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodIf,  .pl2 = 1 },
                (Node){ .tp = tokInt,       .pl2 = 4 },

                (Node){ .tp = nodIf,  .pl2 = 1 },
                (Node){ .tp = tokInt,       .pl2 = 100 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
    }));
}


ParserTestSet* loopTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Loops test set"), a, ((ParserTest[]){
        createTest(
            s("Simple loop 1"),
            s("f = ^{=> Int | for x = 1; x < 101; { x .print }}"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 16 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope,           .pl2 = 14 }, // function body

                (Node){ .tp = nodFor,           .pl2 = 13 },

                (Node){ .tp = nodScope,           .pl2 = 12 },

                (Node){ .tp = nodAssignLeft,      .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1 }, // x
                (Node){ .tp = tokInt,             .pl2 = 1 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 25, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 27, .lenBts = 3 },

                (Node){ .tp = nodExpr,           .pl2 = 3,  .startBt = 40, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1,  .startBt = 40, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodId,   .pl1 = 1, .pl2 = 2,  .startBt = 47, .lenBts = 1 }      // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For with two complex initializers"),
            s("f = ^{\n"
              "    for x = 17; y = x/5; y < 101 {\n"
              "        x .print\n"
              "        x -= 1\n"
              "        y += 1}\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 34 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope,           .pl2 = 32 }, // function body

                (Node){ .tp = nodFor,           .pl2 = 31 },
                (Node){ .tp = nodScope,                 .pl2 = 30 },

                (Node){ .tp = nodAssignLeft,            .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1,              },  // def x
                (Node){ .tp = tokInt,                   .pl2 = 17 },

                (Node){ .tp = nodAssignLeft, .pl2 = 5           },
                (Node){ .tp = nodBinding, .pl1 = 2,             },  // def y
                (Node){ .tp = nodExpr,                  .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opDivBy, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1,          .pl2 = 3 }, // x
                (Node){ .tp = tokInt,                   .pl2 = 5 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl2 = 3,                  },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 2,          .pl2 = 2 }, // y
                (Node){ .tp = tokInt,                   .pl2 = 101 },

                (Node){ .tp = nodExpr,                  .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = I,        .pl2 = 1 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodId,    .pl1 = 1,       .pl2 = 3 }, // x

                (Node){ .tp = nodAssignLeft,              .pl2 = 5 },
                (Node){ .tp = nodBinding,  .pl1 = 1,               },
                (Node){ .tp = nodExpr,                  .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1,      .pl2 = 3 }, // x
                (Node){ .tp = tokInt,               .pl2 = 1 },

                (Node){ .tp = nodAssignLeft,              .pl2 = 5 },
                (Node){ .tp = nodBinding,  .pl1 = 2,               },
                (Node){ .tp = nodExpr,                  .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 2,      .pl2 = 2 },    // y
                (Node){ .tp = tokInt,               .pl2 = 1 },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For without initializers"),
            s("f = ^{=> Int |\n"
              "    x = 4\n"
              "    for x < 101 { \n"
              "        x.print()}}"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,         .pl2 = 16,          .lenBts = 65 },
                (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 1 },
                (Node){ .tp = nodScope, .pl2 = 14,           .startBt = 9, .lenBts = 56 }, // function body

                (Node){ .tp = nodAssignLeft, .pl2 = 2,       .startBt = 18, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 18, .lenBts = 1 },  // x
                (Node){ .tp = tokInt,              .pl2 = 4, .startBt = 22, .lenBts = 1 },

                (Node){ .tp = nodFor,            .pl2 = 10, .startBt = 28, .lenBts = 36 },
                (Node){ .tp = nodScope, .pl2 = 9,           .startBt = 55, .lenBts = 9 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4, .startBt = 36, .lenBts = 9 },
                (Node){ .tp = nodExpr, .pl2 = 3,            .startBt = 36, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2,
                        .startBt = 37, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 39, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 41, .lenBts = 3 },

                (Node){ .tp = nodExpr,         .pl2 = 3,    .startBt = 55, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1,  .startBt = 55, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1,
                        .startBt = 61, .lenBts = 1 }, // $
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 62, .lenBts = 1 }  // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("While with break and continue"),
            s("f = ^{=> Int |\n"
              "    for x = 0; x < 101 {\n"
              "        break\n"
              "        continue}\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,             .pl2 = 14 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope,           .pl2 = 12 }, // function body

                (Node){ .tp = nodFor,           .pl2 = 11 },

                (Node){ .tp = nodScope, .pl2 = 10 },

                (Node){ .tp = nodAssignLeft, .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1 }, // x
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2 }, // x
                (Node){ .tp = tokInt,        .pl2 = 101 },

                (Node){ .tp = nodBreakCont, .pl1 = -1 },
                (Node){ .tp = nodBreakCont, .pl1 = -1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("For with break error"),
            s(errBreakContinueInvalidDepth),
            s("f = ^{=> Int |\n"
              "    for x = 0; x < 101 {\n"
              "        break 2\n"
              "}}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef                },
                (Node){ .tp = nodBinding, .pl1 = 0   },
                (Node){ .tp = nodScope     }, // function body

                (Node){ .tp = nodFor       },
                (Node){ .tp = nodScope     },

                (Node){ .tp = nodAssignLeft,      .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1 }, // x
                (Node){ .tp = tokInt,             .pl2 = 0 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101}
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Nested for with deep break and continue"),
            s("f = ^{\n"
              "    for a = 0; a < 101 {\n"
              "        for b = 0; b < 101 {\n"
              "            for c = 0; c < 101 {\n"
              "                break 3}\n"
              "        }\n"
              "        for d = 0; d < 51 {\n"
              "            for e = 0; e < 101 {\n"
              "                continue 2}\n"
              "        }\n"
              "        print(a)\n"
              "    }\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 58 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope,           .pl2 = 56 }, // function body

                (Node){ .tp = nodFor,  .pl1 = 1, .pl2 = 55 },
                        // for #1. It's being broken
                (Node){ .tp = nodScope, .pl2 = 54 },

                (Node){ .tp = nodAssignLeft, .pl2 = 2 }, // a =
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2 }, // a
                (Node){ .tp = tokInt, .pl2 = 101 },

                (Node){ .tp = nodFor,          .pl2 = 20 }, // for #2
                (Node){ .tp = nodScope,          .pl2 = 19 },

                (Node){ .tp = nodAssignLeft, .pl2 = 2 }, // b =
                (Node){ .tp = nodBinding, .pl1 = 2 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,                     .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = 3 }, // b
                (Node){ .tp = tokInt, .pl2 = 101 },

                (Node){ .tp = nodFor,          .pl2 = 10 }, // for #3, double-nested
                (Node){ .tp = nodScope,          .pl2 = 9 },

                (Node){ .tp = nodAssignLeft, .pl2 = 2 }, // c =
                (Node){ .tp = nodBinding, .pl1 = 3 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,                     .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 3, .pl2 = 4 }, // c
                (Node){ .tp = tokInt,          .pl2 = 101 },

                (Node){ .tp = nodBreakCont, .pl1 = 1 },

                (Node){ .tp = nodFor,  .pl1 = 4, .pl2 = 20 }, // for #4. It's being continued
                (Node){ .tp = nodScope,            .pl2 = 19 },

                (Node){ .tp = nodAssignLeft,    .pl2 = 2 }, // d =
                (Node){ .tp = nodBinding, .pl1 = 4 },
                (Node){ .tp = tokInt,           .pl2 = 0 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,          .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 4, .pl2 = 5}, // d
                (Node){ .tp = tokInt, .pl2 = 51 },

                (Node){ .tp = nodFor,         .pl2 = 10 }, // while #5, last
                (Node){ .tp = nodScope,         .pl2 = 9 },

                (Node){ .tp = nodAssignLeft, .pl2 = 2 }, // e =
                (Node){ .tp = nodBinding, .pl1 = 5 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,         .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 5, .pl2 = 6 }, // e
                (Node){ .tp = tokInt,          .pl2 = 101 },

                (Node){ .tp = nodBreakCont, .pl1 = 4 },

                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2 }  // a
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("For with type error"),
            s(errTypeMustBeBool),
            s("f = ^{=> Int | for x = 1; x/101 { x.print()}}"),
            ((Node[]) {
                (Node){ .tp = nodFnDef         },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope }, // function body

                (Node){ .tp = nodFor },

                (Node){ .tp = nodScope },

                (Node){ .tp = nodAssignLeft,      .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1  }, // x
                (Node){ .tp = tokInt,             .pl2 = 1 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4, .startBt = 22, .lenBts = 7 },
                (Node){ .tp = nodExpr,            .pl2 = 3, .startBt = 22, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opDivBy, tokInt), .pl2 = 2, .startBt = 22, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 24, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 26, .lenBts = 3 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
    }));
}


ParserTestSet* typeTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Types test set"), a, ((ParserTest[]){
        createTest(
            s("Simple type 1"),
            s("Foo = (.id Int .name String)"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 16 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope,           .pl2 = 14 }, // function body

                (Node){ .tp = nodFor,           .pl2 = 13 },

                (Node){ .tp = nodScope,           .pl2 = 12 },

                (Node){ .tp = nodAssignLeft,      .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1 }, // x
                (Node){ .tp = tokInt,             .pl2 = 1 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 25, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 27, .lenBts = 3 },

                (Node){ .tp = nodExpr,           .pl2 = 3,  .startBt = 40, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1,  .startBt = 40, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodId,   .pl1 = 1, .pl2 = 2,  .startBt = 47, .lenBts = 1 }      // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
    }));
}

//}}}


void runATestSet(ParserTestSet* (*testGenerator)(Compiler*, Compiler*, Arena*), int* countPassed,
                 int* countTests, Compiler* proto, Compiler* protoOvs, Arena* a) {
    ParserTestSet* testSet = (testGenerator)(proto, protoOvs, a);
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
    Compiler* proto = createProtoCompiler(a);
    Compiler* protoOvs = createProtoCompiler(a);
    createOverloads(protoOvs);
    int countPassed = 0;
    int countTests = 0;
    runATestSet(&assignmentTests, &countPassed, &countTests, proto, protoOvs, a);
   /*
    runATestSet(&expressionTests, &countPassed, &countTests, proto, protoOvs, a);
    runATestSet(&functionTests, &countPassed, &countTests, proto, protoOvs, a);
    runATestSet(&ifTests, &countPassed, &countTests, proto, protoOvs, a);
    runATestSet(&loopTests, &countPassed, &countTests, proto, protoOvs, a);
    runATestSet(&typeTests, &countPassed, &countTests, proto, protoOvs, a);
   */

    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        if (countTests > 1) {
            printf("\nPassed all %d tests!\n", countTests);
        } else {
            printf("\nThe test was passed.\n");
        }

    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }

    deleteArena(a);
}
