namespace Tl.Compiler {


public static class Lexer {


/// Main lexer function. Produces a stream of tokens.
/// Input: must be in UTF-8, but strictly ASCII outside of comments and string literals.
/// Output: contains all the parsed tokens with a flag for error and error msg.
public static LexResult lexicallyAnalyze(byte[] input) {
    var result = new LexResult();

    if (input == null || input.Length ==0) {
        result.errorOut("Empty input");
        return result;
    }
    int i = 0;

    // Check for UTF-8 BOM at start of file
    if (input.Length >= 3 && input[0] == 0xEF && input[1] == 0xBB && input[2] == 0xBF) {
        i += 3;
    }

    lexLoop(input, result);
    
    return result;
}


/// Lexer's worker function. Loops over input and generates the tokens
private static void lexLoop(byte[] input, LexResult result) {
    int walkLen = input.Length - 1;
    while (result.i < walkLen) {
        byte cByte = input[result.i];
        byte nByte = input[result.i + 1];
        if (cByte == (byte)ASCII.space || cByte == (byte)ASCII.emptyCR) {
            result.i += 1;
        } else if (cByte == (byte)ASCII.emptyLF) {
            result.i += 1;
        } else if (cByte == (byte)ASCII.underscore || isLetter(cByte)) {
            lexWord(input, result, TokenType.word);
        } else if (cByte == (byte)ASCII.dot && (isLetter(nByte) || nByte == (byte)ASCII.underscore)) {
            lexDotWord(input, result);
        } else if (cByte == (byte)ASCII.atSign && (isLetter(nByte) || nByte == (byte)ASCII.underscore)) {
            lexAtWord(input, result);
        } else {
            result.i += 1;
        }
    }
}

/// A dot-separated list of chunks. Every chunk can start only with a letter or a single underscore
/// followed by a letter. First letters of chunks  must follow the rule of 
/// decreasing capitalization: a capitalized chunk must not follow a non-capitalized chunk.
/// Input: @wordType must be either word, dotWord or atWord.
private static void lexWord(byte[] input, LexResult lr, TokenType wordType) {
    int startInd = lr.i;
    bool metUncapitalized = lexWordChunk(input, lr);

    while (lr.i < (input.Length - 1) && input[lr.i] == (byte)ASCII.dot && !lr.wasError) {
        lr.i += 1;
        bool wasCurrUncapitalized = lexWordChunk(input, lr);
        if (metUncapitalized && !wasCurrUncapitalized) {
            lr.errorOut("An identifier may not contain a capitalized piece after an uncapitalized one!");
            return;
        }
        metUncapitalized = wasCurrUncapitalized;
    }
    if (lr.i > startInd) {
        int realStartChar = wordType == TokenType.word 
                            ? startInd 
                            : (startInd - 1);
        lr.addToken(new Token{
            lenChars=lr.i - realStartChar, lenTokens=0, payload=0, startChar=realStartChar, ttype=wordType,
        });
    } else {
        lr.errorOut("Could not lex a word at position " + startInd);
    }
}

/// Returns true if the word chunk was uncapitalized.
/// This function can handle being called on the last byte of input
private static bool lexWordChunk(byte[] input, LexResult lr)  {
    var result = false;
    if (input[lr.i] == (byte)ASCII.underscore) {
        lr.i += 1;
        if (lr.i == input.Length) {
            lr.errorOut("Identifier cannot end with underscore!");
            return false;
        }
    }
    byte firstSymbol = input[lr.i];
    if (isLowercaseLetter(firstSymbol)) {
        result = true;
    } else if (!isCapitalLetter(firstSymbol)) {
        lr.errorOut("In an identifier, each word must start with a letter!");
        return false;
    }
    lr.i += 1;

    while (lr.i < input.Length && isAlphanumeric(input[lr.i])) {
        lr.i += 1;
    }
    return result;
}


private static void lexDotWord(byte[] input, LexResult lr) {
    lr.i += 1;
    lexWord(input, lr, TokenType.dotWord);
}

private static void lexAtWord(byte[] input, LexResult lr) {
    lr.i += 1;
    lexWord(input, lr, TokenType.atWord);
}

private static void lexNumber(byte[] input, LexResult lr) {
    // "0x"
    // "0b"
    // otherwise, walk looking for digits, underscores and dots, writing down the digits
    

}



private static bool isLetter(byte a) {
    return (a >= (byte)ASCII.aLower && a <= (byte)ASCII.zLower) || ((a >= (byte)ASCII.aUpper && a <= (byte)ASCII.zUpper));
}

private static bool isCapitalLetter(byte a) {
    return ((a >= (byte)ASCII.aUpper && a <= (byte)ASCII.zUpper));
}

private static bool isLowercaseLetter(byte a) {
    return ((a >= (byte)ASCII.aLower && a <= (byte)ASCII.zLower));
}

private static bool isAlphanumeric(byte a) {
    return isLetter(a) || isDigit(a);
}

private static bool isDigit(byte a) {
    return (a >= (byte)ASCII.digit0 && a <= (byte)ASCII.digit9);
}

}}