import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;


class Build {
	
public static void main(String[] args) {
	System.out.println("Build");
	try {	
		Process process = Runtime.getRuntime().exec("gcc source/main.c source/foo/foo.c source/foo/foo.h -o cplay");
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
