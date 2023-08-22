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
#define validate(cond) if(!(cond)) {++(*countFailed); return;}

private Compiler* buildLexer0(String* sourceCode, Compiler* proto, Arena *a, int totalTokens, Arr(Token) tokens) {
    Compiler* result = createLexerFromProto(&empty, proto, a);
    result->sourceCode = sourceCode;
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
        pushIntokens(tok, result);
    }
    result->totalTokens = totalTokens;
    
    return result;
}

#define buildLexer(inp, toks) buildLexer0(inp, proto, a, sizeof(toks)/sizeof(Token), toks)


void typerTest1(Compiler* proto, Arena* a) {
    Compiler* cm = buildLexer(prepareInput("fn([U/2 V] lst U(Int V))", a), ((Token[]){
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
    
    Token tk = cm->tokens.content[7];
    cm->i = 8;
    Int typeId = parseTypeName(tk, cm->tokens.content, cm);
    typePrint(typeId, cm);
}

void typerTest2(Compiler* proto, Arena* a) {
    Compiler* cm = buildLexer(prepareInput("fn(lst A(L(A(Double))))", a), ((Token[]){ // ""
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
    
    Token tk = cm->tokens.content[3];
    cm->i = 4;
    Int typeId = parseTypeName(tk, cm->tokens.content, cm);
    typePrint(typeId, cm);
}
   
void typeTest3(Compiler* proto, Arena* a) {
    Compiler* cm = buildLexer(prepareInput("fn(lst A(L(A(Double))))", a), ((Token[]){ // ""
        (Token){ .tp = tokFn, .pl1 = slParenMulti, .pl2 = 9,        .lenBts = 24 },
        (Token){ .tp = tokStmt,               .pl2 = 8, .startBt = 3, .lenBts = 20 },
        (Token){ .tp = tokWord,               .pl2 = 2, .startBt = 11, .lenBts = 3 },

        (Token){ .tp = tokTypeCall, .pl1 = (strA + S), .pl2 = 3, .startBt = 15, .lenBts = 8 },
        (Token){ .tp = tokTypeCall, .pl1 = (strL + S), .pl2 = 2, .startBt = 15, .lenBts = 8 },
        (Token){ .tp = tokTypeCall, .pl1 = (strA + S), .pl2 = 1, .startBt = 15, .lenBts = 8 },
        (Token){ .tp = tokTypeName,           .pl2 = strDouble + S, .startBt = 17, .lenBts = 3 }
    }));
    initializeParser(cm, proto, a);
    //parseToplevelSignature(cm->tokens[0], toplevelSignatures, cm);
} 
   
void skipTest(Int* countFailed, Compiler* proto, Arena* a) {
    /// Checking that we know how to skip a whole type call    
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a); 
    Int startInd = cm->types.length; 
    Int resultInd = startInd;
    Int typeTuple = cm->activeBindings[stToNameId(strTu)]; 
    Int typeList = cm->activeBindings[stToNameId(strL)]; 
    Int typeString = cm->activeBindings[stToNameId(strString)]; 
    // Tu(Str Tu(L(L(Int)) Tu(?1_1(Int) L(?2))))
    // Should skip 11 type nodes 
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm); 
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 0, cm); 
    typeSkipNode(&resultInd, cm); 
    validate(resultInd == (startInd + 11)); 
}
    
//{{{ Generics 
void testGenericsIntersect1(Int* countFailed, Compiler* proto, Arena* a) {
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a); 
    Int typeTuple = cm->activeBindings[stToNameId(strTu)]; 
    Int typeList = cm->activeBindings[stToNameId(strL)]; 
    Int typeString = cm->activeBindings[stToNameId(strString)]; 
    
    // Tu(Str Tu(L(L(Int)) Tu(?1_1(Int) L(?2))))
    Int type1 = cm->types.length; 
    pushIntypes(11, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm); 
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 0, cm); 
    
    // Tu(Str Tu(?1_1(L(Int)) Tu(String L(Int))))
    Int type2 = cm->types.length; 
    pushIntypes(10, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm); 
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(stToNameId(strString), cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);

    bool intersect = typeGenericsIntersect(type1, type2, cm);
    print("1 intersect %d", intersect)
    validate(intersect) 
} 
    
void testGenericsIntersect2(Int* countFailed, Compiler* proto, Arena* a) {
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a); 
    Int typeTuple = cm->activeBindings[stToNameId(strTu)]; 
    Int typeList = cm->activeBindings[stToNameId(strL)]; 
    Int typeArray = cm->activeBindings[stToNameId(strA)]; 
    Int typeString = cm->activeBindings[stToNameId(strString)]; 
    
    // Tu(Str Tu(L(L(Int)) Tu(?1_1(Int) L(?2))))
    // vs 
    // Tu(Str Tu(?1_1(L(Int)) Tu(String A(Int))))
    Int type1 = cm->types.length; 
    pushIntypes(13, cm);
    pushIntypes(0, cm); // these next 2 lines stand for the tag and name, unimportant for this test
    pushIntypes(0, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm); 
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 0, cm); 
    
    Int type2 = cm->types.length; 
    pushIntypes(12, cm);
    pushIntypes(0, cm);
    pushIntypes(0, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm); 
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(stToNameId(strString), cm);
    typeAddTypeCall(typeArray, 1, cm);
    pushIntypes(stToNameId(strInt), cm);

    bool intersect = typeGenericsIntersect(type1, type2, cm);
    print("2 intersect %d", intersect)
    validate(!intersect) 
} 
   
void testGenericsSatisfies1(Int* countFailed, Compiler* proto, Arena* a) {
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a); 
    Int typeTuple = cm->activeBindings[stToNameId(strTu)]; 
    Int typeList = cm->activeBindings[stToNameId(strL)]; 
    Int typeArray = cm->activeBindings[stToNameId(strA)]; 
    Int typeString = cm->activeBindings[stToNameId(strString)]; 
    
    // Tu(Str Tu(L(?1_1(Int)) Tu(?1_1(Int) L(?2))))
    // vs 
    // Tu(Str Tu(?1_1(L(Int)) Tu(String A(Int))))
    Int type1 = cm->types.length; 
    pushIntypes(13, cm);
    pushIntypes(0, cm); // these next 2 lines stand for the tag and name, unimportant for this test
    pushIntypes(0, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm); 
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 0, cm); 
    
    Int type2 = cm->types.length; 
    pushIntypes(12, cm);
    pushIntypes(0, cm);
    pushIntypes(0, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm); 
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(stToNameId(strString), cm);
    typeAddTypeCall(typeArray, 1, cm);
    pushIntypes(stToNameId(strInt), cm);

        
} 

//}}} 
    
int main() {
    printf("----------------------------\n");
    printf("--  TYPER TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    Compiler* proto = createProtoCompiler(a);
    Int countFailed = 0;
    if (setjmp(excBuf) == 0) {
//~    typerTest1(proto, a);
//~        typerTest2(proto, a);
//~        skipTest(&countFailed, proto, a); 
//~        testGenericsIntersect1(&countFailed, proto, a); 
        testGenericsIntersect2(&countFailed, proto, a); 
    } else {
        print("An exception was thrown in the tests") 
    }
    if (countFailed > 0) {
        print("") 
        print("-------------------------") 
        print("Count of failed tests %d", countFailed);
    }
    deleteArena(a);
}
