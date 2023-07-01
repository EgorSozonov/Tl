//{{{ Utils

#define Int int32_t
#define StackInt Stackint32_t
#define private static
#define byte unsigned char
#define Arr(T) T*
#define untt uint32_t


#define LOWER24BITS 0x00FFFFFF
#define LOWER26BITS 0x03FFFFFF
#define LOWER16BITS 0x0000FFFF
#define LOWER32BITS 0x00000000FFFFFFFF
#define THIRTYFIRSTBIT 0x40000000
#define MAXTOKENLEN 67108864 // 2^26
#define SIXTEENPLUSONE 65537 // 2^16 + 1
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

Arena* mkArena();
void* allocateOnArena(size_t allocSize, Arena* ar);
void deleteArena(Arena* ar);

#define DEFINE_STACK_HEADER(T)                                  \
    typedef struct {                                            \
        Int capacity;                                           \
        Int length;                                             \
        Arena* arena;                                           \
        T* content;                                             \
    } Stack##T;                                                 \
    Stack ## T * createStack ## T (Int initCapacity, Arena* a); \
    bool hasValues ## T (Stack ## T * st);                      \
    T pop ## T (Stack ## T * st);                               \
    T peek ## T(Stack ## T * st);                               \
    void push ## T (T newItem, Stack ## T * st);                \
    void clear ## T (Stack ## T * st);


typedef struct {
    int length;
    byte content[];
} String;
void printStringNoLn(String* s);
void printString(String* s);
extern String empty;
String* str(const char* content, Arena* a);
#define s(lit) str(lit, a)
bool endsWith(String* a, String* b);

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
    bool wasOrigColon;
} BtToken;

DEFINE_STACK_HEADER(BtToken)
 
typedef struct {
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;
} Token;


typedef struct LanguageDefinition LanguageDefinition;
typedef struct Lexer Lexer;
typedef void (*LexerFunc)(Lexer*, Arr(byte)); // LexerFunc = &(Lexer* => void)
typedef Int (*ReservedProbe)(int, int, struct Lexer*);
#define countReservedLetters         25 // length of the interval of letters that may be init for reserved words (A to Y)

struct Lexer {
    Int i;                     // index in the input text
    String* inp;
    Int inpLength;
    Int totalTokens;
    Int lastClosingPunctInd;   // the index of the last encountered closing punctuation sign, used for statement length
    Int lastLineInitToken;     // index of the last token that was initial in a line of text
    
    LanguageDefinition* langDef;
    
    Arr(Token) tokens;
    Int capacity;              // current capacity of token storage
    Int nextInd;               // the  index for the next token to be added    
    
    Arr(Int) newlines;
    Int newlinesCapacity;
    Int newlinesNextInd;
    
    Arr(byte) numeric;          // [aTmp]
    Int numericCapacity;
    Int numericNextInd;

    StackBtToken* backtrack;    // [aTmp]
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    
    Stackint32_t* stringTable;   // The table of unique strings from code. Contains only the startByte of each string.
    StringStore* stringStore;    // A hash table for quickly deduplicating strings. Points into stringTable    
    
    bool wasError;
    String* errMsg;

    Arena* arena;
    Arena* aTmp;
};

/**
 * Regular (leaf) Token types
 */
// The following group of variants are transferred to the AST byte for byte, with no analysis
// Their values must exactly correspond with the initial group of variants in "RegularAST"
// The largest value must be stored in "topVerbatimTokenVariant" constant
#define tokInt          0
#define tokLong         1
#define tokFloat        2
#define tokBool         3      // pl2 = value (1 or 0)
#define tokString       4
#define tokUnderscore   5
#define tokDocComment   6

// This group requires analysis in the parser
#define tokWord         7      // pl2 = index in the string table
#define tokTypeName     8
#define tokDotWord      9      // ".fieldName", pl's the same as tokWord
#define tokOperator    10      // pl1 = OperatorToken, one of the "opT" constants below
#define tokDispose     11

// This is a temporary Token type for use during lexing only. In the final token stream it's replaced with tokParens
#define tokColon       12 

// Punctuation (inner node) Token types
#define tokScope       13       // denoted by (.)
#define tokStmt        14
#define tokParens      15
#define tokTypeParens  16
#define tokData        17       // data initializer, like (: 1 2 3)
#define tokAssignment  18       // pl1 = 1 if mutable assignment, 0 if immutable 
#define tokReassign    19       // :=
#define tokMutation    20       // pl1 = like in topOperator. This is the "+=", operatorful type of mutations
#define tokArrow       21       // not a real scope, but placed here so the parser can dispatch on it
#define tokElse        22       // not a real scope, but placed here so the parser can dispatch on it
 
// Single-shot core syntax forms. pl1 = spanLevel
#define tokAlias       23
#define tokAssert      24
#define tokAssertDbg   25
#define tokAwait       26
#define tokBreak       27
#define tokCatch       28       // paren "(catch e. e .print)"
#define tokContinue    29
#define tokDefer       30
#define tokEach        31
#define tokEmbed       32       // Embed a text file as a string literal, or a binary resource file // 200
#define tokExport      33
#define tokExposePriv  34
#define tokFnDef       35
#define tokIface       36
#define tokLambda      37
#define tokMeta        38
#define tokPackage     39       // for single-file packages
#define tokReturn      40
#define tokStruct      41
#define tokTry         42       // early exit
#define tokYield       43

// Resumable core forms
#define tokIf          44    // "(if " or "(-i". pl1 = 1 if it's the "(-" variant
#define tokIfPr        45    // like if, but every branch is a value compared using custom predicate
#define tokMatch       46    // "(-m " or "(match " pattern matching on sum type tag 
#define tokImpl        47    // "(-impl " 
#define tokLoop        48    // "(-loop "
// "(-iface"
#define topVerbatimTokenVariant tokUnderscore

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
#define opTIncrement        10 // ++
#define opTPlusExt          11 // +.
#define opTPlus             12 // +
#define opTToFloat          13 // ,
#define opTDecrement        14 // --
#define opTMinusExt         15 // -.
#define opTMinus            16 // -
#define opTDivByExt         17 // /.
#define opTDivBy            18 // /
#define opTBitShiftLeftExt  19 // <<.
#define opTBitShiftLeft     20 // <<
#define opTLTEQ             21 // <=
#define opTComparator       22 // <>
#define opTLessThan         23 // <
#define opTEquality         24 // ==
#define opTIntervalBoth     25 // >=<= inclusive interval check
#define opTIntervalLeft     26 // >=<  left-inclusive interval check
#define opTIntervalRight    27 // ><=  right-inclusive interval check
#define opTIntervalExcl     28 // ><   exclusive interval check
#define opTGTEQ             29 // >=
#define opTBitShiftRightExt 30 // >>.  unsigned right bit shift
#define opTBitshiftRight    31 // >>   right bit shift
#define opTGreaterThan      32 // >
#define opTNullCoalesce     33 // ?:   null coalescing operator
#define opTQuestionMark     34 // ?    nullable type operator
#define opTAccessor         35 // @
#define opTExponentExt      36 // ^.   exponentiation extended
#define opTExponent         37 // ^    exponentiation
#define opTBoolOr           38 // ||   bitwise or
#define opTXor              39 // |    bitwise xor
#define opTAnd              40
#define opTOr               41
#define opTNegation         42

/** Span levels */
#define slScope         1 // scopes (denoted by brackets): newlines and commas just have no effect in them
#define slParenMulti    2 // things like "(if)": they're multiline but they cannot contain any brackets
#define slStmt          3 // single-line statements: newlines and commas break 'em
#define slSubexpr       4 // parens and the like: newlines have no effect, dots error out
    
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
} OpDef;

/** Count of lexical operators, i.e. things that are lexed as operator tokens.
 * must be equal to the count of following constants
 */ 
#define countLexOperators   40
#define countOperators      43 // count of things that are stored as operators, regardless of how they are lexed
/** Must be the lowest value in the PunctuationToken enum */
#define firstPunctuationTokenType tokScope
/** Must be the lowest value of the punctuation token that corresponds to a core syntax form */
#define firstCoreFormTokenType tokAlias

#define countCoreForms (tokLoop - tokAlias + 1)
#define countSyntaxForms (tokLoop + 1)


//}}}
//{{{ Parser

typedef struct {
    untt tp : 6;
    Int startNodeInd;
    Int sentinelToken;
    void* scopeStackFrame; // only for tp = scope or expr
} ParseFrame;

#define nodId           7      // pl1 = index of entity, pl2 = index of name
#define nodCall         8      // pl1 = index of entity, pl2 = arity
#define nodBinding      9      // pl1 = index of entity
#define nodTypeId      10      // pl1 = index of entity
#define nodAnd         11
#define nodOr          12


// Punctuation (inner node)
#define nodScope       13       // (. This is resumable but trivially so, that's why it's not grouped with the others
#define nodExpr        14
#define nodAssignment  15
#define nodReassign    16       // :=
#define nodMutation    17       // pl1 = like in topOperator. This is the "+=", operatorful type of mutations


// Single-shot core syntax forms
#define nodAlias       18       
#define nodAssert      19       
#define nodAssertDbg   20       
#define nodAwait       21       
#define nodBreak       22       
#define nodCatch       23       // "(catch e. e,print)"
#define nodContinue    24       
#define nodDefer       25       
#define nodEach        26       
#define nodEmbed       27       // noParen. Embed a text file as a string literal, or a binary resource file
#define nodExport      28       
#define nodExposePriv  29       
#define nodFnDef       30       // pl1 = index of overload
#define nodIface       31
#define nodLambda      32
#define nodMeta        33       
#define nodPackage     34       // for single-file packages
#define nodReturn      35       
#define nodStruct      36       
#define nodTry         37       
#define nodYield       38       
#define nodIfClause    39       // pl1 = number of tokens in the left side of the clause
#define nodElse        40
#define nodLoop        41       //
#define nodLoopCond    42

// Resumable core forms
#define nodIf          43       // paren
#define nodIfPr        44       // like if, but every branch is a value compared using custom predicate
#define nodImpl        45       
#define nodMatch       46       // pattern matching on sum type tag

#define firstResumableForm nodIf
#define countResumableForms (nodMatch - nodIf + 1)
typedef struct Compiler Compiler;
typedef void (*ParserFunc)(Token, Arr(Token), Compiler*);
typedef void (*ResumeFunc)(Token*, Arr(Token), Compiler*);

struct LanguageDefinition {
    OpDef (*operators)[countOperators];
    LexerFunc (*dispatchTable)[256];
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    Int (*reservedParensOrNot)[countCoreForms];
    ParserFunc (*nonResumableTable)[countSyntaxForms];
    ResumeFunc (*resumableTable)[countResumableForms];
};

typedef struct {
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;   
} Node;


typedef struct {
    Int typeId;
    Int nameId;
    bool appendUnderscore;
    bool emitAsPrefix;
    bool emitAsOperator;
    bool emitAsMethod;
} Entity;


typedef struct {
    String* name;
    Entity entity;
} EntityImport;


typedef struct {
    String* name;
    Int count;
} OverloadImport;

//}}}


DEFINE_STACK_HEADER(ParseFrame)
DEFINE_STACK_HEADER(Entity)
#define pop(X) _Generic((X), \
    StackBtToken*: popBtToken, \
    StackParseFrame*: popParseFrame, \
    StackEntity*: popEntity, \
    Stackint32_t*: popint32_t \
    )(X)

#define peek(X) _Generic((X), \
    StackBtToken*: peekBtToken, \
    StackParseFrame*: peekParseFrame, \
    StackEntity*: peekEntity, \
    Stackint32_t*: peekint32_t \
    )(X)

#define push(A, X) _Generic((X), \
    StackBtToken*: pushBtToken, \
    StackParseFrame*: pushParseFrame, \
    StackEntity*: pushEntity, \
    Stackint32_t*: pushint32_t \
    )(A, X)
    
#define hasValues(X) _Generic((X), \
    StackBtToken*: hasValuesBtToken, \
    StackParseFrame*: hasValuesParseFrame, \
    StackEntity*: hasValuesEntity, \
    Stackint32_t*:  hasValuesint32_t \
    )(X)

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

/*
 * COMPILER DATA
 * 
 * 1) a = Arena for the results
 * AST (i.e. the resulting code)
 * Entitys
 * Types
 *
 * 2) aBt = Arena for the temporary stuff (backtrack). Freed after end of parsing
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
    String* text;
    Lexer* inp;
    Int inpLength;
    LanguageDefinition* langDef;
    StackParseFrame* backtrack;// [aTmp] 
    ScopeStack* scopeStack;
    Int i;                     // index of current token in the input

    Arr(Node) nodes; 
    Int capacity;               // current capacity of node storage
    Int nextInd;                // the index for the next token to be added    

    StackEntity entities;       // growing array of all entities (variables, function defs, constants etc) ever encountered
    Int entOverloadZero;       // the index of the first parsed (as opposed to being built-in or imported) overloaded binding
    Int entBindingZero;        // the index of the first parsed (as opposed to being built-in or imported) non-overloaded binding

    /**
     * [aTmp] growing array of counts of all fn name definitions encountered (for the typechecker to use)
     * Upper 16 bits contain concrete count, lower 16 bits total count
     */
    Arr(untt) overloadIds;
    Int overlCNext;
    Int overlCCap;

    StackInt overloads;

    StackInt types;

    Stackint32_t* expStack;    // [aTmp] temporary scratch space for type checking/resolving an expression

    // Current bindings and overloads in scope. -1 means "not active"
    // Var & type bindings are nameId (index into stringTable) -> bindingId
    // Function bindings are nameId -> (-overloadId - 2). So negative values less than -1 mean "function is active"
    Arr(int) activeBindings;

    Int countOperatorEntities;
    Int countNonparsedEntities;

    Stackint32_t* stringTable; // The table of unique strings from code. Contains only the startByte of each string.       
    StringStore* stringStore;  // A hash table for quickly deduplicating strings. Points into stringTable 
    Int strLength;             // length of stringTable

    bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aTmp;
};

//}}}    
