#define CHUNK_SIZE 65536

DEFINE_STACK(ParseFrame)                                                                    

                                            

#define pop(X) _Generic((X), \
    StackParseFrame*: popParseFrame \
    )(X)

#define peek(X) _Generic((X), \
    StackParseFrame*: peekParseFrame \
    )(X)

#define push(A, X) _Generic((X), \
    StackParseFrame*: pushParseFrame \
    )((A), X)
    
#define hasValues(X) _Generic((X), \
    StackParseFrame*: hasValuesParseFrame \
    )(X)   

typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct {
    Int bindingId;
    Int arity;
    Int tokId;
} FunctionCall;

typedef struct ScopeChunk ScopeChunk;


struct ScopeChunk {
    ScopeChunk *next;
    int length; // length is divisible by 4
    Int content[];   
};

/** 
 * Either currChunk->next == NULL or currChunk->next->next == NULL
 */
struct ScopeStack {
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    ScopeStackFrame* topScope;
    Int length;
    int nextInd; // next ind inside currChunk, unit of measurement is 4 bytes
};

/** The list of additional parsing info; binding frames followed by function call frames.
 * A binding frame contains string ids introduced in the current scope. Used not for lookups, but for frame popping. 
 * A function call frame contains operators/functions encountered within a subexpression.
 */
struct ScopeStackFrame {
    int length;                // number of elements in scope stack
    
    ScopeChunk* previousChunk;
    int previousInd;           // index of the start of previous frame within its chunk    
    
    ScopeChunk* thisChunk;
    int thisInd;               // index of the start of this frame within this chunk
    
    bool isSubexpr;            // tag for the union. If true, it's Arr(FunctionCall). Otherwise Arr(int)
    union {
        Arr(int) bindings;
        Arr(FunctionCall) fnCalls;
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
    Arr(int) firstFrame = (int*)firstChunk->content + result->nextInd;
        
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
 * only 256 local variables are allowed in one function, and transitively in one scope.
 */
void pushScope(ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
    int necessarySpace = ceiling4(sizeof(ScopeStackFrame))/4 + 64;
    
    ScopeChunk* oldChunk = scopeStack->topScope->thisChunk;
    int oldInd = scopeStack->topScope->thisInd;
    ScopeStackFrame* newScope;
    int newInd;
    if (remainingSpace < necessarySpace) {  
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (ScopeStackFrame*)scopeStack->currChunk->content;          
        newInd = 0;
    } else {
        newScope = (ScopeStackFrame*)((int*)scopeStack->currChunk->content + scopeStack->nextInd);
        newInd = scopeStack->nextInd;
        scopeStack->nextInd += necessarySpace;        
    }
    (*newScope) = (ScopeStackFrame){.previousChunk = oldChunk, .previousInd = oldInd, .length = 0,
        .thisChunk = scopeStack->currChunk, .thisInd = newInd,
        .isSubexpr = false,
        .bindings = (int*)newScope + ceiling4(sizeof(ScopeStackFrame))};
        
    scopeStack->topScope = newScope;    
    scopeStack->length++;
}

/** Allocates a new subexpression.
 * Subexprs have a simple size policy: 32 elements, then resize to 256, then throw exception.
 * Only up to 256 function calls are allowed in an expression, so transitively in a subexpression, too.
 */
void pushSubexpr(ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
    int necessarySpace = ceiling4(sizeof(ScopeStackFrame)+ 64*sizeof(FunctionCall))/4 ;
    
    ScopeChunk* oldChunk = scopeStack->topScope->thisChunk;
    int oldInd = scopeStack->topScope->thisInd;
    ScopeStackFrame* newScope;
    int newInd;
    if (remainingSpace < necessarySpace) {  
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (ScopeStackFrame*)scopeStack->currChunk->content;          
        newInd = 0;
    } else {
        newScope = (ScopeStackFrame*)((int*)scopeStack->currChunk->content + scopeStack->nextInd);
        newInd = scopeStack->nextInd;
        scopeStack->nextInd += necessarySpace;        
    }
    (*newScope) = (ScopeStackFrame){ .previousChunk = oldChunk, .previousInd = oldInd, .length = 0,
        .thisChunk = scopeStack->currChunk, .thisInd = newInd,
        .isSubexpr = true,
        .fnCalls = (FunctionCall*)((int*)newScope + ceiling4(sizeof(ScopeStackFrame)))
    };
    
    scopeStack->topScope = newScope;    
    scopeStack->length++;
}


private void resizeScopeArrayIfNecessary(Int initLength, ScopeStackFrame* topScope, ScopeStack* scopeStack) {
    int newLength = scopeStack->topScope->length + 1;
    if (newLength == initLength) {
        int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
        if (remainingSpace + initLength < 256) {            
            mbNewChunk(scopeStack);
            scopeStack->currChunk = scopeStack->currChunk->next;
            Arr(int) newContent = scopeStack->currChunk->content;
            if (topScope->isSubexpr) {
                memcpy(topScope->fnCalls, newContent, initLength*sizeof(FunctionCall));
                topScope->fnCalls = (FunctionCall*)newContent;            
            } else {
                memcpy(topScope->bindings, newContent, initLength*sizeof(Int));
                topScope->bindings = newContent;            
            }            
        }
    } else if (newLength == 256) {
        longjmp(excBuf, 1);
    }
}

void addBinding(int nameId, int bindingId, Arr(int) activeBindings, ScopeStack* scopeStack) {
    ScopeStackFrame* topScope = scopeStack->topScope;
    resizeScopeArrayIfNecessary(64, topScope, scopeStack);
    
    topScope->bindings[topScope->length] = nameId;
    topScope->length++;
    
    activeBindings[nameId] = bindingId;
}

/** Add function call to the stack of the current subexpression */
void addFnCall(FunctionCall fnCall, ScopeStack* scopeStack) {
    ScopeStackFrame* topScope = scopeStack->topScope;
    resizeScopeArrayIfNecessary(32, topScope, scopeStack);
    
    topScope->fnCalls[topScope->length] = fnCall;
    topScope->length++;
}

/** Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any */
void popScopeFrame(Arr(int) activeBindings, ScopeStack* scopeStack) {  
    ScopeStackFrame* topScope = scopeStack->topScope;
    if (!topScope->isSubexpr) {
        for (int i = 0; i < topScope->length; i++) {
            activeBindings[*(topScope->bindings + i)] = -1;
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
                printf("ScopeStack is freeing a chunk of memory at %p next = %p\n", ch, nextToDelete);
                free(ch);
                
                if (nextToDelete == NULL) break;
                ch = nextToDelete;

            } while (ch != NULL);
        }                
    }   
    scopeStack->length--;
}
