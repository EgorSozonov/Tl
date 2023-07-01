#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>
#include "../source/tl.internal.h"
#include "tlTest.h"

extern jmp_buf excBuf;

#define add(K, V, X) _Generic((X), \
    IntMap*: addIntMap \
    )(K, V, X)

#define hasKey(K, X) _Generic((X), \
    IntMap*: hasKeyIntMap \
    )(K, X)

#define get(A, V, X) _Generic((X), \
    IntMap*: getIntMap \
    )(A, V, X)
 
#define getUnsafe(A, X) _Generic((X), \
    IntMap*: getUnsafeIntMap \
    )(A, X) 
    
    

private void testIntMap(Arena* a) {
    IntMap* hm = createIntMap(150, a);
    add(1, 1000, hm);
    add(2, 2000, hm);
    printf("Find key 1? %d Find key 3? %d\n", hasKey(1, hm), hasKey(3, hm));
}

private void testScopeStack(Arena* a) {
    Compiler* cm = allocateOnArena(sizeof(Compiler), a);
    cm->scopeStack = createScopeStack();
    cm->activeBindings = allocateOnArena(1000*sizeof(int), a);
    pushScope(cm->scopeStack);
    addBinding(999, 1, cm);
    printf("Lookup of nameId = %d is bindingId = %d\n", 999, cm->activeBindings[999]);
    ScopeStack* st = cm->scopeStack;

    for (int i = 0; i < 500; i++) {
        pushScope(st);
        addBinding(i, i + 1, cm);
    }    
    printf("500 scopes in, lookup of nameId = %d is bindingId = %d, expected = 1\n", 999, cm->activeBindings[999]);

    
    for (int i = 0; i < 500; i++) {
        popScopeFrame(cm);
    } 
    
    printf("p1\n");
    popScopeFrame(cm);
    printf("p2\n");
    
    printf("Finally, lookup of nameId = %d is bindingId = %d, should be = 0\n", 999, cm->activeBindings[999]);
    
}


int main() {
    printf("----------------------------\n");
    printf("Utils test\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    
    if (setjmp(excBuf) == 0) {
        //testStringMap(a);
        
        testScopeStack(a);

    } else {
        printf("there was a test failure!\n");
    }
    
    deleteArena(a);
}
