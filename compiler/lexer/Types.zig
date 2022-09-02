const CHUNK_SZ: usize = 10000;

const TokenType = enum(u8) {
    word,
    dotWord,
    atWord,
    intLiteral,
    floatLiteral,
    stringLiteral,
    subexpression,
    curlyBraces,
    squareBraces,
    comment,
};

const Token = struct {
    ttype: TokenType,
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