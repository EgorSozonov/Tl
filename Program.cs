namespace Tl {
using Tl.Compiler;

static class Program {


public static void Main() {
    var testSuite = Tl.Test.CompilerTest.lexerTests();
    testSuite.run();
    
}


}


}
