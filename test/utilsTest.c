#include <stdio.h>
#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/hashMap.h"


int main() {
    printf("----------------------------\n");
    printf("Utils test\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    
    HashMap* hm = createHashMap(150, a);
    add(1, 1000, hm);
    add(2, 2000, hm);
    printf("Find key 1? %d Find key 3? %d\n", hasKey(1, hm), hasKey(3, hm));
    
    deleteArena(a);
}
