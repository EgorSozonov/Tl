const CHUNK_QUANT = 32768;
const MIN_CHUNK_SIZE = (CHUNK_QUANT - 32); // 32 bytes for malloc bookkeeping
const malloc = std.heap.CAllocator;

const ArenaListChunks = struct {
    payload: *[]u8,
    next: *ArenaListChunks,
};

const Arena = struct {
    listChunks: ArenaListChunks,
    currChunk: *[]u8,
    currInd: u32,
};

fn calculateChunkSize(allocSize: usize) usize {
    const fullMemory = @sizeOf(ArenaChunk) + allocSize + 32;
    const mallocMemory = if (fullMemory < CHUNK_QUANT)
                            { CHUNK_QUANT; }
                            else {
                                if (fullMemory % CHUNK_QUANT > 0) 
                                    {(fullMemory/CHUNK_QUANT + 1)*CHUNK_QUANT;}
                                    else fullMemory;
                            };
    return mallocMemory - 32;    
}

pub fn mkArena() *Arena {
    
    var result: *Arena = std.c.malloc(@sizeOf(Arena));


    return result;
}
