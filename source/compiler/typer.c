#include <stdbool.h>
#include <string.h>
#include "parser.h"
#include "typer.h"
#include "typer.internal.h"

DEFINE_STACK(int32_t)

#define push pushint32_t

Arr(Int) createOverloads(Parser* pr) {
    Int neededCount = 0;
    for (Int i = 0; i < pr->overlNext; i++) {
       neededCount += pr->overloads[i];
    }
    Arr(Int) overloads = allocateOnArena(neededCount*4, pr->a);
    return overloads;
}


Typer* createTyper(Parser* pr) {
    Arena* a = pr->a;
    if (pr->wasError) {
        Typer* result = allocateOnArena(sizeof(Parser), a);
        result->wasError = true;
        return result;
    }
    
    Typer* result = allocateOnArena(sizeof(Typer), a);
    (*result) = (Typer) {
        .expStack = createStackint32_t(16, a),
        .entities = pr->entities, .entNext = pr->entNext, .entCap = pr->entCap, .entOverloadZero = pr->entOverloadZero,
        .entBindingZero = pr->entBindingZero, .overloadCounts = pr->overloads, .overlCountsCount = pr->overlNext,
        .overloads = createOverloads(pr), .overlNext = 0, .overlCap = 64, .nodes = pr->nodes, .nodesCount = pr->nextInd,
        .wasError = false, .errMsg = &empty, .a = a
    };
    return result;
}



Int typeResolveExpr(Parser* pr, Int indExpr, Typer* tr) {
    Node expr = pr->nodes[indExpr];
    Int sentinelNode = indExpr + expr.pl2 + 1;
    for (int i = indExpr + 1; i < sentinelNode; ++i) {
        Node nd = pr->nodes[i];
        if (nd.tp == nodInt) {
            push(typInt, tr->expStack);
        }
    }
}
