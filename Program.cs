namespace Tl {
using System.IO;
using System.Text;
using Tl.Lexer;


class Program {
    static void Main() {
        Console.WriteLine("Hello, World!");

        var res1 = Lexer.Lexer.lexicallyAnalyze(Encoding.UTF8.GetBytes("asdf bchk Hagar.john"));

        var expected1 = new LexResult();
        expected1.addToken(new Token{ttype=TokenType.word, lenChars=4, startChar=0, lenTokens=0,payload=0});
        expected1.addToken(new Token{ttype=TokenType.word, lenChars=4, startChar=5, lenTokens=0,payload=0});
        expected1.addToken(new Token{ttype=TokenType.word, lenChars=10, startChar=10, lenTokens=0,payload=0});
        Console.WriteLine($"Result was error: {res1.wasError}, tokens: {res1.totalTokens}");
        var areEqual = LexResult.equality(res1, expected1);
        Console.WriteLine("What I expected? " + areEqual);
    }
}


}
