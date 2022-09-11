use std::borrow::BorrowMut;
use std::cell::RefCell;
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
    firstChunk: Rc<RefCell<TokenSlab>>,
    currChunk: Rc<RefCell<TokenSlab>>,
    nextInd: usize,
    wasError: bool,
    errMsg: Option<String>,
}

impl LexResult {
    pub fn new() -> Self {
        let firstChunk = Rc::new(RefCell::new(TokenSlab{nextSlab: None, content: [Default::default(); CHUNK_SZ], }));
        Self { firstChunk: firstChunk.clone(),
               currChunk: firstChunk.clone(), nextInd: 0, wasError: false, errMsg: None }
    }

    pub fn addToken(&mut self, newToken: Token) {
        let mut value = self.borrow_mut();
        if value.nextInd < (CHUNK_SZ - 1) {
            value.currChunk.borrow_mut().content[value.nextInd] = newToken
        }
    }
}