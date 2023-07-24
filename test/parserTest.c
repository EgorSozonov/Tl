#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../source/tl.internal.h"
#include "tlTest.h"

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

#define S   70000000 // A constant larger than the largest allowed file size. Separates parsed entities from others
#define I  140000000 // The base index for imported entities/overloads
#define S2 210000000 // A constant larger than the largest allowed file size. Separates parsed entities from others
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

/** Try and convert test value to operator entityId (not all operators are supported, only the ones used in tests) */
private Int tryGetOper0(Int opType, Int typeId, Compiler* proto) {
    OpDef opDef = (*proto->langDef->operators)[0];
    Int entitySentinel = opDef.builtinOverloads;
    Int operId = 0;
    for (Int j = 0; j < proto->countOperatorEntities; j++) {
        if (j == entitySentinel) {
            ++operId;
            entitySentinel += (*proto->langDef->operators)[operId].builtinOverloads;
        }
        if (operId != opType) {
            continue;
        }
        Entity ent = proto->entities.content[j];
        Int firstArgType = proto->types.content[ent.typeId + 2];
        if (firstArgType == typeId) {
            return j + O;
        }
    }
    return -1;
}

#define oper(opType, typeId) tryGetOper0(opType, typeId, proto)

private Int transformBindingEntityId(Int inp, Compiler* pr) {
    if (inp < S) { // parsed stuff
        return inp + pr->countNonparsedEntities;
    } else if (inp < S2) {
        return inp - I + pr->countOperatorEntities;
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
        Int initTypeId = cm->types.length;

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

/** Creates a test with two parsers: one is the init parser (contains all the "imported" bindings and
 *  pre-defined nodes), and the other is the output parser (with all the stuff parsed from source code).
 *  When the test is run, the init parser will parse the tokens and then will be compared to the expected output parser.
 *  Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
 *  it will be inserted as 1 + (the number of built-in bindings) etc
 */
private ParserTest createTest0(String* name, String* sourceCode, Arr(Node) nodes, Int countNodes, 
                               Arr(Int) types, Int countTypes, Arr(EntityImport) imports, Int countImports,
                               Compiler* proto, Arena* a) {
    Compiler* test = lexicallyAnalyze(sourceCode, proto, a);
    Compiler* control = lexicallyAnalyze(sourceCode, proto, a);
    initializeParser(control, proto, a);
    initializeParser(test, proto, a);
    Arr(Int) typeIds = importTypes(types, countTypes, control);
    importTypes(types, countTypes, test);
    importEntities(imports, countImports, typeIds, control);
    importEntities(imports, countImports, typeIds, test);
    // The control compiler
    for (Int i = 0; i < countNodes; i++) {
        untt nodeType = nodes[i].tp;
        // All the node types which contain bindingIds
        if (nodeType == nodId || nodeType == nodCall || nodeType == nodBinding || nodeType == nodBinding) {
            pushInnodes((Node){ .tp = nodeType, .pl1 = transformBindingEntityId(nodes[i].pl1, control),
                                .pl2 = nodes[i].pl2, .startBt = nodes[i].startBt, .lenBts = nodes[i].lenBts }, 
                    control);
        } else {
            pushInnodes(nodes[i], control);
        }
    }
    return (ParserTest){ .name = name, .test = test, .control = control };
}

#define createTest(name, input, nodes, types, entities) createTest0((name), (input), \
    (nodes), sizeof(nodes)/sizeof(Node), types, sizeof(types)/4, entities, sizeof(entities)/sizeof(EntityImport), \
    proto, a)

/** Creates a test with two parsers: one is the test parser (contains just the "imported" bindings)
 *  and one is the control parser (with the bindings and the expected nodes). When the test is run,
 *  the test parser will parse the tokens and then will be compared to the control parser.
 *  Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
 *  it will be inserted as 1 + (the number of built-in bindings)
 */
private ParserTest createTestWithError0(String* name, String* message, String* input, Arr(Node) nodes, Int countNodes, 
                               Arr(Int) types, Int countTypes, Arr(EntityImport) entities, Int countEntities,
                               Compiler* proto, Arena* a) {
    ParserTest theTest = createTest0(name, input, nodes, countNodes, types, countTypes, entities, countEntities, proto, a);
    theTest.control->wasError = true;
    theTest.control->errMsg = message;
    return theTest;
}

#define createTestWithError(name, errorMessage, input, nodes, types, entities) createTestWithError0((name), \
    errorMessage, (input), (nodes), sizeof(nodes)/sizeof(Node), types, sizeof(types)/4, \
    entities, sizeof(entities)/sizeof(EntityImport), proto, a)    


/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
int equalityParser(Compiler a, Compiler b) {
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.nodes.length < b.nodes.length ? a.nodes.length : b.nodes.length;
    int i = 0;
    for (; i < commonLength; i++) {
        Node nodA = a.nodes.content[i];
        Node nodB = b.nodes.content[i];
        if (nodA.tp != nodB.tp || nodA.lenBts != nodB.lenBts || nodA.startBt != nodB.startBt 
            || nodA.pl1 != nodB.pl1 || nodA.pl2 != nodB.pl2) {
            printf("\n\nUNEQUAL RESULTS on %d\n", i);
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
    return (a.nodes.length == b.nodes.length) ? -2 : i;        
}

/** Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code */
void runParserTest(ParserTest test, int* countPassed, int* countTests, Arena *a) {
    (*countTests)++;
    if (test.test->totalTokens == 0) {
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


ParserTestSet* assignmentTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Assignment test set"), a, ((ParserTest[]){
        createTest(
            s("Simple assignment"), 
            s("x = 12"),            
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                    (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = tokInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Double assignment"), 
            s("x = 12\n"
              "second = x"  
            ),
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                    (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 },     // x
                    (Node){ .tp = tokInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 },
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 7, .lenBts = 10 },
                    (Node){ .tp = nodBinding, .pl1 = 1, .startBt = 7, .lenBts = 6 }, // second
                    (Node){ .tp = nodId, .pl1 = 0, .pl2 = 0, .startBt = 16, .lenBts = 1 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTestWithError(
            s("Assignment shadowing error"), 
            s(errAssignmentShadowing),
            s("x = 12\n"
              "x = 7"  
            ),
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 0, .lenBts = 6 },
                    (Node){ .tp = nodBinding, .pl2 = 0, .startBt = 0, .lenBts = 1 },
                    (Node){ .tp = tokInt,  .pl2 = 12, .startBt = 4, .lenBts = 2 },
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTestWithError(
            s("Assignment type declaration error"), 
            s(errTypeMismatch),
            s("x String = 12"),
            ((Node[]) {
                    (Node){ .tp = nodAssignment,           .startBt = 0,  .lenBts = 13 },
                    (Node){ .tp = nodBinding,              .startBt = 0,  .lenBts = 1 },      
                    (Node){ .tp = tokInt,       .pl2 = 12, .startBt = 11, .lenBts = 2 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Reassignment"), 
            s("(:f main() = \n"
                   "x = `foo`\n"
                   "x := `bar`)"
            ),
            ((Node[]) {
                    (Node){ .tp = nodFnDef,           .pl2 = 8, .startBt = 0, .lenBts = 35 },
                    (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 4 },
                    (Node){ .tp = nodScope,           .pl2 = 6, .startBt = 8, .lenBts = 27 },
                    (Node){ .tp = nodAssignment,      .pl2 = 2, .startBt = 14, .lenBts = 9 },
                    (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 14, .lenBts = 1 },     // x
                    (Node){ .tp = tokString,                    .startBt = 18, .lenBts = 5 },
                    (Node){ .tp = nodReassign, .pl2 = 2,        .startBt = 24, .lenBts = 10 },
                    (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 24, .lenBts = 1 }, // second
                    (Node){ .tp = tokString,                    .startBt = 29, .lenBts = 5 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Mutation simple"), 
            s("(:f main() = \n"
                   "x = 1\n"
                   "x += 3)"
            ),
            ((Node[]) {
                    (Node){ .tp = nodFnDef,           .pl2 = 11, .startBt = 0, .lenBts = 27 },
                    (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 4 },
                    (Node){ .tp = nodScope,           .pl2 = 9, .startBt = 8, .lenBts = 19 },
                    (Node){ .tp = nodAssignment,      .pl2 = 2, .startBt = 14, .lenBts = 5 },
                    (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 14, .lenBts = 1 },     // x
                    (Node){ .tp = tokInt,             .pl2 = 1, .startBt = 18, .lenBts = 1 },
                    (Node){ .tp = nodReassign,        .pl2 = 5, .startBt = 20, .lenBts = 6 },
                    (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 20, .lenBts = 1 },
                    (Node){ .tp = nodExpr,            .pl2 = 3, .startBt = 20, .lenBts = 6 },
                    (Node){ .tp = nodCall, .pl1 = oper(opTPlus, tokInt), .pl2 = 2,  .startBt = 21, .lenBts = 4 },
                    (Node){ .tp = nodId,      .pl1 = 1, .pl2 = 1, .startBt = 20, .lenBts = 1 },
                    (Node){ .tp = tokInt,             .pl2 = 3, .startBt = 25, .lenBts = 1 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Mutation complex"), 
            s("(:f main() = \n"
                   "x = 2\n"
                   "x ^= (- 15 8))"
            ),
            ((Node[]) {
                    (Node){ .tp = nodFnDef,           .pl2 = 13, .startBt = 0, .lenBts = 34 },
                    (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 4 },
                    (Node){ .tp = nodScope,           .pl2 = 11, .startBt = 8, .lenBts = 26 },
                    (Node){ .tp = nodAssignment,      .pl2 = 2, .startBt = 14, .lenBts = 5 },
                    (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 14, .lenBts = 1 },     // x
                    (Node){ .tp = tokInt,             .pl2 = 2, .startBt = 18, .lenBts = 1 },
                    (Node){ .tp = nodReassign,        .pl2 = 7, .startBt = 20, .lenBts = 13 },
                    (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 20, .lenBts = 1 },
                    (Node){ .tp = nodExpr,            .pl2 = 5, .startBt = 20, .lenBts = 13 },
                    
                    (Node){ .tp = nodCall, .pl1 = oper(opTExponent, tokInt), .pl2 = 2,  .startBt = 21, .lenBts = 4 },
                    (Node){ .tp = nodId,      .pl1 = 1, .pl2 = 1, .startBt = 20, .lenBts = 1 },
                    
                    (Node){ .tp = nodCall, .pl1 = oper(opTMinus, tokInt), .pl2 = 2, .startBt = 26, .lenBts = 1 },
                    (Node){ .tp = tokInt,             .pl2 = 15, .startBt = 28, .lenBts = 2 },
                    (Node){ .tp = tokInt,             .pl2 = 8, .startBt = 31, .lenBts = 1 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        )
    }));
}


ParserTestSet* expressionTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Expression test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function call"), 
            s("x = foo 10 2 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 6, .startBt = 0, .lenBts = 14 },
                    // " + 1" because the first binding is taken up by the "imported" function, "foo"
                    (Node){ .tp = nodBinding, .pl1 = 0,        .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodExpr,  .pl2 = 4,          .startBt = 4, .lenBts = 10 },
                    (Node){ .tp = nodCall, .pl1 = I, .pl2 = 3, .startBt = 4, .lenBts = 3 }, // foo
                    (Node){ .tp = tokInt, .pl2 = 10,           .startBt = 8, .lenBts = 2 },
                    (Node){ .tp = tokInt, .pl2 = 2,            .startBt = 11, .lenBts = 1 },
                    (Node){ .tp = tokInt, .pl2 = 3,            .startBt = 13, .lenBts = 1 }                    
            })),
            ((Int[]) {4, tokFloat, tokInt, tokInt, tokInt}),
            ((EntityImport[]) {(EntityImport){ .name = s("foo"), .typeInd = 0}})
        ),
        createTest(
            s("Nested function call 1"), 
            s("x = foo 10 (bar) 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .pl2 = 6, .startBt = 0, .lenBts = 18 },                    
                    (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodExpr,  .pl2 = 4, .startBt = 4, .lenBts = 14 },
                    (Node){ .tp = nodCall, .pl1 = I, .pl2 = 3, .startBt = 4, .lenBts = 3 }, // foo
                    (Node){ .tp = tokInt, .pl2 = 10,  .startBt = 8, .lenBts = 2 },                    
                    (Node){ .tp = nodCall, .pl1 = I + 1, .pl2 = 0, .startBt = 12, .lenBts = 3 }, // bar
                    (Node){ .tp = tokInt, .pl2 = 3,   .startBt = 17, .lenBts = 1 }               
            })),
            ((Int[]) {4, tokFloat, tokInt, tokFloat, tokInt, 1, tokFloat}),
            ((EntityImport[]) {(EntityImport){ .name = s("foo"), .typeInd = 0},
                               (EntityImport){ .name = s("bar"), .typeInd = 1}})
        ),
        createTest(
            s("Nested function call 2"), 
            s("x =  foo 10 (barr 3.4)"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment,     .pl2 = 6,  .startBt = 0, .lenBts = 22 },                    
                    (Node){ .tp = nodBinding, .pl1 = 0,         .startBt = 0, .lenBts = 1 }, // x
                    (Node){ .tp = nodExpr,           .pl2 = 4,  .startBt = 5, .lenBts = 17 },
                    (Node){ .tp = nodCall, .pl1 = I, .pl2 = 2,  .startBt = 5, .lenBts = 3 },   // foo
                    (Node){ .tp = tokInt,            .pl2 = 10, .startBt = 9, .lenBts = 2 },
                                       
                    (Node){ .tp = nodCall, .pl1 = I + 1, .pl2 = 1, .startBt = 13, .lenBts = 4 },  // barr
                    (Node){ .tp = tokFloat, .pl1 = longOfDoubleBits(3.4) >> 32,
                                            .pl2 = longOfDoubleBits(3.4) & LOWER32BITS, .startBt = 18, .lenBts = 3 }
            })),
            ((Int[]) {3, tokFloat, tokInt, tokBool, 2, tokBool, tokFloat}),
            ((EntityImport[]) {(EntityImport){ .name = s("foo"), .typeInd = 0},
                               (EntityImport){ .name = s("barr"), .typeInd = 1}})
        ),
        createTest(
            s("Triple function call"), 
            s("x = buzz 2 (foo : inner 7 `hw`) 4"),
            (((Node[]) {
                (Node){ .tp = nodAssignment,     .pl2 = 9, .startBt = 0, .lenBts = 33 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodExpr,           .pl2 = 7, .startBt = 4, .lenBts = 29 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 3, .startBt = 4, .lenBts = 4 }, // buzz
                (Node){ .tp = tokInt,            .pl2 = 2, .startBt = 9, .lenBts = 1 },

                (Node){ .tp = nodCall, .pl1 = I + 1, .pl2 = 1, .startBt = 12, .lenBts = 3 }, // foo
                (Node){ .tp = nodCall, .pl1 = I + 2, .pl2 = 2, .startBt = 18, .lenBts = 5 }, // inner
                (Node){ .tp = tokInt,                .pl2 = 7, .startBt = 24, .lenBts = 1 },                
                (Node){ .tp = tokString,                       .startBt = 26, .lenBts = 4 },                
                (Node){ .tp = tokInt,                .pl2 = 4, .startBt = 32, .lenBts = 1 },
            })),
            ((Int[]) {4, tokString, tokInt, tokBool, tokInt, // buzz (String <- Int Bool Int)
                      2, tokBool, tokFloat, // foo (Bool <- Float)                      
                      3, tokFloat, tokInt, tokString}), // inner (Float <- Int String)
            ((EntityImport[]) {(EntityImport){ .name = s("buzz"), .typeInd = 0},
                               (EntityImport){ .name = s("foo"), .typeInd = 1},                               
                               (EntityImport){ .name = s("inner"), .typeInd = 2}})
        ),
        createTest(
            s("Operators simple"), 
            s("x = + 1 : / 9 3"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 7, .startBt = 0, .lenBts = 15 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodExpr,  .pl2 = 5, .startBt = 4, .lenBts = 11 },
                (Node){ .tp = nodCall, .pl1 = oper(opTPlus, tokInt), .pl2 = 2, .startBt = 4, .lenBts = 1 },   // +
                (Node){ .tp = tokInt, .pl2 = 1, .startBt = 6, .lenBts = 1 },
                
                (Node){ .tp = nodCall, .pl1 = oper(opTDivBy, tokInt), .pl2 = 2, .startBt = 10, .lenBts = 1 }, // *
                (Node){ .tp = tokInt, .pl2 = 9, .startBt = 12, .lenBts = 1 },   
                (Node){ .tp = tokInt, .pl2 = 3, .startBt = 14, .lenBts = 1 }
            })),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTestWithError(
            s("Operator arity error"),
            s(errOperatorWrongArity),
            s("x = + 1 20 100"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .startBt = 0, .lenBts = 14 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                (Node){ .tp = nodExpr,  .startBt = 4, .lenBts = 10 }
            })),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest( 
            s("Unary operator precedence"),
            s("x = + 123 #$(-3)"),

            ((Node[])  {
                (Node){ .tp = nodAssignment, .pl2 = 7, .startBt = 0, .lenBts = 16 },
                (Node){ .tp = nodBinding, .pl1 = 00, .startBt = 0, .lenBts = 1 },
                (Node){ .tp = nodExpr,              .pl2 = 5, .startBt = 4, .lenBts = 12 },
                (Node){ .tp = nodCall, .pl1 = oper(opTPlus, tokInt), .pl2 = 2, .startBt = 4, .lenBts = 1 },
                (Node){ .tp = tokInt,               .pl2 = 123, .startBt = 6, .lenBts = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opTSize, tokString), .pl2 = 1, .startBt = 10, .lenBts = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opTToString, tokInt), .pl2 = 1, .startBt = 0, .startBt = 11, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl1 = -1,    .pl2 = -3, .startBt = 13, .lenBts = 2 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        
        // //~ createTest(
        //     //~ s("Unary operator as first-class value"), 
        //     //~ s("x = map (--) coll"),
        //     //~ (((Node[]) {
        //             //~ (Node){ .tp = nodAssignment, .pl2 = 5,        .startBt = 0, .lenBts = 17 },
        //             //~ (Node){ .tp = nodBinding, .pl1 = 0,           .startBt = 0, .lenBts = 1 }, // x
        //             //~ (Node){ .tp = nodExpr,  .pl2 = 3,             .startBt = 4, .lenBts = 13 },
        //             //~ (Node){ .tp = nodCall, .pl1 = -2 + I, .pl2 = 2,    .startBt = 4, .lenBts = 3 }, // map
        //             //~ (Node){ .tp = nodId, .pl1 = oper(opTDecrement, tokInt), .startBt = 9, .lenBts = 2 },
        //             //~ (Node){ .tp = nodId, .pl1 = I + 1, .pl2 = 2,      .startBt = 13, .lenBts = 4 } // coll
        //     //~ })),
        //     //~ ((EntityImport[]) {(EntityImport){ .name = s("map"), .entity = (Entity){  }},
        //                        //~ (EntityImport){ .name = s("coll"), .entity = (Entity){ }},
        //         //~ })
        // //~ ),
        // //~ createTest(
        //     //~ s("Non-unary operator as first-class value"), 
        //     //~ s("x = bimap / dividends divisors"),
        //     //~ (((Node[]) {
        //             //~ (Node){ .tp = nodAssignment,            .pl2 = 6, .startBt = 0, .lenBts = 30 },
        //             //~ // "3" because the first binding is taken up by the "imported" function, "foo"
        //             //~ (Node){ .tp = nodBinding, .pl1 = 0,               .startBt = 0, .lenBts = 1 },    // x
        //             //~ (Node){ .tp = nodExpr,                  .pl2 = 4, .startBt = 4, .lenBts = 26 },
        //             //~ (Node){ .tp = nodCall, .pl1 = F,        .pl2 = 3, .startBt = 4, .lenBts = 5 },    // bimap
        //             //~ (Node){ .tp = nodId, .pl1 = oper(opTDivBy, tokInt), .startBt = 10, .lenBts = 1 },   // /
        //             //~ (Node){ .tp = nodId, .pl1 = M,      .pl2 = 2,     .startBt = 12, .lenBts = 9 },   // dividends
        //             //~ (Node){ .tp = nodId, .pl1 = M + 1,      .pl2 = 3, .startBt = 22, .lenBts = 8 }    // divisors
        //     //~ })),
        //     //~ ((EntityImport[]) {
        //                        //~ (EntityImport){ .name = s("dividends"), .entity = (Entity){}},
        //                        //~ (EntityImport){ .name = s("divisors"), .entity = (Entity){}}
        //     //~ }),
        //     //~ ((EntityImport[]) {(EntityImport){ .name = s("bimap"), .count = 1}})
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
    
ParserTestSet* functionTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Functions test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function definition 1"),
            s("(:f newFn Int(x Int y Float) = a = x)"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,             .pl2 = 7, .startBt = 0, .lenBts = 37 },
                (Node){ .tp = nodBinding, .pl1 = 0,           .startBt = 4, .lenBts = 5 },
                (Node){ .tp = nodScope, .pl2 = 5,             .startBt = 13, .lenBts = 24 },
                (Node){ .tp = nodBinding, .pl1 = 1,           .startBt = 14, .lenBts = 1 },  // param x
                (Node){ .tp = nodBinding, .pl1 = 2,           .startBt = 20, .lenBts = 1 },  // param y
                (Node){ .tp = nodAssignment,        .pl2 = 2, .startBt = 31, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 3,           .startBt = 31, .lenBts = 1 },  // local a
                (Node){ .tp = nodId, .pl1 = 1,      .pl2 = 2, .startBt = 35, .lenBts = 1 }   // x                
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Simple function definition 2"),
            s("(:f newFn String(x String y Float) =\n"
              "    a = x\n"
              "    return a\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 9, .startBt = 0,  .lenBts = 61 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 5 },
                (Node){ .tp = nodScope,            .pl2 = 7, .startBt = 16, .lenBts = 45 },
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 17, .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2,          .startBt = 26, .lenBts = 1 }, // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2, .startBt = 41, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 3,          .startBt = 41, .lenBts = 1 }, // local a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 2, .startBt = 45, .lenBts = 1 }, // x
                (Node){ .tp = nodReturn,           .pl2 = 1, .startBt = 51, .lenBts = 8 },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 5, .startBt = 58, .lenBts = 1 }  // a
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTestWithError(
            s("Function definition wrong return type"),
            s(errTypeWrongReturnType),
            s("(:f newFn String(x Float y Float) =\n"
              "    a = x\n"
              "    return a\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,                      .startBt = 0,  .lenBts = 60 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4,  .lenBts = 5 },
                (Node){ .tp = nodScope,                      .startBt = 16, .lenBts = 44 },
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 17, .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2,          .startBt = 25, .lenBts = 1 }, // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2, .startBt = 40, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 3,          .startBt = 40, .lenBts = 1 }, // local a
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 2, .startBt = 44, .lenBts = 1 }, // x
                (Node){ .tp = nodReturn,                     .startBt = 50, .lenBts = 8 },
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 5, .startBt = 57, .lenBts = 1 }  // a
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Function definition with complex return"),
            s("(:f newFn String(x Int y Float) =\n"
              "    return $(- ,x y)\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 11, .startBt = 0,  .lenBts = 56 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 5 },
                (Node){ .tp = nodScope,            .pl2 = 9, .startBt = 16, .lenBts = 40 },
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 17, .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2,          .startBt = 23, .lenBts = 1 }, // param y
                (Node){ .tp = nodReturn,           .pl2 = 6, .startBt = 38, .lenBts = 16 },
                (Node){ .tp = nodExpr,           .pl2 = 5, .startBt = 45, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = oper(opTToString, tokFloat), .pl2 = 1, .startBt = 45, .lenBts = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opTMinus, tokFloat), .pl2 = 2, .startBt = 47, .lenBts = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opTToFloat, tokInt), .pl2 = 1, .startBt = 49, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1,     .pl2 = 2, .startBt = 50, .lenBts = 1 },  // x
                (Node){ .tp = nodId, .pl1 = 2,     .pl2 = 4, .startBt = 52, .lenBts = 1 }  // y
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Mutually recursive function definitions"),
            s("(:f foo Int(x Int y Float) =\n"
              "    a = x\n"
              "    return bar y a)\n"
              "(:f bar Int(x Float y Int) =\n"
              "    return foo y x)"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,            .pl2 = 12, .startBt = 0,   .lenBts = 58 }, // foo
                (Node){ .tp = nodBinding, .pl1 = 0,           .startBt = 4,  .lenBts = 3 },
                (Node){ .tp = nodScope,            .pl2 = 10, .startBt = 11,  .lenBts = 47 },
                (Node){ .tp = nodBinding, .pl1 = 2,           .startBt = 12,  .lenBts = 1 },  // param x. First 2 bindings = types
                (Node){ .tp = nodBinding, .pl1 = 3,           .startBt = 18,  .lenBts = 1 },  // param y
                (Node){ .tp = nodAssignment,       .pl2 = 2,  .startBt = 33,  .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 4,           .startBt = 33,  .lenBts = 1 },  // local a
                (Node){ .tp = nodId,     .pl1 = 2, .pl2 = 2,  .startBt = 37,  .lenBts = 1 },  // x
                (Node){ .tp = nodReturn,           .pl2 = 4,  .startBt = 43,  .lenBts = 14 },
                (Node){ .tp = nodExpr,             .pl2 = 3,  .startBt = 50,  .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = 1,   .pl2 = 2,  .startBt = 50,  .lenBts = 3 }, // bar call
                (Node){ .tp = nodId, .pl1 = 3,     .pl2 = 3,  .startBt = 54,  .lenBts = 1 }, // y
                (Node){ .tp = nodId, .pl1 = 4,     .pl2 = 5,  .startBt = 56,  .lenBts = 1 }, // a
                
                (Node){ .tp = nodFnDef,            .pl2 = 9,  .startBt = 59,  .lenBts = 48 }, // bar
                (Node){ .tp = nodBinding, .pl1 = 1,           .startBt = 63,  .lenBts = 3 },                
                (Node){ .tp = nodScope,            .pl2 = 7,  .startBt = 70,  .lenBts = 37 },
                (Node){ .tp = nodBinding, .pl1 = 5,           .startBt = 71,  .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 6,           .startBt = 79,  .lenBts = 1 }, // param y
                (Node){ .tp = nodReturn,           .pl2 = 4,  .startBt = 92,  .lenBts = 14 },
                (Node){ .tp = nodExpr,             .pl2 = 3,  .startBt = 99,  .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = 0,  .pl2 = 2,  .startBt = 99,  .lenBts = 3 }, // foo call
                (Node){ .tp = nodId, .pl1 = 6,     .pl2 = 3,  .startBt = 103, .lenBts = 1 }, // y
                (Node){ .tp = nodId, .pl1 = 5,     .pl2 = 2,  .startBt = 105, .lenBts = 1 }  // x
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Function definition with nested scope"),
            s("(:f main(x Int y Float) =\n"
              "    (:\n"
              "        a = 5"
              "    )\n"
              "    a = (- ,x y)\n"
              "    print $a\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 19, .startBt = 0,  .lenBts = 83 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 4 },
                (Node){ .tp = nodScope,            .pl2 = 17, .startBt = 8, .lenBts = 75 },
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 9, .lenBts = 1 }, // param x
                (Node){ .tp = nodBinding, .pl1 = 2,          .startBt = 15, .lenBts = 1 }, // param y
                
                (Node){ .tp = nodScope,            .pl2 = 3, .startBt = 30, .lenBts = 21 },
                (Node){ .tp = nodAssignment,       .pl2 = 2, .startBt = 41, .lenBts = 5 }, 
                (Node){ .tp = nodBinding, .pl1 = 3,          .startBt = 41, .lenBts = 1 }, // first a =
                (Node){ .tp = tokInt,              .pl2 = 5, .startBt = 45, .lenBts = 1 },
                
                (Node){ .tp = nodAssignment,       .pl2 = 6, .startBt = 56, .lenBts = 12 }, 
                (Node){ .tp = nodBinding, .pl1 = 4,          .startBt = 56, .lenBts = 1 }, // second a =
                (Node){ .tp = nodExpr,            .pl2 = 4, .startBt = 60, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = oper(opTMinus, tokFloat), .pl2 = 2,  .startBt = 61, .lenBts = 1 },
                (Node){ .tp = nodCall, .pl1 = oper(opTToFloat, tokInt), .pl2 = 1, .startBt = 63, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 1, .startBt = 64, .lenBts = 1 }, // x
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = 3, .startBt = 66, .lenBts = 1 }, // y

                (Node){ .tp = nodExpr,            .pl2 = 3, .startBt = 73, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I,  .pl2 = 1, .startBt = 73, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opTToString, tokFloat), .pl2 = 1, .startBt = 79, .lenBts = 1 }, // $
                (Node){ .tp = nodId, .pl1 = 4,  .pl2 = 5, .startBt = 80, .lenBts = 1 } // a
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
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


ParserTestSet* ifTests(Compiler* proto, Arena* a) {
    return createTestSet(s("If test set"), a, ((ParserTest[]){
        createTest(
            s("Simple if"),
            s("x = (if == 5 5 => print `5`)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 10, .startBt = 0, .lenBts = 28 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 8, .startBt = 4, .lenBts = 24 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = oper(opTEquality, tokInt), .pl2 = 2, .startBt = 8, .lenBts = 2 }, // ==
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 11, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 13, .lenBts = 1 },
                
                (Node){ .tp = nodIfClause,    .pl2 = 3, .startBt = 18, .lenBts = 9 },                
                (Node){ .tp = nodExpr, .pl2 = 2, .startBt = 18, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1, .startBt = 18, .lenBts = 5 }, // print
                (Node){ .tp = tokString, .startBt = 24, .lenBts = 3 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("If with else"),
            s("x = (if > 5 3 => `5` else `=)`)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 10, .startBt = 0, .lenBts = 31 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 8, .startBt = 4, .lenBts = 27 },
                

                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opTGreaterThan, tokInt), .pl2 = 2, .startBt = 8, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 10, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 3, .startBt = 12, .lenBts = 1 },
                (Node){ .tp = nodIfClause,  .pl2 = 1, .startBt = 17, .lenBts = 3 },      
                (Node){ .tp = tokString, .startBt = 17, .lenBts = 3 },
                
                (Node){ .tp = nodIfClause, .pl2 = 1, .startBt = 26, .lenBts = 4 },
                (Node){ .tp = tokString,         .startBt = 26, .lenBts = 4 },
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("If with elseif"),
            s("x = (if > 5 3  => 11\n"
              "       == 5 3 => 4)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 14, .startBt = 0, .lenBts = 40 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 12, .startBt = 4, .lenBts = 36 },
                
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opTGreaterThan, tokInt), .pl2 = 2, .startBt = 8, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 10, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 3, .startBt = 12, .lenBts = 1 },
                (Node){ .tp = nodIfClause,  .pl2 = 1, .startBt = 18, .lenBts = 2 },
                (Node){ .tp = tokInt, .pl2 = 11, .startBt = 18, .lenBts = 2 },
                
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 28, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = oper(opTEquality, tokInt), .pl2 = 2, .startBt = 28, .lenBts = 2 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 31, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 3, .startBt = 33, .lenBts = 1 },
                (Node){ .tp = nodIfClause,   .pl2 = 1, .startBt = 38, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 4, .startBt = 38, .lenBts = 1 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("If with elseif and else"),
            s("x = (if > 5 3  => 11\n"
              "        == 5 3 =>  4\n"
              "        else     100)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .pl2 = 16, .startBt = 0, .lenBts = 63 },
                (Node){ .tp = nodBinding, .pl1 = 0, .startBt = 0, .lenBts = 1 }, // x
                
                (Node){ .tp = nodIf, .pl1 = slParenMulti, .pl2 = 14, .startBt = 4, .lenBts = 59 },

                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 8, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opTGreaterThan, tokInt), .pl2 = 2, .startBt = 8, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 10, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 3, .startBt = 12, .lenBts = 1 },
                (Node){ .tp = nodIfClause, .pl2 = 1, .startBt = 18, .lenBts = 2 },                
                (Node){ .tp = tokInt, .pl2 = 11, .startBt = 18, .lenBts = 2 },
                
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 29, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = oper(opTEquality, tokInt), .pl2 = 2, .startBt = 29, .lenBts = 2 },
                (Node){ .tp = tokInt, .pl2 = 5, .startBt = 32, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 3, .startBt = 34, .lenBts = 1 },
                (Node){ .tp = nodIfClause,  .pl2 = 1, .startBt = 40, .lenBts = 1 },                
                (Node){ .tp = tokInt, .pl2 = 4, .startBt = 40, .lenBts = 1 },
                
                (Node){ .tp = nodIfClause, .pl2 = 1, .startBt = 59, .lenBts = 3 },
                (Node){ .tp = tokInt, .pl2 = 100, .startBt = 59, .lenBts = 3 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        )
    }));
}


ParserTestSet* loopTests(Compiler* proto, Arena* a) {
    return createTestSet(s("Loops test set"), a, ((ParserTest[]){
        createTest(
            s("Simple loop 1"),
            s("(:f f Int() = (:while (< x 101). x = 1. print $x))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 16, .startBt = 0, .lenBts = 50 },
                (Node){ .tp = nodBinding, .pl1 = 0,          .startBt = 4, .lenBts = 1 },
                (Node){ .tp = nodScope,           .pl2 = 14, .startBt = 9, .lenBts = 41 }, // function body
                
                (Node){ .tp = nodWhile,           .pl2 = 13, .startBt = 14, .lenBts = 35 },
                
                (Node){ .tp = nodScope,           .pl2 = 12, .startBt = 33, .lenBts = 16 },
                
                (Node){ .tp = nodAssignment,      .pl2 = 2, .startBt = 33, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 33, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,             .pl2 = 1, .startBt = 37, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 22, .lenBts = 9 },
                (Node){ .tp = nodExpr,            .pl2 = 3, .startBt = 22, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 23, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 25, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 27, .lenBts = 3 },
                
                (Node){ .tp = nodExpr,           .pl2 = 3,  .startBt = 40, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1,  .startBt = 40, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opTToString, tokInt), .pl2 = 1, .startBt = 46, .lenBts = 1 }, // $
                (Node){ .tp = nodId,   .pl1 = 1, .pl2 = 2,  .startBt = 47, .lenBts = 1 }      // x
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("While with two complex initializers"),
            s("(:f f Int() =\n"
              "    (:while < y 101. x = 17. y = / x 5\n"
              "        print $x\n"
              "        --x\n"
              "        ++y))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 28,        .lenBts = 95 },
                (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 1 },
                (Node){ .tp = nodScope,           .pl2 = 26, .startBt = 9, .lenBts = 86 }, // function body
                
                (Node){ .tp = nodWhile,           .pl2 = 25, .startBt = 18, .lenBts = 76 },                
                (Node){ .tp = nodScope,                 .pl2 = 24, .startBt = 35, .lenBts = 59 },
                
                (Node){ .tp = nodAssignment,            .pl2 = 2, .startBt = 35, .lenBts = 6 },
                (Node){ .tp = nodBinding, .pl1 = 1,                .startBt = 35, .lenBts = 1 },  // def x
                (Node){ .tp = tokInt,                   .pl2 = 17, .startBt = 39, .lenBts = 2 },
                
                (Node){ .tp = nodAssignment, .pl2 = 5,              .startBt = 43, .lenBts = 9 },
                (Node){ .tp = nodBinding, .pl1 = 2,                 .startBt = 43, .lenBts = 1 },  // def y
                (Node){ .tp = nodExpr,                  .pl2 = 3, .startBt = 47, .lenBts = 5 },
                (Node){ .tp = nodCall, .pl1 = oper(opTDivBy, tokInt), .pl2 = 2, .startBt = 47, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1,          .pl2 = 3, .startBt = 49, .lenBts = 1 }, // x          
                (Node){ .tp = tokInt,                   .pl2 = 5, .startBt = 51, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodExpr, .pl2 = 3,                    .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 26, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 2,          .pl2 = 2, .startBt = 28, .lenBts = 1 }, // y
                (Node){ .tp = tokInt,                   .pl2 = 101, .startBt = 30, .lenBts = 3 },
                
                (Node){ .tp = nodExpr,                  .pl2 = 3, .startBt = 61, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I,        .pl2 = 1, .startBt = 61, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opTToString, tokInt), .pl2 = 1, .startBt = 67, .lenBts = 1 }, // $
                (Node){ .tp = nodId,    .pl1 = 1,       .pl2 = 3, .startBt = 68, .lenBts = 1 },   // x

                (Node){ .tp = nodExpr,              .pl2 = 2, .startBt = 78, .lenBts = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opTDecrement, tokInt), .pl2 = 1, .startBt = 78, .lenBts = 2 }, // --
                (Node){ .tp = nodId, .pl1 = 1,      .pl2 = 3, .startBt = 80, .lenBts = 1 }, // x

                (Node){ .tp = nodExpr, .pl2 = 2, .startBt = 90, .lenBts = 3 },
                (Node){ .tp = nodCall, .pl1 = oper(opTIncrement, tokInt), .pl2 = 1, .startBt = 90, .lenBts = 2 }, // ++
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = 2, .startBt = 92, .lenBts = 1 }    // y
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("While without initializers"),
            s("(:f f Int() =\n"
              "    x = 4\n"
              "    (:while (< x 101) \n"
              "        print $x))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,         .pl2 = 16,          .lenBts = 65 },
                (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 1 },
                (Node){ .tp = nodScope, .pl2 = 14,           .startBt = 9, .lenBts = 56 }, // function body

                (Node){ .tp = nodAssignment, .pl2 = 2,       .startBt = 18, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,          .startBt = 18, .lenBts = 1 },  // x
                (Node){ .tp = tokInt,              .pl2 = 4, .startBt = 22, .lenBts = 1 },
                
                (Node){ .tp = nodWhile,            .pl2 = 10, .startBt = 28, .lenBts = 36 },                
                (Node){ .tp = nodScope, .pl2 = 9,           .startBt = 55, .lenBts = 9 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 36, .lenBts = 9 },
                (Node){ .tp = nodExpr, .pl2 = 3,            .startBt = 36, .lenBts = 9 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 37, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 39, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 41, .lenBts = 3 },
                
                (Node){ .tp = nodExpr,         .pl2 = 3,    .startBt = 55, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1,  .startBt = 55, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opTToString, tokInt), .pl2 = 1,  .startBt = 61, .lenBts = 1 }, // $
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 62, .lenBts = 1 }  // x
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("While with break and continue"),
            s("(:f f Int() =\n"
              "    (:while < x 101. x = 0\n"
              "        break\n"
              "        continue))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,             .pl2 = 14, .lenBts = 73 },
                (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 1 },
                (Node){ .tp = nodScope,           .pl2 = 12, .startBt = 9, .lenBts = 64 }, // function body
                
                (Node){ .tp = nodWhile,           .pl2 = 11, .startBt = 18, .lenBts = 54 },
                
                (Node){ .tp = nodScope, .pl2 = 10, .startBt = 35, .lenBts = 37 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 35, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1, .startBt = 35, .lenBts = 1 }, // x
                (Node){ .tp = tokInt, .pl2 = 0, .startBt = 39, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 26, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2, .startBt = 28, .lenBts = 1 }, // x
                (Node){ .tp = tokInt, .pl2 = 101, .startBt = 30, .lenBts = 3 },
                
                (Node){ .tp = nodBreak, .pl1 = -1, .startBt = 49, .lenBts = 5 },
                (Node){ .tp = nodContinue, .pl1 = -1, .startBt = 63, .lenBts = 8 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTestWithError(
            s("While with break error"),
            s(errBreakContinueInvalidDepth),
            s("(:f f Int() =\n"
              "    (:while < x 101. x = 0\n"
              "        break 2))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,                            .lenBts = 58 },
                (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 1 },
                (Node){ .tp = nodScope,      .startBt = 9, .lenBts = 49 }, // function body
                
                (Node){ .tp = nodWhile,            .startBt = 18, .lenBts = 39 },
                
                (Node){ .tp = nodScope,            .startBt = 35, .lenBts = 22 },
                
                (Node){ .tp = nodAssignment,      .pl2 = 2, .startBt = 35, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 35, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,             .pl2 = 0, .startBt = 39, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodExpr,            .pl2 = 3, .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 26, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 28, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 30, .lenBts = 3 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTest(
            s("Nested while with deep break and continue"),
            s("(:f f Int() =\n"
              "    (:while < a 101. a = 0\n"
              "        (:while < b 101. b = 0\n"
              "            (:while < c 101. c = 0\n"
              "                break 3))\n"
              "        (:while < d 51. d = 0\n"
              "            (:while < e 101. e = 0\n"
              "                continue 2))\n"
              "        print $a))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,           .pl2 = 58, .lenBts = 245 },
                (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 1 },                
                (Node){ .tp = nodScope,           .pl2 = 56, .startBt = 9, .lenBts = 236 }, // function body
                
                (Node){ .tp = nodWhile,  .pl1 = 1, .pl2 = 55, .startBt = 18, .lenBts = 226 }, // while #1. It's being broken
                (Node){ .tp = nodScope, .pl2 = 54, .startBt = 35, .lenBts = 209 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 35, .lenBts = 5 }, // a = 
                (Node){ .tp = nodBinding, .pl1 = 1, .startBt = 35, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 0, .startBt = 39, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 26, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 26, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2, .startBt = 28, .lenBts = 1 }, // a
                (Node){ .tp = tokInt, .pl2 = 101, .startBt = 30, .lenBts = 3 },

                (Node){ .tp = nodWhile,          .pl2 = 20, .startBt = 49, .lenBts = 83 }, // while #2
                (Node){ .tp = nodScope,          .pl2 = 19, .startBt = 66, .lenBts = 66 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 66, .lenBts = 5 }, // b = 
                (Node){ .tp = nodBinding, .pl1 = 2, .startBt = 66, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 0, .startBt = 70, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 57, .lenBts = 7 },
                (Node){ .tp = nodExpr,                     .pl2 = 3, .startBt = 57, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 57, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 2, .pl2 = 3, .startBt = 59, .lenBts = 1 }, // b
                (Node){ .tp = tokInt, .pl2 = 101, .startBt = 61, .lenBts = 3 },

                (Node){ .tp = nodWhile,          .pl2 = 10, .startBt = 84, .lenBts = 47 }, // while #3, double-nested
                (Node){ .tp = nodScope,          .pl2 = 9, .startBt = 101, .lenBts = 30 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 101, .lenBts = 5 }, // c = 
                (Node){ .tp = nodBinding, .pl1 = 3, .startBt = 101, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 0, .startBt = 105, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 92, .lenBts = 7 },
                (Node){ .tp = nodExpr,                     .pl2 = 3, .startBt = 92, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 92, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 3, .pl2 = 4, .startBt = 94, .lenBts = 1 }, // c
                (Node){ .tp = tokInt,          .pl2 = 101, .startBt = 96, .lenBts = 3 },

                (Node){ .tp = nodBreak, .pl1 = 1,   .startBt = 123, .lenBts = 7 },

                (Node){ .tp = nodWhile,  .pl1 = 4, .pl2 = 20, .startBt = 141, .lenBts = 85 }, // while #4. It's being continued
                (Node){ .tp = nodScope,            .pl2 = 19, .startBt = 157, .lenBts = 69 },
                
                (Node){ .tp = nodAssignment,    .pl2 = 2, .startBt = 157, .lenBts = 5 }, // d = 
                (Node){ .tp = nodBinding, .pl1 = 4, .startBt = 157, .lenBts = 1 },
                (Node){ .tp = tokInt,           .pl2 = 0, .startBt = 161, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 149, .lenBts = 6 },
                (Node){ .tp = nodExpr,          .pl2 = 3, .startBt = 149, .lenBts = 6 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 149, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 4, .pl2 = 5, .startBt = 151, .lenBts = 1 }, // d
                (Node){ .tp = tokInt, .pl2 = 51, .startBt = 153, .lenBts = 2 },

                (Node){ .tp = nodWhile,         .pl2 = 10, .startBt = 175, .lenBts = 50 }, // while #5, last
                (Node){ .tp = nodScope,         .pl2 = 9,  .startBt = 192, .lenBts = 33 },
                
                (Node){ .tp = nodAssignment, .pl2 = 2, .startBt = 192, .lenBts = 5 }, // e = 
                (Node){ .tp = nodBinding, .pl1 = 5, .startBt = 192, .lenBts = 1 },
                (Node){ .tp = tokInt, .pl2 = 0, .startBt = 196, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 183, .lenBts = 7 },
                (Node){ .tp = nodExpr,         .pl2 = 3, .startBt = 183, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTLessThan, tokInt), .pl2 = 2, .startBt = 183, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 5, .pl2 = 6, .startBt = 185, .lenBts = 1 }, // e
                (Node){ .tp = tokInt,          .pl2 = 101, .startBt = 187, .lenBts = 3 },

                (Node){ .tp = nodContinue, .pl1 = 4,    .startBt = 214, .lenBts = 10 },
                
                (Node){ .tp = nodExpr, .pl2 = 3, .startBt = 235, .lenBts = 8 },
                (Node){ .tp = nodCall, .pl1 = I, .pl2 = 1, .startBt = 235, .lenBts = 5 }, // print
                (Node){ .tp = nodCall, .pl1 = oper(opTToString, tokInt), .pl2 = 1, .startBt = 241, .lenBts = 1 }, // $
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,   .startBt = 242, .lenBts = 1 }  // a
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        ),
        createTestWithError(
            s("While with type error"),
            s(errTypeMustBeBool),
            s("(:f f Int() = (:while / x 101. x = 1. print $x))"),
            ((Node[]) {
                (Node){ .tp = nodFnDef,                             .lenBts = 48 },
                (Node){ .tp = nodBinding, .pl1 = 0,   .startBt = 4, .lenBts = 1 },
                (Node){ .tp = nodScope,      .startBt = 9, .lenBts = 39 }, // function body
                
                (Node){ .tp = nodWhile,            .startBt = 14, .lenBts = 33 },
                
                (Node){ .tp = nodScope,            .startBt = 31, .lenBts = 16 },
                
                (Node){ .tp = nodAssignment,      .pl2 = 2, .startBt = 31, .lenBts = 5 },
                (Node){ .tp = nodBinding, .pl1 = 1,         .startBt = 31, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,             .pl2 = 1, .startBt = 35, .lenBts = 1 },
                
                (Node){ .tp = nodWhileCond, .pl1 = slStmt, .pl2 = 4, .startBt = 22, .lenBts = 7 },
                (Node){ .tp = nodExpr,            .pl2 = 3, .startBt = 22, .lenBts = 7 },
                (Node){ .tp = nodCall, .pl1 = oper(opTDivBy, tokInt), .pl2 = 2, .startBt = 22, .lenBts = 1 },
                (Node){ .tp = nodId, .pl1 = 1, .pl2 = 2,    .startBt = 24, .lenBts = 1 }, // x
                (Node){ .tp = tokInt,          .pl2 = 101,  .startBt = 26, .lenBts = 3 }
            }),
            ((Int[]) {}),
            ((EntityImport[]) {})
        )
    }));
}

void runATestSet(ParserTestSet* (*testGenerator)(Compiler*, Arena*), int* countPassed, int* countTests, 
                 Compiler* proto, Arena* a) {
    ParserTestSet* testSet = (testGenerator)(proto, a);
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
    Compiler* proto = createCompilerProto(a);
    int countPassed = 0;
    int countTests = 0;
    runATestSet(&assignmentTests, &countPassed, &countTests, proto, a);
    runATestSet(&expressionTests, &countPassed, &countTests, proto, a);
    runATestSet(&functionTests, &countPassed, &countTests, proto, a);
    runATestSet(&ifTests, &countPassed, &countTests, proto, a);
    runATestSet(&loopTests, &countPassed, &countTests, proto, a);

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
