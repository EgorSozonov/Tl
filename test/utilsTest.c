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


private void printStack(StackInt* st) {
    if (st->length == 0) {
        print("[]");
        return;
    }
    printf("[ %d", st->content[0]);
    for (int i = 1; i < st->length; i++) {
        printf(", %d", st->content[i]);        
    }
    print(" ]");    
}    
    

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

private void testOverloadSorting(Arena* a) {
    StackInt* st = createStackint32_t(64, a);
    push(3, st);
    push(1, st);
    push(10, st);
    push(5, st);
    push(1, st);
    push(2, st);
    push(3, st);
    printStack(st);
    sortOverloads(0, 6, st->content);
    print("After sorting: ");
    printStack(st);

    st = createStackint32_t(64, a);
    push(1, st);
    push(100, st);
    push(1, st);
    printStack(st);
    sortOverloads(0, 2, st->content);
    print("After sorting: ");
    printStack(st);

    st = createStackint32_t(64, a);
    push(3, st);
    push(300, st);
    push(200, st);
    push(100, st);
    push(1, st);
    push(2, st);
    push(3, st);
    printStack(st);
    sortOverloads(0, 6, st->content);
    print("After sorting: ");
    printStack(st);    
}


private void testOverloadUniqueness(Arena* a) {
    StackInt* st = createStackint32_t(64, a);
    push(1, st);
    push(1, st);
    push(3, st);
    bool areUnique = makeSureOverloadsUnique(0, 2, st->content);
    if (!areUnique) {
        print("Overload uniqueness error");
    }

    st = createStackint32_t(64, a);
    push(3, st);
    push(1, st);
    push(2, st);
    push(3, st);
    push(10, st);
    push(10, st);
    push(10, st);
    areUnique = makeSureOverloadsUnique(0, 6, st->content);
    if (!areUnique) {
        print("Overload uniqueness error");
    }


    st = createStackint32_t(64, a);
    push(3, st);
    push(2, st);
    push(2, st);
    push(3, st);
    push(10, st);
    push(10, st);
    push(10, st);
    areUnique = makeSureOverloadsUnique(0, 6, st->content);
    if (areUnique) {
        print("Overload uniqueness error");
    }
}

private void testOverloadSearch(Arena* a) {
    Int entityId = 0;
    StackInt* st = createStackint32_t(64, a);
    push(3, st);
    push(1, st);
    push(2, st);
    push(3, st);
    push(10, st);
    push(20, st);
    push(30, st);
    bool typeFound = overloadBinarySearch(2, 0, 6, &entityId, st->content);
    if (!typeFound || entityId != 20) {       
        print("Overload search error");
    }

    st = createStackint32_t(64, a);
    push(4, st);
    push(1, st);
    push(2, st);
    push(3, st);
    push(5, st);
    push(10, st);
    push(20, st);
    push(30, st);
    push(50, st);
    typeFound = overloadBinarySearch(5, 0, 8, &entityId, st->content);
    if (!typeFound || entityId != 50) {
        print("Overload search error");
    }

    st = createStackint32_t(64, a);
    push(2, st);
    push(1, st);
    push(3, st);
    push(10, st);
    push(30, st);
    typeFound = overloadBinarySearch(3, 0, 4, &entityId, st->content);
    if (!typeFound || entityId != 30) {
        print("Overload search error");
    }

    st = createStackint32_t(64, a);
    push(1, st);
    push(7, st);
    push(10, st);
    typeFound = overloadBinarySearch(7, 0, 2, &entityId, st->content);
    if (!typeFound || entityId != 10) {
        print("Overload search error");
    }
    
    st = createStackint32_t(64, a);
    push(2, st);
    push(1, st);
    push(3, st);
    push(10, st);
    push(30, st);
    typeFound = overloadBinarySearch(2, 0, 4, &entityId, st->content);
    if (typeFound) {
        print("Overload search error");
    }
}


int main() {
    printf("----------------------------\n");
    printf("Utils test\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    
    if (setjmp(excBuf) == 0) {
        //testStringMap(a);
        
        //testScopeStack(a);
        
        //~ testOverloadSorting(a);
        //~ testOverloadUniqueness(a);
        testOverloadSearch(a);
    } else {
        printf("there was a test failure!\n");
    }
    
    deleteArena(a);
}
