namespace Tl {
using Tl.Compiler;

static class Program {


public static void Main() {
    Console.WriteLine("Hw");
    var testSuite = Tl.Test.Compiler.LexerTest.lexerTests();
    testSuite.run();
    
}


}
}

