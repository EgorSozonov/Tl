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


ScopeStack* createScopeStack() {    
    ScopeStack* result = malloc(sizeof(ScopeStack));

    ScopeChunk* firstChunk = malloc(CHUNK_SIZE);

    firstChunk->length = floor4(CHUNK_SIZE - sizeof(ScopeChunk))/4;
    firstChunk->next = NULL;

    result->firstChunk = firstChunk;
    result->currChunk = firstChunk;
    result->lastChunk = firstChunk;
    result->topScope = (BindingList*)firstChunk->content;
    result->nextInd = ceiling4(sizeof(BindingList))/4;
    Arr(int) firstFrame = (int*)firstChunk->content + result->nextInd;
        
    (*result->topScope) = (BindingList){.length = 0, .previousChunk = NULL, .thisChunk = result->currChunk, 
        .thisInd = result->nextInd, .content = firstFrame };
        
    result->nextInd += 64;
   
    printf("FirstChunk %p TopScope %p \n", firstChunk, result->topScope);
    printf("FirstChunk length %d nextInd %d \n", firstChunk->length, result->nextInd);
    printf("sizeof(BindingList) = %d bytes sizeof(ScopeChunk) = %d bytes\n", sizeof(BindingList), sizeof(ScopeChunk));
   
    
    return result;
}

private void mbNewChunk(ScopeStack* scopeStack) {
    if (scopeStack->currChunk->next != NULL) {
        return;
    }
    ScopeChunk* newChunk = malloc(CHUNK_SIZE);
    scopeStack->currChunk->next = newChunk;
    scopeStack->currChunk = newChunk;
    scopeStack->lastChunk;    
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
BindingList* pushScope(ScopeStack* scopeStack) {
    // check whether the free space in currChunk is enough for the hashmap header + dict
    // if enough, allocate, else allocate a new chunk or reuse lastChunk if it's free    
    int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
    int necessarySpace = ceiling4(sizeof(BindingList)) + 64*sizeof(int);
    ScopeChunk* oldChunk = scopeStack->currChunk;
    BindingList* newScope;
    if (remainingSpace < necessarySpace) {        
        mbNewChunk(scopeStack);
        scopeStack->currChunk = scopeStack->currChunk->next;
        scopeStack->nextInd = necessarySpace;
        newScope = (BindingList*)scopeStack->currChunk->content;        
    } else {
        newScope = (BindingList*)((int*)scopeStack->currChunk->content + scopeStack->nextInd);
    }
    newScope->content = (int*)newScope + ceiling4(sizeof(BindingList));            
    newScope->previousChunk = oldChunk;
    newScope->previousInd = scopeStack->topScope->thisInd;
    scopeStack->topScope = newScope;
}

/** Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any */
BindingList* popScope(Arr(int) scopeBindings, ScopeStack* scopeStack) {  
    BindingList* topScope = scopeStack->topScope;
    for (int i = 0; i < topScope->length; i++) {
        int ind = *(topScope->content + i);
        printf("deleting ind = %d\n", ind);
        scopeBindings[ind] = 0;
    }    
    
    scopeStack->currChunk = topScope->previousChunk;    
    scopeStack->topScope = (BindingList*)(scopeStack->currChunk->content + topScope->previousInd);
    scopeStack->lastChunk = scopeStack->currChunk->next;
    
    // if the lastChunk is defined, it will serve as pre-allocated buffer for future frames, but everything after it needs to go
    if (scopeStack->lastChunk != NULL) {
        ScopeChunk* ch = scopeStack->lastChunk->next;
        while (ch != NULL) {
            ScopeChunk* nextToDelete = ch->next;
            free(ch);
            ch = nextToDelete;
        }
    }
   
}
