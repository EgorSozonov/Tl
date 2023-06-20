#include "../source/utils/aliases.h"
#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/compiler/compiler.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>


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

    Compiler* pr = createCompiler(lx, a);
    pr->entBindingZero = pr->entNext;
    pr->entOverloadZero = pr->overlCNext;
    Int firstTypeId = pr->typeNext;
    // Float(Int Float)
    addFunctionType(2, (Int[]){typFloat, typInt, typFloat}, pr);
    
    Int secondTypeId = pr->typeNext;
    // String(Int Float) - String is the return type
    addFunctionType(2, (Int[]){typString, typFloat, typInt}, pr);

    String* foo = s("foo");
    importEntities(((EntityImport[]) {
        (EntityImport){ .name = foo, .entity = (Entity){.typeId = firstTypeId }},
        (EntityImport){ .name = foo, .entity = (Entity){.typeId = firstTypeId }}
        }), 2, pr);
    importOverloads(((OverloadImport[]){
        (OverloadImport){.name = foo, .count = 2}
    }), 1, pr);

    parseWithCompiler(lx, pr, a);
    if (pr->wasError) {
        print("Compiler error")
        printString(pr->errMsg);
        return 0;
    }

    Int typerResult = typeCheckResolveExpr(2, pr);
    print("the result from the typer is %d", typerResult);
    
    return 0;
}
