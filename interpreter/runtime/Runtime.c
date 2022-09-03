#include "Runtime.h"
#include "../types/Instruction.h"
#include "../utils/Arena.h"
#include "../utils/String.h"
#include "../utils/Stack.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>


DEFINE_STACK(OFunction);
DEFINE_STACK(OConstant);
DEFINE_STACK(OInstr);
DEFINE_STACK(int);

OPackage makeForTest(Arena* ar) {
    //fn f x y = { z = x + 2*y; return z*x }
    //fn main = { print "HW"; x y = 10 100; newValue = f x y; print "Result is:"; print newValue }

    // f:
    // OP_MUL 3 2 K[0]
    // OP_MUL 4 3 1    
    // OP_WRITERET 4

    // main:
    // OP_PRINT 0 0 0
    // OP_LOADI 0 10 0
    // OP_LOADI 1 100 0    

    // OP_CALLPROLOGUE 1 (3)
    // OP_COPY 4 0
    // OP_COPY 5 1
    // OP_CALL 0 -1 0b0..0 # writing return value to slot 3
    // OP_PRINT 0 2 0      # the constant
    // OP_PRINT 0 -1 2     # the local

    OConstant stringConst = {.tag = strr, .value.str = allocateString(ar, 11, "Hello world")};
    OConstant stringConst2 = {.tag = strr, .value.str = allocateString(ar, 11, "The result is:")};
    OConstant intConst = {.tag = intt, .value.num = 77};

    StackOConstant* constants = mkStackOConstant(ar, 3);
    pushOConstant(constants, stringConst);
    pushOConstant(constants, intConst);
    pushOConstant(constants, stringConst2);

    OFunction* entryPoint = arenaAllocate(ar, sizeof(OFunction));
    entryPoint->name = allocateString(ar, 3, "main");
    entryPoint->constants = constants;
    entryPoint->countLocals = 3;
    const int numInstructions = 6;
    OBytecode* instructionsMain = arenaAllocate(ar, sizeof(OBytecode) + 8*numInstructions);
    instructionsMain->length = numInstructions;

    instructionsMain->content[0] = mkInstructionABC(OP_LOADI, 2, 29, 0);
    instructionsMain->content[1] = mkInstructionABC(OP_LOADI, 1, 14, 0);
    instructionsMain->content[2] = mkInstructionABC(OP_ADD, 0, 1, 2);
    instructionsMain->content[3] = mkInstructionABC(OP_PRINT, 0, 0, 0);
    instructionsMain->content[4] = mkInstructionABC(OP_PRINT, 0, 1, 0);
    instructionsMain->content[5] = mkInstructionABC(OP_PRINT, 0, 257, 0);

    entryPoint->instructions = instructionsMain;

    OPackage package = {.constants = constants, .functions = NULL, .entryPoint = entryPoint};
    return package;
}


int runPackage(OPackage package, Arena* ar) {
    if (package.entryPoint == NULL) return -1;    
    OVM* ovm = arenaAllocate(ar, sizeof(OVM));
    ovm->currCallInfo = ovm->callInfoStack;
    CallInfo mainCall = { .frameBase = 0, .frameTop=package.entryPoint->countLocals, .indFixedArg = 1,
                          .returnAddress = 0, .func = package.entryPoint, .needReturnAddresses = false };
    *(ovm->currCallInfo) = mainCall;

    printf("Executing entrypoint with %zu instructions...\n", package.entryPoint->instructions->length);

    OpCode opCode;
    uint64_t r1;
    bool isConst1 = false;
    uint64_t r2;
    bool isConst2 = false;
    uint64_t r3;
    bool isConst3 = false;
    int i = 0;
    int64_t *currFrame = &(ovm->callStack[0]) + ovm->currCallInfo->frameBase;
    OFunction* currFunc = ovm->currCallInfo->func;
    StackOConstant* currConst = currFunc->constants;
    
    while (i < package.entryPoint->instructions->length) {
        uint64_t instr = ((package.entryPoint->instructions)->content)[i];

        opCode = decodeOpcode(instr);
        decodeSlotArgA(instr, currFrame, currConst->content, &r1, &isConst1);
        decodeSlotArgB(instr, currFrame, currConst->content, &r2, &isConst2);
        decodeSlotArgC(instr, currFrame, currConst->content, &r3, &isConst3);

        switch (opCode) {
        case OP_ADD: {
            int32_t a = (*(currFrame + r2));
            int b = (*(currFrame + r3));
            printf("a = %d, b = %d\n", a, b);
            *(currFrame + r1) = a + b;
            break;
        }
        case OP_PRINT:
            if (r2 > -1) { // constant
                OConstant constant = (*currFunc->constants->content)[r2];
                if (constant.tag == intt) {
                    printf("%d\n", constant.value.num);
                } else {
                    String* constString = constant.value.str;
                    printf("%s\n", constString->content);
                }
            } else if (r3 > -1) { // local
                printf("%d\n", *(currFrame + r3));
            } else {
                printf("%d\n", r1);
            }
            break;
        case OP_LOADI: // load integer into local variable
            *(currFrame + r1) = r2;
            break;
        default:
            printf("Something else\n");
        }
        i++;
    }

    return 0;
}




