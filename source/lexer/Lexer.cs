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
    byte firstByte = input[0];
    int i = 0;

    // Check for UTF-8 BOM at start of file
    if (input.Length >= 3 && input[0] == 0xEF && input[1] == 0xBB && input[2] == 0xBF) {
        i += 3;
    }

    int walkLen = input.Length - 1;
    while (i < walkLen) {
        byte cByte = input[i];
        byte nByte = input[i + 1];
        if (cByte == (byte)ASCII.underscore || isLetter(cByte)) {
            lexWord(input, i, result);
        } else if (cByte == (byte)ASCII.dot && (isLetter(nByte) || nByte == (byte)ASCII.underscore)) {
            lexDotWord(input, i, result);
        } else if (cByte == (byte)ASCII.atSign && (isLetter(nByte) || nByte == (byte)ASCII.underscore)) {
            lexAtWord(input, i, result);
        }
    }
    return result;
}

/// A dot-separated list of chunks. Every chunk can start only with a letter
/// or a single underscore followed by a letter. First letters of chunks 
/// must follow the rule of decreasing capitalization: a capitalized chunk
/// cannot follow a non-capitalized chunk.
private static void lexWord(byte[] input, int i, LexResult lr) {
    

}

private static void lexDotWord(byte[] input, int i, LexResult lr) {
    

}

private static void lexAtWord(byte[] input, int i, LexResult lr) {
    

}

private static bool isLetter(byte a) {
    return (a >= (byte)ASCII.aLower && a <= (byte)ASCII.zLower) || ((a >= (byte)ASCII.aUpper && a <= (byte)ASCII.zUpper));
}

private static bool isDigit(byte a) {
    return (a >= (byte)ASCII.digit0 && a <= (byte)ASCII.digit9);
}


}