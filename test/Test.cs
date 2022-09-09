namespace Tl.Test {


public class TestSuite<Inp, Outp> {
    string name;
    Func<Inp, Outp> testedFunc;
    Func<Outp, Outp, bool> equality;
    Test<Inp, Outp>[] tests;

    public TestSuite(string _name, Func<Inp, Outp> _testedFunc, Func<Outp, Outp, bool> _equality, Test<Inp, Outp>[] _tests) {
        name = _name;
        testedFunc = _testedFunc;
        equality = _equality;
        tests = _tests;
    }

    public void run() {
        int countSuccessful = 0;
        int countErrors = 0;
        var listErrors = new List<string>();
        try {
            foreach (var test in tests) {
                var res = testedFunc(test.input);
                if (this.equality(test.expected, res)) {
                    ++countSuccessful;
                } else {
                    ++countErrors;
                    listErrors.Add(test.name);
                }
            }
        } catch (Exception e) {
            Console.WriteLine(this.name + ": EXCEPTION");
            Console.WriteLine(e.Message);
            return;
        }
        if (countErrors > 0) {
            Console.WriteLine(this.name + $": {countErrors} ERRORS");
            foreach (var err in listErrors) {
                Console.WriteLine(err);
            }
        } else {
            Console.WriteLine(this.name + $": {countSuccessful} tests run successfully");
        }
        Console.WriteLine();
    }
}

public struct Test<Inp, Outp> {
    public string name;
    public Inp input;
    public Outp expected;    
}


}