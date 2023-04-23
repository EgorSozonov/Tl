#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/stack.h"
#include "../source/compiler/lexer.h"
#include "../source/compiler/lexerConstants.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    String* name;
    String* input;
    Parser* expectedOutput;
} ParserTest;


typedef struct {
    String* name;
    int totalTests;
    ParserTest* tests;
} ParserTestSet;

/** Must agree in order with token types in ParserConstants.h */
const char* nodeNames[] = {
    "Int", "Float", "Bool", "String", "_", "DocComment", 
    "word", ".word", "@word", ":func", "operator", ";", 
    "{}", ".", "()", "[]", "accessor", "funcExpr", "assignment", 
    "alias", "assert", "assertDbg", "await", "catch", "continue", "continueIf", "embed", "export",
    "finally", "fn", "if", "ifEq", "ifPr", "impl", "interface", "lambda", "lam1", "lam2", "lam3", 
    "loop", "match", "mut", "nodestruct", "return", "returnIf", "struct", "try", "type", "yield"
};


private Parser* buildParser0(Arena *a, int totalNodes, Arr(Node) nodes) {
    Parser* result = createParser(&empty, a);
    if (result == NULL) return result;
    
    result->totalNodes = totalNodes;
        
    for (int i = 0; i < totalNodes; i++) {
        add(nodes[i], result);
    }
    
    return result;
}

// Macro wrapper to get array length
#define buildParser(a, nodes) buildParser0(a, sizeof(nodes)/sizeof(Node), nodes)

private Parser* buildParserWithError0(String* errMsg, Arena *a, int totalNodes, Arr(Node) nodes) {
    Parser* result = allocateOnArena(sizeof(Parser), a);
    result->wasError = true;
    result->errMsg = errMsg;
    result->totalNodes = totalNodes;    
    
    result->tokens = allocateOnArena(totalTokens*sizeof(Token), a);
    if (result == NULL) return result;
    
    for (int i = 0; i < totalNodes; i++) {
        result->nodes[i] = nodes[i];
    }
    
    return result;
}

#define buildParserWithError(msg, a, nodes) buildParserWithError0(msg, a, sizeof(nodes)/sizeof(Node), nodes)


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

/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
int equalityParser(Parser a, Parser b) {
    if (a.wasError != b.wasError || (!endsWith(a.errMsg, b.errMsg))) {
        return -1;
    }
    int commonLength = a.totalTokens < b.totalTokens ? a.totalTokens : b.totalTokens;
    int i = 0;
    for (; i < commonLength; i++) {
        Token tokA = a.tokens[i];
        Token tokB = b.tokens[i];
        if (tokA.tp != tokB.tp || tokA.lenBytes != tokB.lenBytes || tokA.startByte != tokB.startByte 
            || tokA.payload1 != tokB.payload1 || tokA.payload2 != tokB.payload2) {
            printf("\n\nUNEQUAL RESULTS\n", i);
            if (tokA.tp != tokB.tp) {
                printf("Diff in tp, %s but was expected %s\n", nodeNames[tokA.tp], nodeNames[tokB.tp]);
            }
            if (tokA.lenBytes != tokB.lenBytes) {
                printf("Diff in lenBytes, %d but was expected %d\n", tokA.lenBytes, tokB.lenBytes);
            }
            if (tokA.startByte != tokB.startByte) {
                printf("Diff in startByte, %d but was expected %d\n", tokA.startByte, tokB.startByte);
            }
            if (tokA.payload1 != tokB.payload1) {
                printf("Diff in payload1, %d but was expected %d\n", tokA.payload1, tokB.payload1);
            }
            if (tokA.payload2 != tokB.payload2) {
                printf("Diff in payload2, %d but was expected %d\n", tokA.payload2, tokB.payload2);
            }            
            return i;
        }
    }
    return (a.totalTokens == b.totalTokens) ? -2 : i;        
}


void printParser(Parser* a) {
    if (a->wasError) {
        printf("Error: ");
        printString(a->errMsg);
    }
    for (int i = 0; i < a->totalNodes; i++) {
        Token tok = a->tokens[i];
        if (tok.payload1 != 0 || tok.payload2 != 0) {
            printf("%s %d %d [%d; %d]\n", nodeNames[tok.tp], tok.payload1, tok.payload2, tok.startByte, tok.lenBytes);
        } else {
            printf("%s [%d; %d]\n", nodeNames[tok.tp], tok.startByte, tok.lenBytes);
        }
        
    }
}

/** Runs a single lexer test and prints err msg to stdout in case of failure. Returns error code */
void runParserTest(ParserTest test, int* countPassed, int* countTests, LanguageDefinition* lang, Arena *a) {
    (*countTests)++;
    Lexer* lr = lexicallyAnalyze(test.input, lang, a);
    if (lr->wasError) {
        print("Lexer was not without error");
        printLexer(lr);
        return;
    }    
    Parser* pr = parse(lr, lang, a);
        
    int equalityStatus = equalityParser(*result, *test.expectedOutput);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(lr->errMsg);
        printf("\nBut was expected: ");
        printString(test.expectedOutput->errMsg);

    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On node %d\n", equalityStatus);
        printLexer(result);
    }
}


ParserTestSet* expressionTests(Arena* a) {
    return createTestSet(str("Expression test set", a), a, ((Parser[]){
        (ParserTest) { 
            .name = str("Simple function call", a),
            .input = str("10,foo 2 3", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(a, ((Node[]) {
                (Node){ .tp = nodFnDef, .payload2 = 6, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = nodScope, .payload2 = 5, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = nodExpr,  .payload2 = 4, .startByte = 5, .lenBytes = 3 },
                (Node){ .tp = nodInt, .payload2 = 10, .startByte = 0, .lenBytes = 2 },
                (Node){ .tp = nodInt, .startByte = 7, .lenBytes = 1 },
                (Node){ .tp = nodInt, .startByte = 9, .lenBytes = 1 },
                (Node){ .tp = nodFunc, .payload1 = 3, .startByte = 3, .lenBytes = 3 }
            }))
        },
        (ParserTest) { 
            .name = str("Double function call", a),
            .input = str("a,foo ,buzz b c d", a),
            .imports = {(Import){"foo", 3}, (Import){"buzz", "buzz"},
                (Import){"a", "", -1},(Import){"b", "", -1},(Import){"c", "", -1},(Import){"d", "", -1}},
            .expectedOutput = buildParser(a, ((Node[]) {
                (Node){ .tp = nodFnDef, .payload2 = 8, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = nodScope, .payload2 = 7, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = nodExpr,  .payload2 = 6, .startByte = 5, .lenBytes = 3 },
                (Node){ .tp = nodId, .payload2 = 2, .startByte = 0, .lenBytes = 2 },
                (Node){ .tp = nodId, .payload2 = 3, .startByte = 7, .lenBytes = 1 },
                (Node){ .tp = nodId, .payload2 = 4, .startByte = 9, .lenBytes = 1 },
                (Node){ .tp = nodId, .payload2 = 5, .startByte = 7, .lenBytes = 1 },
                (Node){ .tp = nodFunc, .payload1 = 1, .payload2 = 1, .startByte = 9, .lenBytes = 1 },     // buzz
                (Node){ .tp = nodFunc, .payload1 = 2, .startByte = 3, .lenBytes = 3 }  // foo
            }))
        }//,
        //~ (ParserTest) { 
            //~ .name = str("Triple function call", a),
            //~ .input = str("c:foo b a :bar 5 :baz 7.2", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(a, ((Node[]) {
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator precedence simple", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator precedence simple 2", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator precedence simple 3", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 1", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 2", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ }))
        //~ },       
        //~ (ParserTest) { 
            //~ .name = str("Operator prefix 3", a),
            //~ .input = str("a + b*c", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 1", a),
            //~ .input = str("(a + (b - c % 2)^11)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 2", a),
            //~ .input = str("a + -5", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },       
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 3", a),
            //~ .input = str("a + !(-5)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Operator airthmetic 4", a),
            //~ .input = str("a + !!(-3)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },             
        //~ (ParserTest) { 
            //~ .name = str("Operator arity error", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Single-item expr 1", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Single-item expr 2", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Unknown function arity", a),
            //~ .input = str("a + 5 100", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },        
        //~ (ParserTest) { 
            //~ .name = str("Func-expr 1", a),
            //~ .input = str("(4:(a:g)):f", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },            
        //~ (ParserTest) { 
            //~ .name = str("Partial application?", a),
            //~ .input = str("5:(1 + )", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },          
        //~ (ParserTest) { 
            //~ .name = str("Partial application 2?", a),
            //~ .input = str("5:(3*a +)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },    
        //~ (ParserTest) { 
            //~ .name = str("Partial application error?", a),
            //~ .input = str("5:(3+ a*)", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ }
    }));
}


//~ ParserTestSet* assignmentTests(Arena* a) {
    //~ return createTestSet(str("Assignment test set", a), 2, a,
        //~ (ParserTest) { 
            //~ .name = str("Simple assignment 1", a),
            //~ .input = str("a = 1 + 5", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
                //~ (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                //~ (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                //~ (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            //~ )
        //~ },
        //~ (ParserTest) { 
            //~ .name = str("Simple assignment 2", a),
            //~ .input = str("a += 9", a),
            //~ .imports = {(Import){"foo", 3}},
            //~ .expectedOutput = buildParser(3, a, 
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

void runATestSet(ParserTestSet* (*testGenerator)(Arena*), int* countPassed, int* countTests, LanguageDefinition* lang, Arena* a) {
    ParserTestSet* testSet = (testGenerator)(a);
    for (int j = 0; j < testSet->totalTests; j++) {
        ParserTest test = testSet->tests[j];
        runParserTest(test, countPassed, countTests, lang, a);
    }
}


int main() {
    printf("----------------------------\n");
    printf("--  PARSER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    LanguageDefinition* lang = buildLanguageDefinitions(a);

    int countPassed = 0;
    int countTests = 0;
    runATestSet(&expressionTests, &countPassed, &countTests, lang, a);


    if (countTests == 0) {
        printf("\nThere were no tests to run!\n");
    } else if (countPassed == countTests) {
        printf("\nAll %d tests passed!\n", countTests);
    } else {
        printf("\nFailed %d tests out of %d!\n", (countTests - countPassed), countTests);
    }
    
    deleteArena(a);
}
