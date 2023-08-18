#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>
#include "../source/tl.internal.h"
#include "tlTest.h"


extern jmp_buf excBuf;

#define in(x) prepareInput(x, a)
#define S   70000000 // A constant larger than the largest allowed file size. Separates parsed names from others
#define I  140000000 // The base index for imported entities/overloads
#define S2 210000000 // A constant larger than the largest allowed file size. Separates parsed entities from others
#define O  280000000 // The base index for operators

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

private Int transformBindingEntityId(Int inp, Compiler* pr) {
    if (inp < S) { // parsed stuff
        return inp + pr->countNonparsedEntities;
    } else if (inp < S2) {
        return inp - I + pr->countOperatorEntities;
    } else {
        return inp - O;
    }
}

private void buildParser0(Compiler* cm, int countNodes, Arr(Node) nodes) {
    for (Int i = 0; i < countNodes; i++) {
        untt nodeType = nodes[i].tp;
        // All the node types which contain bindingIds
        if (nodeType == nodId || nodeType == nodCall || nodeType == nodBinding || nodeType == nodBinding) {
            pushInnodes((Node){ .tp = nodeType, .pl1 = transformBindingEntityId(nodes[i].pl1, cm),
                                .pl2 = nodes[i].pl2, 
                                .startBt = nodes[i].startBt, .lenBts = nodes[i].lenBts }, cm);
        } else {
            pushInnodes(nodes[i], cm);
        }
    }
}

#define buildParser(cm, nodes) buildParser0(cm, sizeof(nodes)/sizeof(Node), nodes)

void typerTest1(Compiler* proto, Arena* a) {
    Compiler* cm = lexicallyAnalyze(str("fn([U/2 V] lst U(Int V))", a), proto, a);
    initializeParser(cm, proto, a);
    buildParser(cm, ((Node[]){ // 
        (Node){ .tp = nodFnDef, .pl1 = slParenMulti, .pl2 = 9,        .lenBts = 24 },
        (Node){ .tp = nodStmt,               .pl2 = 8, .startBt = 3, .lenBts = 20 },
        (Node){ .tp = nodBrackets,           .pl2 = 3, .startBt = 3, .lenBts = 7 },
        (Node){ .tp = nodTypeName, .pl1 = 1, .pl2 = 0, .startBt = 4,     .lenBts = 1 },
        (Node){ .tp = nodInt,                .pl2 = 2, .startBt = 6,     .lenBts = 1 },
        (Node){ .tp = TypeName,           .pl2 = 1, .startBt = 8,     .lenBts = 1 },

        (Node){ .tp = tokWord,               .pl2 = 2, .startBt = 11, .lenBts = 3 },

        (Node){ .tp = tokTypeCall, .pl1 = 0, .pl2 = 2, .startBt = 15, .lenBts = 8 },
        (Node){ .tp = tokTypeName,           .pl2 = strInt + S, .startBt = 17, .lenBts = 3 },
        (Node){ .tp = tokTypeName,           .pl2 = 1, .startBt = 21, .lenBts = 1 }
    }));
    lx->i = 0;
    Int typeName = parseTypeName(lx->tokens->content[0], lx->tokens, lx);
    print("typeName %d", typeTest1(proto))
}

int main() {
    printf("----------------------------\n");
    printf("--  TYPER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    Compiler* proto = createProtoCompiler(a);

    typerTest1(proto, a);
    deleteArena(a);
}
