#include "scopeStack.h"
#define CHUNK_QUANT 32768


typedef struct {
    int length;
    int value;
    String* string;
} StringValue;


typedef struct {
    uint capAndLen;
    StringValue content[];
} ValueList;

struct BindingMap {
    Arr(ValueList*) dict
   ;int dictLength            // size of the hash array
   ;int length                // number of elements in hash map
   ;bool isFunction           // this isn't just lexical scope but a function/closure
   ;BindingMap* previous;
   ;
};

private size_t floor8(size_t sz) {
    return 0;
}


private size_t minChunkSize() {
    return (size_t)(CHUNK_QUANT - 32);
}


ScopeStack* createScopeStack() {
    ScopeStack* result = malloc(sizeof(ScopeStack));

    size_t firstChunkSize = minChunkSize();

    ScopeChunk* firstChunk = malloc(firstChunkSize);

    firstChunk->length = firstChunkSize - sizeof(ScopeChunk);
    firstChunk->next = NULL;

    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->lastChunk = firstChunk;
    result->nextInd = 0;

    return result;
}

void addBinding(String* name, int id, ScopeStack* scopeStack) {
    // add to hashmap, growing it if needed (using lastChunk if needed, which may be different from currChunk)
}

/** Returns -1 if wasn't found. Sets foundInClosure if the result was found but not within the top function */
int lookupBinding(String* name, bool* foundInClosure, BindingMap* topScope) {
    // lookup in top scope, then if not found, look along the "prev" chain
    // if crossing the function boundary, set foundInClosure to true
    return 0;
}

BindingMap* newFrame(bool isFunction, ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    return NULL
   ;
}

/** Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any */
BindingMap* popFrame(ScopeStack* scopeStack) {
    // use previous pointer inside BindingMap
    // perhaps populate lastChunk with the newly freed chunk (if any)
    // free chunks after lastChunk
    return NULL
   ;
}
