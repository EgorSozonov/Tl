#include <stdio.h>
#include <setjmp.h>
#include "../source/utils/arena.h"
#include "../source/utils/aliases.h"
#include "../source/utils/goodString.h"
#include "../source/utils/structures/intMap.h"
#include "../source/utils/structures/stringMap.h"
#include "../source/utils/structures/stringStore.h"
#include "../source/utils/structures/scopeStack.h"

jmp_buf excBuf;

#define add(K, V, X) _Generic((X), \
    IntMap*: addIntMap, \
    StringMap*: addStringMap \
    )(K, V, X)

#define hasKey(K, X) _Generic((X), \
    IntMap*: hasKeyIntMap, \
    StringMap*: hasKeyStringMap \
    )(K, X)

#define get(A, V, X) _Generic((X), \
    IntMap*: getIntMap, \
    StringMap*: getStringMap \
    )(A, V, X)
 
#define getUnsafe(A, X) _Generic((X), \
    IntMap*: getUnsafeIntMap, \
    StringMap*: getUnsafeStringMap \
    )(A, X) 
    
    

private void testStringMap(Arena* a) {
    StringMap* sm = createStringMap(1024, a);
    String* foo = str("foo", a);
    String* bar = str("barr", a);
    String* baz = str("baz", a);
    String* notAdded = str("notAdded", a);
    
    add(foo, 100, sm);
    add(bar, 200, sm);
    add(baz, 78, sm);
    int valueFromMap = 0;
    int errCode = get(foo, &valueFromMap, sm);
    if (errCode != 0 || valueFromMap != 100) {
        printString(foo);
        longjmp(excBuf, 1);
    }
    
    int valueBar = getUnsafe(bar, sm);
    if (valueBar != 200) {
        printString(bar);
        longjmp(excBuf, 1);
    }
    
    errCode = get(notAdded, &valueFromMap, sm);
    if (errCode != 1) {
        printString(notAdded);
        longjmp(excBuf, 1);
    }    
    
    errCode = get(baz, &valueFromMap, sm);
    if (errCode != 0 || valueFromMap != 78) {
        printString(baz);
        longjmp(excBuf, 1);
    }    
}

private void testStringStore(Arena* a) {
    char* input = "foo bar asdf qwerty asdf bar foo";
    StringStore* ss = createStringStore(16, a);
    StackInt* stringTable = createStackintt(16, a);
    addStringStore(input, 0, 3, stringTable, ss);
    addStringStore(input, 4, 3, stringTable, ss);
    addStringStore(input, 8, 4, stringTable, ss);
    Int indQwerty = addStringStore(input, 13, 5, stringTable, ss);
    Int indAsdf = addStringStore(input, 20, 4, stringTable, ss);
    Int indBar = addStringStore(input, 25, 3, stringTable, ss);
    Int indFoo = addStringStore(input, 29, 3, stringTable, ss);
    print("RESULT: indQwerty %d indAsdf %d indBar %d indFoo %d, should be 3 2 1 0", indQwerty, indAsdf, indBar, indFoo);
}


private void testIntMap(Arena* a) {
    IntMap* hm = createIntMap(150, a);
    add(1, 1000, hm);
    add(2, 2000, hm);
    printf("Find key 1? %d Find key 3? %d\n", hasKey(1, hm), hasKey(3, hm));
}

private void testScopeStack(Arena* a) {
    ScopeStack* st = createScopeStack();
    Arr(int) bindingsInScope = allocateOnArena(1000*sizeof(int), a);
    pushScope(st);
    addBinding(999, 1, bindingsInScope, st);
    printf("Lookup of nameId = %d is bindingId = %d\n", 999, bindingsInScope[999]);

    for (int i = 0; i < 500; i++) {
        pushScope(st);
        addBinding(i, i + 1, bindingsInScope, st);
    }    
    printf("500 scopes in, lookup of nameId = %d is bindingId = %d, expected = 1\n", 999, bindingsInScope[999]);
    for (int i = 0; i < 500; i++) {
        popScope(bindingsInScope, st);
    } 
    
    printf("p1\n");
    popScope(bindingsInScope, st);
    printf("p2\n");
    
    printf("Finally, lookup of nameId = %d is bindingId = %d, should be = 0\n", 999, bindingsInScope[999]);
    
}


int main() {
    printf("----------------------------\n");
    printf("Utils test\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    
    if (setjmp(excBuf) == 0) {
        //testStringMap(a);
        testStringStore(a);
        //testScopeStack(a);

    } else {
        printf("there was a test failure!\n");
    }
    
    deleteArena(a);
}
