#{{{ Params

.RECIPEPREFIX = /

ifndef VERBOSE
.SILENT: # Silent mode unless you run it like "make all VERBOSE=1"
endif

.PHONY: all clean help lexerTest parserTest codegenTest tests

CC=gcc --std=c2x
CONFIG=-g3
WARN=-Wpedantic -Wreturn-type -Wunused-variable -Wshadow -Wfatal-errors \
    -Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
    -Werror=int-conversion -fstrict-flex-arrays=3 \
    -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=malloc 
SANITIZE=-fsanitize=address # include it occasionally
INCLUDES=-iquote src
OPT=
LIBS=-lm
APP=eyr

RELEASE_FLAGS = $(CONFIG) $(WARN) $(OPT) $(DEPFLAGS) $(INCLUDES)
COMPILE = $(CC) $(RELEASE_FLAGS) $(LIBS)


TEST_FLAGS = -g3 -DTEST -DDEBUG -DSAFETY 
TEST_INCLUDES = -iquote test
TEST_COMPILE = $(CC) $(RELEASE_FLAGS) $(TEST_FLAGS) $(TEST_INCLUDES) $(LIBS)

DEBUG_DIR ?= $(HOME)/debug
DEBUG_TGT=$(realpath $(DEBUG_DIR)/$(APP))
EXE=$(DEBUG_TGT)/$(APP)

#}}}
#{{{ Commands

$(DEBUG_TGT):
/ mkdir -p $(DEBUG_TGT)


all: $(DEBUG_TGT) ## Build the whole compiler
/ clear
/ $(COMPILE) -o $(EXE) src/$(APP).c
/ @echo "_________________________________________"
/ @echo "|            BUILD SUCCESS              |"
/ @echo "========================================="
/ $(EXE)


clean: ## Delete cached build results
/ test -f $(DEBUG_TGT) | rm $(DEBUG_TGT)/ *


testLexer: $(DEBUG_TGT) ## Test the lexical analyzer
/ $(TEST_COMPILE) -DLEXER_TEST -o $(DEBUG_TGT)/lexerTest test/lexerTest.c src/$(APP).c
/ $(DEBUG_TGT)/lexerTest


testParser: $(DEBUG_TGT) ## Test the parser & typechecker
/ $(TEST_COMPILE) -DPARSER_TEST -o $(DEBUG_TGT)/parserTest test/parserTest.c src/$(APP).c
/ $(DEBUG_TGT)/parserTest


testCodegen: $(DEBUG_TGT) ## Test the code generator
/ $(TEST_COMPILE) -DCODEGEN_TEST -o $(DEBUG_TGT)/codegenTest test/codegenTest.c src/$(APP).c
/ $(DEBUG_TGT)/codegenTest


tests: | testLexer testParser testCodegen ## Run all tests

#}}}
#{{{ Meta

help: ## Show this help
/ @egrep -h '\s##\s' $(MAKEFILE_LIST) | sort | awk 'BEGIN {print "-- Help --";print ""; FS = ":.*?## "}; {printf "\033[32m%-10s\033[0m %s\n", $$1, $$2}'
/ echo
# MAKEFILE_LIST lists the contents of this present file
# egrep selects only lines with the double sharp, they are then sorted
# BEGIN in AWK means an action to be executed once before the linewise
# FS means "field separator" - the separator between parts of a single line
# the printf looks so scary because of the ASCII color codes

#}}}
