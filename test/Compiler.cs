namespace Tl.Test.Compiler {
using Tl.Compiler;
using System.Text;

public static class LexerTest {
    public static TestSuite<string, LexResult> lexerTests() {        
        return new TestSuite<string, LexResult>(
            "Lexer suite", 
            (inp) => Lexer.lexicallyAnalyze(Encoding.ASCII.GetBytes(inp)),
            LexResult.equality, 
            lexerInputsOutputs().ToArray());
    }

    private static List<Test<string, LexResult>> lexerInputsOutputs() {
        var result = new List<Test<string, LexResult>>();
        var expected1 = new LexResult();
        expected1.addToken(new Token {startChar=0, lenChars=4,lenTokens=0, payload=0,ttype=TokenType.word});
        expected1.addToken(new Token {startChar=5, lenChars=3,lenTokens=0, payload=0,ttype=TokenType.word});
        result.Add(new Test<string, LexResult>(){input="asdf Abc", expected=expected1, name = "Simple word tokens"});

        var expected2 = new LexResult();
        expected2.addToken(new Token {startChar=0, lenChars=4, lenTokens=0, payload=0, ttype=TokenType.word});
        expected2.addToken(new Token {startChar=5, lenChars=3, lenTokens=0, payload=123, ttype=TokenType.litInt});
        expected2.addToken(new Token {startChar=9, lenChars=3, lenTokens=0, payload=0, ttype=TokenType.word});
        result.Add(new Test<string, LexResult>(){input="asdf 123 Abc", expected=expected2, name = "Simple word and numeric tokens"});

        return result;
    }
}


}