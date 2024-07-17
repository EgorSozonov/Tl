#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "../src/ors.internal.h"
#include "orsTest.h"

//{{{ Utils

typedef struct { //:RuntimeTest
    String* name;
    Runner* test;
    Runner* control;
    Bool compareLocsToo;
} RuntimeTest;


typedef struct {
    String* name;
    Int totalTests;
    ParserTest* tests;
} ParserTestSet;

typedef struct { // :TestEntityImport
    Int nameInd; // 0, 1 or 2. Corresponds to the "foobarinner" in standardText
    Int typeInd; // index in the intermediary array of types that is imported alongside
} TestEntityImport;

#define S   70000000 // A constant larger than the largest allowed file size.
                     // Separates parsed entities from others
#define I  140000000 // The base index for imported entities/overloads
#define S2 210000000 // A constant larger than the largest allowed file size.
                     //  Separates parsed entities from others
#define O  280000000 // The base index for operators


private ParserTestSet* createTestSet0(String* name, Arena *a, int count, Arr(ParserTest) tests) {
    ParserTestSet* result = allocateOnArena(sizeof(ParserTestSet), a);
    result->name = name;
    result->totalTests = count;
    result->tests = allocateOnArena(count*sizeof(ParserTest), a);
    if (result->tests == NULL) return result;

    for (int i = 0; i < count; i++) {
        result->tests[i] = tests[i];
    }
    printString(tests[0].control->stats.errMsg);
    return result;
}

#define createTestSet(n, a, tests) createTestSet0(n, a, sizeof(tests)/sizeof(ParserTest), tests)


private Int tryGetOper0(Int opType, Int typeId, Compiler* protoOvs) {
// Try and convert test value to operator entityId
    Int entityId;
    Int ovInd = -protoOvs->activeBindings[opType] - 2;
    bool foundOv = findOverload(typeId, ovInd, &entityId, protoOvs);
    if (foundOv)  {
        return entityId + O;
    } else {
        return -1;
    }
}

#define oper(opType, typeId) tryGetOper0(opType, typeId, protoOvs)


private Int transformBindingEntityId(Int inp, Compiler* pr) {
    if (inp < S) { // parsed stuff
        return inp + pr->stats.countNonparsedEntities;
    } else if (inp < O){ // imported but not operators: "foo", "bar"
        return inp - I + pr->stats.countNonparsedEntities;
    } else { // operators
        return inp - O;
    }
}


private Arr(Int) importTypes(Arr(Int) types, Int countTypes, Compiler* cm) { //:importTypes
    Int countImportedTypes = 0;
    Int j = 0;
    while (j < countTypes) {
        if (types[j] == 0) {
            return NULL;
        }
        ++(countImportedTypes);
        j += types[j] + 1;
    }
    Arr(TypeId) typeIds = allocateOnArena(countImportedTypes*4, cm->aTmp);
    j = 0;
    Int t = 0;
    while (j < countTypes) {
        const Int importLen = types[j];
        const Int sentinel = j + importLen + 1;
        TypeId initTypeId = cm->types.len;

        pushIntypes(importLen + 2, cm); // +3 because the header takes 2 ints, 1 more for
                                        // the return typeId
        typeAddHeader(
           (TypeHeader){.sort = sorFunction, .tyrity = 0, .arity = importLen - 1,
                        .nameAndLen = -1}, cm);
        for (Int k = j + 1; k < sentinel; k++) { // <= because there are (arity + 1) elts -
                                                 // +1 for the return type!
            pushIntypes(types[k], cm);
        }

        Int mergedTypeId = mergeType(initTypeId, cm);
        typeIds[t] = mergedTypeId;
        t++;
        j = sentinel;
    }
    return typeIds;
}

private ParserTest createTest0(String* name, String* sourceCode, Arr(Node) nodes, Int countNodes,
                               Arr(Int) types, Int countTypes, Arr(TestEntityImport) imports,
                               Int countImports, Compiler* proto, Arena* a) { //:createTest0
/** Creates a test with two parsers: one is the init parser (contains all the "imported" bindings and
pre-defined nodes), and the other is the output parser (with all the stuff parsed from source code).
When the test is run, the init parser will parse the tokens and then will be compared to the
expected output parser.
Nontrivial: this handles binding ids inside nodes, so that e.g. if the pl1 in nodBinding is 1,
it will be inserted as 1 + (the number of built-in bindings) etc */
    Compiler* test = lexicallyAnalyze(sourceCode, proto, a);
    Compiler* control = lexicallyAnalyze(sourceCode, proto, a);
    if (control->stats.wasLexerError == true) {
        return (ParserTest) {
            .name = name, .test = test, .control = control, .compareLocsToo = false };
    }
    initializeParser(control, proto, a);
    initializeParser(test, proto, a);
    Arr(Int) typeIds = importTypes(types, countTypes, control);
    importTypes(types, countTypes, test);

    StandardText stText = getStandardTextLength();
    if (countImports > 0) {
        Arr(EntityImport) importsForControl = allocateOnArena(countImports*sizeof(EntityImport), a);
        Arr(EntityImport) importsForTest = allocateOnArena(countImports*sizeof(EntityImport), a);

        for (Int j = 0; j < countImports; j++) {
            importsForControl[j] = (EntityImport) {
                .name = nameOfStandard(strSentinel - 3 + imports[j].nameInd),
                .typeId = typeIds[imports[j].typeInd] };
            importsForTest[j] = (EntityImport) {
                .name = nameOfStandard(strSentinel - 3 + imports[j].nameInd),
                .typeId = typeIds[imports[j].typeInd] };
        }
        importEntities(importsForControl, countImports, control);
        importEntities(importsForTest, countImports, test);
    }

    // The control compiler
    for (Int i = 0; i < countNodes; i++) {
        Node nd = nodes[i];
        Unt nodeType = nd.tp;
        // All the node types which contain entityIds in their pl1
        if (nodeType == nodId || nodeType == nodCall || nodeType == nodBinding
                || nodeType == nodFnDef) {
            nd.pl1 = transformBindingEntityId(nd.pl1, control);
        }
        // transform pl2/pl3 if it holds NameId
        if (nodeType == nodId && nd.pl2 != -1)  {
            nd.pl2 += stText.firstParsed;
        }
        if (nodeType == nodFnDef && nd.pl3 != -1)  {
            nd.pl3 += stText.firstParsed;
        }
        addNode(nd, (SourceLoc){.startBt = 0, .lenBts = 0}, control);
    }
    return (ParserTest){ .name = name, .test = test, .control = control, .compareLocsToo = false };
}

#define createTest(name, input, nodes, types, entities) \
    createTest0((name), (input), (nodes), sizeof(nodes)/sizeof(Node), (types), sizeof(types)/4, \
    (entities), sizeof(entities)/sizeof(TestEntityImport), proto, a)

private ParserTest createTestWithError0(String* name, String* message, String* input, Arr(Node) nodes,
                            Int countNodes, Arr(Int) types, Int countTypes, Arr(TestEntityImport) entities,
                            Int countEntities, Compiler* proto, Arena* a) {
// Creates a test with two parsers where the expected result is an error in parser
    ParserTest theTest = createTest0(name, input, nodes, countNodes, types, countTypes, entities,
                                     countEntities, proto, a);
    theTest.control->stats.wasError = true;
    theTest.control->stats.errMsg = message;
    return theTest;
}

#define createTestWithError(name, errorMessage, input, nodes, types, entities) \
    createTestWithError0((name), errorMessage, (input), (nodes), sizeof(nodes)/sizeof(Node), types,\
    sizeof(types)/4, entities, sizeof(entities)/sizeof(EntityImport), proto, a)

//}}}

int void main(int argc, char** argv) {
    return 0;
}
