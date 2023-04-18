#include "parser.h"
#include "../utils/aliases.h"
#include "../utils/arena.h"
#include "../utils/goodString.h"
#include "../utils/stringMap.h"
#include "../utils/structures/scopeStack.h"
#include <stdbool.h>


jmp_buf excBuf;

typedef struct ScopeStack ScopeStack;


Parser* parse(Lexer* lr, LanguageDefinition* lang, Arena* a) {
    if (setjmp(excBuf) == 0) {
        while (result->i < inpLength) {
            ((*dispatch)[inp[result->i]])(result, inp);
        }
        finalize(result);
    }
    result->totalTokens = result->nextInd;
    return result;
}




/** Returns the non-negative binding id if this name is in scope, -1 otherwise.
 * If the name is found in the current function, the boolean param is set to false, otherwise to true.
 */
int searchForBinding(String* name, bool* foundInOuterFunction, ScopeStack* scSt) {
    
}
