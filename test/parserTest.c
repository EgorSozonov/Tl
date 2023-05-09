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
    "alias", "assert", "assertDbg", "await", "catch", "continue", "continueIf", "embed", "export",
    "finally", "fn", "if", "ifEq", "ifPr", "impl", "interface", "lambda", "lam1", "lam2", "lam3", 
    "loop", "match", "mut", "nodestruct", "return", "returnIf", "struct", "try", "type", "yield"
};

/** A programmatic way of importing bindings into scope, to be used for testing */
private Parser* insertBindings0(Arr(Int) stringIds, Arr(Binding) bindings, Int len, Parser* pr) {
    for (int i = 0; i < len; i++) {
        createBinding(bindings[i], pr);
        pr->activeBindings[i] = i;
    }
    return pr;
}

#define insertBindings(strs, bindings, pr) insertBindings0(strs, bindings, sizeof(strs)/sizeof(Int), pr)


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
 */
private ParserTest createTest0(String* name, String* input, Arr(Node) nodes, Int countNodes,  
                              Arr(Int) stringIds, Arr(Binding) bindings, Int countBindings, 
                              LanguageDefinition* langDef, Arena* a) {
    Lexer* lx = lexicallyAnalyze(input, langDef, a);    
    Parser* initParser = createParser(lx, a);
    Parser* expectedParser = createParser(lx, a);
    if (expectedParser->wasError) {
        return (ParserTest){ .name = name, .input = lx, .initParser = initParser, .expectedOutput = expectedParser };
    }
    for (int i = 0; i < countBindings; i++) {
        Int newBindingId = createBinding(bindings[i], expectedParser);
        createBinding(bindings[i], initParser);
    
        expectedParser->activeBindings[stringIds[i]] = newBindingId;
        initParser->activeBindings[stringIds[i]] = newBindingId;
    }
    
    for (Int i = 0; i < countNodes; i++) {
        addNode(nodes[i], expectedParser);
    }   
    return (ParserTest){ .name = name, .input = lx, .initParser = initParser, .expectedOutput = expectedParser };
}

#define createTest(name, input, nodes, bindingStrs, bindingNodes) createTest0((name), (input), \
    (nodes), sizeof(nodes)/sizeof(Node), bindingStrs, bindingNodes, sizeof(bindingStrs)/sizeof(String), langDef, a)


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
                    (Node){ .tp = nodBinding, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                    (Node){ .tp = nodInt,  .payload2 = 12, .startByte = 4, .lenBytes = 2 }
            }),
            ((Int[]) {}), 
            ((Binding[]) {})
        ),
        createTest(
            s("Double assignment"), 
            s("x = 12\n"
              ".second = x"  
            ),
            ((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 0, .lenBytes = 6 },
                    (Node){ .tp = nodBinding, .payload2 = 1, .startByte = 0, .lenBytes = 1 },
                    (Node){ .tp = nodInt,  .payload2 = 12, .startByte = 4, .lenBytes = 2 },
                    (Node){ .tp = nodAssignment, .payload2 = 2, .startByte = 8, .lenBytes = 10 },
                    (Node){ .tp = nodBinding, .payload2 = 2, .startByte = 8, .lenBytes = 6 },
                    (Node){ .tp = nodId, .payload1 = 1, .payload2 = 0, .startByte = 17, .lenBytes = 1 }
            }),
            ((Int[]) {}), 
            ((Binding[]) {})
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
    print("expression tests");
    return createTestSet(s("Expression test set"), a, ((ParserTest[]){
        createTest(
            s("Simple function call"), 
            s("x = 10,foo 2 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 14 },
                    // " + 1" because the first binding is taken up by the "imported" function, "foo"
                    (Node){ .tp = nodBinding, .payload1 = countOperators + 1, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 10 },
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 4, .lenBytes = 2 },
                    (Node){ .tp = nodInt, .payload2 = 2,   .startByte = 11, .lenBytes = 1 },
                    (Node){ .tp = nodInt, .payload2 = 3,   .startByte = 13, .lenBytes = 1 },
                    (Node){ .tp = nodFunc, .payload1 = 30, .payload2 = 3, .startByte = 6, .lenBytes = 4 }
            })),
            ((Int[]) {1}), 
            ((Binding[]) {(Binding){.flavor = 0, .typeId = 0, }})
        ),      
        createTest(
            s("Nested function call 1"), 
            s("x = 10,foo (,bar) 3"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 6, .startByte = 0, .lenBytes = 19 },                    
                    (Node){ .tp = nodBinding, .payload1 = countOperators + 2, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 15 },
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 4, .lenBytes = 2 },                    
                    (Node){ .tp = nodFunc, .payload1 = 31,    .startByte = 12, .lenBytes = 4 },                    
                    (Node){ .tp = nodInt, .payload2 = 3,   .startByte = 18, .lenBytes = 1 },
                    (Node){ .tp = nodFunc, .payload1 = 30, .payload2 = 3, .startByte = 6, .lenBytes = 4 }
            })),
            ((Int[]) {1, 2}), 
            ((Binding[]) {(Binding){.flavor = bndCallable, .typeId = 0 }, (Binding){.flavor = bndCallable, .typeId = 0 }})
        ),
        createTest(
            s("Nested function call 2"), 
            s("x = 10,foo (,barr 3)"),
            (((Node[]) {
                    (Node){ .tp = nodAssignment, .payload2 = 7, .startByte = 0, .lenBytes = 19 },                    
                    (Node){ .tp = nodBinding, .payload1 = countOperators + 2, .startByte = 0, .lenBytes = 1 }, // x
                    (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 4, .lenBytes = 15 },
                    (Node){ .tp = nodInt, .payload2 = 10,  .startByte = 4, .lenBytes = 2 }, 
                                       
                    (Node){ .tp = nodExpr,  .payload2 = 2, .startByte = 12, .lenBytes = 7 },
                    (Node){ .tp = nodInt,   .payload2 = 3, .startByte = 18, .lenBytes = 1 },
                    (Node){ .tp = nodFunc, .payload1 = 31, .startByte = 12, .lenBytes = 5 },                    
                    
                    (Node){ .tp = nodFunc, .payload1 = 30, .payload2 = 3, .startByte = 6, .lenBytes = 4 }
            })),
            ((Int[]) {1, 2}), 
            ((Binding[]) {(Binding){.flavor = bndCallable, .typeId = 0 }, (Binding){.flavor = bndCallable, .typeId = 0 }})
        )   
    }));
}

        //~ (ParserTest) { 
            //~ .name = str("Double function call", a),
            //~ .input = str("a,foo ,buzz b c d", a),
            //~ .expectedOutput = buildParser(((Node[]) {
                //~ (Node){ .tp = nodFnDef, .payload2 = 8, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = nodScope, .payload2 = 7, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = nodExpr,  .payload2 = 6, .startByte = 5, .lenBytes = 3 },
                //~ (Node){ .tp = nodId, .payload2 = 2, .startByte = 0, .lenBytes = 2 },
                //~ (Node){ .tp = nodId, .payload2 = 3, .startByte = 7, .lenBytes = 1 },
                //~ (Node){ .tp = nodId, .payload2 = 4, .startByte = 9, .lenBytes = 1 },
                //~ (Node){ .tp = nodId, .payload2 = 5, .startByte = 7, .lenBytes = 1 },
                //~ (Node){ .tp = nodFunc, .payload1 = 1, .payload2 = 1, .startByte = 9, .lenBytes = 1 },     // buzz
                //~ (Node){ .tp = nodFunc, .payload1 = 2, .startByte = 3, .lenBytes = 3 }  // foo
            //~ }))
        //~ },
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

//~ ParserTestSet* functionsTest(Arena* a) {
    //~ return createTestSet(str("Functions test set", a), 5, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple function def", a),
            //~ .input = str("fn newFn Int(x Int y Float)(return x + y)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
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
    //~ );
//~ }


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
    
    //runATestSet(&assignmentTests, &countPassed, &countTests, lang, parsDef, a);
    runATestSet(&expressionTests, &countPassed, &countTests, lang, parsDef, a);


    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
