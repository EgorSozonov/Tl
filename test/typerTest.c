#include "../source/utils/aliases.h"
#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/structures/stack.h"
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

    Compiler* pr = createParser(lx, a);
    pr->entBindingZero = pr->entNext;
    pr->entOverloadZero = pr->overlNext;
    Int firstTypeId = pr->typeNext;
    addFunctionType(2, (Int[]){typFloat, typInt, typFloat}, pr);
    
    Int secondTypeId = pr->typeNext;
    addFunctionType(2, (Int[]){typString, typFloat, typInt}, pr);

    String* foo = s("foo");
    importEntities(((EntityImport[]) {
        (EntityImport){ .name = foo, .entity = (Entity){.typeId = firstTypeId }},
        (EntityImport){ .name = foo, .entity = (Entity){.typeId = firstTypeId }}
        }), 2, pr);
    importOverloads(((OverloadImport[]){
        (OverloadImport){.name = foo, .count = 2}
    }), 1, pr);

    parseWithParser(lx, pr, a);
    if (pr->wasError) {
        print("Compiler error")
        printString(pr->errMsg);
        return 0;
    }

    Int typerResult = typeResolveExpr(2, pr);
    print("the result from the typer is %d", typerResult);
    
    return 0;
}
