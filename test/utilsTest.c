#include <stdio.h>
#include <setjmp.h>
#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/intMap.h"
#include "../source/utils/stringMap.h"
#include "../source/utils/aliases.h"

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
    String* foo = allocLit("foo", a);
    String* bar = allocLit("barr", a);
    add(foo, 100, sm);
    add(bar, 200, sm);
    int valueFromMap = 0;
    int errCode = get(foo, &valueFromMap, sm);
    if (errCode != 0 || valueFromMap != 100) {
        printf("Error when getting string ");
        printString(foo);
    }
    
    
    
    
    
}

private void testIntMap(Arena* a) {
    IntMap* hm = createIntMap(150, a);
    add(1, 1000, hm);
    add(2, 2000, hm);
    printf("Find key 1? %d Find key 3? %d\n", hasKey(1, hm), hasKey(3, hm));
}


int main() {
    printf("----------------------------\n");
    printf("Utils test\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    
    if (setjmp(excBuf) == 0) {
        testStringMap(a);
    } else {
        printf("there was a test failure!\n");
    }
    
    deleteArena(a);
}
