#ifndef STACKMAP_H
#define STACKMAP_H
#include <stdbool.h>
#include "../arena.h"
#include "../goodString.h"
#include "../aliases.h"


typedef struct BindingMap BindingMap;
typedef struct ScopeChunk ScopeChunk;
struct BindingMap{
    Arr(ValueList*) dict
   ;int dictSize
   ;int length
   ;BindingMap* previous;
   ;
};

struct ScopeChunk{
    ScopeChunk *next
   ;int length // size is divisible by 8
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
   ;
} ScopeStack;


typedef struct {
    ScopeStack* scopes;
    BindingMap* topScope;
} Parser;

 void addBinding(String* name, int id, ScopeStack* scopeStack)
;int lookupBinding(String* name, bool* wasInTopFunc, BindingMap* topScope)
;void newFrame(bool isFunction, ScopeStack* scopeStack)
;void popFrame(ScopeStack* scopeStack)
;
#endif