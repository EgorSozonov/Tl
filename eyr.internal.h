#ifndef ORS_INTERNAL_H
#define ORS_INTERNAL_H
//CUTSTART
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
#define OUT // the "out" parameters in functions
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
#define print(...) \
  printf(__VA_ARGS__);\
  printf("\n");

#define dg(...) \
  printf(__VA_ARGS__);\
  printf("\n");

typedef struct ArenaChunk ArenaChunk;

//CUTEND
struct ArenaChunk { // :ArenaChunk
    size_t size;
    ArenaChunk* next;
    char memory[]; // flexible array member
};

typedef struct { // :Arena
    ArenaChunk* firstChunk;
    ArenaChunk* currChunk;
    int currInd;
} Arena;

#define DEFINE_STACK_HEADER(T) \
    typedef struct {\
        Int cap;\
        Int len;\
        Arena* arena;\
        T* cont;\
    } Stack##T;\
    testable Stack ## T * createStack ## T (Int initCapacity, Arena* a);\
    testable Bool hasValues ## T (Stack ## T * st);\
    testable T pop ## T (Stack ## T * st);\
    testable T peek ## T(Stack ## T * st);\
    testable void push ## T (T newItem, Stack ## T * st);

#define DEFINE_INTERNAL_LIST_TYPE(T)\
typedef struct {\
    Int len;\
    Int cap;\
    Arr(T) cont;\
} InList##T;

#define DEFINE_INTERNAL_LIST_HEADER(fieldName, T)\
    InList##T createInList ## T (Int initCapacity, Arena* a);\
    void pushIn ## fieldName (T newItem, Compiler* cm);


// A growable list of growable lists of pairs of non-negative Ints. Is smart enough to reuse
// old allocations via an intrusive free list.
// Internal lists have the structure [len cap ...data...] or [nextFree cap ...] for the free sectors
// Units of measurement of len and cap are 1's. I.e. len can never be = 1, it starts with 2
typedef struct { // :MultiAssocList
    Int len;
    Int cap;
    Int freeList;
    Arr(Int) cont;
    Arena* a;
} MultiAssocList;

typedef struct { // :StringBuilder
    Arr(char) cont;
    Int len;
    Int cap;
} StringBuilder;

testable void printStringNoLn(String s);
testable void printString(String s);
//extern String empty;

constexpr String empty = {.cont = null, .len = 0};
testable String str(const char* content);
testable Bool endsWith(String a, String b);

#define s(lit) str(lit)

typedef struct {
    Int len; // length of standardText
    Int firstParsed; // the name index for the first parsed word
    Int firstBuiltin; // the nameId for the first built-in word in standardStrings
} StandardText;

//}}}
//{{{ Int Hashmap

DEFINE_STACK_HEADER(int32_t)
DEFINE_STACK_HEADER(uint32_t)
typedef struct {
    Arr(int*) dict;
    int dictSize;
    int len;
    Arena* a;
} IntMap;

//}}}
//{{{ String Hashmap

// Reference to first occurrence of a string identifier within input text
typedef struct { //:StringValue
    Unt hash;
    Int indString;
} StringValue;


typedef struct { //:Bucket
    Unt capAndLen;
    StringValue cont[];
} Bucket;

// Hash map of all words/identifiers encountered in a source module
typedef struct { //:StringDict
    Arr(Bucket*) dict;
    int dictSize;
    int len;
    Arena* a;
} StringDict;

//}}}
//{{{ Lexer

//{{{ Standard strings :standardStr

#define strAlias      0
#define strAssert     1
#define strBreak      2
#define strCatch      3
#define strContinue   4
#define strDo         5
#define strEach       6
#define strElseIf     7
#define strElse       8
#define strFalse      9
#define strFor       10
#define strIf        11
#define strImpl      12
#define strImport    13
#define strMatch     14
#define strPub       15
#define strReturn    16
#define strTrait     17
#define strTrue      18
#define strTry       19
#define strFirstNonReserved 20
#define strInt       strFirstNonReserved // types must come first here?, see "buildPreludeTypes"
#define strLong      21
#define strDouble    22
#define strBool      23
#define strString    24
#define strVoid      25
#define strF         26 // F(unction type)
#define strL         27 // L(ist)
#define strArray     28
#define strD         29 // D(ictionary)
#define strRec       30 // Record
#define strEnum      31 // Enum
#define strTu        32 // Tu(ple)
#define strPromise   33 // Promise
#define strLen       34
#define strCap       35
#define strF1        36
#define strF2        37
#define strPrint     38
#define strPrintErr  39
#define strMathPi    40
#define strMathE     41
#define strTypeVarT  42
#define strTypeVarU  43
#ifndef TEST
#define strSentinel  44
#else
#define strSentinel  47
#endif

//}}}

// Backtrack token, used during lexing to keep track of all the nested stuff
typedef struct { // :BtToken
    Unt tp : 6;
    Int tokenInd;
    Unt spanLevel : 3;
} BtToken;

DEFINE_STACK_HEADER(BtToken)

typedef struct { // :Token
    Unt tp : 6;
    Unt lenBts: 26;
    Unt startBt;
    Unt pl1;
    Unt pl2;
} Token;


DEFINE_STACK_HEADER(Token)
#define maxWordLength 255

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
#define tokParens      13  // subexpressions and struct/sum type instances
#define tokTypeCall    14  // `(Tu Int Str)`
#define tokIntro       15  // Introduction to a syntax form, like a param list or an if condition
                           // pl1 = original tp of this token before it was converted to tokIntro
#define tokDataList    16  // []
#define tokDataMap     17  // {}
#define tokAccessor    18  // x[]
#define tokAssignment  19  // pl1 == 2 iff type assignment
#define tokAssignRight 20  // Right-hand side of assignment
#define tokAlias       21
#define tokAssert      22
#define tokBreakCont   23  // pl1 = 1 iff it's a continue
#define tokTrait       24
#define tokImport      25  // For test files and package decls
#define tokReturn      26

// Bracketed (multi-statement) token types. pl1 = spanLevel, see the "sl" constants
#define tokScope       27  // `(do ...)` firstScopeTokenType
#define tokFn          28  // `(\ a Int -> Str: body)`. pl1 = entityId
#define tokTry         29  // `(try`
#define tokCatch       30  // `(catch e MyExc:`
#define tokIf          31  // `(if ... `
#define tokMatch       32  // `(match ... ` pattern matching on sum type tag
#define tokElseIf      33  // `ei ... `
#define tokElse        34  // `else: `
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

#define nodAlias       17
#define nodAssert      18  // pl1 = 1 iff it's a debug assert
#define nodBreakCont   19  // pl1 = number of label to break or continue to, -1 if none needed
                           // It's a continue iff it's >= BIG
#define nodCatch       20  // `(catch e `
#define nodDefer       21
#define nodImport      22  // This is for test files only, no need to import anything in main
#define nodFnDef       23  // pl1 = entityId, pl3 = nameId
#define nodTrait       24
#define nodReturn      25
#define nodTry         26
#define nodFor         27  // pl1 = id of loop (unique within a function) if it needs to
                           // have a label in codegen; pl3 = number of nodes to skip to get to body

#define nodIf          28  // pl3 = length till the next elseif/else, or 0 if there are none
#define nodElseIf      29
#define nodImpl        30
#define nodMatch       31  // pattern matching on sum type tag

#define countSpanForms (nodMatch - nodScope + 1)

#define metaDoc         1  // Doc comments
#define metaDefault     2  // Default values for type arguments

#define countAstForms (nodMatch + 1)

// There is a closed set of operators in the language.
//
// For added flexibility, some operators may be extended into another variant,
// e.g. (+) may be extended into (+.), while (/) may be extended into (/.).
// These extended operators are declared but not defined by the language, and may be defined
// for any type by the user, with the return type being arbitrary.
// For example, the type of 3D vectors may have two different multiplication
// operators: *. for vector product and * for scalar product.
//
// Plus, many have automatic assignment counterparts.
// For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
typedef struct { // :OpDef
    char firstSymbol;
    Int arity;
    // Whether this operator permits defining overloads as well as extended operators (e.g. +.= )
    bool overloadable;
    bool assignable;
    bool isTypelevel;
    NameLoc name;
} OpDef;

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


typedef void (*LexerFn)(const Arr(char), Compiler* restrict); // LexerFunc = &(Lexer* => void)
typedef void (*ParserFn)(Token, Arr(Token), Compiler* restrict);


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

typedef struct { // :ParseFrame
    Unt tp : 6;
    Int startNodeInd;
    Int sentinel;    // sentinel token
    Int typeId;      // valid only for fnDef, if, loopCond and the like.
                     // For nodFor, contains the loop depth
} ParseFrame;


// Public classes must always be even-valued, private - odd-valued. Mutable before immutable
#define classMut      1
#define classPubMut   2
#define classImmut    5
#define classPubImmut 6


typedef struct { //:Entity
    TypeId typeId;
    Unt name; // For native names, it's NameId. For "emitInfix", this is operatorId
    Byte class; // mutable or immutable, public or private
} Entity;


typedef struct { //:Toplevel
// Toplevel definitions (functions, variables, types) for parsing order and name searchability
    Int tokenInd;
    Int sentinelToken;
    NameId nameId;
    EntityId entityId; // if n < 0 => -n - 1 is an index into [functions], otherwise n => [entities]
    bool isFunction;
    Int nodeInd;
} Toplevel;


typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct ScopeChunk ScopeChunk;
struct ScopeChunk { //:ScopeChunk
    ScopeChunk *next;
    int len; // length is divisible by 4
    Int cont[];
};


typedef struct { // :ScopeStack
    // Either currChunk->next == NULL or currChunk->next->next == NULL
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    ScopeStackFrame* topScope;
    Int len;
    int nextInd; // next ind inside currChunk, unit of measurement is 4 Bytes
} ScopeStack;


DEFINE_STACK_HEADER(ParseFrame)
DEFINE_STACK_HEADER(Node)
DEFINE_STACK_HEADER(SourceLoc)

DEFINE_INTERNAL_LIST_TYPE(Token) //:InListToken
DEFINE_INTERNAL_LIST_TYPE(Node)

DEFINE_INTERNAL_LIST_TYPE(Entity)

DEFINE_INTERNAL_LIST_TYPE(Toplevel)

DEFINE_INTERNAL_LIST_TYPE(Int)

DEFINE_INTERNAL_LIST_TYPE(uint32_t)
DEFINE_INTERNAL_LIST_TYPE(uint64_t)


typedef struct {  // :ExprFrame
    Byte tp;      // "exfr" constants below
    Int sentinel; // token sentinel
    Int argCount; // accumulated number of arguments. Used for exfrCall & exfrDataAlloc only
    Int startNode; // used for all?
} ExprFrame;

#define exfrParen      1
#define exfrCall       2
#define exfrUnaryCall  3
#define exfrDataAlloc  4
#define exfrAccessor   5

DEFINE_STACK_HEADER(ExprFrame)

typedef struct { // :StateForExprs
    StackInt* exp;   // [aTmp]
    StackExprFrame* frames;
    StackNode* scr;
    StackSourceLoc* locsScr;
    StackNode* calls;
    StackSourceLoc* locsCalls;
    Bool metAnAllocation;
    StackToken* reorderBuf;
} StateForExprs;

typedef struct {   // :TypeFrame
    Byte tp;       // "sor" and "tye" constants
    Int sentinel;  // token id sentinel
    Int countArgs; // accumulated number of type arguments
    NameId nameId;
} TypeFrame;

DEFINE_STACK_HEADER(TypeFrame)

typedef struct { // :StateForTypes
    StackInt* exp;   // [aTmp] Higher 8 bits are the "sor"/"tye" sorts, lower 24 bits are TypeId
    StackInt* params;  // [aTmp] Params of a whole type expression
    StackInt* subParams;  // [aTmp] Type params of a subexpression
    StackInt* paramRenumberings;  // [aTmp]
    StackTypeFrame* frames;  // [aTmp]
    StackInt* names; // [aTmp]
    StackInt* tmp; // [aTmp]
} StateForTypes;


typedef struct { // :CompStats
    Int inpLength;
    Int lastClosingPunctInd;
    Bool wasLexerError;

    Int countNonparsedEntities;
    Int countOverloads;
    Int countOverloadedNames;
    Int loopCounter;
    Bool wasError;
    String errMsg;
} CompStats;


typedef struct { //:BtCodegen Backtrack for generating code
    Byte tp; // instructions, i.e. the "i*" constants
    Int startInstr; // index of starting instruction
    Int sentinel; // sentinel node of current function
} BtCodegen;

DEFINE_STACK_HEADER(BtCodegen)

struct Compiler { // :Compiler
    // LEXING
    String sourceCode;
    InListToken tokens;
    InListToken metas; // TODO - metas with links back into parent span tokens
    InListInt newlines;
    StackSourceLoc* sourceLocs;
    InListInt numeric;          // [aTmp]
    StackBtToken* lexBtrack;    // [aTmp]
    Stackuint32_t* stringTable;  // Operators, then standard strings, then imported ones, then
                                 // parsed. Contains NameLoc pointing into @sourceCode
    StringDict* stringDict;

    // PARSING
    InListToplevel toplevels;
    InListInt importNames;
    StackParseFrame* backtrack; // [aTmp]
    ScopeStack* scopeStack;
    StateForExprs* stateForExprs; // [aTmp]
    Arr(Int) activeBindings;    // [aTmp]
    InListNode nodes;
    InListNode monoCode; // ASTs for monorphizations of generic functions
    MultiAssocList* monoIds;
    InListEntity entities;
    MultiAssocList* rawOverloads; // [aTmp] (firstParamTypeId entityId)
    InListInt overloads;
    InListInt types;
    StringDict* typesDict;
    StateForTypes* stateForTypes; // [aTmp]

    // CODEGEN
    StackBtCodegen* cgBtrack;    // [aTmp]
    InListUlong bytecode;

    // GENERAL STATE
    Int i;
    Arena* a;
    Arena* aTmp;
    CompStats stats;
};


// Span levels, must all be more than 0
#define slScope       1 // scopes (denoted by brackets): newlines and commas have no effect there
#define slStmt        2 // single-line statements: newlines and semicolons break 'em
#define slSubexpr     3 // parenthesized forms: newlines have no effect, semi-colons error out


//}}}
//{{{ Types

// see the Type layout chapter in the docs
#define sorRecord       1 // Used for records and for primitive types
#define sorEnum         2
#define sorFunction     3
#define sorTypeCall     4 // Partially (or completely) applied generic record/enum like L(Int)
#define sorMaxType      sorTypeCall
// the following constants must not clash with the "sor" constants
// Type expression data format: First element is the tag (one of the following
// constants), second is payload. Used in @expStack
#define tyeParam        5 // payload: paramId


typedef struct { // :TypeHeader
    Byte sort;
    Byte tyrity; // "tyrity" = type arity, the number of type parameters
    Byte arity;  // for function types, equals arity. For structs, number of fields
    Unt nameAndLen; // startBt and lenBts
} TypeHeader;

//}}}
//{{{ Interpreter

// Function layout (all instructions are 8-byte sized):
// length
// actual code

#define EyrPtr uint32_t //:Ptr Pointers are aligned to 4 bytes
#define StackAddr int16_t //:StackAddr Offset from "currFrame". Negative values mean previous stack frame


typedef struct {    //:Interpreter
    Unt ip; // current instruction pointer
    Arr(Ulong) code;
    Arr(Int) fns;   // indices into @code
    // global static string
    char* textStart;

    EyrPtr currFrame;
    EyrPtr topOfFrame;
    Arr(Unt) memory;
    StackAddr stackTop;
    EyrPtr heapTop; // index into @memory
    String errMsg;
} Interpreter;

typedef struct { //:CallHeader
    EyrPtr prevFrame;
    Unt ip;        // Unt index into @Interpreter.code
    Unt fnId;     // Index into @Interpreter.fns
} CallHeader;

#define stackFrameStart 2 // Skipped the 2 ints

typedef Unt (*InterpreterFn)(Ulong, Unt, Interpreter* restrict);
typedef void (*BuiltinFn)(Interpreter*);

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

#define countInstructions (iPrint + 1)

//}}}
//{{{ Generics

#define pop(X) _Generic((X),\
    StackBtToken*: popBtToken,\
    StackParseFrame*: popParseFrame,\
    StackExprFrame*: popExprFrame,\
    StackTypeFrame*: popTypeFrame,\
    Stackint32_t*: popint32_t,\
    StackNode*: popNode,\
    StackSourceLoc*: popSourceLoc,\
    StackBtCodegen*: popBtCodegen\
    )(X)

#define peek(X) _Generic((X),\
    StackBtToken*: peekBtToken,\
    StackParseFrame*: peekParseFrame,\
    StackExprFrame*: peekExprFrame,\
    StackTypeFrame*: peekTypeFrame,\
    Stackint32_t*: peekint32_t,\
    StackNode*: peekNode,\
    StackBtCodegen*: peekBtCodegen\
    )(X)

#define push(A, X) _Generic((X),\
    StackBtToken*: pushBtToken,\
    StackParseFrame*: pushParseFrame,\
    StackExprFrame*: pushExprFrame,\
    StackTypeFrame*: pushTypeFrame,\
    Stackint32_t*: pushint32_t,\
    Stackuint32_t*: pushuint32_t,\
    StackNode*: pushNode,\
    StackSourceLoc*: pushSourceLoc,\
    StackBtCodegen*: pushBtCodegen\
    )(A, X)

#define hasValues(X) _Generic((X),\
    StackBtToken*: hasValuesBtToken,\
    StackParseFrame*: hasValuesParseFrame,\
    StackTypeFrame*: hasValuesTypeFrame,\
    Stackint32_t*:  hasValuesint32_t,\
    StackNode*: hasValuesNode,\
    StackBtCodegen*: hasValuesBtCodegen\
    )(X)
//:pop :peek :push :hasValues

//}}}
#endif
