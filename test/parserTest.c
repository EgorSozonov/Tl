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

/** Must agree in order with node types in ParserConstants.h */
const char* nodeNames[] = {
    "Int", "Float", "Bool", "String", "_", "DocComment", 
    "id", "func", "binding", "type", "@annot", "and", "or", 
    "(:)", "expr", "accessor", "assign", "reAssign", "mutation", "whereClause",
    "alias", "assert", "assertDbg", "await", "break", "catch", "continue", "defer", "embed", "export",
    "exposePriv", "fnDef", "interface", "lambda", "lam1", "lam2", "lam3", "package", "return", "struct",
    "try", "yield", "if", "ifEq", "ifPr", "ifClause", "impl", "match",
    "loop", "mut"
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
        if (nodeType == nodId || nodeType == nodFunc || nodeType == nodBinding || nodeType == nodBinding
         || nodeType == nodFnDef) {
            Int actualBinding = (nodes[i].payload1 < 70000000) 
                ? (nodes[i].payload1 + countOperators) 
                : (nodes[i].payload1 - 70000000);
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
    }    
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
              ".second = x"  
            ),
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                    (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 },     // x
                    (Node){ .tp = nodInt,  .payload2 = 12, .startByte = 4, .lenBytes = 2 },
                    (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 8, .lenBytes = 10 },
                    (Node){ .tp = nodBinding, .payload1 = 1, .startByte = 8, .lenBytes = 6 }, // second
                    (Node){ .tp = nodId, .payload1 = 0, .payload2 = 0, .startByte = 17, .lenBytes = 1 }
            }),
            ((BindingImport[]) {})
        )//,
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
            s("x = 10,foo 2 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 14 },
                    // " + 1" because the first binding is taken up by the "imported" function, "foo"
                    (Node){ .tp = nodBinding, .payload1 = 1, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 10 },
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 4, .lenBytes = 2 },
                    (Node){ .tp = nodInt, .payload2 = 2,   .startByte = 11, .lenBytes = 1 },
                    (Node){ .tp = nodInt, .payload2 = 3,   .startByte = 13, .lenBytes = 1 },
                    (Node){ .tp = nodFunc, .payload1 = 0, .payload2 = 3, .startByte = 6, .lenBytes = 4 }
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), 
                                                 .binding = (Binding){.flavor = bndCallable }
            }})
        ),      
        createTest(
            s("Nested function call 1"), 
            s("x = 10,foo (,bar) 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 19 },                    
                    (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 15 },
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 4, .lenBytes = 2 },                    
                    (Node){ .tp = nodFunc, .payload1 = 1,    .startByte = 12, .lenBytes = 4 },                    
                    (Node){ .tp = nodInt, .payload2 = 3,   .startByte = 18, .lenBytes = 1 },
                    (Node){ .tp = nodFunc, .payload1 = 0, .payload2 = 3, .startByte = 6, .lenBytes = 4 }
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), 
                                     .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("bar"), 
                                     .binding = (Binding){.flavor = bndCallable }}
            })
        ),
        createTest(
            s("Nested function call 2"), 
            s("x = 10,foo (,barr 3)"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 7, .startByte = 0, .lenBytes = 20 },                    
                    (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 5, .startByte = 4, .lenBytes = 16 },
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 4, .lenBytes = 2 }, 
                                       
                    (Node){ .tp = nodExpr,  .payload2 = 2, .startByte = 12, .lenBytes = 7 },
                    (Node){ .tp = nodInt,   .payload2 = 3, .startByte = 18, .lenBytes = 1 },
                    (Node){ .tp = nodFunc, .payload1 = 1, .payload2 = 1, .startByte = 12, .lenBytes = 5 },  // barr              
                    
                    (Node){ .tp = nodFunc, .payload1 = 0, .payload2 = 2, .startByte = 6, .lenBytes = 4 }    // foo
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("barr"), .binding = (Binding){.flavor = bndCallable }}
            })
        ),
        createTest(
            s("Double function call"), 
            s("x = 1 ,foo ,buzz 2 3 4"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .payload2 = 8, .startByte = 0, .lenBytes = 22 },
                (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 0, .lenBytes = 1 }, // x
                (Node){ .tp = nodExpr,  .payload2 = 6, .startByte = 4, .lenBytes = 18 },
                (Node){ .tp = nodInt, .payload2 = 1, .startByte = 4, .lenBytes = 1 }, 
                (Node){ .tp = nodFunc, .payload1 = 0, .payload2 = 1, .startByte = 6, .lenBytes = 4 },  // .foo
                (Node){ .tp = nodInt, .payload2 = 2, .startByte = 17, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 19, .lenBytes = 1 },
                (Node){ .tp = nodInt, .payload2 = 4, .startByte = 21, .lenBytes = 1 },
                (Node){ .tp = nodFunc, .payload1 = 1, .payload2 = 4, .startByte = 11, .lenBytes = 5 } // .buzz
                
            })),
            ((BindingImport[]) {(BindingImport){ .name = s("foo"), .binding = (Binding){.flavor = bndCallable }},
                                (BindingImport){ .name = s("buzz"), .binding = (Binding){.flavor = bndCallable }}
            })
        ),
        createTest(
            s("Operator precedence simple"), 
            s("x = 1 + 2*3"),
            (((Node[]) {
                (Node){ .tp = nodAssignment, .payload2 = 7, .startByte = 0, .lenBytes = 11 },
                (Node){ .tp = nodBinding, .payload1 = 0, .startByte = 0, .lenBytes = 1 }, // x
                (Node){ .tp = nodExpr,  .payload2 = 5, .startByte = 4, .lenBytes = 7 },
                (Node){ .tp = nodInt, .payload2 = 1, .startByte = 4, .lenBytes = 1 },  
                (Node){ .tp = nodInt, .payload2 = 2, .startByte = 8, .lenBytes = 1 },   
                (Node){ .tp = nodInt, .payload2 = 3, .startByte = 10, .lenBytes = 1 },  
                (Node){ .tp = nodFunc, .payload1 = opTTimes + 70000000, .payload2 = 2, .startByte = 9, .lenBytes = 1 }, // * 
                (Node){ .tp = nodFunc, .payload1 = opTPlus + 70000000, .payload2 = 2, .startByte = 6, .lenBytes = 1 }  // +   
                
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
            s("Simple function definition"),
            s("fn newFn Int(x Int y Float)(: a = x )"),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .payload1 = 2, .payload2 = 7, .startByte = 0, .lenBytes = 37 },
                (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 3, .lenBytes = 5 }, // newFn
                (Node){ .tp = nodScope, .payload2 = 5, .startByte = 13, .lenBytes = 24 },
                (Node){ .tp = nodBinding, .payload1 = 3, .startByte = 13, .lenBytes = 1 }, // param x
                (Node){ .tp = nodBinding, .payload1 = 4, .startByte = 19, .lenBytes = 1 },  // param y
                (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 30, .lenBytes = 5 },  // param y
                (Node){ .tp = nodBinding, .payload1 = 5, .startByte = 30, .lenBytes = 1 },  // local a
                (Node){ .tp = nodId, .payload1 = 3, .payload2 = 2, .startByte = 34, .lenBytes = 1 }  // x                
            }),
            ((BindingImport[]) {})
        ),
        createTest(
            s("Simple function definition 2"),
            s("fn newFn Int(x Int y Float)(: a = x.\n"
              "return a\n"
              ")"
            ),
            ((Node[]) {
                (Node){ .tp = nodFnDef, .payload1 = 2, .payload2 = 9, .startByte = 0, .lenBytes = 47 },
                (Node){ .tp = nodBinding, .payload1 = 2, .startByte = 3, .lenBytes = 5 }, // newFn
                (Node){ .tp = nodScope, .payload2 = 7, .startByte = 13, .lenBytes = 34 },
                (Node){ .tp = nodBinding, .payload1 = 3, .startByte = 13, .lenBytes = 1 }, // param x
                (Node){ .tp = nodBinding, .payload1 = 4, .startByte = 19, .lenBytes = 1 },  // param y
                (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 30, .lenBytes = 5 },  // param y
                (Node){ .tp = nodBinding, .payload1 = 5, .startByte = 30, .lenBytes = 1 },  // local a
                (Node){ .tp = nodId, .payload1 = 3, .payload2 = 2, .startByte = 34, .lenBytes = 1 },  // x
                (Node){ .tp = nodReturn, .payload2 = 1, .startByte = 37, .lenBytes = 8 },
                (Node){ .tp = nodId, .payload1 = 5, .payload2 = 5, .startByte = 44, .lenBytes = 1 } // a
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


//~ ParserTestSet* ifTest(Arena* a) {
    //~ return createTestSet(str("Functions test set", a), 5, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple if 1", a),
            //~ .input = str("(if true .6)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Simple if 2", a),
            //~ .input = str("x = false\n"
                              //~ ".(if x .88)"                              
            //~ , a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
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
    //~ );
//~ }

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
    LanguageDefinition* lang = buildLanguageDefinitions(a);
    ParserDefinition* parsDef = buildParserDefinitions(lang, a);

    int countPassed = 0;
    int countTests = 0;
    
    runATestSet(&assignmentTests, &countPassed, &countTests, lang, parsDef, a);
    runATestSet(&expressionTests, &countPassed, &countTests, lang, parsDef, a);
    runATestSet(&functionTests, &countPassed, &countTests, lang, parsDef, a);


    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
