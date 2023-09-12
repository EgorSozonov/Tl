//{{{ Utils

#define Int int32_t
#define StackInt Stackint32_t
#define InListUns InListuint32_t
#define private static
#define byte unsigned char
#define Arr(T) T*
#define untt uint32_t
#ifdef TEST
    #define testable
#else
    #define testable static
#endif

#define LOWER24BITS 0x00FFFFFF
#define LOWER26BITS 0x03FFFFFF
#define LOWER16BITS 0x0000FFFF
#define LOWER32BITS 0x00000000FFFFFFFF
#define PENULTIMATE8BITS 0xFF00
#define THIRTYFIRSTBIT 0x40000000
#define MAXTOKENLEN 67108864 // 2^26
#define SIXTEENPLUSONE 65537 // 2^16 + 1
#define LEXER_INIT_SIZE 2000
#define print(...) \
  printf(__VA_ARGS__);\
  printf("\n");

typedef struct ArenaChunk ArenaChunk;

struct ArenaChunk {
    size_t size;
    ArenaChunk* next;
    char memory[]; // flexible array member
};

typedef struct {
    ArenaChunk* firstChunk;
    ArenaChunk* currChunk;
    int currInd;
} Arena;

#define DEFINE_STACK_HEADER(T)                                  \
    typedef struct {                                   \
        Int cap;                                           \
        Int len;                                             \
        Arena* arena;                                           \
        T* cont;                                             \
    } Stack##T;                                                 \
    testable Stack ## T * createStack ## T (Int initCapacity, Arena* a); \
    testable bool hasValues ## T (Stack ## T * st);                      \
    testable T pop ## T (Stack ## T * st);                               \
    testable T peek ## T(Stack ## T * st);                               \
    testable T penultimate ## T(Stack ## T * st);                        \
    testable void push ## T (T newItem, Stack ## T * st);                \
    testable void clear ## T (Stack ## T * st);

#define DEFINE_INTERNAL_LIST_TYPE(T) \
typedef struct {    \
    Int cap;   \
    Int len;     \
    Arr(T) cont; \
} InList##T;

#define DEFINE_INTERNAL_LIST_HEADER(fieldName, T)              \
    InList##T createInList ## T (Int initCapacity, Arena* a); \
    void pushIn ## fieldName (T newItem, Compiler* cm);


/// A growable list of growable lists of pairs of non-negative Ints. Is smart enough to reuse
/// old allocations via an intrusive free list.
/// Internal lists have the structure [len cap ...data...] or [nextFree cap ...] for the free sectors
/// Units of measurement of len and cap are 1's. I.e. len can never be = 1.
typedef struct { // :MultiList
    Int len;
    Int cap;
    Int freeList;
    Arr(Int) cont;
    Arena* a;
} MultiList;


typedef struct {
    int len;
    byte cont[];
} String;
testable void printStringNoLn(String* s);
testable void printString(String* s);
extern String empty;
testable String* str(const char* content, Arena* a);
testable bool endsWith(String* a, String* b);

#define s(lit) str(lit, a)

typedef struct {
    Int len;
    Int numNames;
    Int firstNonreserved;
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

/** Reference to first occurrence of a string identifier within input text */
typedef struct {
    untt hash;
    Int indString;
} StringValue;


typedef struct {
    untt capAndLen;
    StringValue cont[];
} Bucket;

/** Hash map of all words/identifiers encountered in a source module */
typedef struct {
    Arr(Bucket*) dict;
    int dictSize;
    int len;
    Arena* a;
} StringDict;

//}}}
//{{{ Lexer

//{{{ Standard strings

#define strAlias      0
#define strAnd        1
#define strAssert     2
#define strAwait      3
#define strBreak      4
#define strCatch      5
#define strContinue   6
#define strDefer      7
#define strDo         8
#define strElse       9
#define strEmbed     10
#define strFalse     11
#define strFn        12
#define strFor       13
#define strIf        14
#define strIfPr      15
#define strImpl      16
#define strImport    17
#define strInterface 18
#define strMatch     19
#define strOr        20
#define strReturn    21
#define strTrue      22
#define strTry       23
#define strWhile     24
#define strYield     25

#define strInt       26
#define strFirstNonReserved strInt
#define strLong      27
#define strDouble    28
#define strBool      29
#define strString    30
#define strVoid      31
#define strF         32
#define strL         33 // List
#define strA         34 // Array
#define strTu        35 // Tu(ple)
#define strLen       36
#define strCap       37
#define strF1        38
#define strF2        39
#define strPrint     40
#define strAlert     41
#define strMathPi    42
#define strMathE     43

//}}}

/** Backtrack token, used during lexing to keep track of all the nested stuff */
typedef struct { //:BtToken
    untt tp : 6;
    Int tokenInd;
    Int countClauses;
    untt spanLevel : 3;
    bool wasOrigDollar;
} BtToken;

DEFINE_STACK_HEADER(BtToken)

typedef struct { //:Token
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;
} Token;

/**
 * Regular (leaf) Token types
 */
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt          0
#define tokLong         1
#define tokFloat        2
#define tokBool         3    // pl2 = value (1 or 0)
#define tokString       4
#define tokUnderscore   5    // in the world of types, signifies the Void type

#define tokWord         6    // pl2 = index in the string table
#define tokTypeName     7    // pl1 = 1 iff it has arity (like "M/2"), pl2 = same as tokWord
#define tokKwArg        8    // pl2 = same as tokWord. The ":argName"
#define tokStructField  9    // pl2 = same as tokWord. The ".structField"
#define tokOperator    10    // pl1 = OperatorToken, one of the "opT" constants below
#define tokAccessor    11    // pl1 = see "tkAcc" consts. Either an ".accessor" or a "@"
#define tokArrow       12

// Single-statement token types
#define tokStmt        13    // firstSpanTokenType
#define tokParens      14    // this is mostly for data instantiation
#define tokCall        15    // pl1 = nameId, index in the string table
#define tokTypeCall    16    // pl1 = nameId
#define tokOperCall    17    // pl1 = same as tokOperator
#define tokBrackets    18
#define tokAssignment  19
#define tokReassign    20    // :=
#define tokMutation    21    // pl1 = (6 bits opType, 26 bits startBt of the operator symbol) "+="
#define tokAlias       22
#define tokAssert      23
#define tokAssertDbg   24
#define tokAwait       25
#define tokBreak       26
#define tokContinue    27
#define tokDefer       28
#define tokEmbed       29    // Embed a text file as a string literal, or a binary resource file // 200
#define tokIface       30
#define tokImport      31
#define tokReturn      32
#define tokTry         33    // early exit
#define tokYield       34
#define tokColon       35    // not a real span, but placed here so the parser can dispatch on it
#define tokElse        36    // not a real span, but placed here so the parser can dispatch on it

// Parenthesized (multi-statement) token types. pl1 = spanLevel, see "sl" constants
#define tokScope       37    // denoted by do(). firstParenSpanTokenType
#define tokCatch       38    // paren "catch(e => print(e))"
#define tokFn          39
#define tokPublicDef   40
#define tokFor         41
#define tokMeta        42
#define tokPackage     43    // for single-file packages

// Resumable core forms
#define tokIf          44    // "if( "
#define tokIfPr        45    // like if, but every branch is a value compared using custom predicate
#define tokMatch       46    // "(*m " or "(match " pattern matching on sum type tag
#define tokImpl        47
#define tokWhile       48

#define topVerbatimTokenVariant tokUnderscore
#define topVerbatimType tokString
#define firstSpanTokenType tokStmt
#define firstParenSpanTokenType tokScope
#define firstResumableSpanTokenType tokIf


/** Nodes */
#define nodId           7    // pl1 = index of entity, pl2 = index of name
#define nodCall         8    // pl1 = index of entity, pl2 = arity
#define nodComplexCall  9    // pl2 = arity
#define nodBinding     10    // pl1 = index of entity, pl2 = 1 if it's a type binding

// Punctuation (inner node)
#define nodScope       11     // (* This is resumable but trivially so, that's why it's not grouped with the others
#define nodExpr        12
#define nodAssignment  13
#define nodReassign    14     // :=
#define nodAccessor    15     // pl1 = "acc" constants
#define nodArglist     16 // Used after nodComplexCall

// Single-shot core syntax forms
#define nodAlias       17
#define nodAssert      18
#define nodAssertDbg   19
#define nodAwait       20
#define nodBreak       21     // pl1 = number of label to break to, or -1 if none needed
#define nodCatch       22     // "(catch e => print e)"
#define nodContinue    23     // pl1 = number of label to continue to, or -1 if none needed
#define nodDefer       24
#define nodEmbed       25     // noParen. Embed a text file as a string literal, or a binary resource file
#define nodExport      26
#define nodExposePriv  27     // TODO replace with "import". This is for test files
#define nodFnDef       28     // pl1 = entityId
#define nodIface       29
#define nodLambda      30
#define nodMeta        31
#define nodPackage     32     // for single-file packages
#define nodReturn      33
#define nodTry         34     // the Rust kind of "try" (early return from current function)
#define nodYield       35
#define nodIfClause    36
#define nodWhile       37     // pl1 = id of loop (unique within a function) if it needs to have a label in codegen
#define nodWhileCond   38

// Resumable core forms
#define nodIf          39
#define nodImpl        40
#define nodMatch       41     // pattern matching on sum type tag

#define firstResumableForm nodIf
#define countResumableForms (nodMatch - nodIf + 1)
#define countSpanForms (nodMatch - nodScope + 1)


/**
 * There is a closed set of operators in the language.
 *
 * For added flexibility, some operators may be extended into one more planes,
 * for example '+' may be extended into '+.', while '/' may be extended into '/.'.
 * These extended operators are declared by the language, and may be defined
 * for any type by the user, with the return type being arbitrary.
 * For example, the type of 3D vectors may have two different multiplication
 * operators: *. for vector product and * for scalar product.
 *
 * Plus, many have automatic assignment counterparts.
 * For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
 */
typedef struct { //:OpDef
    String* name;
    byte bytes[4];
    Int arity;
    /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
    bool overloadable;
    bool assignable;
    bool isTypelevel;
    byte prece;
    int8_t lenBts;
} OpDef;

#define preceEquality 1
#define preceAdd 2
#define preceMultiply 3
#define preceExponent 4
#define preceFn 5
#define precePrefix 6

/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array in the tl.c file.
 * The order is defined by ASCII.
 */
#define opTNotEqual          0 // !=
#define opTBoolNegation      1 // !. bitwise negation
#define opTBoolNegation      2 // !
#define opTSize              3 // #
#define opTRemainder         4 // %
#define opTBoolAnd           5 // &&. bitwise and
#define opTBoolAnd           6 // && logical and
#define opTPtr               7 // & pointers/values at type level
#define opTIsNull            8 // '
#define opTTimesExt          9 // *.
#define opTTimes            10 // * and interface intersection
#define opTPlusExt          11 // +.
#define opTPlus             12 // +
#define opTToInt            13 // ,,
#define opTToFloat          14 // ,
#define opTMinusExt         15 // -.
#define opTMinus            16 // -
#define opTDivByExt         17 // /.
#define opTIntersection     18 // /|
#define opTDivBy            19 // /
#define opTToString         20 // ;
#define opTBitShiftLeft     21 // <<.
#define opTShiftLeft        22 // <<
#define opTLTEQ             23 // <=
#define opTComparator       24 // <>
#define opTLessThan         25 // <
#define opTEquality         26 // ==
#define opTIntervalBoth     27 // >=<= inclusive interval check
#define opTIntervalRight    28 // ><=  right-inclusive interval check
#define opTIntervalLeft     29 // >=<  left-inclusive interval check
#define opTBitShiftRight    30 // >>.  unsigned right bit shift
#define opTIntervalExcl     31 // ><   exclusive interval check
#define opTGTEQ             32 // >=
#define opTShiftRight       33 // >>  unsigned right bit shift
#define opTGreaterThan      34 // >
#define opTNullCoalesce     35 // ?:   null coalescing operator
#define opTQuestionMark     36 // ?    nullable type operator
#define opTExponentExt      37 // ^.   bitwise XOR
#define opTExponent         38 // ^    exponentiation
#define opTBoolOr           39 // ||.  bitwise or
#define opTBoolOr           40 // ||   logical or

#define countOperators      41

#define countReservedLetters   25 // length of the interval of letters that may be init for reserved words (A to Y)
#define countCoreForms (tokWhile - tokAlias + 1)
#define countSyntaxForms (tokWhile + 1)
typedef struct Compiler Compiler;


// Subclasses of the data accessor tokens/nodes
#define tkAccDot     1
#define tkAccAt      2
#define accField     1    // field accessor in a struct, like "foo.field". pl2 = nameId of the string
#define accArrayInd  2    // single-integer array access, like "arr@5". pl2 = int value of the ind
#define accArrayWord 3    // single-variable array access, like "arr@i". pl2 = nameId of the string
#define accString    4    // string-based access inside hashmap, like "map@`foo`". pl2 = 0
#define accExpr      5    // an expression for an array access, like "arr:+(i 1)". pl2 = number of tokens/nodes
#define accUndef     6    // undefined after lexing (to be determined by the parser)


typedef void (*LexerFunc)(Compiler*, Arr(byte)); // LexerFunc = &(Lexer* => void)
typedef Int (*ReservedProbe)(int, int, Compiler*);
typedef void (*ParserFunc)(Token, Arr(Token), Compiler*);
typedef void (*ResumeFunc)(Token*, Arr(Token), Compiler*);

typedef struct { //:LanguageDefinition
    OpDef (*operators)[countOperators];
    LexerFunc (*dispatchTable)[256];
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    ParserFunc (*nonResumableTable)[countSyntaxForms];
    ResumeFunc (*resumableTable)[countResumableForms];
} LanguageDefinition;


typedef struct {
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    Int pl1;
    Int pl2;
} Node;

typedef struct {
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
#define emitPrefix         1  // normal singular native names
#define emitOverloaded     2  // normal singular native names
#define emitPrefixShielded 3  // a native name in need of shielding from target reserved word (by appending a "_")
#define emitPrefixExternal 4  // prefix names that are emitted differently than in source code
#define emitInfix          5  // infix operators that match between source code and target (e.g. +)
#define emitInfixExternal  6  // infix operators that have a separate external name
#define emitField          7  // emitted as field accesses, like ".length"
#define emitInfixDot       8  // emitted as a "dot-call", like ".toString()"
#define emitNop            9  // for unary operators that don't need to be emitted, like ","

typedef struct { //:Entity
    Int typeId;
    Int name;
    uint16_t externalNameId;
    uint8_t class;
    uint8_t emit;
    bool isPublic;
    bool hasExceptionHandler;
} Entity;

typedef struct {
    untt name;  // 8 bits of length, 24 bits or nameId
    Int externalNameId; // index in the "codegenText"
    Int typeInd; // index in the intermediary array of types that is imported alongside
} EntityImport;

typedef struct { // :Toplevel
    /// Toplevel definitions (functions, variables, types) for parsing order and name searchability
    Int indToken;
    Int sentinelToken;
    untt name;
    Int entityId; // if n < 0 => -n - 1 is an index into [functions], otherwise n => [entities]
    bool isFunction;
} Toplevel;

typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct ScopeChunk ScopeChunk;
struct ScopeChunk {
    ScopeChunk *next;
    int len; // length is divisible by 4
    Int cont[];
};

typedef struct { //:ScopeStack
    /// Either currChunk->next == NULL or currChunk->next->next == NULL
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    ScopeStackFrame* topScope;
    Int len;
    int nextInd; // next ind inside currChunk, unit of measurement is 4 bytes
} ScopeStack;

DEFINE_STACK_HEADER(ParseFrame)
DEFINE_STACK_HEADER(Node)

DEFINE_INTERNAL_LIST_TYPE(Token)
DEFINE_INTERNAL_LIST_TYPE(Node)

DEFINE_INTERNAL_LIST_TYPE(Entity)

DEFINE_INTERNAL_LIST_TYPE(Toplevel)
DEFINE_INTERNAL_LIST_TYPE(Int)

DEFINE_INTERNAL_LIST_TYPE(uint32_t)

DEFINE_INTERNAL_LIST_TYPE(EntityImport)

struct Compiler { //:Compiler
    /// See docs/compiler.txt, docs/architecture.svgz
    LanguageDefinition* langDef;

    // LEXING
    String* sourceCode;
    Int inpLength;
    InListToken tokens;
    InListInt newlines;
    InListInt numeric;          // [aTmp]
    StackBtToken* lexBtrack;    // [aTmp]
    Stackint32_t* stringTable;
    StringDict* stringDict;
    Int lastClosingPunctInd;

    // PARSING
    InListToplevel toplevels;
    InListInt imports;
    StackParseFrame* backtrack; // [aTmp]
    ScopeStack* scopeStack;
    Arr(Int) activeBindings;    // [aTmp]
    Int loopCounter;
    InListNode nodes;
    InListNode monoCode;
    MultiList* monoIds;
    InListEntity entities;
    InListInt overloads;
    InListInt types;
    StringDict* typesDict;
    Int countNonparsedEntities;
    StackInt* expStack;   // [aTmp]
    StackInt* typeStack;  // [aTmp]
    StackInt* tempStack;  // [aTmp]
    Int countOverloads;
    MultiList* rawOverloads; // [aTmp]

    // GENERAL STATE
    Int i;
    bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aTmp;
};


/** Span levels */
#define slScope         1 // scopes (denoted by brackets): newlines and commas just have no effect in them
#define slParenMulti    2 // things like "(if)": they're multiline but they cannot contain any brackets
#define slStmt          3 // single-line statements: newlines and commas break 'em
#define slSubexpr       4 // parens and the like: newlines have no effect, dots error out


//}}}
//{{{ Types

/// see the Type layout chapter in the docs
#define sorStruct       1
#define sorSum          2
#define sorFunction     3
#define sorPartial      4 // Partially applied type
#define sorConcrete     5 // Fully applied type (no type params)

typedef struct {
    byte sort;
    byte arity;
    byte depth;
    untt nameAndLen;
} TypeHeader;

//}}}
//{{{ Generics

#define pop(X) _Generic((X), \
    StackBtToken*: popBtToken, \
    StackParseFrame*: popParseFrame, \
    Stackint32_t*: popint32_t, \
    StackNode*: popNode \
    )(X)

#define peek(X) _Generic((X), \
    StackBtToken*: peekBtToken, \
    StackParseFrame*: peekParseFrame, \
    Stackint32_t*: peekint32_t, \
    StackNode*: peekNode \
    )(X)

#define penultimate(X) _Generic((X), \
    StackBtToken*: penultimateBtToken, \
    StackParseFrame*: penultimateParseFrame, \
    Stackint32_t*: penultimateint32_t, \
    StackNode*: penultimateNode \
    )(X)

#define push(A, X) _Generic((X), \
    StackBtToken*: pushBtToken, \
    StackParseFrame*: pushParseFrame, \
    Stackint32_t*: pushint32_t, \
    StackNode*: pushNode \
    )(A, X)

#define hasValues(X) _Generic((X), \
    StackBtToken*: hasValuesBtToken, \
    StackParseFrame*: hasValuesParseFrame, \
    Stackint32_t*:  hasValuesint32_t, \
    StackNode*: hasValuesNode \
    )(X)

//}}}

