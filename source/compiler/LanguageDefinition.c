#include "LanguageDefinition.h"

LanguageDefinition* buildLanguageDefinitions(Arena* ar) {
    /** All the symbols an operator may start with. The ':' is absent because it's handled by 'lexColon'.
    * The '-' is absent because it's handled by 'lexMinus'.
    */
    const int operatorStartSymbols[] = {
        aExclamation, aAmpersand, aPlus, aDivBy, aTimes,
        aPercent, aCaret, aPipe, aGreaterThan, aLessThan, aQuestion, aEqual,
        aSharp, aApostrophe
    };

    /*
    * Definition of the operators with info for those that act as functions. The order must be exactly the same as
    * OperatorType and "operatorSymbols".
    */
    const OpDef operatorDefinitions[] = {
        (OpDef){ .name = allocateLiteral(ar, "!="), .bytes = {}, .precedence=11, .arity=2, .bindingIndex=0 }


        // OpDef("!", prefixPrec, 1, false, 1),  OpDef("#", prefixPrec, 1, false, 2),
        // OpDef("%", 20, 2, true, 3),                    OpDef("&&", 9, 2, true, 4),           OpDef("&", prefixPrec, 1, false, 5),
        // OpDef("'", prefixPrec, 1, false, 6),           OpDef("**", 23, 2, true, 7),          OpDef("*", 20, 2, true, 8),
        // OpDef("++", functionPrec, 1, false, 9, true),  OpDef("+", 17, 2, true, 10),          noFun /* -> */,
        // OpDef("--", functionPrec, 1, false, 11, true), OpDef("-", 17, 2, true, 12),          OpDef("..<", 1, 2, false, 13),
        // OpDef("..", 1, 2, false, 14),                  OpDef("/", 20, 2, true, 15),          noFun /* := */,
        // noFun /* : */,                                 OpDef("<=", 12, 2, false, 16),        OpDef("<<", 14, 2, true, 17),
        // noFun /* <- */,                                OpDef("<", 12, 2, false, 18),         OpDef("==", 11, 2, false, 19),
        // noFun /* = */,                                 OpDef(">=<=", 12, 3, false, 20),      OpDef(">=<", 12, 3, false, 21),
        // OpDef("><=", 12, 3, false, 22),                OpDef("><", 12, 3, false, 23),        OpDef(">=", 12, 2, false, 24),
        // OpDef(">>", 14, 2, true, 25),                  OpDef(">", 12, 2, false, 26),         OpDef("?", prefixPrec, 1, false, 27),
        // noFun /* \ */,                                 OpDef("^", prefixPrec, 1, false, 28), OpDef("||", 3, 2, true, 29),
        // noFun /* | */,
    };


    // /**
    // * This is an array of 4-byte arrays containing operator byte sequences.
    // * Sorted: 1) by first byte ASC 2) by second byte DESC 3) third byte DESC 4) fourth byte DESC.
    // * It's used to lex operator symbols using left-to-right search.
    // */
    // const int operatorSymbols[] = {
    //     aExclamation, aEqual, 0, 0,          aExclamation, 0, 0, 0,                    aSharp, 0, 0, 0,
    //     aPercent, 0, 0, 0,                   aAmpersand, aAmpersand, 0, 0,             aAmpersand, 0, 0, 0,
    //     aApostrophe, 0, 0, 0,                aTimes, aTimes, 0, 0,                     aTimes, 0, 0, 0,
    //     aPlus, aPlus, 0, 0,                  aPlus, 0, 0, 0,                           aMinus, aGreaterThan, 0, 0,
    //     aMinus, aMinus, 0, 0,                aMinus, 0, 0, 0,                          aDot, aDot, aLessThan, 0,
    //     aDot, aDot, 0, 0,                    aDivBy, 0, 0, 0,                          aColon, aEqual, 0, 0,
    //     aColon, 0, 0, 0,                     aLessThan, aEqual, 0, 0,                  aLessThan, aLessThan, 0, 0,
    //     aLessThan, aMinus, 0, 0,             aLessThan, 0, 0, 0,                       aEqual, aEqual, 0, 0,
    //     aEqual, 0, 0, 0,                     aGreaterThan, aEqual, aLessThan, aEqual,  aGreaterThan, aEqual, aLessThan, 0,
    //     aGreaterThan, aLessThan, aEqual, 0,  aGreaterThan, aLessThan, 0, 0,            aGreaterThan, aEqual, 0, 0,
    //     aGreaterThan, aGreaterThan, 0, 0,    aGreaterThan, 0, 0, 0,                    aQuestion, 0, 0, 0,
    //     aBackslash, 0, 0, 0,                 aCaret, 0, 0, 0,                          aPipe, aPipe, 0, 0,
    //     aPipe, 0, 0, 0,
    // };

}

