#ifndef RUNTIME_H
#define RUNTIME_H
#include "../utils/String.h"
#include "../utils/StackHeader.h"
#include "../types/Instruction.h"
#include <stdint.h>

#define CALL_STACK_SIZE 256000    // 2MB of stack space per interpreter
#define CALLINFO_STACK_SIZE 1000  // Up to 1000 function calls in the stack


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
 * 6. End                                         <--- CallInfo.frameTop points here (to last occupied element)
 */
typedef struct {
    int32_t frameTop; // Index of the top element in the stack for this call frame
    int32_t frameBase; // Index of the bottom element in the stack for this call frame
    int32_t indFixedArg; // Index of the first fixed argument for this call frame
    int32_t returnAddress; // Address for the first return value, which is always on the parent frame
    OFunction* func;
    bool needReturnAddresses;  // If true, there are return addresses in the call frame
} CallInfo;

// Function 8 bit
// Length 16 bit
// return values 8 bit
// number of varargs 8 bit
// First return address (offset) 9 bit
// needReturnAddresses? 1 bit
//
// Prologue: 8 + 9 Length + needReturnAddresses, 9 Return values
// Call: 8 Function, 9 Varargs, 9 First return address

typedef struct {
    StackOPackage* packages;
    CallInfo* currCallInfo; // CallInfo
    CallInfo callInfoStack[CALLINFO_STACK_SIZE]; // Array CallInfo
    int64_t callStack[CALL_STACK_SIZE];
} OVM;



OPackage makeForTest(Arena* ar);

int runPackage(OPackage package, Arena* ar);


#endif
