#include "scopeStack.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>
#define CHUNK_SIZE 65536

extern jmp_buf excBuf;


/** The list of string ids Introduced in the current scope. It's not used for lookups, only for frame popping. */
struct ScopeStackFrame {
    Int length;                // number of elements in hash map
    ScopeChunk* previousChunk;
    Int previousInd; // index of the start of previous frame within its chunk    
    ScopeChunk* thisChunk;
    Int thisInd; // index of the start of this frame within this chunk
    bool isSubexpr; // tag for the union. If true, it's Arr(FunctionCall). Otherwise Arr(Int)
    union {        
        Arr(Int) bindings;
        Arr(FunctionCall) funCalls;
    };
};


private size_t ceiling4(size_t sz) {
    size_t rem = sz % 4;
    return sz + 4 - rem;
}


private size_t floor4(size_t sz) {
    size_t rem = sz % 4;
    return sz - rem;
}

#define FRESH_CHUNK_LEN floor4(CHUNK_SIZE - sizeof(ScopeChunk))/4


ScopeStack* createScopeStack() {    
    ScopeStack* result = malloc(sizeof(ScopeStack));

    ScopeChunk* firstChunk = malloc(CHUNK_SIZE);

    firstChunk->length = FRESH_CHUNK_LEN;
    firstChunk->next = NULL;

    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->lastChunk = firstChunk;
    result->topScope = (ScopeStackFrame*)firstChunk->content;
    result->nextInd = ceiling4(sizeof(ScopeStackFrame))/4;
    Arr(Int) firstFrame = (Int*)firstChunk->content + result->nextInd;
        
    (*result->topScope) = (ScopeStackFrame){.length = 0, .previousChunk = NULL, .thisChunk = firstChunk, 
        .thisInd = result->nextInd, .isSubexpr = false, .bindings = firstFrame };
        
    result->nextInd += 64;  
    
    return result;
}

private void mbNewChunk(ScopeStack* scopeStack) {
    if (scopeStack->currChunk->next != NULL) {
        return;
    }
    ScopeChunk* newChunk = malloc(CHUNK_SIZE);
    newChunk->length = FRESH_CHUNK_LEN;
    newChunk->next = NULL;
    scopeStack->currChunk->next = newChunk;
    scopeStack->lastChunk = NULL;
}

/** Allocates a new scope, either within this chunk or within a pre-existing lastChunk or within a brand new chunk 
 * Scopes have a simple size policy: 64 elements at first, then 256, then throw exception. This is because
 * only 256 local variables are allowed in one function, so transitively in one scope, too.
 */
void pushScope(ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    Int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
    Int necessarySpace = ceiling4(sizeof(ScopeStackFrame))/4 + 64;
    
    ScopeChunk* oldChunk = scopeStack->topScope->thisChunk;
    Int oldInd = scopeStack->topScope->thisInd;
    ScopeStackFrame* newScope;
    Int newInd;
    if (remainingSpace < necessarySpace) {  
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (ScopeStackFrame*)scopeStack->currChunk->content;          
        newInd = 0;
    } else {
        newScope = (ScopeStackFrame*)((Int*)scopeStack->currChunk->content + scopeStack->nextInd);
        newInd = scopeStack->nextInd;
        scopeStack->nextInd += necessarySpace;        
    }
    (*newScope) = (ScopeStackFrame){.previousChunk = oldChunk, .previousInd = oldInd, .length = 0,
        .thisChunk = scopeStack->currChunk, .thisInd = newInd,
        .isSubexpr = false,
        .bindings = (Int*)newScope + ceiling4(sizeof(ScopeStackFrame))};
    scopeStack->topScope = newScope;
}

/** Allocates a new subexpression.
 * Subexprs have a simple size policy: 32 elements, then resize to 256, then throw exception.
 * Only up to 256 function calls are allowed in an expression, so transitively in a subexpression, too.
 */
void pushSubexpr(ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    Int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
    Int necessarySpace = ceiling4(sizeof(ScopeStackFrame)+ 64*sizeof(FunctionCall))/4 ;
    
    ScopeChunk* oldChunk = scopeStack->topScope->thisChunk;
    Int oldInd = scopeStack->topScope->thisInd;
    ScopeStackFrame* newScope;
    Int newInd;
    if (remainingSpace < necessarySpace) {  
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (ScopeStackFrame*)scopeStack->currChunk->content;          
        newInd = 0;
    } else {
        newScope = (ScopeStackFrame*)((Int*)scopeStack->currChunk->content + scopeStack->nextInd);
        newInd = scopeStack->nextInd;
        scopeStack->nextInd += necessarySpace;        
    }
    (*newScope) = (ScopeStackFrame){ .previousChunk = oldChunk, .previousInd = oldInd, .length = 0,
        .thisChunk = scopeStack->currChunk, .thisInd = newInd,
        .isSubexpr = true,
        .funCalls = (FunctionCall*)((Int*)newScope + ceiling4(sizeof(ScopeStackFrame)))
    };
    scopeStack->topScope = newScope;
}


private void resizeScopeArrayIfNecessary(Int initLength, ScopeStackFrame* topScope, ScopeStack* scopeStack) {
    Int newLength = scopeStack->topScope->length + 1;
    if (newLength == initLength) {
        Int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
        if (remainingSpace + initLength < 256) {            
            mbNewChunk(scopeStack);
            scopeStack->currChunk = scopeStack->currChunk->next;
            Arr(Int) newContent = scopeStack->currChunk->content;
            if (topScope->isSubexpr) {
                memcpy(topScope->funCalls, newContent, initLength*sizeof(FunctionCall));
                topScope->funCalls = (FunctionCall*)newContent;            
            } else {
                memcpy(topScope->bindings, newContent, initLength*sizeof(Int));
                topScope->bindings = newContent;            
            }            
        }
    } else if (newLength == 256) {
        longjmp(excBuf, 1);
    }
}

void addBinding(Int nameId, Int bindingId, Arr(Int) activeBindings, ScopeStack* scopeStack) {
    ScopeStackFrame* topScope = scopeStack->topScope;
    resizeScopeArrayIfNecessary(64, topScope, scopeStack);
    
    topScope->bindings[topScope->length] = nameId;
    topScope->length++;
    
    activeBindings[nameId] = bindingId;
}


void addFunCall(FunctionCall funCall, ScopeStack* scopeStack) {
    ScopeStackFrame* topScope = scopeStack->topScope;
    resizeScopeArrayIfNecessary(32, topScope, scopeStack);
    
    topScope->funCalls[topScope->length] = funCall;
    topScope->length++;    
}

/** Returns poInter to previous frame (which will be top after this call) or NULL if there isn't any */
void popScopeFrame(Arr(Int) activeBindings, ScopeStack* scopeStack) {  
    ScopeStackFrame* topScope = scopeStack->topScope;
    if (!topScope->isSubexpr) {
        for (Int i = 0; i < topScope->length; i++) {
            activeBindings[*(topScope->bindings + i)] = 0;
        }    
    }
        
    scopeStack->currChunk = topScope->previousChunk;    
    scopeStack->lastChunk = scopeStack->currChunk->next;
    scopeStack->topScope = (ScopeStackFrame*)(topScope->previousChunk->content + topScope->previousInd);
    
    // if the lastChunk is defined, it will serve as pre-allocated buffer for future frames, but everything after it needs to go
    if (scopeStack->lastChunk != NULL) {        
        ScopeChunk* ch = scopeStack->lastChunk->next;
        if (ch != NULL) {
            scopeStack->lastChunk->next = NULL;
            do {
                ScopeChunk* nextToDelete = ch->next;
                free(ch);
                
                if (nextToDelete == NULL) break;
                ch = nextToDelete;
            } while (ch != NULL);
        }                
    }   
}
