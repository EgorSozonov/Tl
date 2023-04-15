#include "scopeStack.h"
#include <stdio.h>
#define CHUNK_SIZE 65536


/** The list of string ids introduced in the current scope. It's not used for lookups, only for frame popping. */
struct BindingListHeader {
    int length;                // number of elements in hash map
    ScopeChunk* previousChunk;
    int previousInd; // index of the start of previous frame
}



private size_t ceiling4(size_t sz) {
    size_t rem = sz % 4;
    return sz + 4 - rem;
}

private size_t floor4(size_t sz) {
    size_t rem = sz % 4;
    return sz - rem;
}


ScopeStack* createScopeStack() {
    
    ScopeStack* result = malloc(sizeof(ScopeStack))

   ;ScopeChunk* firstChunk = malloc(CHUNK_SIZE)

   ;firstChunk->length = floor4(CHUNK_SIZE - sizeof(ScopeChunk))/4;
   ;firstChunk->next = NULL

   ;result->firstChunk = firstChunk
   ;result->currChunk = firstChunk
   ;result->lastChunk = firstChunk
   result->topScope = (BindingListHeader*)firstChunk->content;
   result->topScope->length = 0;
   result->topScope->previousChunk = NULL;
   
   
   ;result->nextInd = ceiling4((sizeof(BindingList)))/4;
   
   printf("FirstChunk %p TopScope %p \n", firstChunk, result->topScope);
   printf("FirstChunk length %d nextInd %d \n", firstChunk->length, result->nextInd);
   printf("sizeof(BindingList) = %d sizef(ScopeChunk) = %d\n", sizeof(BindingList), sizeof(ScopeChunk));

   return result;
}

void addBinding(String* name, int id, ScopeStack* scopeStack) {
    // add to hashmap, growing it if needed (using lastChunk if needed, which may be different from currChunk)
}

/** Returns -1 if wasn't found. Sets foundInClosure if the result was found but not within the top function */
int lookupBinding(String* name, bool* foundInClosure, BindingList* topScope) {
    // lookup in top scope, then if not found, look along the "prev" chain
    // if crossing the function boundary, set foundInClosure to true
    return 0;
}

BindingList* newFrame(bool isFunction, ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    return NULL
   ;
}

/** Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any */
BindingList* popFrame(ScopeStack* scopeStack) {
    // use previous pointer inside BindingList
    // perhaps populate lastChunk with the newly freed chunk (if any)
    // free chunks after lastChunk
    return NULL
   ;
}
