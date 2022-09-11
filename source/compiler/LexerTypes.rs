use std::rc::Rc;

const CHUNK_SZ: usize = 10000;

#[derive(Clone, Copy)]
pub enum TokenType {
    word,
    dotWord,
    atWord,
    litInt,
    litFloat,
    litString,
    paren,
    curlyBrace,
    bracket,
    comment,
    sentinelToken,
}

#[derive(Copy, Clone)]
pub struct Token {
    tType: TokenType,
    payload: u64,
    startChar: u32,
    lenChars: u32,
    lenTokens: u32,
}

impl Default for Token {
    fn default() -> Self {
        Token {tType: TokenType::sentinelToken, payload: 0, startChar: 0, lenChars: 0, lenTokens: 0}
    }
}

pub struct TokenSlab {
    nextSlab: Option<Box<TokenSlab>>,
    content: [Token; CHUNK_SZ],
}

pub struct LexResult {
    firstChunk: Rc<Box<TokenSlab>>,
    currChunk: Rc<Box<TokenSlab>>,
    currInd: usize,
    wasError: bool,
    errMsg: Option<String>,
}

impl LexResult {
    pub fn new() -> Self {
        let firstChunk = Rc::new(Box::new(TokenSlab{nextSlab: None, content: [Default::default(); CHUNK_SZ], }));
        Self { firstChunk: firstChunk.clone(),
               currChunk: firstChunk.clone(), currInd: 0, wasError: false, errMsg: None }
    }
}