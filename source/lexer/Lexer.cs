namespace Tl.Lexer;

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
        if (cByte == (byte)ASCII.underscore || isLetter(cByte)) {
            lexWord(input, result);
        } else if (cByte == (byte)ASCII.dot && (isLetter(nByte) || nByte == (byte)ASCII.underscore)) {
            lexDotWord(input, result);
        } else if (cByte == (byte)ASCII.atSign && (isLetter(nByte) || nByte == (byte)ASCII.underscore)) {
            lexAtWord(input, result);
        }
    }
}

/// A dot-separated list of chunks. Every chunk can start only with a letter or a single underscore
/// followed by a letter. First letters of chunks  must follow the rule of 
/// decreasing capitalization: a capitalized chunk must not follow a non-capitalized chunk.
private static void lexWord(byte[] input, LexResult lr) {
    int j = lr.i;
    bool metUncapitalized = lexWordChunk(input, lr);
    while (j < input.Length && input[j] == (byte)ASCII.dot) {
        bool wasCurrUncapitalized = lexWordChunk(input, lr);
        if (!metUncapitalized && !wasCurrUncapitalized) {
            lr.errorOut("An identifier may not contain a capitalized piece after an uncapitalized one!");
            return;
        }
        metUncapitalized = wasCurrUncapitalized;
    }
}

/// Returns true if the word chunk was uncapitalized
private static bool lexWordChunk(byte[] input, LexResult lr)  {
    return true;
}


private static void lexDotWord(byte[] input, LexResult lr) {
    

}

private static void lexAtWord(byte[] input, LexResult lr) {
    

}

private static bool isLetter(byte a) {
    return (a >= (byte)ASCII.aLower && a <= (byte)ASCII.zLower) || ((a >= (byte)ASCII.aUpper && a <= (byte)ASCII.zUpper));
}

private static bool isCapitalLetter(byte a) {
    return ((a >= (byte)ASCII.aUpper && a <= (byte)ASCII.zUpper));
}

private static bool isAlphanumeric(byte a) {
    return (a >= (byte)ASCII.aLower && a <= (byte)ASCII.zLower) 
        || ((a >= (byte)ASCII.aUpper && a <= (byte)ASCII.zUpper)) 
        || isAlphanumeric(a);
}

private static bool isDigit(byte a) {
    return (a >= (byte)ASCII.digit0 && a <= (byte)ASCII.digit9);
}


}