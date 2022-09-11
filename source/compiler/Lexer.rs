use crate::compiler::LexerTypes::*;

pub fn lexicallyFoo() {
    println!("Lexer");
}


pub fn lexicallyAnalyze(inp: &[u8]) -> LexResult {
    LexResult::new()
}

pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

const asciiALower: u8 = 97;
const asciiZLower: u8 = 122;
const asciiAUpper: u8 = 65;
const asciiZUpper: u8 = 90;
const asciiDigit0: u8 = 48;
const asciiDigit9: u8 = 57;

const asciiPlus: u8 = 43;
const asciiMinus: u8 = 45;
const asciiTimes: u8 = 42;
const asciiDivBy: u8 = 47;
const asciiDot: u8 = 46;
const asciiPercent: u8 = 37;

const asciiParenLeft: u8 = 40;
const asciiParenRight: u8 = 41;
const asciiCurlyLeft: u8 = 91;
const asciiCurlyRight: u8 = 93;
const asciiBracketLeft: u8 = 40;
const asciiBracketRight: u8 = 41;
const asciiPipe: u8 = 124;
const asciiAmpersand: u8 = 38;
const asciiTilde: u8 = 126;
const asciiBackslash: u8 = 92;

const asciiApostrophe: u8 = 39;
const asciiQuote: u8 = 34;
const asciiSpace: u8 = 32;
const asciiSharp: u8 = 35;
const asciiDollar: u8 = 36;
const asciiUnderscore: u8 = 95;
const asciiCaret: u8 = 94;
const asciiAt: u8 = 64;
const asciiColon: u8 = 58;
const asciiSemicolon: u8 = 59;
const asciiExclamation: u8 = 33;
const asciiQuestion: u8 = 63;
const asciiEquals: u8 = 61;

const asciiLessThan: u8 = 60;
const asciiGreaterThan: u8 = 62;

