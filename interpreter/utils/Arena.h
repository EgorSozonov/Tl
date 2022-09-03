﻿#ifndef ARENA_H
#define ARENA_H


#include <stdlib.h>
typedef struct ArenaChunk ArenaChunk;
typedef struct Arena Arena;

Arena* mkArena();
void* arenaAllocate(Arena* ar, size_t allocSize);
void* arenaAllocateArray(Arena* ar, size_t allocSize, int numElems);
void arenaDelete(Arena* ar);


#endif
