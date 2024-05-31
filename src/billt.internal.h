//{{{ Utils

#define Int int32_t
#define StackInt Stackint32_t
#define InListUns InListuint32_t
#define private static
#define Byte unsigned char
#define Bool bool
#define Arr(T) T*
#define Unt uint32_t
#define null NULL
#define NameId int32_t
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

typedef struct ArenaChunk ArenaChunk;

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
    Int cap;\
    Int len;\
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


typedef struct { // :String
    int len;
    Byte cont[];
} String;

testable void printStringNoLn(String* s);
testable void printString(String* s);
extern String empty;
testable String* str(const char* content, Arena* a);
testable bool endsWith(String* a, String* b);

#define s(lit) str(lit, a)

typedef struct {
    Int len; // length of standardText
    Int firstParsed; // the nameId for the first parsed word
    Int firstBuiltin; // the nameId for the first built-in word in standardStrings
} StandardText;

//}}}
//{{{ Int Hashmap

DEFINE_STACK_HEADER(int32_t)
typedef struct {
    Arr(int*) dict;
    int dictSize;
    int len;
    Arena* a;
} IntMap;

//}}}
//{{{ String Hashmap

// Reference to first occurrence of a string identifier within input text
typedef struct {
    Unt hash;
    Int indString;
} StringValue;


typedef struct {
    Unt capAndLen;
    StringValue cont[];
} Bucket;

// Hash map of all words/identifiers encountered in a source module
typedef struct {
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

#define topVerbatimTokenVariant tokMisc
#define topVerbatimType    topVerbatimTokenVariant
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
#define miscUnderscore 1     // ,
#define miscArrow      2     // ->

#define assiDefinition 0
#define assiType       2


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
    Byte bytes[4];
    Int arity;
    // Whether this operator permits defining overloads as well as extended operators (e.g. +.= )
    bool overloadable;
    bool assignable;
    bool isTypelevel;
    int8_t lenBts;
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
#define opBoolAnd           7 // && logical "and"
#define opPtr               8 // & pointers/values at type level
#define opTimesExt          9 // *:
#define opTimes            10 // *
#define opPlusExt          11 // +:
#define opPlus             12 // +
#define opMinusExt         13 // -:
#define opMinus            14 // -
#define opDivByExt         15 // /:
#define opIntersect        16 // /| type-level trait intersection ?
#define opDivBy            17 // /
#define opBitShiftL        18 // <<.
#define opLTZero           19 // <0   less than zero
#define opShiftL           20 // <<   shift smth, e.g. an iterator, left
#define opLTEQ             21 // <=
#define opComparator       22 // <>
#define opLessTh           23 // <
#define opRefEquality      24 // ===
#define opIsNull           25 // =0
#define opEquality         26 // ==
#define opIntervalBo       27 // >=<= inclusive interval check
#define opIntervalR        28 // ><=  right-inclusive interval check
#define opIntervalL        29 // >=<  left-inclusive interval check
#define opBitShiftR        30 // >>.  unsigned right bit shift
#define opGTZero           31 // >0   greater than zero
#define opIntervalEx       32 // ><   exclusive interval check
#define opGTEQ             33 // >=
#define opShiftR           34 // >>   shift right, e.g. an iterator
#define opGreaterTh        35 // >
#define opNullCoalesce     36 // ?:   null coalescing operator
#define opQuestionMark     37 // ?    nullable type operator
#define opUnused           38 // @    unused yet
#define opBitwiseXor       39 // ^.   bitwise XOR
#define opExponent         40 // ^    exponentiation
#define opBitwiseOr        41 // ||.  bitwise or
#define opBoolOr           42 // ||   logical or
#define opGetElem          43 // Get list element
#define opGetElemPtr       44 // Get pointer to list element

#define countOperators     45
#define countRealOperators 43 // The "unreal" ones, like "a[..]", which have a separate syntax


typedef struct Compiler Compiler;


typedef void (*LexerFunc)(Arr(Byte), Compiler*); // LexerFunc = &(Lexer* => void)
typedef void (*ParserFunc)(Token, Arr(Token), Compiler*);


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


#define classMutable   1
#define classImmutable 5


typedef struct { // :Entity
    TypeId typeId;
    Unt name; // 8 bits of length, 24 bits of nameId
    uint8_t class;
    bool isPublic;
    bool hasExceptionHandler;
} Entity;


typedef struct { // :EntityImport
    Unt name;   // 8 bits of length, 24 bits of nameId
    TypeId typeId;
} EntityImport;


typedef struct { // :Toplevel
    // Toplevel definitions (functions, variables, types) for parsing order and name searchability
    Int indToken;
    Int sentinelToken;
    Unt name;
    EntityId entityId; // if n < 0 => -n - 1 is an index into [functions], otherwise n => [entities]
    bool isFunction;
} Toplevel;


typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct ScopeChunk ScopeChunk;
struct ScopeChunk { // :ScopeChunk
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

DEFINE_INTERNAL_LIST_TYPE(Token)
DEFINE_INTERNAL_LIST_TYPE(Node)

DEFINE_INTERNAL_LIST_TYPE(Entity)

DEFINE_INTERNAL_LIST_TYPE(Toplevel)
DEFINE_INTERNAL_LIST_TYPE(Int)

DEFINE_INTERNAL_LIST_TYPE(uint32_t)

DEFINE_INTERNAL_LIST_TYPE(EntityImport)


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
    Int nameId;
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
    String* errMsg;
} CompStats;


struct Compiler { // :Compiler
    // LEXING
    String* sourceCode;
    InListToken tokens;
    InListToken metas; // TODO - metas with links back into parent span tokens
    InListInt newlines;
    StackSourceLoc* sourceLocs;
    InListInt numeric;          // [aTmp]
    StackBtToken* lexBtrack;    // [aTmp]
    Stackint32_t* stringTable;  // operators, then standard strings, then imported ones, then parsed
                                // Contains (8 bits of lenBts, 24 bits of startBt)
    StringDict* stringDict;

    // PARSING
    InListToplevel toplevels;
    InListInt importNames;
    StackParseFrame* backtrack; // [aTmp]
    ScopeStack* scopeStack;
    StateForExprs* stateForExprs; // [aTmp]
    Arr(Int) activeBindings;    // [aTmp]
    InListNode nodes;
    InListNode monoCode; // code for monorphizations of generic functions
    MultiAssocList* monoIds;
    InListEntity entities;
    MultiAssocList* rawOverloads; // [aTmp] (firstParamTypeId entityId)
    InListInt overloads;
    InListInt types;
    StringDict* typesDict;
    StateForTypes* stateForTypes; // [aTmp]

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
//{{{ Generics

#define pop(X) _Generic((X),\
    StackBtToken*: popBtToken,\
    StackParseFrame*: popParseFrame,\
    StackExprFrame*: popExprFrame,\
    StackTypeFrame*: popTypeFrame,\
    Stackint32_t*: popint32_t,\
    StackNode*: popNode,\
    StackSourceLoc*: popSourceLoc\
    )(X)

#define peek(X) _Generic((X),\
    StackBtToken*: peekBtToken,\
    StackParseFrame*: peekParseFrame,\
    StackExprFrame*: peekExprFrame,\
    StackTypeFrame*: peekTypeFrame,\
    Stackint32_t*: peekint32_t,\
    StackNode*: peekNode\
    )(X)

#define push(A, X) _Generic((X),\
    StackBtToken*: pushBtToken,\
    StackParseFrame*: pushParseFrame,\
    StackExprFrame*: pushExprFrame,\
    StackTypeFrame*: pushTypeFrame,\
    Stackint32_t*: pushint32_t,\
    StackNode*: pushNode,\
    StackSourceLoc*: pushSourceLoc\
    )(A, X)

#define hasValues(X) _Generic((X),\
    StackBtToken*: hasValuesBtToken,\
    StackParseFrame*: hasValuesParseFrame,\
    StackTypeFrame*: hasValuesTypeFrame,\
    Stackint32_t*:  hasValuesint32_t,\
    StackNode*: hasValuesNode\
    )(X)

//}}}

