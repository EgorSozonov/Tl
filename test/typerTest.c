#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>
#include "../source/tl.internal.h"
#include "tlTest.h"


extern jmp_buf excBuf;

#define in(x) prepareInput(x, a)
#define S   70000000 // A constant larger than the largest allowed file size. Separates parsed names from others

/** Must agree in order with toke types in ParserConstants.h */
//~const char* tokNames[] = {
//~    "Int", "Long", "Float", "Bool", "String", "_", "DocComment", 
//~    "id", "call", "binding", "type", "and", "or", 
//~    "(-", "expr", "assign", "reAssign", "mutate",
//~    "alias", "assert", "assertDbg", "await", "break", "catch", "continue",
//~    "defer", "each", "embed", "export", "exposePriv", "fnDef", "interface"
//~    "lambda", "meta", "package", "return", "struct", "try", "yield",
//~    "ifClause", "else", "loop", "loopCond", "if", "ifPr", "impl", "match"
//~};

private Compiler* buildLexer0(Compiler* proto, Arena *a, int totalTokens, Arr(Token) tokens) {
    Compiler* result = createLexerFromProto(&empty, proto, a);
    if (result == NULL) return result;
    StandardText stText = getStandardTextLength();
    
    for (int i = 0; i < totalTokens; i++) {
        Token tok = tokens[i];
        // offset nameIds and startBts for the standardText and standard nameIds correspondingly 
        tok.startBt += stText.lenStandardText;
        if (tok.tp == tokMutation) {
            tok.pl1 += stText.lenStandardText;
        }
        if (tok.tp == tokWord || tok.tp == tokKwArg || tok.tp == tokTypeName || tok.tp == tokStructField
            || (tok.tp == tokAccessor && tok.pl1 == tkAccDot)) {
            if (tok.pl2 < S) {
                tok.pl2 += stText.numNames;
            } else {
                tok.pl2 -= (S + stText.firstNonreserved);
            }
        } else if (tok.tp == tokCall || tok.tp == tokTypeCall) {
            if (tok.pl1 < S) {
                tok.pl1 += stText.numNames;
            } else {
                tok.pl1 -= (S + stText.firstNonreserved);
            }
        }
        add(tok, result);
    }
    result->totalTokens = totalTokens;
    
    return result;
}

#define buildLexer(toks) buildLexer0(proto, a, sizeof(toks)/sizeof(Token), toks)


void typerTest1(Compiler* proto, Arena* a) {
    Compiler* cm = buildLexer(((Token[]){ // "fn([U/2 V] lst U(Int V))"
        (Token){ .tp = tokFn, .pl1 = slParenMulti, .pl2 = 9,        .lenBts = 24 },
        (Token){ .tp = tokStmt,               .pl2 = 8, .startBt = 3, .lenBts = 20 },
        (Token){ .tp = tokBrackets,           .pl2 = 3, .startBt = 3, .lenBts = 7 },
        (Token){ .tp = tokTypeName, .pl1 = 1, .pl2 = 0, .startBt = 4,     .lenBts = 1 },
        (Token){ .tp = tokInt,                .pl2 = 2, .startBt = 6,     .lenBts = 1 },
        (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 8,     .lenBts = 1 },

        (Token){ .tp = tokWord,               .pl2 = 2, .startBt = 11, .lenBts = 3 },

        (Token){ .tp = tokTypeCall, .pl1 = 0, .pl2 = 2, .startBt = 15, .lenBts = 8 },
        (Token){ .tp = tokTypeName,           .pl2 = strInt + S, .startBt = 17, .lenBts = 3 },
        (Token){ .tp = tokTypeName,           .pl2 = 1, .startBt = 21, .lenBts = 1 }
    }));
    initializeParser(cm, proto, a);
    printLexer(cm);
    
    StandardText sta = getStandardTextLength();
    
    // type params
    push(sta.numNames, cm->typeStack);
    push(2, cm->typeStack);
    push(sta.numNames + 1, cm->typeStack);
    push(0, cm->typeStack);
    
    Token tk = cm->tokens[7];
    cm->i = 8;
    Int typeId = parseTypeName(tk, cm->tokens, cm);
    typePrint(typeId, cm);
}

void typerTest2(Compiler* proto, Arena* a) {
    Compiler* cm = buildLexer(((Token[]){ // "fn(lst A(L(A(Double))))"
        (Token){ .tp = tokFn, .pl1 = slParenMulti, .pl2 = 9,        .lenBts = 24 },
        (Token){ .tp = tokStmt,               .pl2 = 8, .startBt = 3, .lenBts = 20 },
        (Token){ .tp = tokWord,               .pl2 = 2, .startBt = 11, .lenBts = 3 },

        (Token){ .tp = tokTypeCall, .pl1 = (strA + S), .pl2 = 3, .startBt = 15, .lenBts = 8 },
        (Token){ .tp = tokTypeCall, .pl1 = (strL + S), .pl2 = 2, .startBt = 15, .lenBts = 8 },
        (Token){ .tp = tokTypeCall, .pl1 = (strA + S), .pl2 = 1, .startBt = 15, .lenBts = 8 },
        (Token){ .tp = tokTypeName,           .pl2 = strDouble + S, .startBt = 17, .lenBts = 3 },
    }));
    initializeParser(cm, proto, a);
    printLexer(cm);
    
    Token tk = cm->tokens[3];
    cm->i = 4;
    Int typeId = parseTypeName(tk, cm->tokens, cm);
    typePrint(typeId, cm);
}
    
int main() {
    printf("----------------------------\n");
    printf("--  TYPER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    Compiler* proto = createProtoCompiler(a);

    if (setjmp(excBuf) == 0) {
//~    typerTest1(proto, a);
        typerTest2(proto, a);
    } 
    deleteArena(a);
}
