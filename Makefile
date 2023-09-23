.RECIPEPREFIX = /

ifndef VERBOSE
.SILENT: # Silent mode unless you run it like "make all VERBOSE=1"
endif

.PHONY: all clean help

CC=gcc --std=c2x
CONFIG=-g3
WARN=-Wpedantic -Wreturn-type -Wunused-variable -Wshadow -Wfatal-errors \
-Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
-Werror=int-conversion -fstrict-flex-arrays=3 \
-Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=malloc
DEPFLAGS=
LIBS=-lm
BIN=tlc

CFLAGS = $(CONFIG) $(WARN) $(OPT) $(DEPFLAGS) $(LIBS)

SOURCE=$(wildcard src/*.c)

OBJ=$(addprefix _bin/cache/, $(notdir $(SOURCE:.c=.o)))



all: _bin/$(BIN) ## Build the whole project
/ @echo "========================================="
/ @echo "              BUILD SUCCESS              "
/ @echo "========================================="

_bin/cache: | _bin
/ mkdir -p _bin/cache

$(OBJ): $(SOURCE) | _bin/cache
/ $(CC) -c $^ $(CFLAGS) -o $@
# $^ means "all prerequisites" and $@ means "current target"

_bin/$(BIN): $(OBJ) | _bin
/ $(CC)  $^ -o $@ $(CFLAGS)

_bin:
/ mkdir _bin

clean: ## Delete cached build results
/ rm -rf _bin

help: ## Show this help
/ @egrep -h '\s##\s' $(MAKEFILE_LIST) | sort | awk 'BEGIN {print "-- Help --";print ""; FS = ":.*?## "}; {printf "\033[32m%-10s\033[0m %s\n", $$1, $$2}'
# MAKEFILE_LIST lists the contents of this present file
# egrep selects only lines with the double sharp, they are then sorted
# BEGIN in AWK means an action to be executed once before the linewise
# FS means "field separator" - the separator between parts of a single line
# the printf looks so scary because of the ASCII color codes
