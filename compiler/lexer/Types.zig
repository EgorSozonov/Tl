const CHUNK_SZ: usize = 10000;
const Token = struct {
    payload: u64,
    startChar: u32,
    lenChars: u32,
    lenTokens: u32,
};

const LexChunk = struct {

};

const LexResult = struct {
    i: u32,
    wasError: bool,
    errMsg: string,
};