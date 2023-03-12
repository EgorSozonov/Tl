#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>


typedef struct ArenaChunk ArenaChunk;
typedef struct Arena Arena;

Arena* mkArena();
void* allocateOnArena(size_t allocSize, Arena* ar);
//void* allocateArrayArena(Arena* ar, size_t allocSize, int numElems);
void deleteArena(Arena* ar);


#endif
