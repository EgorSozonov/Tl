#include "../source/utils/Arena.h"
#include "../source/utils/String.h"
#include "../source/utils/Stack.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


int main() {
	printf("Hello test\n");
    Arena *ar = mkArena();
    
    String* str = allocateLiteral(ar, "Hello from F!");
    printf("Length is %d\n", str->length);
    printString(str);
    arenaDelete(ar);
	
    
}
