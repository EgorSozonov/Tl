

#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/structures/stackHeader.h"

typedef struct {
    uint tp : 6
   ;int startNodeInd
   ;int sentinelToken
   ;int coreType
   ;int clauseInd
   ;
} ParseFrame;


typedef struct {
    Arr(FunctionCall) operators
   ;int sentinelToken
   ;bool isStillPrefix
   ;bool isInHeadPosition
   ;
} Subexpr;

DEFINE_STACK_HEADER(ParseFrame)

typedef struct {
    unsigned int tp : 6
   ;unsigned int lenBytes: 26
   ;unsigned int startByte
   ;unsigned int payload1
   ;unsigned int payload2
   ;
} Node;


// ParseFrame = spanType, startNode, sentinelToken, currClause
// LexicalScope = funDefs, bindings, functions, types, typeFuncs
// Stack Stack Subexpr | Subexpr = Stack FunctionCall, sentinelToken, isInHeadPosition
// FunctionCall = nameId, prec, arity, maxArity, startByte


                   
//~ data class ParseFrame(val spanType: Int, val indStartNode: Int, val sentinelToken: Int, val coreType: Int = 0) {
    //~ var clauseInd: Int = 0
//~ }

//~ data class Subexpr(var operators: ArrayList<FunctionCall>,
                   //~ val sentinelToken: Int,
                   //~ var isStillPrefix: Boolean,
                   //~ val isInHeadPosition: Boolean)
                   
                                
//~ class LexicalScope() {
    //~ /** Array funcId - these are the function definitions in this scope (see 'detectNestedFunctions'). */
    //~ val funDefs: ArrayList<Int> = ArrayList(4)
    //~ /** Map [name -> identifierId] **/
    //~ val bindings: HashMap<String, Int> = HashMap(12)
    //~ /** Map [name -> List (functionId arity)] */
    //~ val functions: HashMap<String, ArrayList<IntPair>> = HashMap(12)

    //~ /** Map [name -> identifierId] **/
    //~ val types: HashMap<String, Int> = HashMap(12)
    //~ /** Map [name -> List (functionId arity)] */
    //~ val typeFuncs: HashMap<String, ArrayList<IntPair>> = HashMap(12)
//~ }




typedef struct {
    Arr(ValueList*) dict
   ;int dictSize
   ;int length
   ;
} BindingMap;

typedef struct {
    int i
    ;String* code
    ;Lexer* inp
    
    ;int inpLength
    ;int totalTokens
    
    ;LanguageDefinition* langDef
    
    ;Arr(Node) nodes
    ;int capacity // current capacity of token storage
    ;int nextInd // the  index for the next token to be added    

    ;StackRememberedToken* backtrack
    ;ReservedProbe (*possiblyReservedDispatch)[countReservedLetters]
    
    ;bool wasError
    ;String* errMsg

    ;Arena* arena
    ;
} Parser;

Parser* parse(Lexer*, LanguageDefinition*, Arena*)
;


