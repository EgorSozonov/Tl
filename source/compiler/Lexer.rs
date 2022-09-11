use crate::compiler::LexerTypes::*;

pub fn lexicallyFoo() {
    println!("Lexer");
}


pub fn lexicallyAnalyze(inp: &[u8]) -> LexResult {
    LexResult::new()
}

fn lexWord(inp: &[u8], lr: &LexResult) {

}

fn lexWordChunk(inp: &[u8], lr: &LexResult) {

}

fn lexDotWord(inp: &[u8], lr: &LexResult) {

}

fn lexAtWord(inp: &[u8], lr: &LexResult) {

}

fn lexNumber(inp: &[u8], lr: &LexResult) {

}

fn isLetter(a: u8) -> bool {
    (a >= asciiALower && a <= asciiZLower) || (a >= asciiAUpper && a <= asciiZUpper)
}

fn isCapitalLetter(a: u8) -> bool {
    a >= asciiAUpper && a <= asciiZUpper
}

fn isLowercaseLetter(a: u8) -> bool {
    a >= asciiALower && a <= asciiZLower
}

fn isAlphanumeric(a: u8) -> bool {
    isLetter(a) || isDigit(a)
}

fn isDigit(a: u8) -> bool {
    a >= asciiDigit0 && a <= asciiDigit9
}

pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

// -------------------------- Consts

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


// -------------------------- Tests


#[cfg(test)]
mod LexerTest {
    use super::*;


    #[test]
    fn isDigitTest() {
        assert!(isDigit(0) == false);
        assert!(isDigit(1) == false);
        assert!(isDigit(47) == false);
        assert!(isDigit(48) == true);
        assert!(isDigit(57) == true);
        assert!(isDigit(58) == false);
        assert!(isDigit(127) == false);
    }

    #[test]
    fn isCapitalLetterTest() {
        assert!(isCapitalLetter(0) == false);
        assert!(isCapitalLetter(1) == false);
        assert!(isCapitalLetter(64) == false);
        assert!(isCapitalLetter(65) == true);
        assert!(isCapitalLetter(90) == true);
        assert!(isCapitalLetter(95) == false);
        assert!(isCapitalLetter(127) == false);
    }

    #[test]
    fn isLowercaseLetterTest() {
        assert!(isLowercaseLetter(0) == false);
        assert!(isLowercaseLetter(1) == false);
        assert!(isLowercaseLetter(96) == false);
        assert!(isLowercaseLetter(97) == true);
        assert!(isLowercaseLetter(122) == true);
        assert!(isLowercaseLetter(123) == false);
        assert!(isLowercaseLetter(127) == false);
    }

    #[test]
    fn isAlphanumericTest() {
        assert!(isAlphanumeric(0) == false);
        assert!(isAlphanumeric(1) == false);


        assert!(isAlphanumeric(47) == false);
        assert!(isAlphanumeric(48) == true);
        assert!(isAlphanumeric(57) == true);
        assert!(isAlphanumeric(58) == false);

        assert!(isAlphanumeric(60) == false);

        assert!(isAlphanumeric(64) == false);
        assert!(isAlphanumeric(65) == true);
        assert!(isAlphanumeric(90) == true);
        assert!(isAlphanumeric(91) == false);

        assert!(isAlphanumeric(93) == false);

        assert!(isAlphanumeric(96) == false);
        assert!(isAlphanumeric(97) == true);
        assert!(isAlphanumeric(122) == true);
        assert!(isAlphanumeric(123) == false);

        assert!(isAlphanumeric(124) == false);
        assert!(isAlphanumeric(127) == false);
    }

    #[test]
    fn isLetterTest() {
        assert!(isLetter(0) == false);
        assert!(isLetter(1) == false);

        assert!(isLetter(64) == false);
        assert!(isLetter(65) == true);
        assert!(isLetter(90) == true);
        assert!(isLetter(91) == false);

        assert!(isLetter(93) == false);

        assert!(isLetter(96) == false);
        assert!(isLetter(97) == true);
        assert!(isLetter(122) == true);
        assert!(isLetter(123) == false);

        assert!(isLetter(124) == false);
        assert!(isLetter(127) == false);
    }
}