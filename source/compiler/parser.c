#include "parser.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stringMap.h"
#include <stdbool.h>



typedef struct ScopeStack ScopeStack;

void addBinding(String*, int, ScopeStack*);
void newScope(ScopeStack*);
void closeScope(ScopeStack*);
int searchForBinding(String*, bool*, ScopeStack*);




Parser* parse(Lexer* lr, LanguageDefinition* lang, Arena* a) {
}



struct ScopeChunk {
    size_t size;
    size_t fillSize;
    ScopeChunk* next;
    char memory[]; // flexible array member
};


struct ScopeStack {
    ScopeChunk* firstChunk;
    ScopeChunk* currChunk;
    ScopeChunk* lastChunk;
    int currInd;
};


void addBinding(String* name, int bindingId, ScopeStack* scSt);
void newScope(ScopeStack* scSt);
void closeScope(ScopeStack* scSt);

/** Returns the non-negative binding id if this name is in scope, -1 otherwise.
 * If the name is found in the current function, the boolean param is set to false, otherwise to true.
 */
int searchForBinding(String* name, bool* foundInOuterFunction, ScopeStack* scSt) {
    
}
