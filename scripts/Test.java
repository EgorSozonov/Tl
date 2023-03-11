import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;


class Build {

public static void main(String[] args) {
    System.out.println("Build");
    try {
        final String command = """
            gcc --std=c11 -o _bin/lexerTest
            -Isource/utils -Isource/compiler/lexer
            test/LexerTest.c source/utils/String.c source/utils/Arena.c source/compiler/lexer/Lexer.c
        """;
        Process process = Runtime.getRuntime()
        .exec("gcc --std=c11 -Isource/utils -Isource/compiler/lexer -o _bin/lexerTest test/LexerTest.c source/utils/String.c source/utils/Arena.c source/compiler/lexer/Lexer.c");
        // Run a shell command

        StringBuilder output = new StringBuilder();

        BufferedReader reader = new BufferedReader(
                new InputStreamReader(process.getInputStream()));

        String line;
        while ((line = reader.readLine()) != null) {
            output.append(line + "\n");
        }
        int exitVal = process.waitFor();
        if (exitVal == 0) {
            System.out.println("Success!");
            System.out.println(output);
            System.exit(0);
        } else {
            System.out.println("Failure!");
            //abnormal...
        }
    } catch (Exception ex) {
        System.out.println(ex.getMessage());
    }
}
}