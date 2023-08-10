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
        Int capacity;                                           \
        Int length;                                             \
        Arena* arena;                                           \
        T* content;                                             \
    } Stack##T;                                                 \
    testable Stack ## T * createStack ## T (Int initCapacity, Arena* a); \
    testable bool hasValues ## T (Stack ## T * st);                      \
    testable T pop ## T (Stack ## T * st);                               \
    testable T peek ## T(Stack ## T * st);                               \
    testable void push ## T (T newItem, Stack ## T * st);                \
    testable void clear ## T (Stack ## T * st);

#define DEFINE_INTERNAL_LIST_TYPE(T) \
typedef struct {    \
    Int capacity;   \
    Int length;     \
    Arr(T) content; \
} InList##T;

#define DEFINE_INTERNAL_LIST_HEADER(fieldName, T)              \
    InList##T createInList ## T (Int initCapacity, Arena* a); \
    void pushIn ## fieldName (T newItem, Compiler* cm);

typedef struct {
    int length;
    byte content[];
} String;
testable void printStringNoLn(String* s);
testable void printString(String* s);
extern String empty;
testable String* str(const char* content, Arena* a);
testable bool endsWith(String* a, String* b);

#define s(lit) str(lit, a)

//}}}
//{{{ Int Hashmap

DEFINE_STACK_HEADER(int32_t)
typedef struct {
    Arr(int*) dict;
    int dictSize;
    int length;    
    Arena* a;
} IntMap;

//}}}
//{{{ String Hashmap

/** Reference to first occurrence of a string identifier within input text */
typedef struct {
    Int length;
    Int indString;
} StringValue;


typedef struct {
    untt capAndLen;
    StringValue content[];
} Bucket;

/** Hash map of all words/identifiers encountered in a source module */
typedef struct {
    Arr(Bucket*) dict;
    int dictSize;
    int length;    
    Arena* a;
} StringStore;

//}}}
//{{{ Lexer

/** Backtrack token, used during lexing to keep track of all the nested stuff */
typedef struct {
    untt tp : 6;
    Int tokenInd;
    Int countClauses;
    untt spanLevel : 3;
    bool wasOrigDollar;
} BtToken;

DEFINE_STACK_HEADER(BtToken)
 
typedef struct {
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
#define tokDocComment   6

// This group requires analysis in the parser
#define tokWord         7    // pl2 = index in the string table
#define tokTypeName     8
#define tokOperator     9    // pl1 = OperatorToken, one of the "opT" constants below
#define tokEqualsSign  10

// Punctuation (inner node) Token types
#define tokScope       11     // denoted by (*)
#define tokStmt        12
#define tokParens      13
#define tokCall        14
#define tokData        15     // data initializer, like (: 1 2 3)
#define tokAssignment  16
#define tokReassign    17     // :=
#define tokMutation    18     // pl1 = (6 bits opType, 26 bits startBt of the operator symbol) These are the "+=" things
#define tokAccessor    19     // pl1 = subclass (field, array ind etc), pl2 = see "acc" consts
#define tokArrow       20     // not a real scope, but placed here so the parser can dispatch on it
#define tokElse        21     // not a real scope, but placed here so the parser can dispatch on it
 
// Single-shot core syntax forms. pl1 = spanLevel
#define tokAlias       22
#define tokAssert      23
#define tokAssertDbg   24
#define tokAwait       25
#define tokBreak       26
#define tokCatch       27    // paren "catch(e => print(e))"
#define tokContinue    28
#define tokDefer       29
#define tokEach        30
#define tokEmbed       31    // Embed a text file as a string literal, or a binary resource file // 200
#define tokExport      32
#define tokExposePriv  33
#define tokFnDef       34
#define tokIface       35
#define tokLambda      36
#define tokMeta        37
#define tokPackage     38    // for single-file packages
#define tokReturn      39
#define tokStruct      40
#define tokTry         41    // early exit
#define tokTypeDef     42
#define tokYield       43

// Resumable core forms
#define tokIf          44    // "if( " 
#define tokIfPr        45    // like if, but every branch is a value compared using custom predicate
#define tokMatch       46    // "(*m " or "(match " pattern matching on sum type tag 
#define tokImpl        47  
#define tokWhile       48   
// "(*iface"
#define topVerbatimTokenVariant tokUnderscore
#define topVerbatimType tokString


#define nodId           7    // pl1 = index of entity, pl2 = index of name
#define nodCall         8    // pl1 = index of entity, pl2 = arity
#define nodBinding      9    // pl1 = index of entity, pl2 = 1 if it's a type binding

// Punctuation (inner node)
#define nodScope       10     // (* This is resumable but trivially so, that's why it's not grouped with the others
#define nodExpr        11
#define nodAssignment  12
#define nodReassign    13     // :=
#define nodAccessor    14     // pl1 = "acc" constants

// Single-shot core syntax forms
#define nodAlias       15
#define nodAssert      16
#define nodAssertDbg   17
#define nodAwait       18
#define nodBreak       19     // pl1 = number of label to break to, or -1 if none needed
#define nodCatch       20     // "(catch e => print e)"
#define nodContinue    21     // pl1 = number of label to continue to, or -1 if none needed
#define nodDefer       22
#define nodEmbed       23     // noParen. Embed a text file as a string literal, or a binary resource file
#define nodExport      24       
#define nodExposePriv  25     // TODO replace with "import". This is for test files
#define nodFnDef       26     // pl1 = entityId
#define nodIface       27
#define nodLambda      28
#define nodMeta        29       
#define nodPackage     30     // for single-file packages
#define nodReturn      31
#define nodTry         32     // the Rust kind of "try" (early return from current function)
#define nodYield       33
#define nodIfClause    34       
#define nodWhile       34     // pl1 = id of loop (unique within a function) if it needs to have a label in codegen
#define nodWhileCond   36

// Resumable core forms
#define nodIf          37
#define nodImpl        38
#define nodMatch       39     // pattern matching on sum type tag

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
typedef struct {
    String* name;
    byte bytes[4];
    Int arity;
    /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
    bool overloadable;
    bool assignable;
    Int builtinOverloads;
    int8_t lenBts;
} OpDef;


/**
 * OperatorType
 * Values must exactly agree in order with the operatorSymbols array in the lexer.c file.
 * The order is defined by ASCII.
 */

#define opTNotEqual          0 // !=
#define opTBoolNegation      1 // !
#define opTSize              2 // #
#define opTToString          3 // $
#define opTRemainder         4 // %
#define opTBinaryAnd         5 // && bitwise and
#define opTTypeAnd           6 // & interface intersection (type-level)
#define opTIsNull            7 // '
#define opTTimesExt          8 // *.
#define opTTimes             9 // *
#define opTPlusExt          10 // +.
#define opTPlus             11 // +
#define opTToInt            12 // ,,
#define opTToFloat          13 // ,
#define opTMinusExt         14 // -.
#define opTMinus            15 // -
#define opTDivByExt         16 // /.
#define opTDivBy            17 // /
#define opTBitShiftLeftExt  18 // <<.
#define opTBitShiftLeft     19 // <<
#define opTLTEQ             20 // <=
#define opTComparator       21 // <>
#define opTLessThan         22 // <
#define opTEquality         23 // ==
#define opTIntervalBoth     24 // >=<= inclusive interval check
#define opTIntervalLeft     25 // >=<  left-inclusive interval check
#define opTIntervalRight    26 // ><=  right-inclusive interval check
#define opTIntervalExcl     27 // ><   exclusive interval check
#define opTGTEQ             28 // >=
#define opTBitShiftRightExt 29 // >>.  unsigned right bit shift
#define opTBitShiftRight    30 // >>   right bit shift
#define opTGreaterThan      31 // >
#define opTNullCoalesce     32 // ?:   null coalescing operator
#define opTQuestionMark     33 // ?    nullable type operator
#define opTExponentExt      34 // ^.   exponentiation extended
#define opTExponent         35 // ^    exponentiation
#define opTBoolOr           36 // ||   bitwise or
#define opTXor              37 // |    bitwise xor
#define opTAnd              38
#define opTOr               39
#define opTNegation         40

/** Count of lexical operators, i.e. things that are lexed as operator tokens.
 * must be equal to the count of following constants
 */ 
#define countLexOperators   38
#define countOperators      41 // count of things that are stored as operators, regardless of how they are lexed

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

typedef struct {
    OpDef (*operators)[countOperators];
    LexerFunc (*dispatchTable)[256];
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    Int (*reservedParensOrNot)[countCoreForms];
    ParserFunc (*nonResumableTable)[countSyntaxForms];
    ResumeFunc (*resumableTable)[countResumableForms];
    String* baseTypes[6];
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


#define classMutableGuaranteed 0
#define classMutatedGuaranteed 1
#define classMutableNullable   2
#define classMutatedNullable   3
#define classImmutable         4
#define emitPrefix         1  // normal singular native names
#define emitOverloaded     2  // normal singular native names
#define emitPrefixShielded 3  // this is a native name that needs to be shielded from target reserved word (by appending a "_")
#define emitPrefixExternal 4  // prefix names that are emitted differently than in source code
#define emitInfix          5  // infix operators that match between source code and target (e.g. arithmetic operators)
#define emitInfixExternal  6  // infix operators that have a separate external name
#define emitField          7  // emitted as field accesses, like ".length"
#define emitInfixDot       8  // emitted as a "dot-call", like ".toString()"
#define emitNop            9  // for unary operators that don't need to be emitted, like ","

typedef struct {
    Int typeId;
    uint16_t externalNameId;
    uint8_t class;
    uint8_t emit;
} Entity;


typedef struct {
    String* name;
    Int nameId;  // index in the stringTable of the current compilation
    Int externalNameId; // index in the array of constant strings
    Int typeInd; // index in the intermediary array of types that is imported alongside
} EntityImport;

typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct ScopeChunk ScopeChunk;
struct ScopeChunk {
    ScopeChunk *next;
    int length; // length is divisible by 4
    Int content[];   
};

/** 
 * Either currChunk->next == NULL or currChunk->next->next == NULL
 */
typedef struct {
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    ScopeStackFrame* topScope;
    Int length;
    int nextInd; // next ind inside currChunk, unit of measurement is 4 bytes
} ScopeStack;

DEFINE_STACK_HEADER(ParseFrame)
DEFINE_STACK_HEADER(Node)

DEFINE_INTERNAL_LIST_TYPE(Node)

DEFINE_INTERNAL_LIST_TYPE(Entity)


DEFINE_INTERNAL_LIST_TYPE(Int)

DEFINE_INTERNAL_LIST_TYPE(uint32_t)

DEFINE_INTERNAL_LIST_TYPE(EntityImport)


/*
 * COMPILER DATA
 * 
 * 1) a = Arena for the results
 * AST (i.e. the resulting code)
 * Entities
 * Types
 *
 * 2) aBt = Arena for the temporary stuff (like the backtrack). Freed after end of parsing
 *
 * 3) ScopeStack (temporary, but knows how to free parts of itself, so in a separate arena)
 *
 * WORKFLOW
 * The "stringTable" is frozen: it was filled by the lexer. The "bindings" table is growing
 * with every new assignment form encountered. "Nodes" is obviously growing with the new AST nodes
 * emitted.
 * Any new span (scope, expression, assignment etc) means 
 * - pushing a ParseFrame onto the "backtrack" stack
 * - if the new frame is a lexical scope, also pushing a scope onto the "scopeStack"
 * The end of a span means popping from "backtrack" and also, if needed, popping from "scopeStack".
 *
 * ENTITIES AND OVERLOADS
 * In the parser, all non-functions are bound to Entities, but functions are instead bound to Overloads.
 * Binding ids go: 0, 1, 2... Overload ids go: -2, -3, -4...
 * Those overloads are used for counting how many functions for each name there are.
 * Then they are resolved at the stage of the typer, after which there are only the Entities.
 */
struct Compiler {
    String* sourceCode;
    Int inpLength;

    LanguageDefinition* langDef;

    // LEXING
    Arr(Token) tokens;
    Int capacity;              // current capacity of token storage
    Int nextInd;               // the  index for the next token to be added    
    
    Arr(Int) newlines;
    Int newlinesCapacity;
    Int newlinesNextInd;
    
    Arr(Int) numeric;          // [aTmp]
    Int numericCapacity;
    Int numericNextInd;

    StackBtToken* lexBtrack;    // [aTmp]
    
    Stackint32_t* stringTable;   // The table of unique strings from code. Contains only the startByte of each string.
    StringStore* stringStore;    // A hash table for quickly deduplicating strings. Points into stringTable

    Int lastClosingPunctInd;   // temp, the index of the last encountered closing punctuation sign, used for statement length

    Int totalTokens;           // set in "finalizeLexing"

    // PARSING
    StackParseFrame* backtrack;// [aTmp] 
    ScopeStack* scopeStack;    // stack of currently active scopes (for active bindings tracking)

    /*
     *  Current entities and overloads in scope. -1 means "inactive"
     * Var & type bindings are nameId (index into stringTable) -> bindingId
     * Function bindings are nameId -> (-overloadId - 2). So a negative value less than -1 means "the function is active"
     */
    Arr(Int) activeBindings;   // [aTmp] length = stringTable.length

    Int loopCounter;           // used to assign unique labels to loops. Restarts at function start

    InListNode nodes;
    InListEntity entities;    // growing array of all entities (variables, function defs, constants etc) ever encountered    
    Int entImportedZero;      // the index of the first imported entity
    
    /**
     * [aTmp] Initially, growing array of counts of all fn names encountered.
     * Upper 16 bits contain concrete count, lower 16 bits total count. After "createOverloads", contains overload indices
     * into the "overloads" table.
     */
    InListUns overloadIds;

    InListInt overloads;

    InListInt types; // ([] (arity + 1) returnType param1Type param2Type...)
    StringStore* typesDict;

    InListEntityImport imports; // [aTmp] imported function overloads

    Int countOperatorEntities;
    Int countNonparsedEntities; // the index of the first parsed (as opposed to being built-in or imported) entity
    Stackint32_t* expStack;    // [aTmp] temporary scratch space for type checking/resolving an expression
    
    // GENERAL STATE
    Int i;                     // index in the input
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

/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokScope
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokAlias

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

