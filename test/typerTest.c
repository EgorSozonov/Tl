//{{{ Includes

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>
#include "../source/tl.internal.h"
#include "tlTest.h"

//}}}
//{{{ Utils

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
        tok.startBt += stText.len;
        if (tok.tp == tokMutation) {
            tok.pl1 += stText.len;
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

    return result;
}

#define buildLexer(inp, toks) buildLexer0(inp, proto, a, sizeof(toks)/sizeof(Token), toks)

//}}}
//{{{ Parsing functions

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

//}}}
//{{{ Generics

void skipTest(Int* countFailed, Compiler* proto, Arena* a) {
    /// Checking that we know how to skip a whole type call
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a);
    Int startInd = cm->types.len;
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

//{{{ Intersections

void testGenericsIntersect1(Int* countFailed, Compiler* proto, Arena* a) {
    /// These should not intersect because ?1_1(Int) cannot be unified with String
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a);
    Int typeTuple = cm->activeBindings[stToNameId(strTu)];
    Int typeList = cm->activeBindings[stToNameId(strL)];
    Int typeString = cm->activeBindings[stToNameId(strString)];

    // Tu(Str Tu(L(L(Int)) Tu(?1_1(Int) L(?2))))
    // vs
    // Tu(Str Tu(?1_1(L(Int)) Tu(String L(Int))))
    Int type1 = cm->types.len;
    pushIntypes(15, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 2}, cm);
    pushIntypes(1, cm); // arity of first param
    pushIntypes(0, cm); // arity of second param
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

    Int type2 = cm->types.len;
    pushIntypes(14, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 1}, cm);
    pushIntypes(1, cm); // arity of first param
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
    validate(!intersect)
}

void testGenericsIntersect2(Int* countFailed, Compiler* proto, Arena* a) {
    /// These should not match because L is different from A near the end
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a);
    Int typeTuple = cm->activeBindings[stToNameId(strTu)];
    Int typeList = cm->activeBindings[stToNameId(strL)];
    Int typeArray = cm->activeBindings[stToNameId(strA)];
    Int typeString = cm->activeBindings[stToNameId(strString)];

    // Tu(Str Tu(L(L(Int)) Tu(?1_1(Int) L(?2))))
    // vs
    // Tu(Str Tu(?1_1(L(Int)) Tu(L(Int) A(Int))))
    Int type1 = cm->types.len;
    pushIntypes(15, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 2}, cm);
    pushIntypes(0, cm); // name, unused here
    pushIntypes(1, cm); // arities of the type params
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

    Int type2 = cm->types.len;
    pushIntypes(14, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 1}, cm);
    pushIntypes(1, cm); // arities of the type params
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeArray, 1, cm);
    pushIntypes(stToNameId(strInt), cm);

    bool intersect = typeGenericsIntersect(type1, type2, cm);
    validate(!intersect)
}

void testGenericsIntersect3(Int* countFailed, Compiler* proto, Arena* a) {
    /// These should not intersect because of different parameter arities
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a);
    Int typeList = cm->activeBindings[stToNameId(strL)];

    // L(?1_2(Int Int))
    // vs
    // L(?1_1(Int))
    Int type1 = cm->types.len;
    pushIntypes(7, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 1}, cm);
    pushIntypes(2, cm); // arities of the type params
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 2, cm);
    pushIntypes(stToNameId(strInt), cm);
    pushIntypes(stToNameId(strInt), cm);

    Int type2 = cm->types.len;
    pushIntypes(6, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 1}, cm);
    pushIntypes(1, cm); // arities of the type params
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    bool intersect = typeGenericsIntersect(type1, type2, cm);
    validate(!intersect)
}

//}}}

void testGenericsSatisfies1(Int* countFailed, Compiler* proto, Arena* a) {
    /// This doesn't satisfy because a single param can't unify with L and A simultaneously
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a);
    Int typeTuple = cm->activeBindings[stToNameId(strTu)];
    Int typeList = cm->activeBindings[stToNameId(strL)];
    Int typeArray = cm->activeBindings[stToNameId(strA)];
    Int typeString = cm->activeBindings[stToNameId(strString)];

    // Tu(Str Tu(L(?1_1(Int)) Tu(?1_1(Int) L(?2))))
    // vs
    // Tu(Str Tu(L(A(Int)) Tu(L(Int) L(Int))))
    Int type1 = cm->types.len;
    pushIntypes(15, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 2}, cm);
    pushIntypes(1, cm); // arities of the type params
    pushIntypes(0, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 0, cm);

    Int type2 = cm->types.len;
    pushIntypes(13, cm);
    typeAddHeader((TypeHeader){.sort = sorConcrete, .arity = 0}, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeCall(typeArray, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);

    StackInt* result = typeSatisfiesGeneric(type2, type1, cm);
    validate(result == NULL);
}

void testGenericsSatisfies2(Int* countFailed, Compiler* proto, Arena* a) {
    /// This type satisfies
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a);
    Int typeTuple = cm->activeBindings[stToNameId(strTu)];
    Int typeList = cm->activeBindings[stToNameId(strL)];
    Int typeArray = cm->activeBindings[stToNameId(strA)];
    Int typeString = cm->activeBindings[stToNameId(strString)];

    // Tu(Str Tu(L(?1_1(Int)) Tu(?1_1(Int) L(?2))))
    // vs
    // Tu(Str Tu(L(A(Int)) Tu(A(Int) L(Int))))
    Int type2 = cm->types.len;
    pushIntypes(13, cm);
    typeAddHeader((TypeHeader){.sort = sorConcrete, .arity = 0}, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeCall(typeArray, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeArray, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strInt), cm);

    Int generic = cm->types.len;
    pushIntypes(15, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 2}, cm);
    pushIntypes(1, cm); // arities of the type params
    pushIntypes(0, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeParam(0, 1, cm);
    pushIntypes(stToNameId(strInt), cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(1, 0, cm);

    StackInt* result = typeSatisfiesGeneric(type2, generic, cm);

    validate(result != NULL && result->len == 2);
    validate(result->cont[0] == typeArray && result->cont[1] == (strInt - strFirstNonReserved))
}

void testGenericsSatisfies3(Int* countFailed, Compiler* proto, Arena* a) {
    /// This type satisfies 3 type params of different arities
    Compiler* cm = createLexerFromProto(&empty, proto, a);
    initializeParser(cm, proto, a);
    Int typeTuple = cm->activeBindings[stToNameId(strTu)];
    Int typeList = cm->activeBindings[stToNameId(strL)];
    Int typeString = cm->activeBindings[stToNameId(strString)];

    // Tu(Str ?3_2(L(?1_1(?2)) Double))
    // vs
    // Tu(Str Tu(L(L(String)) Double))
    Int type2 = cm->types.len;
    pushIntypes(9, cm);
    typeAddHeader((TypeHeader){.sort = sorConcrete, .arity = 0}, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeCall(typeList, 1, cm);
    pushIntypes(stToNameId(strString), cm);
    pushIntypes(stToNameId(strDouble), cm);

    Int generic = cm->types.len;
    pushIntypes(12, cm);
    typeAddHeader((TypeHeader){.sort = sorPartial, .arity = 3}, cm);
    pushIntypes(1, cm); // arities of the type params
    pushIntypes(0, cm);
    pushIntypes(2, cm);
    typeAddTypeCall(typeTuple, 2, cm);
    pushIntypes(typeString, cm);
    typeAddTypeParam(2, 2, cm);
    typeAddTypeCall(typeList, 1, cm);
    typeAddTypeParam(0, 1, cm);
    typeAddTypeParam(1, 0, cm);
    pushIntypes(stToNameId(strDouble), cm);

    StackInt* result = typeSatisfiesGeneric(type2, generic, cm);

    validate(result != NULL && result->len == 3);
    validate(result->cont[0] == typeList
            && result->cont[1] == (strString - strFirstNonReserved)
            && result->cont[2] == typeTuple)
}
//}}}
//{{{ Parsing structs

void structTest1(Int* countFailed, Compiler* proto, Arena* a) {
    Compiler* cm = lexicallyAnalyze(prepareInput("Foo = (.id Int .name String)", a), proto, a);
    initializeParser(cm, proto, a);
    printLexer(cm);
    cm->i = 2;
    print("types length before %d", cm->types.len)
    Int newType = typeDef(cm->tokens.cont[0], false, cm->tokens.cont, cm);
    print("newType %d", newType)
    typePrint(newType, cm);
    return;

    // type params
//~    push(sta.numNames, cm->typeStack);
//~
//~    Token tk = cm->tokens.content[7];
//~    cm->i = 8;
//~    Int typeId = parseTypeName(tk, cm->tokens.content, cm);
//~    typePrint(typeId, cm);
}


void structTest2(Int* countFailed, Compiler* proto, Arena* a) {
    Compiler* cm = lexicallyAnalyze(prepareInput(
                "Foo = (.id Int .pl Tu(Int (.id String)))", a), proto, a);
    initializeParser(cm, proto, a);
    printLexer(cm);
    //StandardText sta = getStandardTextLength();
    cm->i = 2;
    Int newType = typeDef(cm->tokens.cont[0], false, cm->tokens.cont, cm);
    print("\nnew Type has id = %d", newType)
    typePrint(newType, cm);
    print("anonymous struct:")
    typePrint(111, cm);
    print("the Tu(...):")
    typePrint(116, cm);
}


void structTest3(Int* countFailed, Compiler* proto, Arena* a) {
    Compiler* cm = lexicallyAnalyze(prepareInput(
                "Foo = (.callback F(Int Int => (.id Int .breadth Double)))", a), proto, a);
    initializeParser(cm, proto, a);
    printLexer(cm);
    cm->i = 2;
    Int newType = typeDef(cm->tokens.cont[0], false, cm->tokens.cont, cm);
    print("\nnew Type %d", newType)
    typePrint(newType, cm);
    print("fn type")
    typePrint(118, cm);
    print("anon struct type")
    typePrint(111, cm);
    return;
}

//}}}
//{{{ Main

int main() {
    printf("----------------------------\n");
    printf("--  TYPES TEST  --\n");
    printf("----------------------------\n");
    Arena *a = mkArena();
    Compiler* proto = createProtoCompiler(a);
    Int countFailed = 0;
    if (setjmp(excBuf) == 0) {
//~        typerTest1(proto, a);
//~        typerTest2(proto, a);

//~        skipTest(&countFailed, proto, a);

//~        testGenericsIntersect1(&countFailed, proto, a);
//~        testGenericsIntersect2(&countFailed, proto, a);
//~        testGenericsIntersect3(&countFailed, proto, a);

//~        testGenericsSatisfies1(&countFailed, proto, a);
//~        testGenericsSatisfies2(&countFailed, proto, a);
//~        testGenericsSatisfies3(&countFailed, proto, a);

        structTest1(&countFailed, proto, a);
//~        structTest2(&countFailed, proto, a);
//~        structTest3(&countFailed, proto, a);
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
//}}}
