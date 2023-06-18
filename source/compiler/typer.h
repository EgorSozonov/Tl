
#include "../utils/common.h"


typedef struct {
    Stackint32_t* expStack;

    Arr(Entity) entities;      // growing array of all bindings ever encountered
    Int entNext;
    Int entCap;
    Int entOverloadZero;       // the index of the first parsed (as opposed to being built-in or imported) overloaded binding
    Int entBindingZero;        // the index of the first parsed (as opposed to being built-in or imported) non-overloaded binding

    Arr(Int) overloadCounts;   // growing array of counts of all fn name definitions encountered (for the typechecker to use)
    Int overlCountsCount;

    Arr(Int) overloads;        // growing array of counts of all fn name definitions encountered (for the typechecker to use)
    Int overlNext;
    Int overlCap;
    
    Arr(Node) nodes; 
    Int nodesCount;            // the index for the next token to be added    
    
    bool wasError;
    String* errMsg;
    Arena* a;
} Typer;

Typer* createTyper(Parser*);

Int typeResolveExpr(Int indNode, Typer* tr);

#define typInt 0
#define typFloat 1
#define typBool 2
#define typString 3

