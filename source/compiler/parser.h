

#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stackHeader.h"

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
    
} AST;

typedef struct {
} FunctionDef;

typedef struct {
    int i;
    String* code;
    Lexer* inp;
    
    
    
    
    int inpLength;
    int totalTokens;    
    
    LanguageDefinition* langDef;
    
    Arr(Node) nodes;
    int capacity; // current capacity of token storage
    int nextInd; // the  index for the next token to be added    

    StackRememberedToken* backtrack;
    ReservedProbe (*possiblyReservedDispatch)[countReservedLetters];
    
    bool wasError;
    String* errMsg;

    Arena* arena;
} Parser;


