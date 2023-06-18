#include <stdbool.h>
#include <string.h>
#include "parser.h"
#include "typer.h"



#define push pushint32_t

Arr(Int) createOverloads(Parser* pr) {
    Int neededCount = 0;
    for (Int i = 0; i < pr->overlNext; i++) {
        neededCount += (2*pr->overloads[i] + 1); // a typeId and an entityId for each overload, plus a length field for the list
    }
    Arr(Int) overloads = allocateOnArena(neededCount*4, pr->a);

    Int j = 0;
    for (Int i = 0; i < pr->overlNext; i++) {
        overloads[j] = pr->overloads[i]; // the length of the overload list (which will be filled during type check/resolution)
        j += (2*pr->overloads[i] + 1);
    }
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



Int typeResolveExpr(Int indExpr, Typer* tr) {
    Node expr = tr->nodes[indExpr];
    Int sentinelNode = indExpr + expr.pl2 + 1;
    for (int i = indExpr + 1; i < sentinelNode; ++i) {
        Node nd = tr->nodes[i];
        if (nd.tp <= nodString) {
            push((Int)nd.tp, tr->expStack);
            push(-1, tr->expStack); // -1 arity means it's not a call
        } else {
            push(nd.pl1, tr->expStack); // bindingId or overloadId            
            push(nd.tp == nodCall ? nd.pl2 : -1, tr->expStack);            
        }
    }
    Int j = 2*expr.pl2 - 1;
    Arr(Int) st = tr->expStack->content;
    while (j > -1) {        
        if (st[j + 1] == -1) {
            j -= 2;
        } else { // a function call
            // resolve overload
            // check & resolve arg types
            // replace the arg types on the stack with the return type
            // shift the remaining stuff on the right so it directly follows the return
        }
    }
}
