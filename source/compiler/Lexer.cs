namespace Tl.Compiler {
using System.Text;

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
        } else if (isDigit(cByte)) {
            lexNumber(input, result);
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

/// Lexes an integer literal, a hex integer literal, a binary integer literal, or a floating literal.
/// This function can handle being called on the last byte of input.
private static void lexNumber(byte[] input, LexResult lr) {
    if (lr.i == input.Length - 1) {
        lr.addToken(new Token { ttype = TokenType.litInt, startChar = lr.i, lenChars = 1, lenTokens = 0, 
                                   payload = (long)(input[lr.i] - ASCII.digit0), });
        return;

    }
    byte cByte = input[lr.i];
    byte nByte = input[lr.i + 1];
    if (nByte == (byte)ASCII.xLower) {
        lexHexNumber(input, lr);
    } else if (nByte == (byte)ASCII.bLower) {
        lexBinNumber(input, lr);
    } else {
        lexDecNumber(input, lr);
    }
}

/// Lexes a decimal numeric literal (integer or floating-point).
private static void lexDecNumber(byte[] input, LexResult lr) {
    // write 
    int indDot = 0; // relative index of the dot symbol in literal
    int i = lr.i + 1;
    while (i < input.Length) {
        byte cByte = input[i];
        if (isDigit(cByte)) {
            i++;
        } else if (cByte == (byte)ASCII.underscore) {
            if (i == input.Length - 1) {
                lr.errorOut("Numeric literal cannot end with an underscore!");
                return;
            }
            cByte = input[i + 1];
            if (!isDigit(cByte)) {
                lr.errorOut("In a numeric literal, underscores must be followed by digits!");
                return;
            }
            i += 2;
        } else if (cByte == (byte)ASCII.dot) {
            if (i == input.Length - 1) {
                lr.errorOut("Numeric literal must not end with a dot!");
                return;
            } else if (indDot > 0) {
                lr.errorOut("A floating point literal can contain only one dot!");
                return;
            }            
            indDot = i - lr.i;
            cByte = input[i + 1];
            if (!isDigit(cByte)) {
                lr.errorOut("In a floating point literal, a dot must be followed by a digit!");
                return;
            }
            i += 2;            
        } else {
            break;
        }           
    }
    var byteSubstring = new byte[i - lr.i];
    for (int j = lr.i; j < i; j++) {

    }
    string substr = Encoding.ASCII.GetString(byteSubstring);
    lr.i = i;
}

/// Lexes a hexadecimal numeric literal. 
/// Checks it for fitting into a signed 64-bit fixnum.
private static void lexHexNumber(byte[] input, LexResult lr) {

}

/// Lexes a hexadecimal numeric literal. 
/// Checks it for fitting into a signed 64-bit fixnum.
private static void lexBinNumber(byte[] input, LexResult lr) {
    
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