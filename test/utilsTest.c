#include "../source/utils/arena.h"
#include "../source/utils/goodString.h"
#include "../source/utils/stack.h"
#include "../source/utils/hashMap.h"

int main() {
    printf("----------------------------\n");
    printf("Utils test\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    
    HashMap* hm = createHashMap(150, a);
    add(
    
    deleteArena(a);
}
