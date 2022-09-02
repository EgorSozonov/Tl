const std = @import("std");
const mAlloc = std.c.malloc;
const mFree = std.c.free;
const print = std.log.info;
const CHUNK_QUANT = 32768;
const MIN_CHUNK_SIZE = (CHUNK_QUANT - 32); // 32 bytes for malloc bookkeeping

const ArenaListChunks = struct {
    payload: *[]u8,
    next: ?*ArenaListChunks,
};

const ArenaError = error {
    outOfMemory,    
};

pub const Arena = struct {
    const This = @This();
    chunksHead: ?*ArenaListChunks,
    chunksTail: ?*ArenaListChunks,
    currChunk: *[]u8,
    currInd: u32,

    pub fn allocate(this: This, T: type, allocSize: usize) ArenaError!T {
        if (this.currInd + allocSize >= this.currChunk.len) {
            const newSize = calculateChunkSize(allocSize);
            const newChunk: *[newSize]u8 = try mAlloc(newSize + @sizeOf(ArenaListChunks));
            const newLink = @ptrCast(*ArenaListChunks, newChunk + newSize);
            this.chunksTail.next = newLink;
            this.chunksTail = newLink;
            print("Allocated a new chunk\n", .{});
            this.currChunk = newChunk;
            this.currInd = 0;
            
        }
        const result = @ptrCast(T, &this.currChunk[this.currInd]);
        this.currInd += allocSize;
        return result;
    }

    pub fn delete(this: *This) void {
        var curr = this.chunksHead;
        //var nextToFree = curr;
        while (curr != null) {
            const curr2 = curr.?;
            var nextToFree: ?*ArenaListChunks = curr2.next;
            print("freeing a chunk\n", .{});
            
            mFree(curr2.payload);
            curr = nextToFree;
        }
        print("Freeing arena head", .{});
        mFree(this);
    }
};

fn calculateChunkSize(allocSize: usize) usize {
    const fullMemory = @sizeOf(ArenaListChunks) + allocSize + 32;
    const mallocMemory = if (fullMemory < CHUNK_QUANT)
                            { CHUNK_QUANT; }
                            else {
                                if (fullMemory % CHUNK_QUANT > 0) 
                                    {(fullMemory/CHUNK_QUANT + 1)*CHUNK_QUANT;}
                                    else fullMemory;
                            };
    return mallocMemory - 32;    
}

fn rawAlloc(comptime T: type, sz: u32) ArenaError!*T {
    const result = @ptrCast(?*T, @alignCast(@alignOf(*u64), mAlloc(sz)));
    if (result == null) return ArenaError.outOfMemory;
    return result.?;
}

pub fn mkArena() ArenaError!*Arena {    
    const result = try rawAlloc(Arena, @sizeOf(Arena));    

    const firstChunk: *[]u8 = try rawAlloc([]u8, MIN_CHUNK_SIZE + @sizeOf(ArenaListChunks));
    const listHead = @ptrCast(*ArenaListChunks, firstChunk + @ptrCast(*u8, MIN_CHUNK_SIZE));
    result.chunksHead = listHead;
    result.chunksTail = listHead;
    result.chunksHead.payload = firstChunk;
    result.chunksHead.next = null;
    result.currChunk = firstChunk;
    result.currInd = 0;
    return result;
}

