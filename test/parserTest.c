#include "../source/utils/aliases.h"
#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/structures/stack.h"
#include "../source/compiler/parser.h"
#include "../source/compiler/parserConstants.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    String* name;
    Lexer* input;
    Parser* initParser;
    Parser* expectedOutput;
} ParserTest;


typedef struct {
    String* name;
    Int totalTests;
    ParserTest* tests;
} ParserTestSet;

#define M 70000000 // A constant larger than the largest allowed file size to distinguish unshifted binding ids

/** Must agree in order with node types in ParserConstants.h */
const char* nodeNames[] = {
    "Int", "Float", "Bool", "String", "_", "DocComment", 
    "id", "call", "binding", "type", "@annot", "and", "or", 
    "(-", "expr", "accessor", "=", ":=", "+=",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue",
    "defer", "each", "embed", "export", "exposePriv", "fnDef", "interface",
    "lambda", "package", "return", "struct", "try", "yield",
    "ifClause", "else", "loop", "loopCond", "if", "ifPr", "impl", "match"
};

private Parser* buildParserWithError0(String* errMsg, Lexer* lx, Arena *a, int nextInd, Arr(Node) nodes) {
    Parser* result = createParser(lx, a);
    result->wasError = true;
    result->errMsg = errMsg;
    result->nextInd = nextInd;
    result->nodes = allocateOnArena(nextInd*sizeof(Token), a);
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

/** Creates a test with two parser: one is the init parser (contains all the "imported" bindings)
 *  and one is the output parser (with the bindings and the expected nodes). When the test is run,
 *  the init parser will parse the tokens and then will be compared to the expected output parser.
 *  Nontrivial: this handles binding ids inside nodes, so that e.g. if the payload1 in nodBinding is 1,
 *  it will be inserted as 1 + (the number of built-in bindings)
 */
private ParserTest createTest0(String* name, String* input, Arr(Node) nodes, Int countNodes, 
                               Arr(BindingImport) bindings, Int countBindings, 
                               LanguageDefinition* langDef, Arena* a) {
    Lexer* lx = lexicallyAnalyze(input, langDef, a);
    Parser* initParser     = createParser(lx, a);
    Parser* expectedParser = createParser(lx, a);
    
    if (expectedParser->wasError) {
        return (ParserTest){ .name = name, .input = lx, .initParser = initParser, .expectedOutput = expectedParser };
    }
    importBindings(bindings, countBindings, initParser);
    importBindings(bindings, countBindings, expectedParser);    

    for (Int i = 0; i < countNodes; i++) {
        untt nodeType = nodes[i].tp;
        // All the node types which contain bindingIds
        if (nodeType == nodId || nodeType == nodCall || nodeType == nodBinding || nodeType == nodBinding
         || nodeType == nodFnDef) {
            Int actualBinding = (nodes[i].payload1 < M) 
                ? (nodes[i].payload1 + countOperators) 
                : (nodes[i].payload1 - M);
            addNode((Node){ .tp = nodeType, .payload1 = actualBinding, .payload2 = nodes[i].payload2, 
                            .startByte = nodes[i].startByte, .lenBytes = nodes[i].lenBytes }, 
                    expectedParser);
        } else {
            addNode(nodes[i], expectedParser);
        }
    }
    return (ParserTest){ .name = name, .input = lx, .initParser = initParser, .expectedOutput = expectedParser };
}

#define createTest(name, input, nodes, bindings) createTest0((name), (input), \
    (nodes), sizeof(nodes)/sizeof(Node), bindings, sizeof(bindings)/sizeof(BindingImport), langDef, a)


/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
int equalityParser(Parser a, Parser b) {
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.nextInd < b.nextInd ? a.nextInd : b.nextInd;
    int i = 0;
    for (; i < commonLength; i++) {
        Node nodA = a.nodes[i];
        Node nodB = b.nodes[i];
        if (nodA.tp != nodB.tp || nodA.lenBytes != nodB.lenBytes || nodA.startByte != nodB.startByte 
            || nodA.payload1 != nodB.payload1 || nodA.payload2 != nodB.payload2) {
            printf("\n\nUNEQUAL RESULTS\n", i);
            if (nodA.tp != nodB.tp) {
                printf("Diff in tp, %s but was expected %s\n", nodeNames[nodA.tp], nodeNames[nodB.tp]);
            }
            if (nodA.lenBytes != nodB.lenBytes) {
                printf("Diff in lenBytes, %d but was expected %d\n", nodA.lenBytes, nodB.lenBytes);
            }
            if (nodA.startByte != nodB.startByte) {
                printf("Diff in startByte, %d but was expected %d\n", nodA.startByte, nodB.startByte);
            }
            if (nodA.payload1 != nodB.payload1) {
                printf("Diff in payload1, %d but was expected %d\n", nodA.payload1, nodB.payload1);
            }
            if (nodA.payload2 != nodB.payload2) {
                printf("Diff in payload2, %d but was expected %d\n", nodA.payload2, nodB.payload2);
            }            
            return i;
        }
    }
    return (a.nextInd == b.nextInd) ? -2 : i;        
}


void printParser(Parser* a) {
    if (a->wasError) {
        printf("Error: ");
        printString(a->errMsg);
    }
    for (int i = 0; i < a->nextInd; i++) {
        Node nod = a->nodes[i];
        if (nod.payload1 != 0 || nod.payload2 != 0) {
            printf("%s %d %d [%d; %d]\n", nodeNames[nod.tp], nod.payload1, nod.payload2, nod.startByte, nod.lenBytes);
        } else {
            printf("%s [%d; %d]\n", nodeNames[nod.tp], nod.startByte, nod.lenBytes);
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

    Parser* resultParser = parseWithParser(test.input, test.initParser, a);
        
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
        printParser(resultParser);

    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On node %d\n", equalityStatus);
        print("    LEXER:")
        printLexer(test.input);
        print("    PARSER:")
        printParser(resultParser);
    }
}


ParserTestSet* assignmentTests(LanguageDefinition* langDef, Arena* a) {
    return createTestSet(s("Assignment test set"), a, ((ParserTest[]){
        createTest(
            s("Simple assignment"), 
            s("x = 12"),            
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                    (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodInt,  .payload2 = 12, .startByte = 4, .lenBytes = 2 }
            }),
            ((BindingImport[]) {})
        ),
        createTest(
            s("Double assignment"), 
            s("x = 12\n"
              "second = x"  
            ),
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                    (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 },     // x
                    (Node){ .tp = nodInt,  .payload2 = 12, .startByte = 4, .lenBytes = 2 },
                    (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 7, .lenBytes = 10 },
                    (Node){ .tp = nodBinding, .payload1 = 1, .startByte = 7, .lenBytes = 6 }, // second
                    (Node){ .tp = nodId, .payload1 = 0, .payload2 = 0, .startByte = 16, .lenBytes = 1 }
            }),
            ((BindingImport[]) {})
        ),
        //~ createTestWithError(
            //~ s("Assignment shadowing error"), 
            //~ errorAssignmentShadowing,
            //~ s("x = 12\n"
              //~ ".x = 7"  
            //~ ),
            //~ ((Node[]) {
                    //~ (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                    //~ (Node){ .tp = nodBinding, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                    //~ (Node){ .tp = nodInt,  .payload2 = 12, .startByte = 4, .lenBytes = 2 }
            //~ }),
            //~ ((Int[]) {}), 
            //~ ((Binding[]) {})
        //~ )          
    }));
}


ParserTestSet* expressionTests(LanguageDefinition* langDef, Arena* a) {
    return createTestSet(s("Expression test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function call"), 
            s("x = foo 10 2 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 14 },
                    // " + 1" because the first binding is taken up by the "imported" function, "foo"
                    (Node){ .tp = nodBinding, .payload1 = 1, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 10 },
                    (Node){ .tp = nodCall, .payload1 = 0, .payload2 = 3, .startByte = 4, .lenBytes = 3 },
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 8, .lenBytes = 2 },
                    (Node){ .tp = nodInt, .payload2 = 2,   .startByte = 11, .lenBytes = 1 },
                    (Node){ .tp = nodInt, .payload2 = 3,   .startByte = 13, .lenBytes = 1 }                    
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), .binding = (Binding){ .flavor = bndCallable }
            }})
        ),
        createTest(
            s("Unary operator as first-class value"), 
            s("x = map (--) coll"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 5, .startByte = 0, .lenBytes = 17 },
                    (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 3, .startByte = 4, .lenBytes = 13 },
                    (Node){ .tp = nodCall, .payload1 = 0, .payload2 = 2, .startByte = 4, .lenBytes = 3 }, // map
                    (Node){ .tp = nodId, .payload1 = opTDecrement + M, .startByte = 9, .lenBytes = 2 },  // --
                    (Node){ .tp = nodId, .payload1 = 1, .payload2 = 2, .startByte = 13, .lenBytes = 4 }  // coll
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("map"), .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("coll"), .binding = (Binding){.flavor = bndImmut }}
            })
        ),
        createTest(
            s("Non-unary operator as first-class value"), 
            s("x = bimap / dividends divisors"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 30 },
                    // "3" because the first binding is taken up by the "imported" function, "foo"
                    (Node){ .tp = nodBinding, .payload1 = 3, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 26 },
                    (Node){ .tp = nodCall, .payload1 = 0, .payload2 = 3, .startByte = 4, .lenBytes = 5 },                    
                    (Node){ .tp = nodId, .payload1 = opTDivBy + M,  .startByte = 10, .lenBytes = 1 },// /
                    (Node){ .tp = nodId, .payload1 = 1, .payload2 = 2,   .startByte = 12, .lenBytes = 9 },   // dividends
                    (Node){ .tp = nodId, .payload1 = 2, .payload2 = 3,   .startByte = 22, .lenBytes = 8 }   // divisors
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("bimap"), 
                                                 .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("dividends"), 
                                                 .binding = (Binding){.flavor = bndImmut }},
                                (BindingImport){ .name = s("divisors"), 
                                                 .binding = (Binding){.flavor = bndImmut }}
            })
        ), 
        createTest(
            s("Nested function call 1"), 
            s("x = foo 10 (bar) 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 18 },                    
                    (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 14 },
                    (Node){ .tp = nodCall, .payload1 = 0, .payload2 = 3, .startByte = 4, .lenBytes = 3 }, // foo
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 8, .lenBytes = 2 },                    
                    (Node){ .tp = nodCall, .payload1 = 1, .payload2 = 0, .startByte = 12, .lenBytes = 3 }, // bar
                    (Node){ .tp = nodInt, .payload2 = 3,   .startByte = 17, .lenBytes = 1 }               
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), 
                                     .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("bar"), 
                                     .binding = (Binding){.flavor = bndCallable }}
            })
        ),
        createTest(
            s("Nested function call 2"), 
            s("x =  foo 10 (barr 3)"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 20 },                    
                    (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 5, .lenBytes = 15 },
                    (Node){ .tp = nodCall, .payload1 = 0, .payload2 = 2, .startByte = 5, .lenBytes = 3 },   // foo
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 9, .lenBytes = 2 },
                                       
                    (Node){ .tp = nodCall, .payload1 = 1, .payload2 = 1, .startByte = 13, .lenBytes = 4 },  // barr
                    (Node){ .tp = nodInt,   .payload2 = 3, .startByte = 18, .lenBytes = 1 }
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("barr"), .binding = (Binding){.flavor = bndCallable }}
            })
        ),
        createTest(
            s("Triple function call"), 
            s("x = buzz 2 3 4 (foo : triple 5)"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .payload2 = 9, .startByte = 0, .lenBytes = 31 },
                (Node){ .tp = nodBinding, .payload1 = 3, .startByte = 0, .lenBytes = 1 }, // x
                
                (Node){ .tp = nodExpr, .payload2 = 7, .startByte = 4, .lenBytes = 27 },
                (Node){ .tp = nodCall, .payload1 = 1, .payload2 = 4, .startByte = 4, .lenBytes = 4 }, // buzz
                (Node){ .tp = nodInt, .payload2 = 2, .startByte = 9, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 11, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 4, .startByte = 13, .lenBytes = 1 },

                (Node){ .tp = nodCall, .payload1 = 0, .payload2 = 1, .startByte = 16, .lenBytes = 3 }, // foo
                (Node){ .tp = nodCall, .payload1 = 2, .payload2 = 1, .startByte = 22, .lenBytes = 6 },  // triple
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 29, .lenBytes = 1 }
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("buzz"), .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("triple"), .binding = (Binding){.flavor = bndCallable }}
            })
        ),
        createTest(
            s("Operators simple"), 
            s("x = + 1 : * 2 3"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .payload2 = 7, .startByte = 0, .lenBytes = 15 },
                (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 }, // x
                (Node){ .tp = nodExpr,  .payload2 = 5, .startByte = 4, .lenBytes = 11 },
                (Node){ .tp = nodCall, .payload1 = opTPlus + M, .payload2 = 2, .startByte = 4, .lenBytes = 1 },   // +
                (Node){ .tp = nodInt, .payload2 = 1, .startByte = 6, .lenBytes = 1 },
                
                (Node){ .tp = nodCall, .payload1 = opTTimes + M, .payload2 = 2, .startByte = 10, .lenBytes = 1 }, // *
                (Node){ .tp = nodInt, .payload2 = 2, .startByte = 12, .lenBytes = 1 },   
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 14, .lenBytes = 1 }
            })),
            ((BindingImport[]) {})
        )
    }));
}

        //~ (ParserTest) { 
            //~ .name = str("Triple function call", a),
            //~ .input = str("c:foo b a :bar 5 :baz 7.2", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) {
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator precedence simple", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator precedence simple 2", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator precedence simple 3", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 1", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 2", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },       
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 3", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 1", a),
            //~ .input = str("(a + (b - c % 2)^11)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 2", a),
            //~ .input = str("a + -5", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },       
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 3", a),
            //~ .input = str("a + !(-5)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 4", a),
            //~ .input = str("a + !!(-3)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },             
        //~ (ParserTest) { 
            //~ .name = str("Operator arity error", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Single-item expr 1", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Single-item expr 2", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Unknown function arity", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },        
        //~ (ParserTest) { 
            //~ .name = str("Func-expr 1", a),
            //~ .input = str("(4:(a:g)):f", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },            
        //~ (ParserTest) { 
            //~ .name = str("Partial application?", a),
            //~ .input = str("5:(1 + )", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },          
        //~ (ParserTest) { 
            //~ .name = str("Partial application 2?", a),
            //~ .input = str("5:(3*a +)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Partial application error?", a),
            //~ .input = str("5:(3+ a*)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ }
        
        
        
        


//~ ParserTestSet* assignmentTests(Arena* a) {
    //~ return createTestSet(str("Assignment test set", a), 2, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple assignment 1", a),
            //~ .input = str("a = 1 + 5", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Simple assignment 2", a),
            //~ .input = str("a += 9", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(((Node[]) 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                (Node){ .tp = nodFnDef, .payload1 = 2, .payload2 = 7, .startByte = 0, .lenBytes = 37 },
                (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 4, .lenBytes = 5 }, // newFn
                
                (Node){ .tp = nodScope, .payload2 = 5, .startByte = 14, .lenBytes = 23 },
                (Node){ .tp = nodBinding, .payload1 = 3, .startByte = 15, .lenBytes = 1 }, // param x
                (Node){ .tp = nodBinding, .payload1 = 4, .startByte = 21, .lenBytes = 1 },  // param y
                (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 31, .lenBytes = 5 },  // param y
                (Node){ .tp = nodBinding, .payload1 = 5, .startByte = 31, .lenBytes = 1 },  // local a
                (Node){ .tp = nodId, .payload1 = 3, .payload2 = 2, .startByte = 35, .lenBytes = 1 }  // x                
            }),
            ((BindingImport[]) {})
        ),
        createTest(
            s("Simple function definition 2"),
            s("(.f newFn Int (x Int y Float)\n"
              "    a = x\n"
              "    return a\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .payload1 = 2, .payload2 = 9, .startByte = 0, .lenBytes = 54 },
                (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 4, .lenBytes = 5 }, // newFn
                (Node){ .tp = nodScope, .payload2 = 7, .startByte = 14, .lenBytes = 40 },
                (Node){ .tp = nodBinding, .payload1 = 3, .startByte = 15, .lenBytes = 1 }, // param x
                (Node){ .tp = nodBinding, .payload1 = 4, .startByte = 21, .lenBytes = 1 },  // param y
                (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 34, .lenBytes = 5 },  // param y
                (Node){ .tp = nodBinding, .payload1 = 5, .startByte = 34, .lenBytes = 1 },  // local a
                (Node){ .tp = nodId, .payload1 = 3, .payload2 = 2, .startByte = 38, .lenBytes = 1 },  // x
                (Node){ .tp = nodReturn, .payload2 = 1, .startByte = 44, .lenBytes = 8 },
                (Node){ .tp = nodId, .payload1 = 5, .payload2 = 5, .startByte = 51, .lenBytes = 1 } // a
            }),
            ((BindingImport[]) {})
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Function def error 1", a),
            //~ .input = str("fn newFn Int(x Int newFn Float)(return x + y)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },        
        //~ (ParserTest) { 
            //~ .name = str("Function def error 3", a),
            //~ .input = str("fn foo Int(x Int y Int)"
                              
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
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
                (Node){ .tp = nodAssignment, .payload2 = 10, .startByte = 0, .lenBytes = 28 },
                (Node){ .tp = nodBinding, .payload1 = 1, .startByte = 0, .lenBytes = 1 }, // x
                
                (Node){ .tp = nodIf, .payload1 = slParenMulti, .payload2 = 8, .startByte = 4, .lenBytes = 24 },
                (Node){ .tp = nodIfClause, .payload1 = 4, .payload2 = 7, .startByte = 8, .lenBytes = 19 },
                (Node){ .tp = nodExpr, .payload2 = 3, .startByte = 8, .lenBytes = 6 },
                (Node){ .tp = nodCall, .payload1 = opTEquality + M, .payload2 = 2, .startByte = 8, .lenBytes = 2 }, // ==
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 11, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 13, .lenBytes = 1 },
                
                (Node){ .tp = nodExpr, .payload2 = 2, .startByte = 18, .lenBytes = 9 },
                (Node){ .tp = nodCall, .payload1 = 0, .payload2 = 1, .startByte = 18, .lenBytes = 5 }, // print                
                (Node){ .tp = nodString, .startByte = 24, .lenBytes = 3 }
            }),
            ((BindingImport[]) {(BindingImport){ .name = s("print"), .binding = (Binding){.flavor = bndCallable }}})
        ),
        createTest(
            s("If with else"),
            s("x = (if > 5 3 => \"5\" else \"=)\")"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .payload2 = 10, .startByte = 0, .lenBytes = 31 },
                (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 }, // x
                
                (Node){ .tp = nodIf, .payload1 = slParenMulti, .payload2 = 8, .startByte = 4, .lenBytes = 27 },
                
                (Node){ .tp = nodIfClause, .payload1 = 4, .payload2 = 5, .startByte = 8, .lenBytes = 12 },
                (Node){ .tp = nodExpr, .payload2 = 3, .startByte = 8, .lenBytes = 5 },
                (Node){ .tp = nodCall, .payload1 = opTGreaterThan + M, .payload2 = 2, .startByte = 8, .lenBytes = 1 }, // >
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 10, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 12, .lenBytes = 1 },                
                (Node){ .tp = nodString, .startByte = 17, .lenBytes = 3 },
                
                (Node){ .tp = nodElse, .payload2 = 1, .startByte = 26, .lenBytes = 4 },
                (Node){ .tp = nodString, .startByte = 26, .lenBytes = 4 },
            }),
            ((BindingImport[]) {})
        ),
        createTest(
            s("If with elseif"),
            s("x = (if > 5 3  => 11\n"
              "       == 5 3 => 4)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .payload2 = 14, .startByte = 0, .lenBytes = 40 },
                (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 }, // x
                
                (Node){ .tp = nodIf, .payload1 = slParenMulti, .payload2 = 12, .startByte = 4, .lenBytes = 36 },
                
                (Node){ .tp = nodIfClause, .payload1 = 4, .payload2 = 5, .startByte = 8, .lenBytes = 12 },
                (Node){ .tp = nodExpr, .payload2 = 3, .startByte = 8, .lenBytes = 5 },
                (Node){ .tp = nodCall, .payload1 = opTGreaterThan + M, .payload2 = 2, .startByte = 8, .lenBytes = 1 }, // >
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 10, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 12, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 11, .startByte = 18, .lenBytes = 2 },
                
                (Node){ .tp = nodIfClause, .payload1 = 4, .payload2 = 5, .startByte = 28, .lenBytes = 11 },
                (Node){ .tp = nodExpr, .payload2 = 3, .startByte = 28, .lenBytes = 6 },
                (Node){ .tp = nodCall, .payload1 = opTEquality + M, .payload2 = 2, .startByte = 28, .lenBytes = 2 }, // ==
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 31, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 33, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 4, .startByte = 38, .lenBytes = 1 }
            }),
            ((BindingImport[]) {})
        ),
        createTest(
            s("If with elseif and else"),
            s("x = (if > 5 3  => 11\n"
              "        == 5 3 =>  4\n"
              "       else      100)"),
            ((Node[]) {
                (Node){ .tp = nodAssignment, .payload2 = 16, .startByte = 0, .lenBytes = 63 },
                (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 }, // x
                
                (Node){ .tp = nodIf, .payload1 = slParenMulti, .payload2 = 14, .startByte = 4, .lenBytes = 59 },
                
                (Node){ .tp = nodIfClause, .payload1 = 4, .payload2 = 5, .startByte = 8, .lenBytes = 12 },
                (Node){ .tp = nodExpr, .payload2 = 3, .startByte = 8, .lenBytes = 5 },
                (Node){ .tp = nodCall, .payload1 = opTGreaterThan + M, .payload2 = 2, .startByte = 8, .lenBytes = 1 }, // >
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 10, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 12, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 11, .startByte = 18, .lenBytes = 2 },
                
                (Node){ .tp = nodIfClause, .payload1 = 4, .payload2 = 5, .startByte = 29, .lenBytes = 12 },
                (Node){ .tp = nodExpr, .payload2 = 3, .startByte = 29, .lenBytes = 6 },
                (Node){ .tp = nodCall, .payload1 = opTEquality + M, .payload2 = 2, .startByte = 29, .lenBytes = 2 }, // ==
                (Node){ .tp = nodInt, .payload2 = 5, .startByte = 32, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 34, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 4, .startByte = 40, .lenBytes = 1 },
                
                (Node){ .tp = nodElse, .payload2 = 1, .startByte = 59, .lenBytes = 3 },
                (Node){ .tp = nodInt, .payload2 = 100, .startByte = 59, .lenBytes = 3 },
            }),
            ((BindingImport[]) {})
        ),

        //~ (ParserTest) { 
            //~ .name = str("Simple if 3", a),
            //~ .input = str("x = false .(if !x .88)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("If-else 1", a),
            //~ .input = str("x = false .(if !x .88 .1)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },        
        //~ (ParserTest) { 
            //~ .name = str("If-else 2", a),
            //~ .input = str("x = 10\n"
                              //~ ".(if x > 8 .88\n"
                              //~ "     x > 7 .77\n"
                              //~ "           .1\n"
                              //~ ")"                              
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ }         
    }));
}


//~ ParserTestSet* loopTest(Arena* a) {
    //~ return createTestSet(str("Functions test set", a), 1, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple loop 1", a),
            //~ .input = str("(if true .6)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ }
    //~ );
//~ }

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

    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
