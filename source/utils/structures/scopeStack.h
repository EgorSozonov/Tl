#ifndef SCOPESTACK_H
#define SCOPESTACK_H
#include <stdbool.h>
#include "../arena.h"
#include "../goodString.h"
#include "../aliases.h"


typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct {
    Int bindingId;
    Int precedence;
    Int arity;
    Int startByte;
    Int lenBytes;   
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
typedef struct {
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    ScopeStackFrame* topScope;
    int nextInd; // next ind inside currChunk, unit of measurement is 4 bytes
} ScopeStack;


ScopeStack* createScopeStack();
void addBinding(Int nameId, Int bindingId, Arr(int) activeBindings, ScopeStack*);
void addFunCall(FunctionCall, ScopeStack*);
void pushScope(ScopeStack*);
void pushSubexpr(ScopeStack*);
void popScopeFrame(Arr(int), ScopeStack*);

#endif
