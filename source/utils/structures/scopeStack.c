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
    int previousInd; // index of the start of previous frame
    bool wasEnlarged; // if false, capacity = 64, if true = 256
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
    result->nextInd += 64;
    
    (*result->topScope) = (BindingList){.length = 0, .previousChunk = NULL, .wasEnlarged = false, .content = firstFrame };
   
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

void addBinding(int nameId, ScopeStack* scopeStack) {
    BindingList* topScope = scopeStack->topScope;
    int newLength = topScope->length + 1;
    if (!topScope->wasEnlarged && newLength == 64) {
        int remainingSpace = scopeStack->currChunk->length - scopeStack->nextInd + 1;
        if (remainingSpace < 192) {            
            mbNewChunk(scopeStack);
            scopeStack->currChunk = scopeStack->currChunk->next;
            Arr(int) newContent = scopeStack->currChunk->content;
            memcpy(topScope->content, newContent, 64*sizeof(int));
            topScope->content = newContent;            
        }
        topScope->wasEnlarged = true;
    } else if (newLength == 256) {
        longjmp(excBuf, 1);
    }
    topScope->content[topScope->length] = nameId;
    topScope->length++;
}

/** Allocates a new scope, either within this chunk or within a pre-existing lastChunk or within a brand new chunk */
BindingList* newScope(bool isFunction, ScopeStack* scopeStack) {
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
        newScope = scopeStack->currChunk->content;        
    } else {
        newScope = (int*)scopeStack->currChunk->content + scopeStack->nextInd;
    }
    newScope->content = (int*)newScope + ceiling4(sizeof(BindingList));            
    newScope->previousChunk = oldChunk;
    newScope->previousInd = scopeStack->topScope - ;
}

/** Returns pointer to previous frame (which will be top after this call) or NULL if there isn't any */
BindingList* popScope(ScopeStack* scopeStack) {
    BindingList* topScope = scopeStack->topScope;
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
