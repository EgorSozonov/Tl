#include "../utils/aliases.h"
#include "lexerConstants.h"
#include "lexer.h"
#include "parserConstants.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/structures/stackHeader.h"
#include "../utils/structures/stringMap.h"


typedef struct {
    untt tp : 6;
    Int startNodeInd;
    Int sentinelToken;
    Int coreType;
    Int clauseInd;   
} ParseFrame;

DEFINE_STACK_HEADER(ParseFrame)

typedef struct Lexer Lexer;
typedef struct Parser Parser;
typedef struct ScopeStack ScopeStack;
typedef struct {
    untt tp : 6;
    untt lenBytes: 26;
    untt startByte;
    untt payload1;
    untt payload2;   
} Node;


typedef struct {
    untt flavor : 6;        // mutable, immutable, callable?
    untt typeId : 26;
    untt defStart;          // start token of the definition
    untt defSentinel;       // the first ind of token after definition
    untt extentSentinel;    // the first ind of token after the scope of this binding has ended
} Binding;


typedef void (*ParserFunc)(Lexer*, Arr(byte), Parser*);
typedef void (*ResumeFunc)(untt, Lexer*, Arr(byte), Parser*);

#define countNonresumableForms (tokYield + 1)
#define firstResumableForm tokIf
#define countResumableForms (tokMut - tokIf + 1)

typedef struct {
    ParserFunc (*nonresumableTable)[countNonresumableForms];
    ResumeFunc (*resumableTable)[countResumableForms];
} ParserDefinition;

// PARSER DATA
// 
// 1) a = Arena for the results
// AST (i.e. the resulting code)
// Strings
// Bindings
// Types
//
// 2) aBt = Arena for the temporary stuff (backtrack). Freed after end of parsing
//
// 3) ScopeStack (temporary, but knows how to free parts of itself, so in a separate arena)
//
// WORKFLOW
// The "stringTable" is frozen, it was filled by the lexer. The "bindings" table is growing
// with every new assignment form encountered. "Nodes" is obviously growing with the new AST nodes
// emitted.
// Any new span (scope, expression, assignment etc) means 
// - pushing a ParseFrame onto the "backtrack" stack
// - if the new frame is a lexical scope, also pushing a scope onto the "scopeStack"
// - else if the new frame is an expression, pushing a subexpr onto the "scopeStack"
// The scopeStack order is always: some scopes, then maybe some subexpressions.
// The end of a span means popping from "backtrack" and also, if needed, popping from "scopeStack".
//
struct Parser {
    String* text;
    Lexer* inp;      
    ParserDefinition* parDef;
    ScopeStack* scopeStack;
    StackParseFrame* backtrack;
    Int i;                      // index of current token in the input
    
    Arr(Int) stringTable;
    Int strLength;
    
    Arr(Binding) bindings;      // current bindings in scope, array of nameId -> bindingId
    Int bindNext;
    Int bindCap;
    
    Arr(Node) nodes; 
    Int capacity;               // current capacity of node storage
    Int nextInd;                // the  index for the next token to be added    
    
    bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aBt;
};

Parser* createParser(Lexer*, Arena*);
Parser* parse(Lexer*, LanguageDefinition*, Arena*);
void addNode(Node t, Parser* lexer);

