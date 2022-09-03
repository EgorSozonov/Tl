# Usage: make build

.PHONY = all clean
CC=gcc                        # compiler to use
COMPFLAGS=-std=c11
LINKERFLAGS = -lm # Link the math library
ODIR = _temp

SOURCES_COMMON=$(wildcard utils/*.c)
SOURCES_COMPILER=$(wildcard interpreter/*.c)
OBJECTS_COMPILER=$(SOURCES_COMPILER:%.c=%)

SOURCES_INTERPRETER=$(wildcard interpreter/*.c)
OBJECTS_INTERPRETER=$(SOURCES_INTERPRETER:%.c=%)


EXE_COMPILER = tlco
EXE_INTERPRETER = tlvm

all: ${BINS}
%: %.o
	@echo "Checking.."
	${CC} ${LINKERFLAG} $< -o $@
# $< is patterned to match prerequisites and $@ matches the target
# So the last line expands to: gcc -lm foo.o -o foo

%.o: %.c
	@echo "Creating object.."
	${CC} ${COMPFLAGS} -c $<

# patsubst % substitutes the whole argument (3rd expression) with the 2nd expression
OBJS = $(patsubst %,$(ODIR)/%,$(OBJECTS_COMPILER)) # Move all .o files to temp dir.

$(EXE_COMPILER): $(OBJECTS)
	$(CC) $(LINKERFLAGS) $(COMPFLAGS) $(OBJECTS) -o _bin/$@

$(EXE_INTERPRETER): $(OBJECTS)
	$(CC) $(LINKERFLAGS) $(COMPFLAGS) $(OBJECTS) -o _bin/$@

