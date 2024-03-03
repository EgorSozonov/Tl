//{{{ Utils

#define Int int32_t
#define StackInt Stackint32_t
#define InListUns InListuint32_t
#define private static
#define Byte unsigned char
#define Bool bool
#define Arr(T) T*
#define untt uint32_t
#define null nullptr
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

#define DEFINE_STACK_HEADER(T)                                  \
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
    testable T penultimate ## T(Stack ## T * st);\
    testable void push ## T (T newItem, Stack ## T * st);\
    testable void clear ## T (Stack ## T * st);

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
    untt hash;
    Int indString;
} StringValue;


typedef struct {
    untt capAndLen;
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
#define strEach       5
#define strElseIf     6
#define strElse       7
#define strFalse      8
#define strFinally    9
#define strFor       10
#define strIf        11
#define strImpl      12
#define strImport    13
#define strInterface 14
#define strMatch     15
#define strPub       16
#define strReturn    17
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
#define strA         28 // A(rray)
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
    untt tp : 6;
    Int tokenInd;
    untt spanLevel : 3;
} BtToken;

DEFINE_STACK_HEADER(BtToken)

typedef struct { // :Token
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;
} Token;

#define maxWordLength 255

// Regular (leaf) Token types
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt          0
#define tokLong         1
#define tokDouble       2
#define tokBool         3  // pl2 = value (1 or 0)
#define tokString       4
#define tokTilde        5  // marks up to 2 anonymous params in a lambda. pl1 = tilde count
#define tokMisc         6  // pl1 = see the misc* constants

#define tokWord         7  // pl2 = nameId (index in the string table)
#define tokTypeName     8  // pl1 = 1 iff it has arity (like "M/2"), pl2 = same as tokWord
#define tokKwArg        9  // pl2 = same as tokWord. The ":argName"
#define tokUnaryOper   10  // pl1 = OperatorType, one of the "opT" constants below
#define tokFieldAcc    11  // pl2 = nameId

// Statement or subexpr span types. pl2 = count of inner tokens
#define tokStmt        12  // firstSpanTokenType
#define tokParens      13  // subexpressions and struct/sum type instances
#define tokCall        14  // pl1 = nameId, index in the string table. pl2 > 0 if prefix call.
                           // Includes non-prefix operators: for them, pl1 = opT and pl2 = 0
#define tokTypeCall    15  // pl1 = nameId of type. Either a type call or a type constructor
#define tokParamList   16  // Parameter lists, ended with `|` */
#define tokAssignLeft  17  // pl1 == 1 iff reassignment, pl1 == 2 iff type assignment,
                           // pl1 == (BIG + opType) iff mutation. If pl2 == 0 then
                           // pl1 = nameId for the single word on the left (and it's an
                           // assignment of a var, i.e. neither reassignment/mut nor type)
#define tokAssignRight 18  // Right-hand side of assignment
#define tokAlias       19
#define tokAssert      20
#define tokBreakCont   21  // pl1 >= BIG iff it's a continue
#define tokTrait       22
#define tokImport      23  // For test files and package decls
#define tokReturn      24

// Bracketed (multi-statement) token types. pl1 = spanLevel, see the "sl" constants
#define tokScope       25  // denoted by {}. firstScopeTokenType
#define tokFn          26  // `^{ a Int => String | body}`
#define tokTry         27  // `try {`
#define tokCatch       28  // `catch MyExc e {`
#define tokDefer       29  // `defer { `

// Resumable core forms
#define tokIf          30  // `if ... {`
#define tokMatch       31  // `match ... {` pattern matching on sum type tag
#define tokElseIf      32  // `ei ... {`
#define tokElse        33  // `else {`
#define tokImpl        34
#define tokFor         35
#define tokEach        36

#define topVerbatimTokenVariant tokMisc
#define topVerbatimType    tokString
#define firstSpanTokenType tokStmt
#define firstScopeTokenType tokScope
#define firstResumableSpanTokenType tokIf


// List of keywords that don't correspond directly to a token.
// All these numbers must be below firstKeywordToken to avoid any clashes
#define keywTrue        1
#define keywFalse       2
#define keywBreak       3
#define keywContinue    4

#define miscPub   0     // pub. It must be 0 because it's the only one denoted by a keyword
#define miscEmpty 1     // null, written as `()`
#define miscComma 2     // ,
#define miscArrow 3     // =>

#define assiDefinition 0
#define assiReassign   1
#define assiType       2


// AST nodes
#define nodId           7  // pl1 = index of entity, pl2 = index of name
#define nodCall         8  // pl1 = index of entity, pl2 = arg count
#define nodBinding      9  // pl1 = index of entity, pl2 = 1 if it's a type binding
#define nodFieldAcc    10  // pl1 = nameId; after type resolution pl1 = offset
#define nodLoad        11  // a read from memory (a reference or list, array etc)
#define nodStore       12  // a store to memory

// Punctuation (inner node). pl2 = node count
#define nodScope       13
#define nodExpr        14  // pl1 = 1 iff it's a composite expression (has internal var decls)
#define nodAssignLeft  15  // if pl2 = 0, then pl1 = entityId. Next span is always a nodExpr or
                           // a nodDataAlloc
#define nodDataAlloc   16  // pl1 = typeId, pl3 = count of elements

// Single-shot core syntax forms
#define nodAlias       17
#define nodAssert      18  // pl1 = 1 iff it's a debug assert
#define nodBreakCont   19  // pl1 = number of label to break or continue to, -1 if none needed
                           // It's a continue iff it's >= BIG
#define nodCatch       20  // `catch e {`
#define nodDefer       21
#define nodImport      22  // This is for test files only, no need to import anything in main
#define nodFnDef       23  // pl1 = entityId
#define nodTrait       24
#define nodMeta        25
#define nodReturn      26
#define nodTry         27
#define nodFor         28  // pl1 = id of loop (unique within a function) if it needs to
                           // have a label in codegen
#define nodForCond     29
#define nodForStep     30  // the "++i" of a for loop

#define nodIf          31
#define nodElseIf      32
#define nodImpl        33
#define nodMatch       34  // pattern matching on sum type tag

#define countSpanForms (nodMatch - nodScope + 1)

#define metaDoc         1  // Doc comments
#define metaDefault     2  // Default values for type arguments


// There is a closed set of operators in the language.
//
// For added flexibility, some operators may be extended into one more planes,
// e.g. (+) may be extended into (+.), while (/) may be extended into (/.).
// These extended operators are declared by the language, and may be defined
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
#define opIsNull            9 // '
#define opTimesExt         10 // *:
#define opTimes            11 // *
#define opIncrement        12 // ++
#define opPlusExt          13 // +:
#define opPlus             14 // +
#define opDecrement        15 // --
#define opMinusExt         16 // -:
#define opMinus            17 // -
#define opDivByExt         18 // /:
#define opIntersect        19 // /|
#define opDivBy            20 // /
#define opBitShiftL        21 // <<.
#define opLTEQ             22 // <=
#define opComparator       23 // <>
#define opLessTh           24 // <
#define opRefEquality      25 // ===
#define opEquality         26 // ==
#define opIntervalBo       27 // >=<= inclusive interval check
#define opIntervalR        28 // ><=  right-inclusive interval check
#define opIntervalL        29 // >=<  left-inclusive interval check
#define opBitShiftR        30 // >>.  unsigned right bit shift
#define opIntervalEx       31 // ><   exclusive interval check
#define opGTEQ             32 // >=
#define opGreaterTh        33 // >
#define opNullCoalesce     34 // ?:   null coalescing operator
#define opQuestionMark     35 // ?    nullable type operator
#define opBitwiseXor       36 // ^.   bitwise XOR
#define opExponent         37 // ^    exponentiation
#define opDataAcc          38 // _    array accessor
#define opBitwiseOr        39 // ||.  bitwise or
#define opBoolOr           40 // ||   logical or

#define countOperators     41

#define countSyntaxForms (tokEach + 1)

typedef struct Compiler Compiler;


typedef void (*LexerFunc)(Arr(Byte), Compiler*); // LexerFunc = &(Lexer* => void)
typedef void (*ParserFunc)(Token, Arr(Token), Compiler*);

typedef struct { // :LanguageDefinition
    LexerFunc (*dispatchTable)[256];
    ParserFunc (*parserTable)[countSyntaxForms];
} LanguageDefinition;


typedef struct { // :Node
    untt tp : 6;
    untt pl3: 26;
    Int pl1;
    Int pl2;
} Node;

typedef struct { // :SourceLoc
    Int startBt;
    Int lenBts;
} SourceLoc;

typedef struct { // :ParseFrame
    untt tp : 6;
    Int startNodeInd;
    Int sentinelToken;
    Int typeId;            // valid only for fnDef, if, loopCond and the like
    void* scopeStackFrame; // only for tp = scope or expr
} ParseFrame;


#define classMutableGuaranteed 1
#define classMutatedGuaranteed 2
#define classMutableNullable   3
#define classMutatedNullable   4
#define classImmutable         5


typedef struct { // :Entity
    TypeId typeId;
    untt name; // 8 bits of length, 24 bits of nameId
    uint8_t class;
    bool isPublic;
    bool hasExceptionHandler;
} Entity;


typedef struct { // :EntityImport
    untt name;   // 8 bits of length, 24 bits of nameId
    TypeId typeId;
} EntityImport;


typedef struct { // :Toplevel
    // Toplevel definitions (functions, variables, types) for parsing order and name searchability
    Int indToken;
    Int sentinelToken;
    untt name;
    Int entityId; // if n < 0 => -n - 1 is an index into [functions], otherwise n => [entities]
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
#define exfrDataAlloc  3

DEFINE_STACK_HEADER(ExprFrame)

typedef struct { // :StateForExprs
    StackExprFrame* frames;
    StackNode* scr;
    StackSourceLoc* locsScr;
    StackNode* calls;
    StackSourceLoc* locsCalls;
    Bool metAnAllocation;
} StateForExprs;


typedef struct {  // :TypeFrame
    Byte tp;      // "tye" constants in tl.c
    Int sentinel; // token sentinel
    Int countArgs; // accumulated number of type arguments
    Int nameId;
} TypeFrame;

DEFINE_STACK_HEADER(TypeFrame)


struct Compiler { // :Compiler
    // See docs/compiler.txt, docs/architecture.svgz
    LanguageDefinition* langDef;

    // LEXING
    String* sourceCode;
    Int inpLength;
    InListToken tokens;
    InListToken metas; // TODO - metas with links back into parent span tokens
    InListInt newlines;
    StackSourceLoc* sourceLocs;
    Int indentation;
    InListInt numeric;          // [aTmp]
    StackBtToken* lexBtrack;    // [aTmp]
    Stackint32_t* stringTable;  // operators, then standard strings, then imported ones, then parsed
    StringDict* stringDict;
    Int lastClosingPunctInd;

    // PARSING
    InListToplevel toplevels;
    InListInt importNames;
    StackParseFrame* backtrack; // [aTmp]
    ScopeStack* scopeStack;
    StateForExprs* stateForExprs; // [aTmp]
    Arr(Int) activeBindings;    // [aTmp]
    Int loopCounter;
    InListNode nodes;
    InListNode monoCode; // code for monorphizations of generic functions
    MultiAssocList* monoIds;
    InListEntity entities;
    MultiAssocList* rawOverloads; // [aTmp] (firstParamTypeId entityId)
    InListInt overloads;
    InListInt types;
    StringDict* typesDict;
    StackInt* expStack;   // [aTmp]
    StackInt* typeParams;  // [aTmp]
    StackTypeFrame* typeStack;  // [aTmp]
    StackInt* tempStack; // [aTmp]
    Int countNonparsedEntities;
    Int countOverloads;
    Int countOverloadedNames;

    // GENERAL STATE
    Int i;
    Bool wasLexerError;
    Bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aTmp;
};


// Span levels, must all be more than 0
#define slScope       1 // scopes (denoted by brackets): newlines and commas have no effect there
#define slDoubleScope 2 // double scopes like `if`, `for` etc. They last until the first
                        // {} span gets closed
#define slStmt        3 // single-line statements: newlines and semicolons break 'em
#define slSubexpr     4 // parenthesized forms: newlines have no effect, semi-colons error out


//}}}
//{{{ Types

// see the Type layout chapter in the docs
#define sorRecord       1
#define sorEnum         2
#define sorFunction     3
#define sorPartial      4 // Partially applied type
#define sorConcrete     5 // Fully applied type (no type params)

// Type expression data format: First element is the tag (one of the following
// constants), second is payload. Type calls need to have an extra payload, so their tag
// is (8 bits of "tye", 24 bits of typeId). Used in [expStack]
#define tyeRecord     1 // payload: count of fields in the struct
#define tyeEnum       2 // payload: count of variants
#define tyeFunction   3 // payload: count of parameters. This is a function signature, not the F(...)
#define tyeFnType     4 // payload: count of parameters. This is the F(...)
#define tyeType       5 // payload: typeId
#define tyeTypeCall   6 // payload: count of args. Payl in tag: nameId
#define tyeParam      7 // payload: paramId
#define tyeParamCall  8 // payload: count of args. Payl in tag: nameId
#define tyeName       9 // payload: nameId. Used for struct fields, function params, sum variants
#define tyeMeta      10 // payload: index of this meta's token
#define tyeRetType   11 // payload: none


typedef struct { // :TypeHeader
    Byte sort;
    Byte tyrity; // "tyrity" = type arity, the number of type parameters
    Byte arity;  // for function types, equals arity. For structs, number of fields
    untt nameAndLen;
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

#define penultimate(X) _Generic((X),\
    StackBtToken*: penultimateBtToken,\
    StackParseFrame*: penultimateParseFrame,\
    Stackint32_t*: penultimateint32_t,\
    StackNode*: penultimateNode\
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

