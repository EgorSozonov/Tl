

pub fn lexicallyAnalyze(inp: *[]u8, ar: *Arena) *LexResult {
    return null;
}

fn lexWord(inp: *[]u8, lr: *LexResult) void {
    const startInd = lr.i;
    var metUncapitalized = lexWordChunk(inp, lr);
    while (lr.i < (inp.len - 1) && inp[lr.i] == ASCII.dot && !lr.wasError) {
        lr.i++;
        const wasCurrUncapitalized = lexWordChunk(input, lr);
        if (metUncapitalized && !wasCurrUncapitalized) {
            lr.errorOut("An identifier may not contain a capitalized piece after an uncapitalized one!");
            return;

        }
        metUncapitalized = wasCurrUncapitalized;
    }
    if (lr.i > startInd) {
        lr.addToken(Token{lenChars = lr.i - startInd, lenTokens =0, payload = 0,startChar=startInd,ttype=TokenType.word});
    } else {
        lr.errorOut("Could not lex a word at position " + startInd);
    }
}

fn lexWordChunk(inp: *[]u8, lr: *LexResult) bool {
    var result = false;
    if (input[lr.i] == ASCII.underscore) {
        lr.i += 1;
        if (lr.i == input.Length) {
            lr.ErrorOut("Identifier cannot end with an underscore!");
            return false;
        }
    }
    var cByte = input[lr.i];
    if (isCapitalLetter(cByte)){

    } else if (isLowerCaseLetter(cByte)) {
        result = true;
    } else {
        lr.errorOut("In an identifier, each word must start with a letter!");
        return false;
    }
    lr.i += 1;
    while (lr.i < inp.len && isAlphanumeric(input[lr.i])) {
        lr.i += 1;
    }
    return result;
}