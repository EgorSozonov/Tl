#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/structures/stackHeader.h"
#include "../utils/structures/stringMap.h"

typedef struct {
    uintt tp : 6;
    intt startNodeInd;
    intt sentinelToken;
    intt coreType;
    intt clauseInd;   
} ParseFrame;


typedef struct {
    Arr(FunctionCall) operators;
    intt sentinelToken;
    bool isStillPrefix;
    bool isInHeadPosition;   
} Subexpr;

DEFINE_STACK_HEADER(ParseFrame)

typedef struct {
    untt tp : 6;
    untt lenBytes: 26;
    untt startByte;
    untt payload1;
    untt payload2;   
} Node;

typedef struct {
    untt flavor : 6;  // mutable, immutable, callable?
    untt typeId : 26;
    untt defStart;            // start token of the definition
    untt defSentinel;         // the first ind of token after definition
    untt extentSentinel;      // the first ind of token after the scope of this binding has ended
} Binding;


typedef void (*ParserFunc)(Lexer*, Arr(byte), Parser*);
typedef void (*ResumeFunc)(uint, Lexer*, Arr(byte), Parser*);


struct ParserDefinition {
    ParserFunc (*nonresumableTable)[countNonresumableForms];
    ResumeFunc (*resumableTable)[countResumableForms];
};

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
typedef struct {
    String* text;
    Lexer* inp;      
    ParserDefinition* parDef;
    ScopeStack* scopeStack;
    StackParseFrame* backtrack;
    intt i;
    
    Arr(String) strings;
    intt strNext;
    intt strCap;
    StringMap* stringMap;
    
    Arr(Binding) bindings; // current bindings in scope, array of nameId -> bindingId
    intt bindNext;
    intt bindCap;
    
    Arr(Node) nodes; 
    intt capacity; // current capacity of node storage
    intt nextInd; // the  index for the next token to be added    
    
    bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aBt;
} Parser;

Parser* parse(Lexer*, LanguageDefinition*, Arena*);


