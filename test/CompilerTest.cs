namespace Tl.Test {
using Tl.Compiler;
using System.Text;

/// Tests for the compiler
public static class CompilerTest {
    /// Tests for the lexical analyzer
    public static TestSuite<string, LexResult> lexerTests() {        
        return new TestSuite<string, LexResult>(
            "Lexer suite", 
            (inp) => Lexer.lexicallyAnalyze(Encoding.ASCII.GetBytes(inp)),
            LexResult.equality, 
            lexerInputsOutputs().ToArray());
    }

    private static List<Test<string, LexResult>> lexerInputsOutputs() {
        var result = new List<Test<string, LexResult>>();
        result.Add(new Test<string, LexResult>(){
            input="asdf Abc", 
            expected=new LexResult(new List<Token>() {
                new Token {startChar=0, lenChars=4, lenTokens=0, payload=0, ttype=TokenType.word},
                new Token {startChar=5, lenChars=3, lenTokens=0, payload=0, ttype=TokenType.word}
            }), 
            name = "Simple word tokens"
        });
        result.Add(new Test<string, LexResult>(){
            input="@a123 .Abc ", 
            expected=new LexResult(new List<Token>() {
                new Token {startChar=0, lenChars=5, lenTokens=0, payload=0, ttype=TokenType.atWord},
                new Token {startChar=6, lenChars=4, lenTokens=0, payload=0, ttype=TokenType.dotWord},
            }), 
            name = "Dot word and @-word"
        });
        // result.Add(new Test<string, LexResult>(){
        //     input="asdf 123 Abc", 
        //     expected=new LexResult(new List<Token>() {
        //         new Token {startChar=0, lenChars=4, lenTokens=0, payload=0, ttype=TokenType.word},
        //         new Token {startChar=5, lenChars=3, lenTokens=0, payload=123, ttype=TokenType.litInt},
        //         new Token {startChar=9, lenChars=3, lenTokens=0, payload=0, ttype=TokenType.word},
        //     }), 
        //     name = "Simple word and numeric tokens"
        // });

        return result;
    }
}


}