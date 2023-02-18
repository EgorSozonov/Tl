#include "languageDefinition.h"

LanguageDefinition* buildLanguageDefinitions(Arena* ar) {
    LanguageDefinition* result = allocateOnArena(ar, sizeof(LanguageDefinition));
    /*
    * Definition of the operators with info for those that act as functions. The order must be exactly the same as
    * OperatorType.
    */
    result->countOperators = 35;
    result->operators = allocateOnArena(ar, result->countOperators*sizeof(OpDef));

    /**
    * This is an array of 4-byte arrays containing operator byte sequences.
    * Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
    * It's used to lex operator symbols using left-to-right search.
    */
    result->operators[ 0] = (OpDef){ .name = allocateLiteral(ar, "!="), .precedence=11, .arity=2,
                                    .bindingIndex=0, .bytes={aExclamation, aEqual, 0, 0 } };
    result->operators[ 1] = (OpDef){ .name = allocateLiteral(ar, "!"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=1, .bytes={aExclamation, 0, 0, 0 } };
    result->operators[ 2] = (OpDef){ .name = allocateLiteral(ar, "#"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=2, .bytes={aSharp, 0, 0, 0 } };
    result->operators[ 3] = (OpDef){ .name = allocateLiteral(ar, "$"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=3, .bytes={aDollar, 0, 0, 0 } };
    result->operators[ 4] = (OpDef){ .name = allocateLiteral(ar, "%"), .precedence=20, .arity=2, .extensible=true,
                                    .bindingIndex=4, .bytes={aPercent, 0, 0, 0 } };
    result->operators[ 5] = (OpDef){ .name = allocateLiteral(ar, "&&"), .precedence=9, .arity=2, .extensible=true,
                                    .bindingIndex=5, .bytes={aAmpersand, aAmpersand, 0, 0 } };
    result->operators[ 6] = (OpDef){ .name = allocateLiteral(ar, "&"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=6, .bytes={aAmpersand, 0, 0, 0 } };
    result->operators[ 7] = (OpDef){ .name = allocateLiteral(ar, "*"), .precedence=20, .arity=2, .extensible=true,
                                    .bindingIndex=7, .bytes={aTimes, 0, 0, 0 } };
    result->operators[ 8] = (OpDef){ .name = allocateLiteral(ar, "++"), .precedence=functionPrec, .arity=1,
                                    .bindingIndex=8, .bytes={aPlus, aPlus, 0, 0 } };
    result->operators[ 9] = (OpDef){ .name = allocateLiteral(ar, "+"), .precedence=17, .arity=2, .extensible=true,
                                    .bindingIndex=9, .bytes={aPlus, 0, 0, 0 } };
    result->operators[10] = (OpDef){ .name = allocateLiteral(ar, "--"), .precedence=functionPrec, .arity=1,
                                    .bindingIndex=10, .bytes={aMinus, aMinus, 0, 0 } };
    result->operators[11] = (OpDef){ .name = allocateLiteral(ar, "-"), .precedence=17, .arity=2, .extensible=true,
                                    .bindingIndex=11, .bytes={aMinus, 0, 0, 0 } };
    result->operators[12] = (OpDef){ .name = allocateLiteral(ar, "/"), .precedence=20, .arity=2, .extensible=true,
                                    .bindingIndex=12, .bytes={aDivBy, 0, 0, 0 } };
    result->operators[13] = (OpDef){ .name = allocateLiteral(ar, ":="), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aColon, aEqual, 0, 0 }};
    result->operators[14] = (OpDef){ .name = allocateLiteral(ar, ";<"), .precedence=1, .arity=2,
                                    .bindingIndex=13, .bytes={aSemicolon, aLessThan, 0, 0 } };
    result->operators[15] = (OpDef){ .name = allocateLiteral(ar, ";"), .precedence=1, .arity=2,
                                    .bindingIndex=14, .bytes={aSemicolon, 0, 0, 0 } };
    result->operators[16] = (OpDef){ .name = allocateLiteral(ar, "<="), .precedence=12, .arity=2,
                                    .bindingIndex=15, .bytes={aLessThan, aEqual, 0, 0 } };
    result->operators[17] = (OpDef){ .name = allocateLiteral(ar, "<<"), .precedence=14, .arity=2, .extensible=true,
                                    .bindingIndex=16, .bytes={aTimes, 0, 0, 0 } };
    result->operators[18] = (OpDef){ .name = allocateLiteral(ar, "<-"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aLessThan, aMinus, 0, 0 } };
    result->operators[19] = (OpDef){ .name = allocateLiteral(ar, "<"), .precedence=12, .arity=2,
                                    .bindingIndex=17, .bytes={aLessThan, 0, 0, 0 } };
    result->operators[20] = (OpDef){ .name = allocateLiteral(ar, "=>"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aEqual, aGreaterThan, 0, 0 } };
    result->operators[21] = (OpDef){ .name = allocateLiteral(ar, "=="), .precedence=11, .arity=2,
                                    .bindingIndex=18, .bytes={aEqual, aEqual, 0, 0 } };
    result->operators[22] = (OpDef){ .name = allocateLiteral(ar, "="), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aEqual, 0, 0, 0 } };
    result->operators[23] = (OpDef){ .name = allocateLiteral(ar, ">=<="), .precedence=12, .arity=3,
                                    .bindingIndex=19, .bytes={aGreaterThan, aEqual, aLessThan, aEqual } };
    result->operators[24] = (OpDef){ .name = allocateLiteral(ar, ">=<"), .precedence=12, .arity=3,
                                    .bindingIndex=20, .bytes={aGreaterThan, aEqual, aLessThan, 0 } };
    result->operators[25] = (OpDef){ .name = allocateLiteral(ar, "><="), .precedence=12, .arity=3,
                                    .bindingIndex=21, .bytes={aGreaterThan, aLessThan, aEqual, 0 } };
    result->operators[26] = (OpDef){ .name = allocateLiteral(ar, "><"), .precedence=12, .arity=3,
                                    .bindingIndex=22, .bytes={aGreaterThan, aLessThan, 0, 0 } };
    result->operators[27] = (OpDef){ .name = allocateLiteral(ar, ">="), .precedence=12, .arity=2,
                                    .bindingIndex=23, .bytes={aGreaterThan, aEqual, 0, 0 } };
    result->operators[28] = (OpDef){ .name = allocateLiteral(ar, ">>"), .precedence=14, .arity=2, .extensible=true,
                                    .bindingIndex=24, .bytes={aGreaterThan, aGreaterThan, 0, 0 } };
    result->operators[29] = (OpDef){ .name = allocateLiteral(ar, ">"), .precedence=12, .arity=2,
                                    .bindingIndex=25, .bytes={aGreaterThan, 0, 0, 0 } };
    result->operators[30] = (OpDef){ .name = allocateLiteral(ar, "?"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=26, .bytes={aQuestion, 0, 0, 0 } };
    result->operators[31] = (OpDef){ .name = allocateLiteral(ar, "\\"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aBackslash, 0, 0, 0 } };
    result->operators[32] = (OpDef){ .name = allocateLiteral(ar, "^"), .precedence=prefixPrec, .arity=1,
                                    .bindingIndex=27, .bytes={aCaret, 0, 0, 0 } };
    result->operators[33] = (OpDef){ .name = allocateLiteral(ar, "||"), .precedence=3, .arity=2, .extensible=true,
                                    .bindingIndex=28, .bytes={aPipe, aPipe, 0, 0 } };
    result->operators[34] = (OpDef){ .name = allocateLiteral(ar, "|"), .precedence=0, .arity=0,
                                    .bindingIndex=-1, .bytes={aPipe, 0, 0, 0 } };
    return result;
}

