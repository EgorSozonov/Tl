#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../src/eyr.internal.h"
#include "../src/eyr.h"
#include "eyrTest.h"


typedef struct {
    String* name;
    String* input;
    String* expectedOutput;
} CodegenTest;


typedef struct {
    String* name;
    Int totalTests;
    CodegenTest* tests;
} CodegenTestSet;

