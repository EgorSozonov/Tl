#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/structures/stackHeader.h"

typedef struct {
    uint tp : 6;
    int startNodeInd;
    int sentinelToken;
    int coreType;
    int clauseInd;   
} ParseFrame;


typedef struct {
    Arr(FunctionCall) operators;
    int sentinelToken;
    bool isStillPrefix;
    bool isInHeadPosition;   
} Subexpr;

DEFINE_STACK_HEADER(ParseFrame)

typedef struct {
    unsigned int tp : 6;
    unsigned int lenBytes: 26;
    unsigned int startByte;
    unsigned int payload1;
    unsigned int payload2;   
} Node;




typedef struct {
    Arr(ValueList*) dict;
    int dictSize;
    int length;
} BindingMap;


// PARSER DATA

// -- Arena for the results
// AST (i.e. the resulting code)
// Strings
// Bindings
// Types

// -- Arena for the temporary stuff (freed after end of parsing)
// Functions (stack of pieces of code currently being parsed)
// ParseFrames (stack of 

// -- ScopeStack (temporary, but knows how to free parts of itself, so in a separate arena)
typedef struct {
    int i;
    String* text;
    Lexer* inp;
    

    
    
    
    
    
    
    int totalTokens;
    
    LanguageDefinition* langDef;
    
    Arr(Node) nodes;
    int capacity; // current capacity of token storage
    int nextInd; // the  index for the next token to be added    

    StackRememberedToken* backtrack;
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    
    Arr(int) bindings; // current bindings in scope, array of nameId -> bindingId
    ScopeStack* scopeStack;
    
    bool wasError;
    String* errMsg;

    Arena* arena;    
} Parser;

Parser* parse(Lexer*, LanguageDefinition*, Arena*);


