.RECIPEPREFIX = /

ifndef VERBOSE
.SILENT: # Silent mode unless you run it like "make all VERBOSE=1"
endif

.PHONY: all clean help parserTest

CC=gcc --std=c2x
CONFIG=-g3
WARN=-Wpedantic -Wreturn-type -Wunused-variable -Wshadow -Wfatal-errors \
    -Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
    -Werror=int-conversion -fstrict-flex-arrays=3 \
    -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=malloc 
SANITIZE=-fsanitize=address
INCLUDES="-I src"
OPT="-DTRACE"
LIBS=-lm
APP=eyr

CFLAGS= $(CONFIG) $(WARN) $(OPT) $(DEPFLAGS) $(INCLUDES) $(LIBS)
#CFLAGS= $(CONFIG) $(WARN) $(OPT) $(DEPFLAGS) $(INCLUDES) $(SANITIZE) $(LIBS)
TARGET=$(DEBUGDIR)/$(APP)/$(APP)

all: ## Build the whole project
/ clear
/ mkdir -p $(DEBUGDIR)/$(APP)
/ $(CC) $(CFLAGS) -o $(TARGET) src/$(APP).c
/ @echo "_________________________________________"
/ @echo "|            BUILD SUCCESS              |"
/ @echo "========================================="
/ $(TARGET)

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

parserTest:
/ . scripts/parserTest.txt

# grep -Pzo '//CUTSTART\n[\s\S.]*?\n//CUTEND' eyr.internal.h

help: ## Show this help
/ @egrep -h '\s##\s' $(MAKEFILE_LIST) | sort | awk 'BEGIN {print "-- Help --";print ""; FS = ":.*?## "}; {printf "\033[32m%-10s\033[0m %s\n", $$1, $$2}'
# MAKEFILE_LIST lists the contents of this present file
# egrep selects only lines with the double sharp, they are then sorted
# BEGIN in AWK means an action to be executed once before the linewise
# FS means "field separator" - the separator between parts of a single line
# the printf looks so scary because of the ASCII color codes
