#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../src/tl.internal.h"
#include "tlTest.h"

//{{{ Utils

typedef struct { //:ParserTest
    String* name;
    Compiler* test;
    Compiler* control;
    Bool compareLocsToo;
} ParserTest;


typedef struct {
    String* name;
    Int totalTests;
    ParserTest* tests;
} ParserTestSet;

typedef struct { // :TestEntityImport
    Int nameInd; // 0, 1 or 2. Corresponds to the "foobarinner" in standardText
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
    printString(tests[0].control->stats.errMsg);
    return result;
}

#define createTestSet(n, a, tests) createTestSet0(n, a, sizeof(tests)/sizeof(ParserTest), tests)


private Int tryGetOper0(Int opType, Int typeId, Compiler* protoOvs) {
// Try and convert test value to operator entityId
    Int entityId;
    Int ovInd = -protoOvs->activeBindings[opType] - 2;
    bool foundOv = findOverload(typeId, ovInd, &entityId, protoOvs);
    if (foundOv)  {
        return entityId + O;
    } else {
        return -1;
    }
}

#define oper(opType, typeId) tryGetOper0(opType, typeId, protoOvs)


private Int transformBindingEntityId(Int inp, Compiler* pr) {
    if (inp < S) { // parsed stuff
        return inp + pr->stats.countNonparsedEntities;
    } else if (inp < O){ // imported but not operators: "foo", "bar"
        return inp - I + pr->stats.countNonparsedEntities;
    } else { // operators
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
    Arr(TypeId) typeIds = allocateOnArena(countImportedTypes*4, cm->aTmp);
    j = 0;
    Int t = 0;
    while (j < countTypes) {
        const Int importLen = types[j];
        const Int sentinel = j + importLen + 1;
        TypeId initTypeId = cm->types.len;

        pushIntypes(importLen + 3, cm); // +3 because the header takes 2 ints, 1 more for
                                        // the return typeId
        typeAddHeader(
           (TypeHeader){.sort = sorFunction, .tyrity = 0, .arity = importLen - 1,
                        .nameAndLen = -1}, cm);
        for (Int k = j + 1; k < sentinel; k++) { // <= because there are (arity + 1) elts -
                                                 // +1 for the return type!
            pushIntypes(types[k], cm);
        }

        Int mergedTypeId = mergeType(initTypeId, cm);
        typeIds[t] = mergedTypeId;
        t++;
        j = sentinel;
    }
    return typeIds;
}

private ParserTest createTest0(String* name, String* sourceCode, Arr(Node) nodes, Int countNodes,
                               Arr(Int) types, Int countTypes, Arr(TestEntityImport) imports,
                               Int countImports, Compiler* proto, Arena* a) { //:createTest0
/** Creates a test with two parsers: one is the init parser (contains all the "imported" bindings and
pre-defined nodes), and the other is the output parser (with all the stuff parsed from source code).
When the test is run, the init parser will parse the tokens and then will be compared to the
expected output parser.
Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
it will be inserted as 1 + (the number of built-in bindings) etc */
    Compiler* test = lexicallyAnalyze(sourceCode, proto, a);
    Compiler* control = lexicallyAnalyze(sourceCode, proto, a);
    if (control->stats.wasLexerError == true) {
        return (ParserTest) {
            .name = name, .test = test, .control = control, .compareLocsToo = false };
    }
    initializeParser(control, proto, a);
    initializeParser(test, proto, a);
    Arr(Int) typeIds = importTypes(types, countTypes, control);
    importTypes(types, countTypes, test);

    StandardText stText = getStandardTextLength();
    if (countImports > 0) {
        Arr(EntityImport) importsForControl = allocateOnArena(countImports*sizeof(EntityImport), a);
        Arr(EntityImport) importsForTest = allocateOnArena(countImports*sizeof(EntityImport), a);

        for (Int j = 0; j < countImports; j++) {
            importsForControl[j] = (EntityImport) {
                .name = nameOfStandard(strSentinel - 3 + imports[j].nameInd),
                .typeId = typeIds[imports[j].typeInd] };
            importsForTest[j] = (EntityImport) {
                .name = nameOfStandard(strSentinel - 3 + imports[j].nameInd),
                .typeId = typeIds[imports[j].typeInd] };
        }
        importEntities(importsForControl, countImports, control);
        importEntities(importsForTest, countImports, test);
    }

    // The control compiler
    for (Int i = 0; i < countNodes; i++) {
        Node nd = nodes[i];
        Unt nodeType = nd.tp;
        // All the node types which contain entityIds in their pl1
        if (nodeType == nodId || nodeType == nodCall || nodeType == nodBinding
                || (nodeType == nodAssignLeft && nd.pl2 == 0) || nodeType == nodFnDef) {
            nd.pl1 = transformBindingEntityId(nd.pl1, control);
        } else if (nodeType == nodDataAlloc) {
            nd.pl1 =  control->activeBindings[nd.pl1];
        }
        // transform pl2/pl3 if it holds NameId
        if (nodeType == nodId && nd.pl2 != -1)  {
            nd.pl2 += stText.firstParsed;
        }
        if (nodeType == nodFnDef && nd.pl3 != -1)  {
            nd.pl3 += stText.firstParsed;
        }
        addNode(nd, 0, 0, control);
    }
    return (ParserTest){ .name = name, .test = test, .control = control, .compareLocsToo = false };
}

#define createTest(name, input, nodes, types, entities) \
    createTest0((name), (input), (nodes), sizeof(nodes)/sizeof(Node), (types), sizeof(types)/4, \
    (entities), sizeof(entities)/sizeof(TestEntityImport), proto, a)

private ParserTest createTestWithError0(String* name, String* message, String* input, Arr(Node) nodes,
                            Int countNodes, Arr(Int) types, Int countTypes, Arr(TestEntityImport) entities,
                            Int countEntities, Compiler* proto, Arena* a) {
// Creates a test with two parsers where the expected result is an error in parser
    ParserTest theTest = createTest0(name, input, nodes, countNodes, types, countTypes, entities,
                                     countEntities, proto, a);
    theTest.control->stats.wasError = true;
    theTest.control->stats.errMsg = message;
    return theTest;
}

#define createTestWithError(name, errorMessage, input, nodes, types, entities) \
    createTestWithError0((name), errorMessage, (input), (nodes), sizeof(nodes)/sizeof(Node), types,\
    sizeof(types)/4, entities, sizeof(entities)/sizeof(EntityImport), proto, a)


private ParserTest createTestWithLocs0(String* name, String* input, Arr(Node) nodes,
                            Int countNodes, Arr(Int) types, Int countTypes, Arr(TestEntityImport) entities,
                            Int countEntities, Arr(SourceLoc) locs, Int countLocs,
                            Compiler* proto, Arena* a) {
// Creates a test with two parsers where the source locs are specified (unlike most parser tests)
    ParserTest theTest = createTest0(name, input, nodes, countNodes, types, countTypes, entities,
                                     countEntities, proto, a);
    if (theTest.control->stats.wasLexerError) {
        return theTest;
    }
    StandardText stText = getStandardTextLength();
    for (Int j = 0; j < countLocs; ++j) {
        SourceLoc loc = locs[j];
        loc.startBt += stText.len;
        theTest.control->sourceLocs->cont[j] = loc;
    }
    theTest.compareLocsToo = true;
    return theTest;
}

#define createTestWithLocs(name, input, nodes, types, entities, locs) \
    createTestWithLocs0((name), (input), (nodes), sizeof(nodes)/sizeof(Node), types,\
    sizeof(types)/4, entities, sizeof(entities)/sizeof(EntityImport),\
    locs, sizeof(locs)/sizeof(SourceLoc), proto, a)

int equalityParser(/* test specimen */Compiler a, /* expected */Compiler b, Bool compareLocsToo) {
// Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first
// differing token otherwise
    if (a.stats.wasError != b.stats.wasError || (!endsWith(a.stats.errMsg, b.stats.errMsg))) {
        return -1;
    }
    int commonLength = a.nodes.len < b.nodes.len ? a.nodes.len : b.nodes.len;
    int i = 0;
    for (; i < commonLength; i++) {
        Node nodA = a.nodes.cont[i];
        Node nodB = b.nodes.cont[i];
        if (nodA.tp != nodB.tp
            || nodA.pl1 != nodB.pl1 || nodA.pl2 != nodB.pl2 || nodA.pl3 != nodB.pl3) {
            printf("\n\nUNEQUAL RESULTS on %d\n", i);
            if (nodA.tp != nodB.tp) {
                printf("Diff in tp, %d but was expected %d\n", nodA.tp, nodB.tp);
            }
            if (nodA.pl1 != nodB.pl1) {
                printf("Diff in pl1, %d but was expected %d\n", nodA.pl1, nodB.pl1);
            }
            if (nodA.pl2 != nodB.pl2) {
                printf("Diff in pl2, %d but was expected %d\n", nodA.pl2, nodB.pl2);
            }
            if (nodA.pl3 != nodB.pl3) {
                printf("Diff in pl3, %d but was expected %d\n", nodA.pl3, nodB.pl3);
            }
            return i;
        }
    }
    if (compareLocsToo) {
        for (i = 0; i < commonLength; ++i) {
            SourceLoc locA = a.sourceLocs->cont[i];
            SourceLoc locB = b.sourceLocs->cont[i];
            if (locA.startBt != locB.startBt || locA.lenBts != locB.lenBts) {
                printf("\n\nUNEQUAL SOURCE LOCS on %d\n", i);
                if (locA.lenBts != locB.lenBts) {
                    printf("Diff in lenBts, %d but was expected %d\n", locA.lenBts, locB.lenBts);
                }
                if (locA.startBt != locB.startBt) {
                    printf("Diff in startBt, %d but was expected %d\n", locA.startBt, locB.startBt);
                }
                return i;
            }
        }
    }
    return (a.nodes.len == b.nodes.len) ? -2 : i;
}


void runTest(ParserTest test, int* countPassed, int* countTests, Arena *a) {
// Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code
    (*countTests)++;
    if (test.test->tokens.len == 0) {
        print("Lexer result empty");
        return;
    } else if (test.control->stats.wasLexerError) {
        print("Lexer error");
        printLexer(test.control);
        return;
    }
    parseMain(test.test, a);

    int equalityStatus = equalityParser(*test.test, *test.control, test.compareLocsToo);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(test.test->stats.errMsg);
        printf("\nBut was expected: ");
        printString(test.control->stats.errMsg);
        printf("\n");
        print("    LEXER:")
        printLexer(test.test);
        print("    PARSER:")
        printParser(test.test, a);
    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On node %d\n", equalityStatus);
        print("    LEXER:")
        printLexer(test.test);
        print("    PARSER:")
        printParser(test.test, a);
    }
}


private Node doubleNd(double value) {
    return (Node){ .tp = tokDouble, .pl1 = longOfDoubleBits(value) >> 32,
                                    .pl2 = longOfDoubleBits(value) & LOWER32BITS };
}

//}}}
//{{{ Assignment tests

ParserTestSet* assignmentTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Assignment test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple assignment"),
            s("x = 12"),
            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0 }, // x
                (Node){ .tp = nodExpr, .pl2 = 1 },
                (Node){ .tp = tokInt,  .pl2 = 12 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                { .startBt = 0, .lenBts = 1 },
                { .startBt = 2, .lenBts = 4 },
                { .startBt = 4, .lenBts = 2 }
            })
        ),
        createTestWithLocs(
            s("Double assignment"),
            s("x = 12;\n"
              "second = x"
            ),
            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0 }, // x
                (Node){ .tp = nodExpr, .pl2 = 1 },
                (Node){ .tp = tokInt,  .pl2 = 12 },
                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 0}, // second
                (Node){ .tp = nodExpr, .pl2 = 1 },
                (Node){ .tp = nodId,          .pl2 = 0 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                { .startBt =  0, .lenBts = 1 },
                { .startBt =  2, .lenBts = 4 },
                { .startBt =  4, .lenBts = 2 },
                { .startBt =  7, .lenBts = 6 },
                { .startBt = 14, .lenBts = 3 },
                { .startBt = 16, .lenBts = 1 }
            })
        ),
        createTestWithError(
            s("Assignment shadowing error"),
            s(errAssignmentShadowing),
            s("x = 12;\n"
              "x = 7"
            ),
            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl2 = 1 },
                (Node){ .tp = tokInt,  .pl2 = 12 },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Reassignment"),
            s("main = {{\n"
              "x~ = `foo`;\n"
              "x = `bar`}}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 9 },
                (Node){ .tp = nodBinding, .pl1 = 0         },
                (Node){ .tp = nodScope,           .pl2 = 7 },
                (Node){ .tp = nodAssignLeft, .pl1 = 1,  .pl2 = 0 },   // x
                (Node){ .tp = nodExpr, .pl2 = 1         },
                (Node){ .tp = tokString,                   },
                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = nodBinding, .pl1 = 1         }, // second
                (Node){ .tp = nodExpr, .pl2 = 1 },
                (Node){ .tp = tokString,                   }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),

//~        createTest(
//~            s("Mutation simple"),
//~            s("main = {{\n"
//~              "    x~ = 12;\n"
//~              "    x += 3\n"
//~              "}}"
//~            ),
//~            ((Node[]) {
//~                (Node){ .tp = nodFnDef,           .pl2 = 11 },
//~                (Node){ .tp = nodBinding, .pl1 = 0,         },
//~                (Node){ .tp = nodScope,           .pl2 = 9 },
//~                (Node){ .tp = nodAssignLeft,  .pl1 = 1, .pl2 = 0 }, // 1 = x, 0 is taken by "main"
//~                (Node){ .tp = nodExpr, .pl2 = 1,        },
//~                (Node){ .tp = tokInt,             .pl2 = 12 },
//~                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 1 },
//~                (Node){ .tp = nodBinding, .pl1 = 1,        },
//~                (Node){ .tp = nodExpr, .pl2 = 3 },
//~                (Node){ .tp = nodExpr,            .pl2 = 3 },
//~                (Node){ .tp = nodId,   .pl1 = 1,  .pl2 = 1 },
//~                (Node){ .tp = tokInt,             .pl2 = 3 },
//~                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 }
//~            }),
//~            ((Int[]) {}),
//~            ((TestEntityImport[]) {})
//~        ),
        /*
        createTestWithLocs(
            s("Mutation complex"),
            s("main = {{ \n"
                   "x~ = 2;\n"
                   "x ^= 15 - 8}}"
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
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {})
        ),
        createTestWithLocs(
            s("Complex left side"),
            s("arr@i = 5"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 13, .startBt = 0, .lenBts = 34 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 4 },
                (Node){ .tp = nodScope,           .pl2 = 11, .startBt = 8, .lenBts = 26 },
                (Node){ .tp = nodAssignLeft,      .pl2 = 2, .startBt = 14, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 14, .lenBts = 1 },     // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                { .startBt = 0, .lenBts = 1 },
                { .startBt = 2, .lenBts = 4 },
                { .startBt = 4, .lenBts = 2 }
             })
        )
        */
    }));
}

//}}}
//{{{ Expression tests

ParserTestSet* expressionTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Expression test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple function call"),
            s("x = foo 10 2 `hw`"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl2 = 4 },
                (Node){ .tp = tokInt, .pl2 = 10,      },
                (Node){ .tp = tokInt, .pl2 = 2,       },
                (Node){ .tp = tokString,              },
                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 3 } // foo
            })),
            ((Int[]) { 4, tokInt, tokInt, tokString, tokDouble }),
            ((TestEntityImport[]) {{ .nameInd = 0, .typeInd = 0 }}),
            ((SourceLoc[]) {
                { .startBt = 0, .lenBts = 1 },
                { .startBt = 2, .lenBts = 16 },
                { .startBt = 4, .lenBts = 2 },
                { .startBt = 12, .lenBts = 1 },
                { .startBt = 14, .lenBts = 4 },
                { .startBt = 7, .lenBts = 4 }
            })
        ),
        createTest(
            s("Data allocation"),
            s("x = L(1 2 3)"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 6 },

                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = nodDataAlloc, .pl1 = stToNameId(strL), .pl2 = 3,
                        .pl3 = 3 },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = -1 } // the allocated array
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Data allocation with expression inside"),
            s("x = L(4 (2 ^ 7))"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 8 },

                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = nodDataAlloc, .pl1 = stToNameId(strL), .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl1 = 0, .pl2 = 3 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opExponent, tokInt), .pl2 = 2 },

                (Node){ .tp = nodId, .pl1 = 1, .pl2 = -1 } // the allocated array
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Nested data allocation with expression inside"),
            s("x = L(L(1) L(4 (2 - 7)) L(2 3))"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 20 },

                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 0 }, // L(1)
                (Node){ .tp = nodDataAlloc, .pl1 = stToNameId(strL), .pl2 = 1, .pl3 = 1 },
                (Node){ .tp = tokInt, .pl2 = 1 },

                (Node){ .tp = nodAssignLeft, .pl1 = 2, .pl2 = 0 }, // L(2 (2 - 7))
                (Node){ .tp = nodDataAlloc, .pl1 = stToNameId(strL), .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl1 = 0, .pl2 = 3 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokInt), .pl2 = 2 },

                (Node){ .tp = nodAssignLeft, .pl1 = 3, .pl2 = 0 }, // L(2 3)
                (Node){ .tp = nodDataAlloc, .pl1 = stToNameId(strL), .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 3 },

                (Node){ .tp = nodAssignLeft, .pl1 = 4, .pl2 = 0 }, // L(2 3)
                (Node){ .tp = nodDataAlloc, .pl1 = stToNameId(strL), .pl2 = 3, .pl3 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = -1 },
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = -1 },
                (Node){ .tp = nodId, .pl1 = 3, .pl2 = -1 },

                (Node){ .tp = nodId, .pl1 = 4, .pl2 = -1 } // the allocated array
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Nested function call 1"),
            s("x = foo 10 (bar) 3"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0},
                (Node){ .tp = nodExpr, .pl2 = 4},

                (Node){ .tp = tokInt, .pl2 = 10},
                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 0}, // bar
                (Node){ .tp = tokInt, .pl2 = 3},
                (Node){ .tp = nodCall, .pl1 = I - 2, .pl2 = 3}, // foo
            })),
            ((Int[]) {4, tokInt, tokDouble, tokInt, tokString, // Int Double Int -> String
                      1, tokDouble}), // () -> Double
            ((TestEntityImport[]) {(TestEntityImport){ .nameInd = 0, .typeInd = 0},
                               (TestEntityImport){ .nameInd = 1, .typeInd = 1}})
        ),
        createTest(
            s("Nested function call 2"),
            s("x = foo 10 (bar 3.4)"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl1 = 0 },

                (Node){ .tp = nodExpr,           .pl2 = 4  },
                (Node){ .tp = tokInt,            .pl2 = 10 },
                doubleNd(3.4),
                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 1 }, // bar
                (Node){ .tp = nodCall, .pl1 = I - 2, .pl2 = 2 }  // foo
            })),
            ((Int[]) {3, tokInt, tokBool, tokBool,
                      2, tokDouble, tokBool }
            ),
            ((TestEntityImport[]) {(TestEntityImport){ .nameInd = 0, .typeInd = 0},
                               (TestEntityImport){ .nameInd = 1, .typeInd = 1}}
            )
        ),
        createTest(
            s("Triple function call"),
            s("x = bar 2 (foo (foo `hw`)) 4"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft,     .pl2 = 0 },
                (Node){ .tp = nodExpr,           .pl2 = 6 },
                (Node){ .tp = tokInt,            .pl2 = 2 },
                (Node){ .tp = tokString,                      },
                (Node){ .tp = nodCall, .pl1 = I - 2, .pl2 = 1 }, // foo
                (Node){ .tp = nodCall, .pl1 = I - 2, .pl2 = 1 }, // foo
                (Node){ .tp = tokInt,                .pl2 = 4 },
                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 3 } // bar
            })),
            ((Int[]) {4, tokInt, tokString, tokInt, tokDouble,
                      2, tokString, tokString}),
            ((TestEntityImport[]) {(TestEntityImport){ .nameInd = 0, .typeInd = 1},
                               (TestEntityImport){ .nameInd = 1, .typeInd = 0}})
        ),
        createTest(
            s("Operators simple"),
            s("x = 1 + (9/3)"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr,  .pl2 = 5 },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl2 = 9 },
                (Node){ .tp = tokInt, .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opDivBy, tokInt), .pl2 = 2 }, // *
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 } // +
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Operator arity error"),
            s(errTypeNoMatchingOverload),
            s("x = 1 + 20 100"),
            (((Node[]) {
                (Node){ .tp = nodAssignLeft },
                (Node){ .tp = nodExpr },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl2 = 20 },
                (Node){ .tp = tokInt, .pl2 = 100 },
                (Node){ .tp = nodCall, .pl1 = O + opPlus, .pl2 = 3 }
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Unary operator precedence"),
            s("x = `12` + -3##$"),

            ((Node[]) {
                (Node){ .tp = nodAssignLeft, .pl2 = 0 },
                (Node){ .tp = nodExpr,              .pl2 = 5 },
                (Node){ .tp = tokString },
                (Node){ .tp = tokInt, .pl1 = -1,    .pl2 = -3 },
                (Node){ .tp = nodCall, .pl1 = oper(opSize, tokInt), .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokString), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
    }));
}

//}}}
//{{{ Function tests

ParserTestSet* functionTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Functions test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple function definition 1"),
            s("newFn = (\\x Int y (L Bool) -> : a = x)"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,             .pl2 = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1 },  // param x
                (Node){ .tp = nodBinding, .pl1 = 2 },  // param y
                (Node){ .tp = nodAssignLeft, .pl1 = 3 },
                (Node){ .tp = nodExpr, .pl2 = 1 },
                (Node){ .tp = nodId, .pl1 = 1,      .pl2 = 1 }   // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                (SourceLoc){ .startBt = 8, .lenBts = 30 },
                (SourceLoc){ .startBt = 10, .lenBts = 1 },
                (SourceLoc){ .startBt = 16, .lenBts = 1 },
                (SourceLoc){ .startBt = 32, .lenBts = 1 },
                (SourceLoc){ .startBt = 34, .lenBts = 3 },
                (SourceLoc){ .startBt = 36, .lenBts = 1 }
             })
        ),
       /*
        createTest(
            s("Simple function definition 2"),
            s("newFn = (\\x Str y Double -> Str:\n"
              "    a = x;\n"
              "    return a\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 9 },
                (Node){ .tp = nodBinding, .pl1 = 0,          },
                (Node){ .tp = nodScope,            .pl2 = 7 },
                (Node){ .tp = nodBinding, .pl1 = 1           }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2           }, // param y
                (Node){ .tp = nodAssignLeft,       .pl2 = 2  },
                (Node){ .tp = nodBinding, .pl1 = 3           }, // local a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 2  }, // x
                (Node){ .tp = nodReturn,           .pl2 = 1  },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 5  }  // a
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Function definition wrong return type"),
            s(errTypeWrongReturnType),
            s("newFn = (\\x Double y Double -> Str:\n"
              "    a = x;\n"
              "    return a\n"
              ")"
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
            s("newFn = (\x Int y Double -> Str:\n"
              "    return $((toDouble x) - y))"
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
            s("foo = (\\x Int y Double -> Int:\n"
              "    a = x;\n"
              "    return bar y a\n"
              ")\n"
              "bar = (\\x Double y Int -> Int:\n"
              "    return foo y x\n"
              ")"
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
            s("main = (\\x Int y Float ->:\n"
              "    (do\n"
              "        a = 5\n"
              "    )\n"
              "    a = ,x - y;\n"
              "    print a\n"
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
            //~ .input = str("foo = (\\x Int y Int -> Int:\n"
                              //~ "z = * x y;\n"
                              //~ "return (+ z x):inner 5 2\n"
                              //~ "fn inner Int(a Int b Int c Int)(return a - 2*b + 3*c)"
                              //~ ")\n"

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
       */
    }));
}

//}}}
//{{{ If tests
/*
ParserTestSet* ifTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("If test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple if"),
            s("f = (\\\n"
              "    (if == 5 5: print `5` }\n"
              ")"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl2 = 11 },
                (Node){ .tp = nodBinding, .pl1 = 0 }, // f
                (Node){ .tp = nodScope,    .pl2 = 9 },

                (Node){ .tp = nodIf, .pl1 = slScope, .pl2 = 8 },
                (Node){ .tp = nodExpr,     .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opEquality, tokInt), .pl2 = 2 }, // ==
                (Node){ .tp = tokInt,      .pl2 = 5 },
                (Node){ .tp = tokInt,      .pl2 = 5},

                (Node){ .tp = nodIf,       .pl2 = 3 },
                (Node){ .tp = nodExpr,     .pl2 = 2 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1 }, // print
                (Node){ .tp = tokString }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                (SourceLoc){ .startBt = 0, .lenBts = 38 },
                (SourceLoc){ .startBt = 4, .lenBts = 1 },
                (SourceLoc){ .startBt = 5, .lenBts = 33 },

                (SourceLoc){ .startBt = 13, .lenBts = 24 },
                (SourceLoc){ .startBt = 17, .lenBts = 6 },
                (SourceLoc){ .startBt = 17, .lenBts = 2 },
                (SourceLoc){ .startBt = 20, .lenBts = 1 },
                (SourceLoc){ .startBt = 22, .lenBts = 1 },

                (SourceLoc){ .startBt = 27, .lenBts = 9 },
                (SourceLoc){ .startBt = 27, .lenBts = 9 },
                (SourceLoc){ .startBt = 27, .lenBts = 5 },
                (SourceLoc){ .startBt = 33, .lenBts = 3 }

             })
        ),
        createTest(
            s("If with else"),
            s("f = {{-> Str:\n"
              "    if{ 5 > 3: `5` } else{`=)` }\n"
              "}}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl2 = 11 },
                (Node){ .tp = nodBinding, .pl1 = 0 }, // f
                (Node){ .tp = nodScope, .pl2 = 9 },

                (Node){ .tp = nodIf, .pl1 = slScope, .pl2 = 8 },

                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opGreaterTh, tokInt), .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 5 },
                (Node){ .tp = tokInt, .pl2 = 3 },
                (Node){ .tp = nodIf,  .pl2 = 1 },
                (Node){ .tp = tokString },

                (Node){ .tp = nodIf, .pl2 = 1 },
                (Node){ .tp = tokString     },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("If with elseif"),
            s("f = (\\\n"
              "    (if > 5 3: 11}\n"
              "    eif == 5 3: 4}\n"
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
            s("f = (\\\n"
              "    (if > 5 3: 11 }\n"
              "    eif == 5 3: 4 }\n"
              "    else 100 )\n"
              ")"
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

*/
//}}}
//{{{ Loop tests
ParserTestSet* loopTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Loops test set"), a, ((ParserTest[]){
        createTest(
            s("Simple loop 1"),
            s("f = (\\(for x~ = 1; < x 101; x = + x 1: print $x ))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 19, .pl3 = 0 },
                (Node){ .tp = nodFor,           .pl2 = 18 },
                
                (Node){ .tp = nodScope, .pl2 = 17 },
                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 0 }, // x~ = 1
                (Node){ .tp = nodExpr, .pl2 = 1 },
                (Node){ .tp = tokInt,          .pl2 = 1 },
                (Node){ .tp = nodExpr, .pl2 = 3 }, // < x 101
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1, },
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 9 },
                (Node){ .tp = nodExpr,           .pl2 = 3 }, // print $x
                (Node){ .tp = nodId,   .pl1 = 1, .pl2 = 1 },      // x
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 }, // print
                (Node){ .tp = nodAssignLeft, .pl1 = 1, .pl2 = 0 }, // x = x + 1
                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl1 = 0, .pl2 = 1 },
                (Node){ .tp = nodCall,   .pl1 = oper(opPlus, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
/*
        createTest(
            s("For with two complex initializers"),
            s("f = {{->:\n"
              "    for{ x~ = 17; y~ = x/5; < y 101:\n"
              "        print x;\n"
              "        x -= 1;\n"
              "        y += 1;}\n"
              "}}"
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
            s("f = (\\\n"
              "    x = 4;\n"
              "    (for < x 101: \n"
              "        print x) )"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,         .pl2 = 16 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope, .pl2 = 14 }, // function body

                (Node){ .tp = nodAssignLeft, .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1 },  // x
                (Node){ .tp = tokInt,              .pl2 = 4 },

                (Node){ .tp = nodFor,            .pl2 = 10 },
                (Node){ .tp = nodScope, .pl2 = 9,          },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl2 = 3,           },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101 },

                (Node){ .tp = nodExpr,         .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2 }  // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For with break and continue"),
            s("f = (\\\n"
              "    (for x = 0; < x 101:\n"
              "        break;\n"
              "        continue;)\n"
              ")"
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
            s("f = (\\\n"
              "    (for x = 0; < x 101:\n"
              "        break 2\n"
              "))"
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
            s("f = (\\\n"
              "    (for a = 0; < a 101:\n"
              "        (for b = 0; < b 101:\n"
              "            (for c = 0; < c 101:\n"
              "                break 3)\n"
              "        }\n"
              "        (for d = 0; < d 51:\n"
              "            (for e = 0; < e 101:\n"
              "                continue 2)\n"
              "        )\n"
              "        print a\n"
              "    )\n"
              ")"
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
            s("f = (\\ (for x~ = 1; / x 101: print x) )"),
            ((Node[]) {
                (Node){ .tp = nodFnDef         },
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = nodScope }, // function body

                (Node){ .tp = nodFor },

                (Node){ .tp = nodScope },

                (Node){ .tp = nodAssignLeft,      .pl2 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1  }, // x
                (Node){ .tp = tokInt,             .pl2 = 1 },

                (Node){ .tp = nodForCond, .pl1 = slStmt, .pl2 = 4 },
                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opDivBy, tokInt), .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
       */
    }));
}
/*
ParserTestSet* typeTests(Compiler* proto, Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Types test set"), a, ((ParserTest[]){
        createTest(
            s("Simple type 1"),
            s("Foo = (* id Int name Str)"),
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
*/
//}}}


void runATestSet(ParserTestSet* (*testGenerator)(Compiler*, Compiler*, Arena*), int* countPassed,
                 int* countTests, Compiler* proto, Compiler* protoOvs, Arena* a) {
    ParserTestSet* testSet = (testGenerator)(proto, protoOvs, a);
    for (int j = 0; j < testSet->totalTests; j++) {
        ParserTest test = testSet->tests[j];
        runTest(test, countPassed, countTests, a);
    }
}


int main() {
    printf("----------------------------\n");
    printf("--  PARSER TEST  --\n");
    printf("----------------------------\n");
    if (sizeof(TypeHeader) != 8)  {
        print("Sizeof TypeHeader != 8, but it should be 8. Something fishy goin' on! "
              "Tests are cancelled");
        return 1;
    }

    Arena *a = mkArena();
    Compiler* proto = createProtoCompiler(a);
    Compiler* protoOvs = createProtoCompiler(a);
    createOverloads(protoOvs);
    int countPassed = 0;
    int countTests = 0;
   /*
    runATestSet(&functionTests, &countPassed, &countTests, proto, protoOvs, a);
    runATestSet(&expressionTests, &countPassed, &countTests, proto, protoOvs, a);
    runATestSet(&assignmentTests, &countPassed, &countTests, proto, protoOvs, a);

    runATestSet(&ifTests, &countPassed, &countTests, proto, protoOvs, a);
    runATestSet(&typeTests, &countPassed, &countTests, proto, protoOvs, a);
   */

    runATestSet(&loopTests, &countPassed, &countTests, proto, protoOvs, a);
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
