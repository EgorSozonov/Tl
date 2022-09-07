# Usage: make build

CC=gcc                        # compiler to use
COMPFLAGS=-std=c11
LINKERFLAGS = -lm # Link the math library
TMPDIR = _temp
SRCDIR = source
SRCUTILS = utils
SRCTESTS = test
INC = -Iinc
EXETEST=_bin/tlTest
EXE=_bin/tlco

# Main app

compiler: main.o lexer.o arena.o string.o
	$(CC) $(COMPFLAGS) -o $(EXE) $(TMPDIR)/main.o $(TMPDIR)/lexer.o $(TMPDIR)/arena.o $(TMPDIR)/string.o

main.o: $(SRCDIR)/Main.c
	$(CC) $(COMPFLAGS) -o $(TMPDIR)/main.o -c $<

lexer.o: $(SRCDIR)/lexer/Lexer.c
	$(CC) $(COMPFLAGS) -o $(TMPDIR)/lexer.o -c $<

#parser.o: $(SRCDIR)/parser/Parser.c
#	$(CC) $(COMPFLAGS) -o $(TMPDIR)/parser.o -c $<

#typechecker.o: $(SRCDIR)/typechecker/Typechecker.c
#	$(CC) $(COMPFLAGS) -o $(TMPDIR)/typechecker.o -c $<

#codegen.o: $(SRCCODEGEN)/Codegen.c
#	$(CC) $(COMPFLAGS) -o $(TMPDIR)/codegen.o -c $(SRCCODEGEN)/Codegen.c


# Utils

arena.o: $(SRCUTILS)/Arena.c
	$(CC) $(COMPFLAGS) -o $(TMPDIR)/arena.o -c $<

string.o: $(SRCUTILS)/String.c
	$(CC) $(COMPFLAGS) -o $(TMPDIR)/string.o -c $<


# Tests

tests: test.o
	$(CC) $(COMPFLAGS) -o $(EXETEST) $(TMPDIR)/test.o

test.o: $(SRCTESTS)/Test.c
	$(CC) $(COMPFLAGS) -o $(TMPDIR)/test.o -c $<

clean:
	rm -f $(TMPDIR)/*.o $(EXE) $(EXETEST)
