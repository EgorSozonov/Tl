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


static Lexer* buildLexer(int totalTokens, Arena *a, /* Tokens */ ...) {
    Lexer* result = createLexer(&empty, a);
    if (result == NULL) return result;
    
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, a);
    
    for (int i = 0; i < totalTokens; i++) {
        add(va_arg(tokens, Token), result);
    }
    
    va_end(tokens);
    return result;
}


Lexer* buildLexerWithError(String* errMsg, int totalTokens, Arena *a, /* Tokens */ ...) {
    Lexer* result = allocateOnArena(sizeof(Lexer), a);
    result->wasError = true;
    result->errMsg = errMsg;
    result->totalTokens = totalTokens;
    
    va_list tokens;
    va_start (tokens, a);
    
    result->tokens = allocateOnArena(totalTokens*sizeof(Token), a);
    if (result == NULL) return result;
    
    for (int i = 0; i < totalTokens; i++) {
        result->tokens[i] = va_arg(tokens, Token);
    }
    
    va_end(tokens);
    return result;
}


LexerTestSet* createTestSet(String* name, int count, Arena *a, ...) {
    LexerTestSet* result = allocateOnArena(sizeof(LexerTestSet), a);
    result->name = name;
    result->totalTests = count;
    
    va_list tests;
    va_start (tests, a);
    
    result->tests = allocateOnArena(count*sizeof(LexerTest), a);
    if (result->tests == NULL) return result;
    
    for (int i = 0; i < count; i++) {
        result->tests[i] = va_arg(tests, LexerTest);
    }
    
    va_end(tests);
    return result;
}

/** Returns -2 if lexers are equal, -1 if they differ in errorfulness, and the index of the first differing token otherwise */
int equalityLexer(Lexer a, Lexer b) {
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
    Parser* result = lexicallyAnalyze(test.input, lang, a);
        
    int equalityStatus = equalityLexer(*result, *test.expectedOutput);
    if (equalityStatus == -2) {
        (*countPassed)++;
        return;
    } else if (equalityStatus == -1) {
        printf("\n\nERROR IN [");
        printStringNoLn(test.name);
        printf("]\nError msg: ");
        printString(result->errMsg);
        printf("\nBut was expected: ");
        printString(test.expectedOutput->errMsg);

    } else {
        printf("ERROR IN ");
        printString(test.name);
        printf("On token %d\n", equalityStatus);
        printLexer(result);
    }
}


ParserTestSet* expressionTests(Arena* a) {
    return createTestSet(allocLit("Expression test set", a), 21, a,
        (ParserTest) { 
            .name = allocLit("Simple function call", a),
            .input = allocLit("10:foo 2 3", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Double function call", a),
            .input = allocLit("10:foo 2 3", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Triple function call", a),
            .input = allocLit("c:foo b a :bar 5 :baz 7.2", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator precedence simple", a),
            .input = allocLit("a + b*c", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator precedence simple 2", a),
            .input = allocLit("a + b*c", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator precedence simple 3", a),
            .input = allocLit("a + b*c", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator prefix 1", a),
            .input = allocLit("a + b*c", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator prefix 2", a),
            .input = allocLit("a + b*c", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },       
        (ParserTest) { 
            .name = allocLit("Operator prefix 3", a),
            .input = allocLit("a + b*c", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator airthmetic 1", a),
            .input = allocLit("(a + (b - c % 2)^11)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator airthmetic 2", a),
            .input = allocLit("a + -5", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },       
        (ParserTest) { 
            .name = allocLit("Operator airthmetic 3", a),
            .input = allocLit("a + !(-5)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Operator airthmetic 4", a),
            .input = allocLit("a + !!(-3)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },             
        (ParserTest) { 
            .name = allocLit("Operator arity error", a),
            .input = allocLit("a + 5 100", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },    
        (ParserTest) { 
            .name = allocLit("Single-item expr 1", a),
            .input = allocLit("a + 5 100", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Single-item expr 2", a),
            .input = allocLit("a + 5 100", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },    
        (ParserTest) { 
            .name = allocLit("Unknown function arity", a),
            .input = allocLit("a + 5 100", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },        
        (ParserTest) { 
            .name = allocLit("Func-expr 1", a),
            .input = allocLit("(4:(a:g)):f", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },            
        (ParserTest) { 
            .name = allocLit("Partial application?", a),
            .input = allocLit("5:(1 + )", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },          
        (ParserTest) { 
            .name = allocLit("Partial application 2?", a),
            .input = allocLit("5:(3*a +)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },    
        (ParserTest) { 
            .name = allocLit("Partial application error?", a),
            .input = allocLit("5:(3+ a*)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        }
    );
}


ParserTestSet* assignmentTests(Arena* a) {
    return createTestSet(allocLit("Assignment test set", a), 2, a,
        (ParserTest) { 
            .name = allocLit("Simple assignment 1", a),
            .input = allocLit("a = 1 + 5", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Simple assignment 2", a),
            .input = allocLit("a += 9", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        }
    );
}


ParserTestSet* scopeTests(Arena* a) {
    return createTestSet(allocLit("Scopes test set", a), 4, a,
        (ParserTest) { 
            .name = allocLit("Simple scope 1", a),
            .input = allocLit("x = 5"
                              "x:print"
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Simple scope 2", a),
            .input = allocLit("x = 123"
                              "yy = x * 10"  
                              "yy:print"
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Scope with curly braces 1", a),
            .input = allocLit("x = 123\n"
                              "{\n"
                              "yy = x * 10\n"  
                              ".yy:print\n"
                              "}"
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },   
        (ParserTest) { 
            .name = allocLit("Scope inside statement error", a),
            .input = allocLit("x = 123 +(\n"
                              "{\n"
                              "yy = x * 10\n"  
                              ".yy:print\n"
                              "})"
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        }             
    );
}


ParserTestSet* typesTest(Arena* a) {
    return createTestSet(allocLit("Types test set", a), 2, a,
        (ParserTest) { 
            .name = allocLit("Simple type 1", a),
            .input = allocLit("Int"
                              "x:print"
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Simple type 2", a),
            .input = allocLit("List Int"
                              "yy = x * 10"  
                              "yy:print"
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        }           
    );
}

ParserTestSet* functionsTest(Arena* a) {
    return createTestSet(allocLit("Functions test set", a), 5, a,
        (ParserTest) { 
            .name = allocLit("Simple function def", a),
            .input = allocLit("fn newFn Int(x Int y Float)(return x + y)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Nested function def", a),
            .input = allocLit("fn foo Int(x Int y Int) {\n"
                              "z = x*y\n"
                              ".return (z + x):inner 5 2\n"
                              "fn inner Int(a Int b Int c Int)(return a - 2*b + 3*c)"
                              "}\n"
                              
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },    
        (ParserTest) { 
            .name = allocLit("Function def error 1", a),
            .input = allocLit("fn newFn Int(x Int newFn Float)(return x + y)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Function def error 3", a),
            .input = allocLit("fn foo Int(x Int x Int) {\n"
                              "z = x*y\n"
                              ".return (z + x):inner 5 2\n"
                              "fn inner Int(a Int b Int c Int)(return a - 2*b + 3*c)"
                              "}\n"
                              
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },        
        (ParserTest) { 
            .name = allocLit("Function def error 3", a),
            .input = allocLit("fn foo Int(x Int y Int)"
                              
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        } 
    );
}


ParserTestSet* ifTest(Arena* a) {
    return createTestSet(allocLit("Functions test set", a), 5, a,
        (ParserTest) { 
            .name = allocLit("Simple if 1", a),
            .input = allocLit("(if true .6)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("Simple if 2", a),
            .input = allocLit("x = false\n"
                              ".(if x .88)"                              
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },    
        (ParserTest) { 
            .name = allocLit("Simple if 3", a),
            .input = allocLit("x = false .(if !x .88)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },
        (ParserTest) { 
            .name = allocLit("If-else 1", a),
            .input = allocLit("x = false .(if !x .88 .1)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        },        
        (ParserTest) { 
            .name = allocLit("If-else 2", a),
            .input = allocLit("x = 10\n"
                              ".(if x > 8 .88\n"
                              "     x > 7 .77\n"
                              "           .1\n"
                              ")"                              
            , a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        } 
    );
}

ParserTestSet* loopTest(Arena* a) {
    return createTestSet(allocLit("Functions test set", a), 1, a,
        (ParserTest) { 
            .name = allocLit("Simple loop 1", a),
            .input = allocLit("(if true .6)", a),
            .imports = {(Import){"foo", 3}},
            .expectedOutput = buildParser(3, a, 
                (Node){ .tp = tokStmt, .payload2 = 2, .startByte = 0, .lenBytes = 8 },
                (Node){ .tp = tokWord, .startByte = 0, .lenBytes = 4 },
                (Node){ .tp = tokWord, .startByte = 5, .lenBytes = 3 }
            )
        }
    );
}

void runATestSet(ParserTestSet* (*testGenerator)(Arena*), int* countPassed, int* countTests, LanguageDefinition* lang, Arena* a) {
    ParserTestSet* testSet = (testGenerator)(a);
    for (int j = 0; j < testSet->totalTests; j++) {
        ParserTest test = testSet->tests[j];
        runParserTest(test, countPassed, countTests, lang, a);
    }
}


int main() {
    printf("----------------------------\n");
    printf("Parser test\n");
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
