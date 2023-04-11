#ifndef SCOPESTACK_H
#define SCOPESTACK_H
#include <stdbool.h>
#include "../arena.h"
#include "../goodString.h"
#include "../aliases.h"


typedef struct BindingMap BindingMap;
typedef struct ScopeChunk ScopeChunk;

typedef struct BindingMap BindingMap;

struct ScopeChunk{
    ScopeChunk *next
   ;int length // length is divisible by 8
   ;byte content[]
   ;
};

/** 
 * Either currChunk->next == NULL or currChunk->next->next == NULL
 */
typedef struct {
    ScopeChunk *firstChunk
   ;ScopeChunk *currChunk
   ;ScopeChunk *lastChunk
   ;BindingMap* topScope
   ;int nextInd
   ;
} ScopeStack;


typedef struct {
    ScopeStack* scopes;
    BindingMap* topScope;
} Parser;

 ScopeStack* createScopeStack()
;void addBinding(String* name, int id, ScopeStack* scopeStack)
;int lookupBinding(String* name, bool* wasInTopFunc, BindingMap* topScope)
;BindingMap* newFrame(bool isFunction, ScopeStack* scopeStack)
;BindingMap* popFrame(ScopeStack* scopeStack)
;
#endif