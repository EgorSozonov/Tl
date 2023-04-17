#ifndef SCOPESTACK_H
#define SCOPESTACK_H
#include <stdbool.h>
#include "../arena.h"
#include "../goodString.h"
#include "../aliases.h"


typedef struct BindingList BindingList;
typedef struct ScopeChunk ScopeChunk;


struct ScopeChunk{
    ScopeChunk *next;
    int length; // length is divisible by 4
    int content[];   
};

/** 
 * Either currChunk->next == NULL or currChunk->next->next == NULL
 */
typedef struct {
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    BindingList* topScope;
    int nextInd; // next ind inside currChunk, unit of measurement is 4 bytes
} ScopeStack;


typedef struct {
    ScopeStack* scopes;
    BindingList* topScope;
} Parser;


ScopeStack* createScopeStack();
void addBinding(int, int, Arr(int), ScopeStack*);
void pushScope(ScopeStack*);
void popScope(Arr(int), ScopeStack*);

#endif
