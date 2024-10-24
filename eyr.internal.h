#ifndef EYR_INTERNAL_H
#define EYR_INTERNAL_H
//{{{ Utils

#define Int int32_t
#define Long int64_t
#define Ulong uint64_t
#define Short int16_t
#define Ushort uint16_t
#define StackInt Stackint32_t
#define StackUnt Stackuint32_t
#define InListUlong InListuint64_t
#define InListUns InListuint32_t
#define Any void
#define private static
#define Byte uint8_t
#define Bool bool
#define Arr(T) T*
#define Unt uint32_t
#define null NULL
#define NameId int32_t // name index (in @stringTable)
#define NameLoc uint32_t // 8 bit of length, 24 bits of startBt (in @standardText or @externalText)
#define EntityId int32_t
#define TypeId int32_t
#define FirstArgTypeId int32_t
#define OuterTypeId int32_t
#ifdef TEST
    #define testable
#else
    #define testable static
#endif
#define OUT // the "out" parameters and args in functions
#define BIG 70000000
#define LOWER24BITS 0x00FFFFFF
#define LOWER26BITS 0x03FFFFFF
#define LOWER16BITS 0x0000FFFF
#define LOWER32BITS 0x00000000FFFFFFFF
#define PENULTIMATE8BITS 0xFF00
#define THIRTYFIRSTBIT 0x40000000
#define MAXTOKENLEN 67108864 // 2^26
#define SIXTEENPLUSONE 65537 // 2^16 + 1
#define LEXER_INIT_SIZE 1000
#define ei else if
#define print(...) \
  printf(__VA_ARGS__);\
  printf("\n");

#define dg(...) \
  printf(__VA_ARGS__);\
  printf("\n");

typedef struct ArenaChunk ArenaChunk;
typedef struct Arena Arena;


testable void printStringNoLn(String s);
testable void printString(String s);

constexpr String empty = {.cont = null, .len = 0};
testable String str(const char* content);
testable Bool endsWith(String a, String b);

#define s(lit) str(lit)

//}}}
//{{{ Lexer

typedef struct { // :Token
    Unt tp : 6;
    Unt lenBts: 26;
    Unt startBt;
    Unt pl1;
    Unt pl2;
} Token;


// Atomic (leaf) Token types
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "Node"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt          0
#define tokLong         1
#define tokDouble       2
#define tokBool         3  // pl2 = value (1 or 0)
#define tokString       4

#define tokMisc         5  // pl1 = see the misc* constants. pl2 = underscore count iff miscUscore
                           // Also stands for "Void" among the primitive types
#define tokWord         6  // pl1 = nameId (index in the string table). pl2 = 1 iff followed by ~
#define tokTypeName     7  // same as tokWord
#define tokTypeVar      8  // same as tokWord. The `'a`. May be lower- or upper-case
#define tokKwArg        9  // pl2 = same as tokWord. The ":argName"
#define tokOperator    10  // pl1 = OperatorType, one of the "opT" constants below
#define tokFieldAcc    11  // pl2 = nameId

// Statement or subexpr span types. pl2 = count of inner tokens
#define tokStmt        12  // firstSpanTokenType
#define tokDef         13  // Compile-time known constant's definition. pl1 == 2 iff type def
#define tokParens      14  // subexpressions and struct/sum type instances
#define tokTypeCall    15  // `(Tu Int Str)`
#define tokData        16  // []
#define tokAccessor    17  // x[]
#define tokAssignment  18 
#define tokAssignRight 19  // Right-hand side of assignment
#define tokAlias       20
#define tokAssert      21
#define tokBreakCont   22  // pl1 = 1 iff it's a continue
#define tokTrait       23
#define tokImport      24  // For test files and package decls
#define tokReturn      25

// Bracketed (multi-statement) token types. pl1 = spanLevel, see the "sl" constants
#define tokScope       26  // `(do ...)` firstScopeTokenType
#define tokIf          27  // `if ... { `. The If, ElseIf and Else tokens must be in that order
#define tokElseIf      28  // `eif ... {`
#define tokElse        29  // `else { `
#define tokMatch       30  // `(match ... ` pattern matching on sum type tag
#define tokFn          31  // `{{ a Int -> Str } body)`. pl1 = entityId
#define tokFnParams    32  //  `{ a Int -> Str }`. pl1 = entityId
#define tokTry         33  // `(try`
#define tokCatch       34  // `(catch e MyExc:`
#define tokImpl        35
#define tokFor         36
#define tokEach        37

#define topVerbatimTokenVariant tokString
#define topVerbatimType    tokMisc
#define firstSpanTokenType tokStmt
#define firstScopeTokenType tokScope
#define firstResumableSpanTokenType tokIf
#define countSyntaxForms (tokEach + 1)


// List of keywords that don't correspond directly to a token.
// All these numbers must be below firstKeywordToken to avoid any clashes
#define keywTrue        1
#define keywFalse       2
#define keywBreak       3
#define keywContinue    4

#define miscPub        0     // pub. It must be 0 because it's the only one denoted by a keyword
#define miscUnderscore 1     // _
#define miscArrow      2     // ->

#define assiDefinition 0
#define assiType       2

//}}}
//{{{ Parser

// AST nodes
#define nodId           7  // pl1 = index of entity, pl2 = index of name
#define nodCall         8  // pl1 = index of entity, pl2 = arg count
#define nodBinding      9  // pl1 = index of entity, pl2 = 1 iff it's a type binding
#define nodFieldAcc    10  // pl1 = nameId; after type resolution pl1 = offset
#define nodGetElemPtr  11  // get pointer to array/list elem
#define nodGetElem     12  // get value of array/list elem

// Punctuation (inner node). pl2 = node count inside (so for [span node1 node2], span.pl2 = 2)
#define nodScope       13  // if it's the outer scope of a forNode, then pl3 = length of nodes till
                           // inner scope. See parser tests for examples
#define nodExpr        14  // pl1 = 1 iff it's a composite expression (has internal var decls)
#define nodAssignment  15  // Followed by binding or complex left side. pl3 = distance to the inner
                           // right side, which is always a literal, nodExpr or a nodDataAlloc
#define nodDataAlloc   16  // pl1 = typeId, pl3 = count of elements

#define nodAssert      17  // pl1 = 1 iff it's a debug assert
#define nodBreakCont   18  // pl1 = number of label to break or continue to, -1 if none needed
                           // It's a continue iff it's >= BIG
#define nodCatch       19  // `(catch e `
#define nodImport      20  // This is for test files only, no need to import anything in main
#define nodFnDef       21  // pl1 = entityId, pl3 = nameId
#define nodDef         22  // pl1 = entityId, pl3 = nameId. For non-function compile-time consts
#define nodTrait       23
#define nodReturn      24
#define nodTry         25
#define nodFor         26  // pl1 = id of loop (unique within a function) if it needs to
                           // have a label in codegen; pl3 = number of nodes to skip to get to body

#define nodIf          27  // pl3 = length till the next elseif/else, or 0 if there are none
#define nodElseIf      28
#define nodImpl        29
#define nodMatch       30  // pattern matching on sum type tag

#define countSpanForms (nodMatch - nodScope + 1)

#define metaDoc         1  // Doc comments
#define metaDefault     2  // Default values for type arguments

#define countAstForms (nodMatch + 1)

// :OperatorType
// Values must exactly agree in order with the operatorSymbols array in the tl.c file.
// The order is defined by ASCII. Operator is bitwise <=> it ends wih dot
#define opBitwiseNeg        0 // !. bitwise negation
#define opNotEqual          1 // !=
#define opBoolNeg           2 // !
#define opSize              3 // ##
#define opToString          4 // $
#define opRemainder         5 // %
#define opBitwiseAnd        6 // &&. bitwise "and"
#define opBoolAnd           7 // &&  logical "and"
#define opTimesExt          8 // *:
#define opTimes             9 // *
#define opPlusExt          10 // +:
#define opPlus             11 // +
#define opMinusExt         12 // -:
#define opMinus            13 // -
#define opDivByExt         14 // /:
#define opIntersect        15 // /|   type-level trait intersection ?
#define opDivBy            16 // /
#define opBitShiftL        17 // <<.
#define opComparator       18 // <=>
#define opLTZero           19 // <0   less than zero
#define opShiftL           20 // <<   shift smth, e.g. an iterator, left
#define opLTEQ             21 // <=
#define opLessTh           22 // <
#define opRefEquality      23 // ===
#define opIsNull           24 // =0 //gonna cut it because will have references and deref
#define opEquality         25 // ==
#define opInterval         26 // >=<  left-inclusive interval check
#define opBitShiftR        27 // >>.  unsigned right bit shift
#define opGTZero           28 // >0   greater than zero
#define opGTEQ             29 // >=
#define opShiftR           30 // >>   shift right, e.g. an iterator. Unary op
#define opGreaterTh        31 // >
#define opNullCoalesce     32 // ?:   null coalescing operator
#define opQuestionMark     33 // ?    nullable type operator
#define opUnused           34 // @    unused yet. await? seems yep
#define opBitwiseXor       35 // ^.   bitwise XOR
#define opExponent         36 // ^    exponentiation - gonna cut it to prevent confusion
                              //      (in math everyone's used to it being right-associated)
#define opBitwiseOr        37 // ||.  bitwise or
#define opBoolOr           38 // ||   logical or
#define opGetElem          39 // Get list element
#define opGetElemPtr       40 // Get pointer to list element

#define countOperators     41
#define countRealOperators 39 // The "unreal" ones, like "a[..]", have a separate syntax

typedef struct Compiler Compiler;

typedef struct { // :Node
    Unt tp : 6;
    Unt pl3: 26;
    Int pl1;
    Int pl2;
} Node;

typedef struct { // :SourceLoc
    Int startBt;
    Int lenBts;
} SourceLoc;


typedef struct StandardText StandardText;
typedef struct Entity Entity;

typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct ScopeChunk ScopeChunk;

typedef struct { // :CompStats
    Int inpLength;
    Bool wasLexerError;

    Int countNonparsedEntities;
    Int countOverloads;
    Int countOverloadedNames;
    Int toksLen;
    Int nodesLen;
    Int typesLen;
    Int loopCounter;
    Bool wasError;
    String errMsg;
    
    Int standardTextLen; // length of standardText
    Int firstParsed; // the name index for the first parsed word
    Int firstBuiltin; // the nameId for the first built-in word in standardStrings
} CompStats;

//}}}
//{{{ Interpreter

// Instructions (opcodes)
// An instruction is 8 byte long and consists of 6-bit opcode and some data
// Notation: [A] is a 2-byte stack address, it's signed and is measured relative to currFrame
//           [~A] is a 3-byte constant or offset
//           {A} is a 4-byte constant or address
//           {{A}} is an 8-byte constant (i.e. it takes up a whole second instruction slot)
#define iPlus              0 // [Dest] [Operand1] [Operand2]
#define iMinus             1
#define iTimes             2
#define iDivBy             3
#define iPlusFl            4
#define iMinusFl           5
#define iTimesFl           6
#define iDivByFl           7 // /end
#define iPlusConst         8 // [Src=Dest] {Increment}
#define iMinusConst        9
#define iTimesConst       10
#define iDivByConst       11 // /end
#define iPlusFlConst      12 // [Src=Dest] {{Double constant}}
#define iMinusFlConst     13
#define iTimesFlConst     14
#define iDivByFlConst     15 // /end
#define iConcatStrs       16 // [Dest] [Operand1] [Operand2]
#define iNewstring        17 // [Dest] [Start] [~Len]
#define iSubstring        18 // [Dest] [Src] {{ {Start} {Len}  }}
#define iReverseString    19 // [Dest] [Src]
#define iIndexOfSubstring 20 // [Dest] [String] [Substring]
#define iGetFld           21 // [Dest] [Obj] [~Offset]
#define iNewList          22 // [Dest] {Capacity}
#define iGetElemPtr       23 // [Dest] [ArrAddress] {{ {0} {Elem index} }}
#define iAddToList        24 // [List] {Value or reference}
#define iRemoveFromList   25 // [List] {Elem Index}
#define iSwap             26 // [List] {{ {Index1} {Index2} }}
#define iConcatLists      27 // [Dest] [Operand1] [Operand2]
#define iJump             28 // { Code pointer }
#define iBranchLt         29 // [Operand] { Code pointer }
#define iBranchEq         30
#define iBranchGt         31 // /end
#define iShortCircuit     32 // if [B] == [C] then [A] = [B] else ip += 1
#define iCall             33 // [New frame pointer] { New instruction pointer }
#define iBuiltinCall      34 // [Builtin index]
#define iReturn           35 // [Size of return value = 0, 1 or 2]
#define iSetLocal         36 // [Dest] {Value}
#define iSetBigLocal      37 // [Dest] {{Value}}
#define iPrint            38 // [String]
#define iPrintErr         39 // [String]

//}}}

#endif
