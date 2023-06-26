#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../source/tl.internal.h"
#include "tlTest.h"


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


void printParser(Compiler* a, Arena* ar) {
    if (a->wasError) {
        printf("Error: ");
        printString(a->errMsg);
    }
    Int indent = 0;
    Stackint32_t* sentinels = createStackint32_t(16, ar);
    for (int i = 0; i < a->nextInd; i++) {
        Node nod = a->nodes[i];
        for (int m = sentinels->length - 1; m > -1 && sentinels->content[m] == i; m--) {
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

int main() {
    Arena *a = mkArena();
    LanguageDefinition* langDef = buildLanguageDefinitions(a);
    ParserDefinition* parsDef = buildParserDefinitions(langDef, a);
    
    Lexer* lx = lexicallyAnalyze(s("x = foo 5 1.2"), langDef, a);    
    if (lx->wasError) {
        print("lexer error")
        printString(lx->errMsg);
        return 0;
    }
    printLexer(lx);

    Compiler* cm = createCompiler(lx, a);
    cm->entBindingZero = cm->entNext;
    cm->entOverloadZero = cm->overlCNext;
    Int firstTypeId = cm->types.length;
    
    // Float(Int Float)
    addFunctionType(2, (Int[]){tokFloat, tokInt, tokFloat}, cm);
    
    Int secondTypeId = cm->types.length;
    // String(Int Float) - String is the return type
    addFunctionType(2, (Int[]){tokString, tokFloat, tokInt}, cm);
    
    String* foo = s("foo");
    importEntities(((EntityImport[]) {
        (EntityImport){ .name = foo, .entity = (Entity){.typeId = firstTypeId }},
        (EntityImport){ .name = foo, .entity = (Entity){.typeId = secondTypeId }}
        }), 2, cm);

    parseWithCompiler(lx, cm, a);
    printParser(cm, a);
    if (cm->wasError) {
        print("Compiler error")
        printString(cm->errMsg);
        return 0;
    }
    createOverloads(cm);
    Int typerResult = typeCheckResolveExpr(2, cm);
    print("the result from the typer is %d", typerResult);
    
    return 0;
}
