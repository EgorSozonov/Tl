#include "scopeStack.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>
#define CHUNK_SIZE 65536

extern jmp_buf excBuf;

/** The list of string ids introduced in the current scope. It's not used for lookups, only for frame popping. */
struct BindingList {
    int length;                // number of elements in hash map
    ScopeChunk* previousChunk;
    int previousInd; // index of the start of previous frame within its chunk    
    ScopeChunk* thisChunk;
    int thisInd; // index of the start of this frame within this chunk
    Arr(int) content;
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
    result->topScope = (BindingList*)firstChunk->content;
    result->nextInd = ceiling4(sizeof(BindingList))/4;
    Arr(int) firstFrame = (int*)firstChunk->content + result->nextInd;
        
    (*result->topScope) = (BindingList){.length = 0, .previousChunk = NULL, .thisChunk = firstChunk, 
        .thisInd = result->nextInd, .content = firstFrame };
        
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

void addBinding(int nameId, int bindingId, Arr(int) scopeBindings, ScopeStack* scopeStack) {
    BindingList* topScope = scopeStack->topScope;
    int newLength = topScope->length + 1;
    if (newLength == 64) {
        int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
        if (remainingSpace < 192) {            
            mbNewChunk(scopeStack);
            scopeStack->currChunk = scopeStack->currChunk->next;
            Arr(int) newContent = scopeStack->currChunk->content;
            memcpy(topScope->content, newContent, 64*sizeof(int));
            topScope->content = newContent;            
        }
    } else if (newLength == 256) {
        longjmp(excBuf, 1);
    }
    topScope->content[topScope->length] = nameId;
    topScope->length++;
    
    scopeBindings[nameId] = bindingId;
}

/** Allocates a new scope, either within this chunk or within a pre-existing lastChunk or within a brand new chunk */
void pushScope(ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
    int necessarySpace = ceiling4(sizeof(BindingList))/4 + 64;
    
    ScopeChunk* oldChunk = scopeStack->topScope->thisChunk;
    int oldInd = scopeStack->topScope->thisInd;
    BindingList* newScope;
    int newInd;
    if (remainingSpace < necessarySpace) {  
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (BindingList*)scopeStack->currChunk->content;          
        newInd = 0;
    } else {
        newScope = (BindingList*)((int*)scopeStack->currChunk->content + scopeStack->nextInd);
        newInd = scopeStack->nextInd;
        scopeStack->nextInd += necessarySpace;        
    }
    (*newScope) = (BindingList){.previousChunk = oldChunk, .previousInd = oldInd, .length = 0,
        .thisChunk = scopeStack->currChunk, .thisInd = newInd,
        .content = (int*)newScope + ceiling4(sizeof(BindingList))};
    scopeStack->topScope = newScope;
}

/** Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any */
void popScope(Arr(int) scopeBindings, ScopeStack* scopeStack) {  
    BindingList* topScope = scopeStack->topScope;
    for (int i = 0; i < topScope->length; i++) {
        int ind = *(topScope->content + i);
        scopeBindings[ind] = 0;
    }    
    
    scopeStack->currChunk = topScope->previousChunk;    
    scopeStack->lastChunk = scopeStack->currChunk->next;
    scopeStack->topScope = (BindingList*)(topScope->previousChunk->content + topScope->previousInd);
    
    // if the lastChunk is defined, it will serve as pre-allocated buffer for future frames, but everything after it needs to go
    if (scopeStack->lastChunk != NULL) {
        
        ScopeChunk* ch = scopeStack->lastChunk->next;
        if (ch != NULL) {
            scopeStack->lastChunk->next = NULL;
            do {
                ScopeChunk* nextToDelete = ch->next;
                printf("ScopeStack is freeing a chunk of memory at %p next = %p\n", ch, nextToDelete);
                free(ch);
                printf("p50\n");
                
                if (nextToDelete == NULL) break;
                printf("p51\n");
                ch = nextToDelete;

            } while (ch != NULL);
        }                
    }   
}
