#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>


typedef struct ArenaChunk ArenaChunk;
typedef struct Arena Arena;

Arena* mkArena();
void* allocateOnArena(Arena* ar, size_t allocSize);
//void* allocateArrayArena(Arena* ar, size_t allocSize, int numElems);
void deleteArena(Arena* ar);


#endif
