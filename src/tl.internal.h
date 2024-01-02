/*{{{ Utils */

#define Int int32_t
#define StackInt Stackint32_t
#define InListUns InListuint32_t
#define private static
#define Byte unsigned char
#define Arr(T) T*
#define untt uint32_t
#define null nullptr
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
#define MAXTOKENLEN 67108864 /* 2^26 */
#define SIXTEENPLUSONE 65537 /* 2^16 + 1 */
#define LEXER_INIT_SIZE 2000
#define print(...) \
  printf(__VA_ARGS__);\
  printf("\n");

typedef struct ArenaChunk ArenaChunk;

struct ArenaChunk {
    size_t size;
    ArenaChunk* next;
    char memory[]; /* flexible array member */
};

typedef struct { /* :Arena */
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
    testable bool hasValues ## T (Stack ## T * st);\
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


/* A growable list of growable lists of pairs of non-negative Ints. Is smart enough to reuse
old allocations via an intrusive free list.
Internal lists have the structure [len cap ...data...] or [nextFree cap ...] for the free sectors
Units of measurement of len and cap are 1's. I.e. len can never be = 1, it starts with 2. */
typedef struct { /* :MultiAssocList */
    Int len;
    Int cap;
    Int freeList;
    Arr(Int) cont;
    Arena* a;
} MultiAssocList;


typedef struct { /* :String */
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
    Int len; /* length of standardText */
    Int firstParsed; /* the nameId for the first parsed word */
    Int firstBuiltin; /* the nameId for the first built-in word in standardStrings */
} StandardText;

/*}}}*/
/*{{{ Int Hashmap */

DEFINE_STACK_HEADER(int32_t)
typedef struct {
    Arr(int*) dict;
    int dictSize;
    int len;
    Arena* a;
} IntMap;

/*}}}*/
/*{{{ String Hashmap */

/* Reference to first occurrence of a string identifier within input text */
typedef struct {
    untt hash;
    Int indString;
} StringValue;


typedef struct {
    untt capAndLen;
    StringValue cont[];
} Bucket;

/* Hash map of all words/identifiers encountered in a source module */
typedef struct {
    Arr(Bucket*) dict;
    int dictSize;
    int len;
    Arena* a;
} StringDict;

/*}}}*/
/*{{{ Lexer */

/*{{{ Standard strings */

#define strAlias      0
#define strAssert     1
#define strBreak      2
#define strCatch      3
#define strContinue   4
#define strElseIf     5
#define strElse       6
#define strFalse      7
#define strFinally    8
#define strFor        9
#define strForeach   10
#define strIf        11
#define strIfPr      12
#define strImpl      13
#define strImport    14
#define strInterface 15
#define strMatch     16
#define strPub       17
#define strReturn    18
#define strTrue      19
#define strTry       20
#define strFirstNonReserved 21
#define strInt       strFirstNonReserved
#define strLong      22
#define strDouble    23
#define strBool      24
#define strString    25
#define strVoid      26
#define strF         27 /* F(unction type) */
#define strL         28 /* List */
#define strA         29 /* Array */
#define strD         30 /* Dictionary */
#define strTu        31 /* Tu(ple) */
#define strLen       32
#define strCap       33
#define strF1        34
#define strF2        35
#define strPrint     36
#define strPrintErr  37
#define strMathPi    38
#define strMathE     39
#define strSentinel  40

/*}}}*/

/* Backtrack token, used during lexing to keep track of all the nested stuff */
typedef struct { /* :BtToken */
    untt tp : 6;
    Int tokenInd;
    Int countClauses;
    untt spanLevel : 3;
} BtToken;

DEFINE_STACK_HEADER(BtToken)

typedef struct { /* :Token */
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;
} Token;

#define maxWordLength 255

/* Regular (leaf) Token types
 The following group of variants are transferred to the AST byte for byte, with no analysis
 Their values must exactly correspond with the initial group of variants in "RegularAST"
 The largest value must be stored in "topVerbatimTokenVariant" constant */
#define tokInt          0
#define tokLong         1
#define tokDouble       2
#define tokBool         3    /* pl2 = value (1 or 0) */
#define tokString       4
#define tokTilde        5    /* marks up to 2 anonymous params in a lambda. pl1 = tilde count */
#define tokUnderscore   6

#define tokWord         7    /* pl2 = index in the string table */
#define tokTypeName     8    /* pl1 = 1 iff it has arity (like "M/2"), pl2 = same as tokWord */
#define tokKwArg        9    /* pl2 = same as tokWord. The ":argName" */
#define tokDotWord     10    /* pl2 = same as tokWord. The ".structField" */
#define tokOperator    11    /* pl1 = OperatorToken, one of the "opT" constants below */
#define tokAccessor    12    /* pl1 = see "tkAcc" consts. Either an ".accessor" or a `_smth` */
#define tokArrow       13
#define tokPub         14

/* Single-statement token types */
#define tokStmt        15    /* firstSpanTokenType */
#define tokParens      16    /* subexpressions and struct/sum type instances */
#define tokPrefixCall  17    /* pl1 = nameId, index in the string table */
#define tokInfixCall   18    /* pl1 = nameId, index in the string table */
#define tokTypeCall    19    /* pl1 = nameId */
#define tokTypeCon     20    /* Built-in type initializers. pl1 = typeId */
#define tokParamList   21    /* Parameter lists, ended with `|` */
#define tokAssignment  22    /* `=` */
#define tokReassign    23    /* `<-`  */
#define tokMutation    24    /* `+=`. pl1 = (6 bits opType, 26 bits startBt of the operator symbol) */
#define tokAlias       25
#define tokAssert      26
#define tokBreakCont   27    /* pl1 = BIG iff it's a continue */
#define tokIface       28
#define tokImport      29    /* For test files and package decls */
#define tokReturn      30

/* Bracketed (multi-statement) token types. pl1 = spanLevel, see "sl" constants */
#define tokScope       31    /* denoted by {}. firstScopeTokenType */
#define tokFn          32    /* `{^ a Int => String;; body}` */
#define tokTry         33    /* early exit */
#define tokCatch       34    /* `catch MyExc e {` */
#define tokFinally     35    /* `finally { ` */
#define tokMeta        36    /* `[[` */

/* Resumable core forms */
#define tokIf          37    /* `if ... {` */
#define tokIfPr        38    /* like if, but every branch is a value compared using custom predicate */
#define tokMatch       39    /* "match ... {" pattern matching on sum type tag */
#define tokElseIf      40    /* `ei ... {` */
#define tokElse        41    /* `else {` */
#define tokImpl        42
#define tokFor         43
#define tokForeach     44

#define topVerbatimTokenVariant tokUnderscore
#define topVerbatimType    tokString
#define firstSpanTokenType tokStmt
#define firstScopeTokenType tokScope
#define firstResumableSpanTokenType tokIf

/* List of keywords that don't correspond directly to a token.
 All these numbers must be below firstKeywordToken to avoid any clashes */
#define keywTrue        1
#define keywFalse       2
#define keywBreak       3
#define keywContinue    4

/* AST nodes */
#define nodId           7    /* pl1 = index of entity, pl2 = index of name */
#define nodCall         8    /* pl1 = index of entity, pl2 = arity */
#define nodBinding      9    /* pl1 = index of entity, pl2 = 1 if it's a type binding */

/* Punctuation (inner node) */
#define nodScope       10     /* This syntax form is resumable but trivially so,
                               that's why it's not grouped with the others */
#define nodExpr        11
#define nodAssignment  12
#define nodReassign    13     /* := */
#define nodAccessor    14     /* pl1 = "acc" constants */

/* Single-shot core syntax forms */
#define nodAlias       15
#define nodAssert      16     /* pl1 = 1 iff it's a debug assert */
#define nodBreakCont   17     /* pl1 = number of label to break or continue to, -1 if none needed
                               It's a continue iff it's >= BIG. */
#define nodCatch       18     /* "catch e {` */
#define nodDefer       19
#define nodImport      21     /* This is for test files only, no need to import anything in main */
#define nodFnDef       22     /* pl1 = entityId */
#define nodIface       23
#define nodMeta        24
#define nodReturn      25
#define nodTry         26
#define nodIfClause    27
#define nodWhile       28     /* pl1 = id of loop (unique within a function) if it needs to
                               have a label in codegen */
#define nodWhileCond   29

/* Resumable core forms */
#define nodIf          30
#define nodImpl        31
#define nodMatch       32     /* pattern matching on sum type tag */

#define firstResumableForm nodIf
#define countResumableForms (nodMatch - nodIf + 1)
#define countSpanForms (nodMatch - nodScope + 1)

#define metaDoc         1     /* Doc comments */
#define metaDefault     2     /* Default values for type arguments */


/**
 There is a closed set of operators in the language.
 
 For added flexibility, some operators may be extended into one more planes,
 for example '+' may be extended into '+.', while '/' may be extended into '/.'.
 These extended operators are declared by the language, and may be defined
 for any type by the user, with the return type being arbitrary.
 For example, the type of 3D vectors may have two different multiplication
 operators: *. for vector product and * for scalar product.
 
 Plus, many have automatic assignment counterparts.
 For example, "a &&.= b" means "a = a &&. b" for whatever "&&." means.
*/
typedef struct { /* :OpDef */
    Byte bytes[4];
    Int arity;
    /* Whether this operator permits defining overloads as well as extended operators (e.g. +.= ) */
    bool overloadable;
    bool assignable;
    bool isTypelevel;
    Byte prece;
    int8_t lenBts;
} OpDef;

#define preceEquality 1
#define preceAdd 2
#define preceMultiply 3
#define preceExponent 4
#define preceFn 5
#define precePrefix 6

/* :OperatorType
 Values must exactly agree in order with the operatorSymbols array in the tl.c file.
 The order is defined by ASCII. Operator is bitwise <=> it ends wih dot */
#define opBitwiseNegation   0 /* !. bitwise negation */
#define opNotEqual          1 /* != */
#define opBoolNegation      2 /* ! */
#define opSize              3 /* ## */
#define opToString          4 /* $ */
#define opRemainder         5 /* % */
#define opBitwiseAnd        6 /* &&. bitwise and */
#define opBoolAnd           7 /* && logical and */
#define opPtr               8 /* & pointers/values at type level */
#define opIsNull            9 /* ' */
#define opTimesExt         10 /* *: */
#define opTimes            11 /* * */
#define opIncrement        12 /* ++ */
#define opPlusExt          13 /* +: */
#define opPlus             14 /* + */
#define opDecrement        15 /* -- */
#define opMinusExt         16 /* -: */
#define opMinus            17 /* - */
#define opDivByExt         18 /* /: */
#define opIntersection     19 /* /| */
#define opDivBy            20 /* / */
#define opBitShiftLeft     21 /* <<. */
#define opLTEQ             22 /* <= */
#define opComparator       23 /* <> */
#define opLessThan         24 /* < */
#define opRefEquality      25 /* === */
#define opEquality         26 /* == */
#define opIntervalBoth     27 /* >=<= inclusive interval check */
#define opIntervalRight    28 /* ><=  right-inclusive interval check */
#define opIntervalLeft     29 /* >=<  left-inclusive interval check */
#define opBitShiftRight    30 /* >>.  unsigned right bit shift */
#define opIntervalExcl     31 /* ><   exclusive interval check */
#define opGTEQ             32 /* >= */
#define opGreaterThan      33 /* > */
#define opNullCoalesce     34 /* ?:   null coalescing operator */
#define opQuestionMark     35 /* ?    nullable type operator */
#define opBitwiseXor       36 /* ^.   bitwise XOR */
#define opExponent         37 /* ^    exponentiation */
#define opBitwiseOr        38 /* ||.  bitwise or */
#define opBoolOr           39 /* ||   logical or */

#define countOperators     40

#define countSyntaxForms (tokForeach + 1)

typedef struct Compiler Compiler;


/* Subclasses of the data accessor tokens/nodes */
#define tkAccDot     1
#define tkAccArray   2
#define accField     1 /* field accessor in a struct, like "foo.field". pl2 = nameId of the string */
#define accArrayInd  2 /* single-integer array access, like "arr_5". pl2 = int value of the ind */
#define accArrayWord 3 /* single-variable array access, like "arr_i". pl2 = nameId of the string */
#define accString    4 /* string-based access inside hashmap, like "map_`foo`". pl2 = 0 */
#define accExpr      5 /* expr array access, like "arr_(i + 1)". pl2 = number of tokens/nodes */
#define accUndef     6 /* undefined after lexing (to be determined by the parser) */


typedef void (*LexerFunc)(Arr(Byte), Compiler*); /* LexerFunc = &(Lexer* => void) */
typedef void (*ParserFunc)(Token, Arr(Token), Compiler*);
typedef void (*ResumeFunc)(Token*, Arr(Token), Compiler*);

typedef struct { /* :LanguageDefinition */
    OpDef (*operators)[countOperators];
    LexerFunc (*dispatchTable)[256];
    ParserFunc (*nonResumableTable)[countSyntaxForms];
    ResumeFunc (*resumableTable)[countResumableForms];
} LanguageDefinition;


typedef struct { /* :Node */
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    Int pl1;
    Int pl2;
} Node;

typedef struct { /* :ParseFrame */
    untt tp : 6;
    Int startNodeInd;
    Int sentinelToken;
    Int typeId;            /* valid only for fnDef, if, loopCond and the like */
    void* scopeStackFrame; /* only for tp = scope or expr */
} ParseFrame;


#define classMutableGuaranteed 1
#define classMutatedGuaranteed 2
#define classMutableNullable   3
#define classMutatedNullable   4
#define classImmutable         5
#define emitPrefix         1  /* normal singular native names */
#define emitOverloaded     2  /* normal singular native names */
#define emitPrefixShielded 3  /* a native name in need of shielding from target reserved word 
                                 (by appending a "_") */
#define emitPrefixExternal 4  /* prefix names that are emitted differently than in source code */
#define emitInfix          5  /* infix operators that match between source code and target (e.g. +) */
#define emitInfixExternal  6  /* infix operators that have a separate external name */
#define emitField          7  /* emitted as field accesses, like ".length" */
#define emitInfixDot       8  /* emitted as a "dot-call", like ".toString()" */
#define emitNop            9  /* for unary operators that don't need to be emitted, like "," */

typedef struct { /* :Entity */
    Int typeId;
    Int name;
    uint16_t externalNameId;
    uint8_t class;
    uint8_t emit;
    bool isPublic;
    bool hasExceptionHandler;
} Entity;

typedef struct {
    untt name;  /* 8 bits of length, 24 bits or nameId */
    Int externalNameId; /* index in the "codegenText" */
    Int typeInd; /* index in the intermediary array of types that is imported alongside */
} EntityImport;

typedef struct { /* :Toplevel */
    /* Toplevel definitions (functions, variables, types) for parsing order and name searchability */
    Int indToken;
    Int sentinelToken;
    untt name;
    Int entityId; /* if n < 0 => -n - 1 is an index into [functions], otherwise n => [entities] */
    bool isFunction;
} Toplevel;

typedef struct ScopeStackFrame ScopeStackFrame;
typedef struct ScopeChunk ScopeChunk;
struct ScopeChunk {
    ScopeChunk *next;
    int len; /* length is divisible by 4 */
    Int cont[];
};

typedef struct { /* :ScopeStack */
    /* Either currChunk->next == NULL or currChunk->next->next == NULL */
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    ScopeStackFrame* topScope;
    Int len;
    int nextInd; /* next ind inside currChunk, unit of measurement is 4 Bytes */
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

struct Compiler { /* :Compiler */
    /* See docs/compiler.txt, docs/architecture.svgz */
    LanguageDefinition* langDef;

    /* LEXING */
    String* sourceCode;
    Int inpLength;
    InListToken tokens;
    InListInt newlines;
    Int indentation;
    InListInt numeric;          /* [aTmp] */
    StackBtToken* lexBtrack;    /* [aTmp] */
    Stackint32_t* stringTable;
    StringDict* stringDict;
    Int lastClosingPunctInd;

    /* PARSING */
    InListToplevel toplevels;
    InListInt imports;
    StackParseFrame* backtrack; /* [aTmp] */
    ScopeStack* scopeStack;
    Arr(Int) activeBindings;    /* [aTmp] */
    Int loopCounter;
    InListNode nodes;
    InListNode monoCode;
    MultiAssocList* monoIds;
    InListEntity entities;
    InListInt overloads;
    InListInt types;
    StringDict* typesDict;
    Int countNonparsedEntities;
    StackInt* expStack;   /* [aTmp] */
    StackInt* typeStack;  /* [aTmp] */
    StackInt* tempStack;  /* [aTmp] */
    Int countOverloads;
    Int countOverloadedNames;
    MultiAssocList* rawOverloads; /* [aTmp] */

    /* GENERAL STATE */
    Int i;
    bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aTmp;
};


/** Span levels */
#define slScope       1 /* scopes (denoted by brackets): newlines and commas have no effect there */
#define slDoubleScope 2 /* double scopes like `if`, `for` etc. They last until the first 
                           {} span gets closed */
#define slStmt        3 /* single-line statements: newlines and semicolons break 'em */
#define slSubexpr     4 /* parenthesized forms: newlines have no effect, semi-colons error out */


/*}}}*/
/*{{{ Types */

/* see the Type layout chapter in the docs */
#define sorStruct       1
#define sorSum          2
#define sorFunction     3
#define sorPartial      4 /* Partially applied type */
#define sorConcrete     5 /* Fully applied type (no type params) */

typedef struct { /* :TypeHeader */
    Byte sort;
    Byte arity;
    Byte depth;
    untt nameAndLen;
} TypeHeader;

/*}}}*/
/*{{{ Generics */

#define pop(X) _Generic((X),\
    StackBtToken*: popBtToken,\
    StackParseFrame*: popParseFrame,\
    Stackint32_t*: popint32_t,\
    StackNode*: popNode\
    )(X)

#define peek(X) _Generic((X),\
    StackBtToken*: peekBtToken,\
    StackParseFrame*: peekParseFrame,\
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
    Stackint32_t*: pushint32_t,\
    StackNode*: pushNode\
    )(A, X)

#define hasValues(X) _Generic((X),\
    StackBtToken*: hasValuesBtToken,\
    StackParseFrame*: hasValuesParseFrame,\
    Stackint32_t*:  hasValuesint32_t,\
    StackNode*: hasValuesNode\
    )(X)

/*}}}*/

