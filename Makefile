#{{{ Boilerplate
.RECIPEPREFIX = /

ifndef VERBOSE
.SILENT: # Silent mode unless you run it like "make all VERBOSE=1"
endif

ifndef EXEDIR
EXEDIR = $(abspath ../../exes)
endif

ifndef DEBUGDIR
DEBUGDIR = $(abspath ../../debug)
endif
.PHONY: all clean help parserTest

CC=gcc --std=c2x
CONFIG=-g3
WARN=-Wreturn-type -Wunused-variable -Wshadow -Wfatal-errors \
-Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
-Werror=int-conversion -fstrict-flex-arrays=3 \
-Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=malloc
DEPFLAGS=
BUILDDIR=$(EXEDIR)/$(APP)

#}}}

LIBS=-lm -lgccjit
APP=prok

CFLAGS = $(CONFIG) $(WARN) $(OPT) $(DEPFLAGS) $(LIBS)

SOURCE=src/$(APP).c src/$(APP).internal.h

EXE=$(EXEDIR)/$(APP)/$(APP)




all: _bin/$(BIN) ## Build the whole project
/ @echo "========================================="
/ @echo "              BUILD SUCCESS              "
/ @echo "========================================="

run:
/ mkdir -p $(BUILDDIR)
/ $(CC) $(CFLAGS) -o $(EXE) $(SOURCE)
/ $(EXE)


clean: ## Delete cached build results
/ rm -rf _bin

parserTest:
/ . scripts/parserTest.txt

help: ## Show this help
/ @egrep -h '\s##\s' $(MAKEFILE_LIST) | sort | awk 'BEGIN {print "-- Help --";print ""; FS = ":.*?## "}; {printf "\033[32m%-10s\033[0m %s\n", $$1, $$2}'
# MAKEFILE_LIST lists the contents of this present file
# egrep selects only lines with the double sharp, they are then sorted
# BEGIN in AWK means an action to be executed once before the linewise
# FS means "field separator" - the separator between parts of a single line
# the printf looks so scary because of the ASCII color codes
