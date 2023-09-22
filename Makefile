.RECIPEPREFIX = >

PHONY: help

CC=gcc
CFLAGS=-g3 --std=c2x -Wpedantic -Wreturn-type -Wunused-variable -Wshadow -Wfatal-errors \
-Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
-Werror=int-conversion -fstrict-flex-arrays=3 \
-Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=malloc \

TARGET=_bin

%.o: %.c
> $(CC) $(CFLAGS) -o $(TARGET)/$<.o

clean
> rm $(TARGET)/*



help: ## Show this help
> @egrep -h '\s##\s' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
