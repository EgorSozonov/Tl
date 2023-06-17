#include <setjmp.h>
#include <stdio.h>
#include "../utils/aliases.h"
#include "lexerConstants.h"
#include "lexer.h"
#include "parserConstants.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/structures/stack.h"
#include "../utils/structures/stackHeader.h"
#include "../utils/structures/stringMap.h"

extern jmp_buf excBuf;
#define VALIDATE(cond, errMsg) if (!(cond)) { throwExc0(errMsg, pr); }

typedef struct {
    untt tp : 6;
    Int startNodeInd;
    Int sentinelToken;
    void* scopeStackFrame; // only for tp = scope or expr
} ParseFrame;

DEFINE_STACK_HEADER(ParseFrame)
//~ typedef struct {                                            
    //~ Int capacity;                                           
    //~ Int length;                                             
    //~ Arena* arena;                                           
    //~ ParseFrame (* content)[];                                        
//~ } StackParseFrame;

typedef struct Lexer Lexer;
typedef struct Parser Parser;
typedef struct ScopeStack ScopeStack;
typedef struct {
    untt tp : 6;
    untt lenBts: 26;
    untt startBt;
    untt pl1;
    untt pl2;   
} Node;


typedef struct {
    untt flavor : 6;        // mutable, immutable, callable?
    untt typeId : 26;
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


typedef void (*ParserFunc)(Token, Arr(Token), Parser*);
typedef void (*ResumeFunc)(Token*, Arr(Token), Parser*);

#define countSyntaxForms (tokLoop + 1)
#define firstResumableForm nodIf
#define countResumableForms (nodMatch - nodIf + 1)

typedef struct {
    ParserFunc (*nonResumableTable)[countSyntaxForms];
    ResumeFunc (*resumableTable)[countResumableForms];
    bool (*allowedSpanContexts)[countResumableForms + countSyntaxForms][countResumableForms];
    OpDef (*operators)[countOperators];
} ParserDefinition;

/*
 * PARSER DATA
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
struct Parser {
    String* text;
    Lexer* inp;
    Int inpLength;
    ParserDefinition* parDef;
    StackParseFrame* backtrack;    
    ScopeStack* scopeStack;
    Int i;                     // index of current token in the input

    Stackint32_t* stringTable; // The table of unique strings from code. Contains only the startBt of each string.       
    StringStore* stringStore;  // A hash table for quickly deduplicating strings. Points into stringTable 
    Int strLength;             // length of stringTable
        
    Arr(Entity) entities;      // growing array of all bindings ever encountered
    Int entNext;
    Int entCap;
    Int entOverloadZero;       // the index of the first parsed (as opposed to being built-in or imported) overloaded binding
    Int entBindingZero;        // the index of the first parsed (as opposed to being built-in or imported) non-overloaded binding

    Arr(Int) overloads;        // growing array of counts of all fn name definitions encountered (for the typechecker to use)
    Int overlNext;
    Int overlCap;    

    // Current bindings and overloads in scope. -1 means "not active"
    // Var & type bindings are nameId (index into stringTable) -> bindingId
    // Function bindings are nameId -> (-overloadId - 2). So negative values less than -1 mean "function is active"
    Arr(int) activeBindings;
    
    Arr(Node) nodes; 
    Int capacity;               // current capacity of node storage
    Int nextInd;                // the index for the next token to be added    
    
    bool wasError;
    String* errMsg;
    Arena* a;
    Arena* aBt;
};

ParserDefinition* buildParserDefinitions(LanguageDefinition*, Arena*);
Parser* createParser(Lexer*, Arena*);
Int createBinding(Token, Entity, Parser*);
void importEntities(Arr(EntityImport) bindings, Int countBindings, Parser* pr);
Parser* parse(Lexer*, Arena*);
Parser* parseWithParser(Lexer*, Parser*, Arena*);
void addNode(Node t, Parser* lexer);

