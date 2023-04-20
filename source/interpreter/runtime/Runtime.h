#ifndef RUNTIME_H
#define RUNTIME_H
#include "../../utils/String.h"
#include "../../utils/StackHeader.h"
#include "../types/Instruction.h"
#include <stdint.h>

#define FRAME_SIZE 256                                      // 256 slots in a stack frame. This includes args, varargs, return addresses, and local variables
#define CALLINFO_STACK_SIZE 1000                            // Up to 1000 function calls in the stack
#define CALL_STACK_SIZE (FRAME_SIZE*CALLINFO_STACK_SIZE)    // 2MB of stack space per interpreter
#define ERR_MSG_SIZE 200



DEFINE_STACK_HEADER(int)

typedef struct {
    size_t length;
    uint64_t content[];
} OBytecode;

typedef struct {
    String* name;
    OBytecode* instructions;
    int countArgs;
    int countLocals;
    StackOConstant* constants;
} OFunction;
DEFINE_STACK_HEADER(OFunction)

typedef struct {
    StackOConstant* constants;
    StackOFunction* functions;
    OFunction* entryPoint;
} OPackage;


DEFINE_STACK_HEADER(OPackage)

/**
 * The layout of a call frame is:
 * 1. Number of extra return addresses            <--- Callinfo.frameBase points here
 * 2. Space for extra return addresses (optional)
 * 3. Space for varargs (optional)
 * 4. Space for normal args                       <--- CallInfo.indFixedArg points here
 * 5. Locals
 */
typedef struct {
    OFunction* func;
    int32_t indFixedArg; // Index of the first fixed argument for this call frame
    int32_t returnAddress; // Index for the first return value, which is always on the parent frame
    bool needReturnAddresses;  // If true, there are return addresses in the call frame. If false, returns are written continuously onto the parent frame
    int pc; // Program counter
} CallInfo;

typedef struct {
    StackOPackage* packages;
    CallInfo* currCallInfo; // CallInfo
    CallInfo callInfoStack[CALLINFO_STACK_SIZE]; // Array CallInfo
    int64_t callStack[CALL_STACK_SIZE];
    bool wasError;
    char errMsg[ERR_MSG_SIZE];
} OVM;


OPackage makeForTestCall(Arena* ar);
OPackage makeForTestBasic(Arena* ar);

void runPackage(OPackage package, OVM* vm, Arena* ar);


#endif
