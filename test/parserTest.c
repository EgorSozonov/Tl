#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../include/eyr.h"
#include "../eyr.internal.h"
#include "eyrTest.h"

//{{{ Utils

typedef struct { //:ParserTest
    String name;
    Compiler* test;
    Compiler* control;
    Bool compareLocsToo;
} ParserTest;


typedef struct {
    String name;
    Int totalTests;
    Arr(ParserTest) tests;
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


private ParserTestSet* createTestSet0(String name, Arena *a, int count, Arr(ParserTest) tests) {
    ParserTestSet* result = allocateOnArena(sizeof(ParserTestSet), a);
    result->name = name;
    result->totalTests = count;
    result->tests = allocateOnArena(count*sizeof(ParserTest), a);
    if (result->tests == NULL) return result;

    for (int i = 0; i < count; i++) {
        result->tests[i] = tests[i];
    }
    printString(getStats(tests[0].control).errMsg);
    return result;
}

#define createTestSet(n, a, tests) createTestSet0(n, a, sizeof(tests)/sizeof(ParserTest), tests)


private Int tryGetOper0(Int opType, Int typeId, Compiler* protoOvs) {
// Try and convert test value to operator entityId
    Int entityId;
    Int ovInd = -getBinding(opType, protoOvs) - 2;
    bool foundOv = findOverload(typeId, ovInd, protoOvs, OUT &entityId);
    if (foundOv)  {
        return entityId + O;
    } else {
        return -1;
    }
}

#define oper(opType, typeId) tryGetOper0(opType, typeId, protoOvs)

private Int
transformBindingEntityId(Int inp, CompStats const* stats) {
    if (inp < S) { // parsed stuff
        return inp + stats->countNonparsedEntities;
    } else if (inp < O){ // imported but not operators: "foo", "bar"
        return inp - I + stats->countNonparsedEntities;
    } else { // operators
        return inp - O;
    }
}

private Arr(TypeId)
importTypes(Arr(Int) types, Int countTypes, Compiler* cm, Arena* aTmp) { //:importTypes
// Importing simple function types for testing purposes
    Int countImportedTypes = 0;
    Int j = 0;
    while (j < countTypes) {
        if (types[j] == 0) {
            return NULL;
        }
        ++(countImportedTypes);
        j += types[j] + 1;
    }
    Arr(TypeId) typeIds = allocateOnArena(countImportedTypes*4, aTmp);
    j = 0;
    Int t = 0;
    while (j < countTypes) {
        const Int importLen = types[j];
        const Int sentinel = j + importLen + 1;
        TypeId initTypeId = getStats(cm).typesLen;

        pushIntypes(importLen + 2, cm); // +3 because the header takes 2 ints, 1 more for
                                        // the return typeId
        addTypeHeaderForTestFunction(importLen - 1, cm);
        for (Int k = j + 1; k < sentinel; k++) { // <= because there are (arity + 1) elts -
                                                 // +1 for the return type!
            pushIntypes(types[k], cm);
        }

        TypeId mergedTypeId = mergeType(initTypeId, cm);
        typeIds[t] = mergedTypeId;
        t++;
        j = sentinel;
    }
    return typeIds;
}


private ParserTest createTest0(String name, String sourceCode, Arr(Node) nodes, Int countNodes,
                               Arr(Int) types, Int countTypes, Arr(TestEntityImport) imports,
                               Int countImports, Arena* a) { //:createTest0
// Creates a test with two parsers: one is the init parser (contains all the "imported" bindings and
// pre-defined nodes), and the other is the output parser (with all the stuff parsed from source code).
// When the test is run, the init parser will parse the tokens and then will be compared to the
// expected output parser.
// Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
// it will be inserted as 1 + (the number of built-in bindings) etc
    Compiler* test = lexicallyAnalyze(sourceCode, a);
    Compiler* control = lexicallyAnalyze(sourceCode, a);
    CompStats controlStats = getStats(control);
    if (controlStats.wasLexerError == true) {
        return (ParserTest) {
            .name = name, .test = test, .control = control, .compareLocsToo = false };
    }
    initializeParser(control, a);
    initializeParser(test, a);
    Arr(TypeId) typeIds = importTypes(types, countTypes, control, a);
    importTypes(types, countTypes, test, a);

    if (countImports > 0) {
        Arr(Entity) importsForControl = allocateOnArena(countImports*sizeof(Entity), a);
        Arr(Entity) importsForTest = allocateOnArena(countImports*sizeof(Entity), a);

        for (Int j = 0; j < countImports; j++) {
            importsForControl[j] = (Entity) {
                .name = nameOfStandard(strSentinel - 3 + imports[j].nameInd),
                .typeId = typeIds[imports[j].typeInd] };
            importsForTest[j] = (Entity) {
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
                || nodeType == nodFnDef) {
            nd.pl1 = transformBindingEntityId(nd.pl1, &controlStats);
        }
        // transform pl2/pl3 if it holds NameId
        if (nodeType == nodId && nd.pl2 != -1)  {
            nd.pl2 += controlStats.firstParsed;
        }
        if (nodeType == nodFnDef && nd.pl3 != -1)  {
            nd.pl3 += controlStats.firstParsed;
        }
        addNode(nd, (SourceLoc){.startBt = 0, .lenBts = 0}, control);
    }
    return (ParserTest){ .name = name, .test = test, .control = control, .compareLocsToo = false };
}

#define createTest(name, input, nodes, types, entities) \
    createTest0((name), (input), (nodes), sizeof(nodes)/sizeof(Node), (types), sizeof(types)/4, \
    (entities), sizeof(entities)/sizeof(TestEntityImport), a)


private ParserTest createTestWithError0(String name, String message, String input,
        Arr(Node) nodes, Int countNodes, Arr(Int) types, Int countTypes,
        Arr(TestEntityImport) entities, Int countEntities, Arena* a) {
// Creates a test with two parsers where the expected result is an error in parser
    ParserTest theTest = createTest0(name, input, nodes, countNodes, types, countTypes, entities,
                                     countEntities, a);
    setStats((CompStats){.wasError = true, .errMsg = message}, theTest.control);
    return theTest;
}

#define createTestWithError(name, errorMessage, input, nodes, types, entities) \
    createTestWithError0((name), errorMessage, (input), (nodes), sizeof(nodes)/sizeof(Node), types,\
    sizeof(types)/4, entities, sizeof(entities)/sizeof(Entity), a)


private ParserTest createTestWithLocs0(String name, String input, Arr(Node) nodes,
                    Int countNodes, Arr(Int) types, Int countTypes, Arr(TestEntityImport) entities,
                    Int countEntities, Arr(SourceLoc) locs, Int countLocs,
                    Arena* a) {
// Creates a test with two parsers where the source locs are specified (unlike most parser tests)
    ParserTest theTest = createTest0(name, input, nodes, countNodes, types, countTypes, entities,
                                     countEntities, a);
    if (getStats(theTest.control).wasLexerError) {
        return theTest;
    }
    StandardText stText = getStandardTextLength();
    for (Int j = 0; j < countLocs; ++j) {
        SourceLoc loc = locs[j];
        loc.startBt += stText.len;
        setLoc(loc, theTest.control);
    }
    theTest.compareLocsToo = true;
    return theTest;
}

#define createTestWithLocs(name, input, nodes, types, entities, locs) \
    createTestWithLocs0((name), (input), (nodes), sizeof(nodes)/sizeof(Node), types,\
    sizeof(types)/4, entities, sizeof(entities)/sizeof(TestEntityImport),\
    locs, sizeof(locs)/sizeof(SourceLoc), a)


void runTest(ParserTest test, TestContext* ct) {
// Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code
    ct->countTests += 1;
    CompStats testStats = getStats(test.test);
    CompStats controlStats = getStats(test.control);
    if (testStats.toksLen == 0) {
        print("Lexer result empty");
        return;
    } else if (controlStats.wasLexerError) {
        print("Lexer error");
        printLexer(test.control);
        return;
    }
    parseMain(test.test, ct->a);

    int equalityStatus = equalityParser(test.test, test.control, test.compareLocsToo);
    if (equalityStatus == -2) {
        ct->countPassed += 1;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(testStats.errMsg);
        printf("\nBut was expected: ");
        printString(controlStats.errMsg);
        printf("\n");
        print("    LEXER:")
        printLexer(test.test);
        print("    PARSER:")
        printParser(test.test, ct->a);
    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On node %d\n", equalityStatus);
        print("    LEXER:")
        printLexer(test.test);
        print("    PARSER:")
        printParser(test.test, ct->a);
    }
}


private Node doubleNd(double value) {
    return (Node){ .tp = tokDouble, .pl1 = longOfDoubleBits(value) >> 32,
                                    .pl2 = longOfDoubleBits(value) & LOWER32BITS };
}

//}}}
//{{{ Assignment tests

ParserTestSet* assignmentTests(Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Assignment test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple top-level definition"),
            s("def x = 12;"),
            ((Node[]) {
                (Node){ .tp = nodDef, .pl2 = 2, .pl3 = 2 }, // x
                (Node){ .tp = nodBinding, .pl2 = 0 },
                (Node){ .tp = tokInt,  .pl2 = 12 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                { .startBt = 0, .lenBts = 6 },
                { .startBt = 0, .lenBts = 1 },
                { .startBt = 4, .lenBts = 2 }
            })
        ),
       /* 
        createTestWithLocs(
            s("Simple assignment"),
            s("x = 12;"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // x
                (Node){ .tp = nodBinding, .pl2 = 0 },
                (Node){ .tp = tokInt,  .pl2 = 12 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                { .startBt = 0, .lenBts = 6 },
                { .startBt = 0, .lenBts = 1 },
                { .startBt = 4, .lenBts = 2 }
            })
        ),
        createTestWithLocs(
            s("Double assignment"),
            s("x = 12;\n"
              "second = x;"
            ),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // x = 12
                (Node){ .tp = nodBinding, .pl1 = 0 },
                (Node){ .tp = tokInt,  .pl2 = 12 },
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2}, // second
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = nodId,          .pl2 = 0 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                { .startBt =  0, .lenBts = 7 },
                { .startBt =  0, .lenBts = 1 },
                { .startBt =  4, .lenBts = 2 },
                { .startBt =  8, .lenBts = 10 },
                { .startBt =  8, .lenBts = 6 },
                { .startBt = 17, .lenBts = 1 }
            })
        ),

        createTestWithError(
            s("Assignment shadowing error"),
            s(errCannotMutateImmutable),
            s("x = 12;\n"
              "x = 7;"
            ),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl2 = 0 },
                (Node){ .tp = tokInt,  .pl2 = 12 },
                (Node){ .tp = nodAssignment }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Reassignment"),
            s("def main = {{}\n"
              "    x~ = `foo`;\n"
              "    x = `bar`;\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 6 },
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 },   // x~ = `foo`
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = tokString,                   },
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // x = `bar`
                (Node){ .tp = nodBinding, .pl1 = 1         },
                (Node){ .tp = tokString,                   }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Mutation simple"),
            s("def main = {{}\n"
              "    x~ = 12;\n"
              "    x += 55\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 9 },
                (Node){ .tp = nodAssignment,      .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1,        },
                (Node){ .tp = tokInt,             .pl2 = 12 },
                (Node){ .tp = nodAssignment,      .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1,        },
                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodId,   .pl1 = 1,  .pl2 = 1 },
                (Node){ .tp = tokInt,             .pl2 = 55 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Mutation complex"),
            s("def main = {{}\n"
              "    a = [1 2 3];\n"
              "    a[1] *= (+ a[0] a[2])\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 27 },

                (Node){ .tp = nodAssignment,      .pl2 = 9, .pl3 = 2 }, // a = [1 2 3]
                (Node){ .tp = nodBinding, .pl1 = 1,        },
                (Node){ .tp = nodExpr, .pl1 = 1,  .pl2 = 7,        },
                (Node){ .tp = nodAssignment,      .pl2 = 5, .pl3 = 2  },
                (Node){ .tp = nodBinding,      .pl1 = 2, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 3, .pl3 = 3 },
                (Node){ .tp = tokInt,             .pl2 = 1 },
                (Node){ .tp = tokInt,             .pl2 = 2 },
                (Node){ .tp = tokInt,             .pl2 = 3 },
                (Node){ .tp = nodId,    .pl1 = 2, .pl2 = -1 },

                (Node){ .tp = nodAssignment,      .pl2 = 16, .pl3 = 5 }, // a[1] *= ...
                (Node){ .tp = nodExpr,            .pl2 = 3 }, // a[1] on the left
                (Node){ .tp = nodId,   .pl1 = 1,  .pl2 = 1 },
                (Node){ .tp = tokInt,             .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = opGetElemPtr + O, .pl2 = 2 },
                (Node){ .tp = nodExpr,            .pl2 = 11 },
                (Node){ .tp = nodId,   .pl1 = 1,  .pl2 = 1 }, // a[1] on the right
                (Node){ .tp = tokInt,             .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = opGetElem + O, .pl2 = 2 },
                (Node){ .tp = nodId,   .pl1 = 1,  .pl2 = 1 },
                (Node){ .tp = tokInt,             .pl2 = 0 },
                (Node){ .tp = nodCall, .pl1 = opGetElem + O, .pl2 = 2 },
                (Node){ .tp = nodId,   .pl1 = 1,  .pl2 = 1 },
                (Node){ .tp = tokInt,             .pl2 = 2 },
                (Node){ .tp = nodCall, .pl1 = opGetElem + O, .pl2 = 2 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 },
                (Node){ .tp = nodCall, .pl1 = oper(opTimes, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Complex left side"),
            s("def main = {{}\n"
              "arr = [1 2];\n"
              "arr[0] = 21;\n"
              "}"
             ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 15 },
                (Node){ .tp = nodAssignment, .pl2 = 8, .pl3 = 2 },   // arr = [1 2]
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 6 },
                (Node){ .tp = nodAssignment, .pl2 = 4, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 2, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = -1 },

                (Node){ .tp = nodAssignment, .pl2 = 5, .pl3 = 5 }, // arr[0] = 21
                (Node){ .tp = nodExpr,         .pl2 = 3  },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1  },
                (Node){ .tp = tokInt, .pl2 = 0           },
                (Node){ .tp = nodCall, .pl1 = opGetElemPtr + O, .pl2 = 2 },
                (Node){ .tp = tokInt,          .pl2 = 21 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Very complex left side"),
            s("def main = {{\n"
              "arr = [[1 2] [4 3]];\n"
              "arr[1][0] = 21;\n"
              "}"
             ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 27 },
                (Node){ .tp = nodAssignment, .pl2 = 18, .pl3 = 2 },   // arr = [1 2]
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 16 },

                (Node){ .tp = nodAssignment, .pl2 = 4, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 2, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl2 = 2 },

                (Node){ .tp = nodAssignment, .pl2 = 4, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 3, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 4 },
                (Node){ .tp = tokInt, .pl2 = 3 },

                (Node){ .tp = nodAssignment, .pl2 = 4, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 4, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = -1 },
                (Node){ .tp = nodId, .pl1 = 3, .pl2 = -1 },

                (Node){ .tp = nodId, .pl1 = 4, .pl2 = -1 },

                (Node){ .tp = nodAssignment, .pl2 = 7, .pl3 = 7 }, // arr[1][0] = 21
                (Node){ .tp = nodExpr,         .pl2 = 5  },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1  },
                (Node){ .tp = tokInt, .pl2 = 1           },
                (Node){ .tp = nodCall, .pl1 = opGetElem + O, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 0           },
                (Node){ .tp = nodCall, .pl1 = opGetElemPtr + O, .pl2 = 2 },
                (Node){ .tp = tokInt,          .pl2 = 21 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
       */ 
    }));
}

//}}}
//{{{ Expression tests

ParserTestSet* expressionTests(Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Expression test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple function call"),
            s("x = foo 10 2 `hw`;"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 6, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl2 = 4 },
                (Node){ .tp = tokInt, .pl2 = 10,      },
                (Node){ .tp = tokInt, .pl2 = 2,       },
                (Node){ .tp = tokString,              },
                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 3 } // foo
            })),
            ((Int[]) { 4, tokInt, tokInt, tokString, tokDouble }),
            ((TestEntityImport[]) {{ .nameInd = 0, .typeInd = 0 }}),
            ((SourceLoc[]) {
                { .startBt = 0, .lenBts = 17 },
                { .startBt = 0, .lenBts = 1 },
                { .startBt = 2, .lenBts = 15 },
                { .startBt = 8, .lenBts = 2 },
                { .startBt = 11, .lenBts = 1 },
                { .startBt = 13, .lenBts = 4 },
                { .startBt = 4, .lenBts = 3 }
            })
        ),
        createTest(
            s("Data allocation"),
            s("x = [1 2 3];"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 9, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 7 },

                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 3,
                        .pl3 = 3 },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = -1 } // the allocated array
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Data allocation type error"),
            s(errListDifferentEltTypes),
            s("x = [1 true];"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl1 = 0, .pl2 = 0 },

                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 4, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 2,
                        .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokBool, .pl2 = 1 },
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Data allocation with expression inside"),
            s("x = [4 (* 2 7)];"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 11, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 9 },

                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 7, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl1 = 0, .pl2 = 3 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTimes, tokInt), .pl2 = 2 },

                (Node){ .tp = nodId, .pl1 = 1, .pl2 = -1 } // the allocated array
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Nested data allocation with expression inside"),
            s("x = [[1] [4 (- 2 7)] [2 3]];"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 26, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 0, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl1 = 1, .pl2 = 24 },

                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 3, .pl3 = 2 }, // [1]
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 1, .pl3 = 1 },
                (Node){ .tp = tokInt, .pl2 = 1 },

                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 7, .pl3 = 2 }, // [2 (2 - 7)]
                (Node){ .tp = nodBinding, .pl1 = 2, .pl2 = -1 }, // [2 (2 - 7)]
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 4 },
                (Node){ .tp = nodExpr, .pl1 = 0, .pl2 = 3 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokInt), .pl2 = 2 },

                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 4, .pl3 = 2 }, // [2 3]
                (Node){ .tp = nodBinding, .pl1 = 3, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = tokInt, .pl2 = 2 },
                (Node){ .tp = tokInt, .pl2 = 3 },

                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 5, .pl3 = 2 }, // [2 3]
                (Node){ .tp = nodBinding, .pl1 = 4, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 3, .pl3 = 3 },
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
            s("x = foo 10 (bar) 3;"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 6, .pl3 = 2},
                (Node){ .tp = nodBinding, .pl1 = 0},
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
            s("x = foo 10 (bar 3.4);"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 6, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 0 },
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
            s("Nested function call 2"),
            s("x = foo ##($(bar));"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 6, .pl3 = 2},
                (Node){ .tp = nodBinding, .pl1 = 0},
                (Node){ .tp = nodExpr, .pl2 = 4},

                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 0 }, // bar
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokDouble), .pl2 = 1 }, // $
                (Node){ .tp = nodCall, .pl1 = oper(opSize, tokString), .pl2 = 1}, // ##
                (Node){ .tp = nodCall, .pl1 = I - 2, .pl2 = 1}, // foo
            })),
            ((Int[]) {2, tokInt, tokInt, // Int -> Int
                      1, tokDouble}), // () -> Double
            ((TestEntityImport[]) {(TestEntityImport){ .nameInd = 0, .typeInd = 0},
                               (TestEntityImport){ .nameInd = 1, .typeInd = 1}})
        ),
        createTest(
            s("Triple function call"),
            s("x = bar 2 (foo (foo `hw`)) 4;"),
            (((Node[]) {
                (Node){ .tp = nodAssignment,     .pl2 = 8, .pl3 = 2 },
                (Node){ .tp = nodBinding,        .pl1 = 0 },
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
            s("x = + 1 (/ 9 3);"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 7, .pl3 = 2 },
                (Node){ .tp = nodBinding,        .pl1 = 0 },
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
        createTest(
            s("Unary operator precedence"),
            s("x = + `12` $ ## -3;"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 7, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl2 = 0 },
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
        createTestWithError(
            s("Operator arity error"),
            s(errTypeNoMatchingOverload),
            s("x = + 1 20 100;"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl3 = 2 },
                (Node){ .tp = nodBinding },
                (Node){ .tp = nodExpr },
                (Node){ .tp = tokInt, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl2 = 20 },
                (Node){ .tp = tokInt, .pl2 = 100 },
                (Node){ .tp = nodCall, .pl1 = opPlus + O, .pl2 = 3 }
            })),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("List accessor"),
            s("arr = [true false true];\n"
              "x = arr[1];"
             ),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 9, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl2 = 0 },
                (Node){ .tp = nodExpr,  .pl1 = 1, .pl2 = 7 },
                (Node){ .tp = nodAssignment,      .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = nodBinding,  .pl1 = 1, .pl2 = -1 },
                (Node){ .tp = nodDataAlloc, .pl1 = nameOfStandard(strL), .pl2 = 3, .pl3 = 3 },
                (Node){ .tp = tokBool,            .pl2 = 1 },
                (Node){ .tp = tokBool,            .pl2 = 0 },
                (Node){ .tp = tokBool,            .pl2 = 1 },
                (Node){ .tp = nodId,    .pl1 = 1, .pl2 = -1 },

                (Node){ .tp = nodAssignment, .pl2 = 5, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 2, .pl2 = 0 },
                (Node){ .tp = nodExpr,              .pl2 = 3 },
                (Node){ .tp = nodId,      .pl1 = 0, .pl2 = 0 }, // arr
                (Node){ .tp = tokInt,               .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = opGetElem + O, .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
    }));
}

//}}}
//{{{ Function tests

ParserTestSet* functionTests(Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Functions test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple function definition 1"),
            s("def newFn = {{x Int; y L Bool } a = x;}"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,             .pl2 = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1 },  // param x
                (Node){ .tp = nodBinding, .pl1 = 2 },  // param y
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 3 },
                (Node){ .tp = nodId, .pl1 = 1,      .pl2 = 1 }   // x
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                (SourceLoc){ .startBt = 8, .lenBts = 31 },
                (SourceLoc){ .startBt = 10, .lenBts = 1 },
                (SourceLoc){ .startBt = 16, .lenBts = 1 },
                (SourceLoc){ .startBt = 32, .lenBts = 5 },
                (SourceLoc){ .startBt = 32, .lenBts = 1 },
                (SourceLoc){ .startBt = 36, .lenBts = 1 }
             })
        ),
        createTest(
            s("Simple function definition 2"),
            s("def newFn Str = {{x Str; y Double}\n"
              "    a = x;\n"
              "    return a\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 7 },
                (Node){ .tp = nodBinding, .pl1 = 1           }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2           }, // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2, .pl3 = 2  },
                (Node){ .tp = nodBinding, .pl1 = 3           }, // local a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 1  }, // x
                (Node){ .tp = nodReturn,           .pl2 = 1  },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 3  }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Simple function definition 3"),
            s("def main = {{}\n"
              "     print `asdf`;\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,      .pl2 = 3 },
                (Node){ .tp = nodExpr,       .pl2 = 2 },
                (Node){ .tp = tokString     },
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("Function definition wrong return type"),
            s(errTypeWrongReturnType),
            s("def newFn Str = {{x Double; y Double}\n"
              "    a = x;\n"
              "    return a;\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef                       },
                (Node){ .tp = nodBinding, .pl1 = 1           }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2           }, // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2, .pl3 = 2  },
                (Node){ .tp = nodBinding, .pl1 = 3,          }, // local a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 1  }, // x
                (Node){ .tp = nodReturn,                     },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 3  }  // a
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Function definition with complex return"),
            s("newFn = {{x Int y Double -> Str}\n"
              "    return $(- (foo x) y))}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 9 },
                (Node){ .tp = nodBinding, .pl1 = 1           },  // param x
                (Node){ .tp = nodBinding, .pl1 = 2           },  // param y
                (Node){ .tp = nodReturn,           .pl2 = 6  },
                (Node){ .tp = nodExpr,             .pl2 = 5  },
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 1 },   // x
                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 1 }, // foo
                (Node){ .tp = nodId, .pl1 = 2,     .pl2 = 2 },   // y
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokDouble), .pl2 = 2 },
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokDouble), .pl2 = 1 },
            }),
            ((Int[]) { 2, tokInt, tokDouble }),
            ((TestEntityImport[]) {{ .nameInd = 0, .typeInd = 0 }})
        ),
        createTest(
            s("Mutually recursive function definitions"),
            s("def func1 Int = {{x Int; y Double}\n"
              "    a = x;\n"
              "    return func2 y a;\n"
              "}\n"
              "func2 Int = {{x Double; y Int}\n"
              "    return func1 y x;\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 10  }, // func1
                (Node){ .tp = nodBinding, .pl1 = 2            },
                        // param x. First 2 bindings = types
                (Node){ .tp = nodBinding, .pl1 = 3           },  // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2, .pl3 = 2  },
                (Node){ .tp = nodBinding, .pl1 = 4           },  // local a
                (Node){ .tp = nodId,     .pl1 = 2, .pl2 = 1  },  // x
                (Node){ .tp = nodReturn,           .pl2 = 4  },
                (Node){ .tp = nodExpr,             .pl2 = 3  },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 2  }, // y
                (Node){ .tp = nodId, .pl1 = 4,     .pl2 = 3  }, // a
                (Node){ .tp = nodCall, .pl1 = 1,   .pl2 = 2  }, // func2 call

                (Node){ .tp = nodFnDef,  . pl1 = 1, .pl2 = 7, .pl3 = 4 }, // func2
                (Node){ .tp = nodBinding, .pl1 = 5            }, // param x
                (Node){ .tp = nodBinding, .pl1 = 6            }, // param y
                (Node){ .tp = nodReturn,           .pl2 = 4   },
                (Node){ .tp = nodExpr,             .pl2 = 3   },
                (Node){ .tp = nodId, .pl1 = 6,     .pl2 = 2   }, // y
                (Node){ .tp = nodId, .pl1 = 5,     .pl2 = 1   },  // x
                (Node){ .tp = nodCall, .pl1 = 0,  .pl2 = 2    } // func1 call
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Function definition with nested scope"),
            s("def main = {{x Int; y Double}\n"
              "    {\n"
              "        a = 5;\n"
              "    }\n"
              "    a = - (foo x) y;\n"
              "    print $a;\n"
              "}"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 17 },
                (Node){ .tp = nodBinding, .pl1 = 1,          }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2,          }, // param y

                (Node){ .tp = nodScope,            .pl2 = 3 },
                (Node){ .tp = nodAssignment,       .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 3,         }, // first a =
                (Node){ .tp = tokInt,              .pl2 = 5 },

                (Node){ .tp = nodAssignment,       .pl2 = 6, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 4 },
                (Node){ .tp = nodExpr,            .pl2 = 4 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 }, // x
                (Node){ .tp = nodCall, .pl1 = I - 1, .pl2 = 1 }, // foo
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = 2 }, // y
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokDouble), .pl2 = 2 },

                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 4,  .pl2 = 3 }, // a
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokDouble),  .pl2 = 1 }, // $
                (Node){ .tp = nodCall, .pl1 = I - 5, .pl2 = 1 } // print Double
            }),
            ((Int[]) { 2, tokInt, tokDouble }), // Int -> Double
            ((TestEntityImport[]) {(TestEntityImport){ .nameInd = 0, .typeInd = 0 }})
        )
    }));
}

//}}}
//{{{ If tests

ParserTestSet* ifTests(Compiler* protoOvs, Arena* a) {
    return createTestSet(s("If test set"), a, ((ParserTest[]){
        createTestWithLocs(
            s("Simple if"),
            s("f = {{}\n"
              "    {if == 5 5: print `5` }\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl2 = 9 },

                (Node){ .tp = nodIf, .pl2 = 8 },
                (Node){ .tp = nodExpr,     .pl2 = 3 },
                (Node){ .tp = tokInt,      .pl2 = 5 },
                (Node){ .tp = tokInt,      .pl2 = 5},
                (Node){ .tp = nodCall, .pl1 = oper(opEquality, tokInt), .pl2 = 2 }, // ==

                (Node){ .tp = nodScope,     .pl2 = 3 },
                (Node){ .tp = nodExpr,     .pl2 = 2 },
                (Node){ .tp = tokString },
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 } // print
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {}),
            ((SourceLoc[]) {
                (SourceLoc){ .startBt = 4, .lenBts = 33 },

                (SourceLoc){ .startBt = 11, .lenBts = 23 },
                (SourceLoc){ .startBt = 15, .lenBts = 7 },
                (SourceLoc){ .startBt = 18, .lenBts = 1 },
                (SourceLoc){ .startBt = 20, .lenBts = 1 },
                (SourceLoc){ .startBt = 15, .lenBts = 2 },

                (SourceLoc){ .startBt = 23, .lenBts = 9 },
                (SourceLoc){ .startBt = 23, .lenBts = 9 },
                (SourceLoc){ .startBt = 29, .lenBts = 3 },
                (SourceLoc){ .startBt = 23, .lenBts = 5 }

             })
        ),
        createTest(
            s("If with else"),
            s("f = {{-> Str}\n"
              "    {if > 5 3: `5` else `=)` }\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .pl2 = 10 },

                (Node){ .tp = nodIf, .pl2 = 9, .pl3 = 7 },

                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = tokInt, .pl2 = 5 },
                (Node){ .tp = tokInt, .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opGreaterTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,  .pl2 = 1 },
                (Node){ .tp = tokString },

                (Node){ .tp = nodElseIf,  .pl2 = 2 },
                (Node){ .tp = nodScope,  .pl2 = 1 },
                (Node){ .tp = tokString },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("If with elseif"),
            s("f = {{}\n"
              "    (if > 5 3: 11\n"
              "    eif == 5 3: 4)\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,     .pl2 = 14 },

                (Node){ .tp = nodIf, .pl2 = 13, .pl3 = 7 },

                (Node){ .tp = nodExpr,      .pl2 = 3 },
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opGreaterTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodScope,  .pl2 = 1       },
                (Node){ .tp = tokInt,       .pl2 = 11 },

                (Node){ .tp = nodElseIf, .pl2 = 6, .pl3 = 0 },
                (Node){ .tp = nodExpr,      .pl2 = 3 },
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opEquality, tokInt), .pl2 = 2 },
                (Node){ .tp = nodScope,  .pl2 = 1 },
                (Node){ .tp = tokInt,       .pl2 = 4 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("If with elseif and else"),
            s("f = ((\n"
              "    (if > 5 3: print `11` \n"
              "    eif == 5 3: print `4` \n"
              "    else print `100` )\n"
              "))"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,     .pl2 = 23 },

                (Node){ .tp = nodIf, .pl2 = 22, .pl3 = 8 },

                (Node){ .tp = nodExpr,      .pl2 = 3},
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opGreaterTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodScope,  .pl2 = 3       },
                (Node){ .tp = nodExpr,   .pl2 = 2       },
                (Node){ .tp = tokString,   },
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 },

                (Node){ .tp = nodElseIf,      .pl2 = 8 },
                (Node){ .tp = nodExpr,      .pl2 = 3},
                (Node){ .tp = tokInt,       .pl2 = 5 },
                (Node){ .tp = tokInt,       .pl2 = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opEquality, tokInt), .pl2 = 2 },
                (Node){ .tp = nodScope,  .pl2 = 3 },
                (Node){ .tp = nodExpr,  .pl2 = 2       },
                (Node){ .tp = tokString,  },
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 },

                (Node){ .tp = nodElseIf,  .pl2 = 4 },
                (Node){ .tp = nodScope,  .pl2 = 3 },
                (Node){ .tp = nodExpr,  .pl2 = 2       },
                (Node){ .tp = tokString },
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("If error: must be bool"),
            s(errTypeMustBeBool),
            s("f = ((\n"
              "    (if + 5 5: print `5` )\n"
              "))"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef },

                (Node){ .tp = nodIf },
                (Node){ .tp = nodExpr },
                (Node){ .tp = tokInt, .pl2 = 5 },
                (Node){ .tp = tokInt, .pl2 = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
    }));
}

//}}}
//{{{ Loop tests

ParserTestSet* loopTests(Compiler* protoOvs, Arena* a) {
    return createTestSet(s("Loops test set"), a, ((ParserTest[]){
        createTest(
            s("Simple loop"),
            s("def f = {{} for {x~ = 1; < x 101; x = + x 1;} {print $x;} }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 20, .pl3 = 0 },
                (Node){ .tp = nodFor,           .pl2 = 19, .pl3 = 9 },

                (Node){ .tp = nodScope, .pl2 = 18 },
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 2, .pl3 = 2 }, // x~ = 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = tokInt,          .pl2 = 1 },
                (Node){ .tp = nodExpr, .pl2 = 3 }, // < x 101
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1, },
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 10 },
                (Node){ .tp = nodExpr,           .pl2 = 3 }, // print $x
                (Node){ .tp = nodId,   .pl1 = 1, .pl2 = 1 },      // x
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 }, // print
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 5, .pl3 = 2 }, // x = x + 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl1 = 0, .pl2 = 1 },
                (Node){ .tp = nodCall,   .pl1 = oper(opPlus, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For with two complex initializers"),
            s("def f = {{}\n"
              "    for {x~ = 17; y~ = / x 5; < y 101; x = - x 1; y = + y 1;}{\n"
              "        print $x;}\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 32 },
                (Node){ .tp = nodFor,             .pl2 = 31, .pl3 = 15 },
                (Node){ .tp = nodScope,           .pl2 = 30 }, // function body

                (Node){ .tp = nodAssignment,            .pl2 = 2, .pl3 = 2 }, // x~ = 17
                (Node){ .tp = nodBinding, .pl1 = 1,              },
                (Node){ .tp = tokInt,                   .pl2 = 17 },

                (Node){ .tp = nodAssignment, .pl2 = 5, .pl3 = 2 }, // y~ = / x 5
                (Node){ .tp = nodBinding, .pl1 = 2,             },  // def y
                (Node){ .tp = nodExpr,                  .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1,          .pl2 = 1 }, // x
                (Node){ .tp = tokInt,                   .pl2 = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opDivBy, tokInt), .pl2 = 2 },

                (Node){ .tp = nodExpr, .pl2 = 3,                  }, // < y 101
                (Node){ .tp = nodId, .pl1 = 2,          .pl2 = 2 },
                (Node){ .tp = tokInt,                   .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,                 .pl2 = 16 },
                (Node){ .tp = nodExpr,                  .pl2 = 3 }, // print $x
                (Node){ .tp = nodId,    .pl1 = 1,       .pl2 = 1 }, // x
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodCall, .pl1 = I - 4,        .pl2 = 1 }, // print

                (Node){ .tp = nodAssignment,            .pl2 = 5, .pl3 = 2}, // x = - x 1
                (Node){ .tp = nodBinding,  .pl1 = 1,             },
                (Node){ .tp = nodExpr,                  .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1,      .pl2 = 1 }, // x
                (Node){ .tp = tokInt,               .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opMinus, tokInt), .pl2 = 2 },

                (Node){ .tp = nodAssignment,              .pl2 = 5, .pl3 = 2}, // y = + y 1
                (Node){ .tp = nodBinding,  .pl1 = 2,               },
                (Node){ .tp = nodExpr,                  .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 2,      .pl2 = 2 },    // y
                (Node){ .tp = tokInt,               .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opPlus, tokInt), .pl2 = 2 },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For without initializers"),
            s("def f = {{}\n"
              "    x = 4;\n"
              "    for {< x 101}{ \n"
              "        print $x; } }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,         .pl2 = 13 },

                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // x = 4
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = tokInt,              .pl2 = 4 },

                (Node){ .tp = nodFor,            .pl2 = 9, .pl3 = 5 },

                (Node){ .tp = nodExpr, .pl2 = 3,           }, // < x 101
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope, .pl2 = 4,          }, // print $x
                (Node){ .tp = nodExpr,         .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),

        createTest(
            s("For loop without body"),
            s("def f = {{} for {x~ = 1; < x 101; x = + x 1} {} }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 16, .pl3 = 0 },
                (Node){ .tp = nodFor,           .pl2 = 15, .pl3 = 9 },

                (Node){ .tp = nodScope, .pl2 = 14 },
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 2, .pl3 = 2 }, // x~ = 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = tokInt,          .pl2 = 1 },

                (Node){ .tp = nodExpr, .pl2 = 3 }, // < x 101
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1, },
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 6 },
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 5, .pl3 = 2 }, // x = x + 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl1 = 0, .pl2 = 1 },
                (Node){ .tp = nodCall,   .pl1 = oper(opPlus, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For loop with no step"),
            s("def f = {{} for {x~ = 1; < x 101} { print $x; } }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 14, .pl3 = 0 },
                (Node){ .tp = nodFor,           .pl2 = 13, .pl3 = 9 },

                (Node){ .tp = nodScope, .pl2 = 12 },
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 2, .pl3 = 2 }, // x~ = 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = tokInt,          .pl2 = 1 },
                (Node){ .tp = nodExpr, .pl2 = 3 }, // < x 101
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1, },
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 4 },
                (Node){ .tp = nodExpr,           .pl2 = 3 }, // print $x
                (Node){ .tp = nodId,   .pl1 = 1, .pl2 = 1 },      // x
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 }, // print
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For with no initializers nor step"),
            s("def f = {{} x = 0;\n"
              " for {< x 101}{ print $x; } }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 13, .pl3 = 0 },

                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // x = 0
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = tokInt,              .pl2 = 0 },

                (Node){ .tp = nodFor,           .pl2 = 9, .pl3 = 5 },
                (Node){ .tp = nodExpr, .pl2 = 3 }, // < x 101
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1, },
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 4 },
                (Node){ .tp = nodExpr,           .pl2 = 3 }, // print $x
                (Node){ .tp = nodId,   .pl1 = 1, .pl2 = 1 },      // x
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 }, // $
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 }, // print
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),

        createTest(
            s("For loop with no initalizers nor body"),
            s("def f = {{} x~ = 7;\n"
              " for {< x 101; x = + x 1}{} }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 15, .pl3 = 0 },
                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // x = 0
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = tokInt,              .pl2 = 7 },

                (Node){ .tp = nodFor,           .pl2 = 11, .pl3 = 5 },

                (Node){ .tp = nodExpr, .pl2 = 3 }, // < x 101
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1, },
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 6 },
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 5, .pl3 = 2 }, // x = + x 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = tokInt, .pl1 = 0, .pl2 = 1 },
                (Node){ .tp = nodCall,   .pl1 = oper(opPlus, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For loop with single-token condition"),
            s("def f = {{} x~ = true;\n"
              " for {x} {x = !x;} }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 11, .pl3 = 0 },
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 2, .pl3 = 2 }, // x~ = 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = tokBool,          .pl2 = 1 },

                (Node){ .tp = nodFor,           .pl2 = 7, .pl3 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },

                (Node){ .tp = nodScope,          .pl2 = 5 },
                (Node){ .tp = nodAssignment, .pl1 = 0, .pl2 = 4, .pl3 = 2 }, // x = x + 1
                (Node){ .tp = nodBinding, .pl1 = 1, .pl2 = 0 },
                (Node){ .tp = nodExpr, .pl2 = 2 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },
                (Node){ .tp = nodCall,   .pl1 = oper(opBoolNeg, tokBool), .pl2 = 1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),

        createTestWithError(
            s("For loop error: neither step nor body"),
            s(errLoopEmptyStepBody),
            s("def f = {{ for {x~ = 1; < x 101} {} }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 0, .pl3 = 0 },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("For loop error: no condition"),
            s(errLoopNoCondition),
            s("f = (( (for x~ = 1; x = + x 1: print $x) ))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 0, .pl3 = 0 },
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("For with break and continue"),
            s("def f = {{}\n"
              "    for {x = 0; < x 301} {\n"
              "        break;\n"
              "        continue;}\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,             .pl2 = 12 },
                (Node){ .tp = nodFor,           .pl2 = 11, .pl3 = 9 },

                (Node){ .tp = nodScope, .pl2 = 10 },

                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // x = 0
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 }, // x
                (Node){ .tp = tokInt,        .pl2 = 301 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope, .pl2 = 2 },
                (Node){ .tp = nodBreakCont, .pl1 = 1 },
                (Node){ .tp = nodBreakCont, .pl1 = BIG + 1 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("For with break error"),
            s(errBreakContinueInvalidDepth),
            s("def f = {{}\n"
              "    for {x = 0; < x 101}{\n"
              "        break 2;\n"
              "} }"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef                },
                (Node){ .tp = nodFor, .pl3 = 9       },

                (Node){ .tp = nodScope     },
                (Node){ .tp = nodAssignment,      .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1 }, // x
                (Node){ .tp = tokInt,             .pl2 = 0 },

                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },
                (Node){ .tp = nodScope }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTest(
            s("Nested for with deep break and continue"),
            s("def f = {{}\n"
              "    for {a = 0; < a 101}{\n"
              "        for {b = 0; < b 201}{\n"
              "            for {c = 0; < c 301}{\n"
              "                break 3;}\n"
              "        }\n"
              "        for {d = 0; < d 51}{\n"
              "            for {e = 0; < e 401}{\n"
              "                continue 2;}\n"
              "        }\n"
              "        print $a;\n"
              "    }\n"
              "}"
              ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 56 },

                (Node){ .tp = nodFor,  .pl1 = 1, .pl2 = 55, .pl3 = 9 }, // for #1. It's being
                                                                        // "broken" from
                (Node){ .tp = nodScope, .pl2 = 54 },

                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2}, // a = 0
                (Node){ .tp = nodBinding, .pl1 = 1 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodExpr, .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 }, // a
                (Node){ .tp = tokInt, .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 46 },
                (Node){ .tp = nodFor,          .pl2 = 20, .pl3 = 9 }, // for #2
                (Node){ .tp = nodScope,          .pl2 = 19 },

                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // b = 0
                (Node){ .tp = nodBinding, .pl1 = 2 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodExpr,                     .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = 2 }, // b
                (Node){ .tp = tokInt, .pl2 = 201 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,          .pl2 = 11 }, // for #3, double-nested
                (Node){ .tp = nodFor,          .pl2 = 10, .pl3 = 9 }, // for #3, double-nested
                (Node){ .tp = nodScope,          .pl2 = 9 },

                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // c =
                (Node){ .tp = nodBinding, .pl1 = 3 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodExpr,         .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 3, .pl2 = 3 }, // c
                (Node){ .tp = tokInt,          .pl2 = 301 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,         .pl2 = 1 },
                (Node){ .tp = nodBreakCont, .pl1 = 1 },

                (Node){ .tp = nodFor,  .pl1 = 4, .pl2 = 20, .pl3 = 9 }, // for #4. It's "continued"
                (Node){ .tp = nodScope,            .pl2 = 19 },

                (Node){ .tp = nodAssignment,    .pl2 = 2, .pl3 = 2 }, // d = 0
                (Node){ .tp = nodBinding, .pl1 = 4 },
                (Node){ .tp = tokInt,           .pl2 = 0 },

                (Node){ .tp = nodExpr,          .pl2 = 3 }, // < d 51
                (Node){ .tp = nodId, .pl1 = 4, .pl2 = 4 },
                (Node){ .tp = tokInt, .pl2 = 51 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,       .pl2 = 11 },
                (Node){ .tp = nodFor,         .pl2 = 10, .pl3 = 9 }, // for #5, the last one
                (Node){ .tp = nodScope,       .pl2 = 9 },

                (Node){ .tp = nodAssignment, .pl2 = 2, .pl3 = 2 }, // e = 0
                (Node){ .tp = nodBinding, .pl1 = 5 },
                (Node){ .tp = tokInt, .pl2 = 0 },

                (Node){ .tp = nodExpr,         .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 5, .pl2 = 5 }, // < e 401
                (Node){ .tp = tokInt,          .pl2 = 401 },
                (Node){ .tp = nodCall, .pl1 = oper(opLessTh, tokInt), .pl2 = 2 },

                (Node){ .tp = nodScope,       .pl2 = 1 },
                (Node){ .tp = nodBreakCont, .pl1 = 4 + BIG },

                (Node){ .tp = nodExpr, .pl2 = 3 }, // print a
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 },  // a
                (Node){ .tp = nodCall, .pl1 = oper(opToString, tokInt), .pl2 = 1 },
                (Node){ .tp = nodCall, .pl1 = I - 4, .pl2 = 1 } // print
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        ),
        createTestWithError(
            s("For with type error"),
            s(errTypeMustBeBool),
            s("def f = {{} for {x~ = 1; / x 101}{ print x; } }"),
            ((Node[]) {
                (Node){ .tp = nodFnDef },
                (Node){ .tp = nodFor },

                (Node){ .tp = nodScope },

                (Node){ .tp = nodAssignment,      .pl2 = 2, .pl3 = 2 },
                (Node){ .tp = nodBinding, .pl1 = 1  }, // x
                (Node){ .tp = tokInt,             .pl2 = 1 },

                (Node){ .tp = nodExpr,            .pl2 = 3 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101 },
                (Node){ .tp = nodCall, .pl1 = oper(opDivBy, tokInt), .pl2 = 2 }
            }),
            ((Int[]) {}),
            ((TestEntityImport[]) {})
        )
    }));
}

//}}}


void runATestSet(ParserTestSet* (*testGenerator)(Compiler*, Arena*),
                 TestContext* ct,
                 Compiler* protoOvs) {
    ParserTestSet* testSet = (testGenerator)(protoOvs, ct->a);
    for (Int j = 0; j < testSet->totalTests; j++) {
        ParserTest test = testSet->tests[j];
        runTest(test, ct);
    }
}


int main() {
    printf("----------------------------\n");
    printf("--  PARSER TEST  --\n");
    printf("----------------------------\n");
    auto ct = (TestContext){.countTests = 0, .countPassed = 0, .a = createArena() };

    // An empty compiler that we need for the built-in overloads
    Compiler* protoOvs = createLexer(empty, ct.a);
    initializeParser(protoOvs, ct.a);
    createOverloads(protoOvs);

    runATestSet(&assignmentTests, &ct, protoOvs);
//~    runATestSet(&expressionTests, &ct, protoOvs);
//~    runATestSet(&functionTests, &ct, protoOvs);
//~    runATestSet(&ifTests, &ct, protoOvs);
//~    runATestSet(&loopTests, &ct, protoOvs);

    if (ct.countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (ct.countPassed == ct.countTests) {
        if (ct.countTests > 1) {
            printf("\nPassed all %d tests!\n", ct.countTests);
        } else {
            printf("\nThe test was passed.\n");
        }

    } else {
        printf("\nFailed %d tests out of %d!\n", (ct.countTests - ct.countPassed), ct.countTests);
    }

    deleteArena(ct.a);
}
