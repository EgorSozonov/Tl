#include "Runtime.h"
#include "../types/Instruction.h"
#include "../../utils/Arena.h"
#include "../../utils/String.h"
#include "../../utils/Stack.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


DEFINE_STACK(OFunction);
DEFINE_STACK(OConstant);
DEFINE_STACK(OInstr);
DEFINE_STACK(int);

//fn f x y = { z = x - 2*y; return z*x }
//fn main = { print "HW"; x y = 10 100; newValue = f x y; print "Result is:"; print newValue }
// Expected output: "HW" "Result is:" -1900;
OPackage makeForTestCall(Arena* ar) {    
    // f
    StackOConstant* constantsF = mkStackOConstant(ar, 2);
    pushOConstant(constantsF, (OConstant){.tag = strr, .value.str = allocateLiteral(ar, 13, "Hello from F!")});
    pushOConstant(constantsF, (OConstant){.tag = intt, .value.num = 2});

    OFunction* f = arenaAllocate(ar, sizeof(OFunction));
    f->name = allocateLiteral(ar, 1, "f");
    f->constants = constantsF;
    f->countLocals = 3;
    f->countArgs = 2;
    const int numInstructionsF = 5;
    OBytecode* instructionsF = arenaAllocate(ar, sizeof(OBytecode) + 8*numInstructionsF);
    instructionsF->length = numInstructionsF;
    instructionsF->content[0] = mkInstructionABC(OP_PRINT, 0, 256, 0);
    instructionsF->content[1] = mkInstructionABC(OP_MUL, 3, 2, 257);
    instructionsF->content[2] = mkInstructionABC(OP_SUB, 4, 1, 3);
    instructionsF->content[3] = mkInstructionABC(OP_MUL, 259, 4, 1);
    instructionsF->content[4] = mkInstructionABC(OP_RETURN, 0, 0, 0);
    f->instructions = instructionsF;

    // main
    OConstant stringConst = {.tag = strr, .value.str = allocateLiteral(ar, 20, "Function calling test")};
    OConstant stringConst2 = {.tag = strr, .value.str = allocateLiteral(ar, 10, "Result is:")};
    OConstant funcConst = {.tag = funcc, .value.func = f};
    StackOConstant* constantsMain = mkStackOConstant(ar, 3);
    pushOConstant(constantsMain, stringConst);
    pushOConstant(constantsMain, stringConst2);
    pushOConstant(constantsMain, funcConst);

    OFunction* entryPoint = arenaAllocate(ar, sizeof(OFunction));
    entryPoint->name = allocateLiteral(ar, 4, "main");
    entryPoint->constants = constantsMain;
    entryPoint->countLocals = 3;
    const int numInstructionsMain = 8;
    OBytecode* instructionsMain = arenaAllocate(ar, sizeof(OBytecode) + 8*numInstructionsMain);
    instructionsMain->length = numInstructionsMain;
    instructionsMain->content[0] = mkInstructionABC(OP_PRINT, 0, 256, 0);
    instructionsMain->content[1] = mkInstructionABC(OP_LOADI, 1, 10, 0);
    instructionsMain->content[2] = mkInstructionABC(OP_LOADI, 2, 100, 0);
    instructionsMain->content[3] = mkInstructionABC(OP_COPY, 257, 1, 0);
    instructionsMain->content[4] = mkInstructionABC(OP_COPY, 258, 2, 0);
    instructionsMain->content[5] = mkInstructionABCD(OP_CALL, 258, 1, 3, 0);
    instructionsMain->content[6] = mkInstructionABC(OP_PRINT, 0, 257, 0);
    instructionsMain->content[7] = mkInstructionABC(OP_PRINT, 0, 3, 0);
    entryPoint->instructions = instructionsMain;

    StackOFunction* functions = mkStackOFunction(ar, 1);
    pushOFunction(functions, *f);

    OPackage package = {.constants = constantsMain, .functions = functions, .entryPoint = entryPoint};
    return package;
}

/**
 * @brief Should display: HW, 29, 14, The sum is:, 43
 * @param ar
 * @return
 */
OPackage makeForTestBasic(Arena* ar) {
    StackOConstant* constants = mkStackOConstant(ar, 3);
    pushOConstant(constants, (OConstant){.tag = strr, .value.str = allocateLiteral(ar, 11, "Hello world")});
    pushOConstant(constants, (OConstant){.tag = intt, .value.num = 77});
    pushOConstant(constants, (OConstant){.tag = strr, .value.str = allocateLiteral(ar, 12, "The sum is:")});

    OFunction* entryPoint = arenaAllocate(ar, sizeof(OFunction));
    entryPoint->name = allocateLiteral(ar, 3, "main");
    entryPoint->constants = constants;
    entryPoint->countLocals = 3;
    const int numInstructions = 8;
    OBytecode* instructionsMain = arenaAllocate(ar, sizeof(OBytecode) + 8*numInstructions);
    instructionsMain->length = numInstructions;

    instructionsMain->content[0] = mkInstructionABC(OP_PRINT, 0, 256, 0);
    instructionsMain->content[1] = mkInstructionABC(OP_LOADI, 1, 14, 0);
    instructionsMain->content[2] = mkInstructionABC(OP_LOADI, 2, 29, 0);
    instructionsMain->content[3] = mkInstructionABC(OP_ADD, 3, 1, 2);
    instructionsMain->content[4] = mkInstructionABC(OP_PRINT, 0, 1, 0);
    instructionsMain->content[5] = mkInstructionABC(OP_PRINT, 0, 2, 0);
    instructionsMain->content[6] = mkInstructionABC(OP_PRINT, 0, 258, 0);
    instructionsMain->content[7] = mkInstructionABC(OP_PRINT, 0, 3, 0);

    entryPoint->instructions = instructionsMain;

    OPackage package = {.constants = constants, .functions = NULL, .entryPoint = entryPoint};
    return package;
}


void runPackage(OPackage package, OVM* vm, Arena* ar) {
    if (package.entryPoint == NULL) {
        vm->wasError = true;
        return;
    }

    CallInfo mainCall = { .func = package.entryPoint, .indFixedArg = 1,
                          .returnAddress = 0, .needReturnAddresses = false, .pc = 0, };
    *(vm->currCallInfo) = mainCall;



    // ---- Current instruction stuff
    OpCode opCode;
    int64_t r1;
    bool isConst1 = false;
    int64_t r2;
    bool isConst2 = false;
    int64_t r3;
    bool isConst3 = false;
    int64_t r4;
    bool isConst4 = false;
    int i = 0;

    // ---- Current interpreter state
    int64_t *currFrame = &(vm->callStack[0]);
    OFunction* currFunc = vm->currCallInfo->func;
    StackOConstant* currConst = currFunc->constants;

    printf("Executing entrypoint %s with %zu instructions...\n", currFunc->name->content, package.entryPoint->instructions->length);

    while (i < currFunc->instructions->length) {
        uint64_t instr = ((currFunc->instructions)->content)[i];
        opCode = decodeOpcode(instr);

        switch (opCode) {
        case OP_ADD: {
            decodePlainArgA(instr, &r1);
            decodeIntArgB(instr, currFrame, currConst->content, &r2);
            decodeIntArgC(instr, currFrame, currConst->content, &r3);
            if (r1 < 256) {
                *(currFrame + r1) = r2 + r3;
            } else {
                *(currFrame - FRAME_SIZE + r1 - 256) = r2 + r3;
            }

            break;
        }
        case OP_SUB: {
            decodePlainArgA(instr, &r1);
            decodeIntArgB(instr, currFrame, currConst->content, &r2);
            decodeIntArgC(instr, currFrame, currConst->content, &r3);
            if (r1 < 256) {
                *(currFrame + r1) = r2 - r3;                
            } else {
                *(currFrame - FRAME_SIZE + r1 - 256) = r2 - r3;
            }
            break;
        }
        case OP_MUL: {
            decodePlainArgA(instr, &r1);
            decodeIntArgB(instr, currFrame, currConst->content, &r2);
            decodeIntArgC(instr, currFrame, currConst->content, &r3);
            if (r1 < 256) {
                *(currFrame + r1) = r2 * r3;
            } else {
                *(currFrame - FRAME_SIZE + r1 - 256) = r2 * r3;
            }
            break;
        }
        case OP_DIV: {
            decodePlainArgA(instr, &r1);
            decodeIntArgB(instr, currFrame, currConst->content, &r2);
            decodeIntArgC(instr, currFrame, currConst->content, &r3);
            if (r3 == 0) {
                vm->wasError = true;
                strcpy(&(vm->errMsg[0]), "Division by zero error!");
                return;
            }
            if (r1 < 256) {
                *(currFrame + r1) = r2 / r3;
            } else {
                *(currFrame - FRAME_SIZE + r1 - 256) = r2 / r3;
            }
            break;
        }
        case OP_PRINT:
            decodeDerefArgB(instr, currFrame, currConst->content, &r2, &isConst2);
            if (isConst2) {
                OConstant constant = *(OConstant*)r2;
                if (constant.tag == intt) {
                    printf("%jd\n", constant.value.num);
                } else {
                    String* constString = constant.value.str;
                    printf("%s\n", constString->content);
                }
            } else { // local
                printf("%jd\n", r2);
            }
            break;
        case OP_LOADI: // load small integer into local variable
            decodePlainArgA(instr, &r1);
            decodePlainArgB(instr, &r2);
            *(currFrame + r1) = r2;
            break;
        case OP_COPY: // load integer into local variable
            // TODO constants?
            decodePlainArgA(instr, &r1);
            decodeDerefArgB(instr, currFrame, currConst->content, &r2, &isConst2);            
            *(currFrame + r1) = r2;
            break;
        case OP_CALL:
            printf("Encountered a function call\n");
            vm->currCallInfo->pc = i;

            int newLen = currFrame + 2*FRAME_SIZE - &(vm->callStack[0]);
            printf("Length of stack after adding new frame: %d\n", newLen);
            if (newLen >= CALL_STACK_SIZE) {
                vm->wasError = true;
                strcpy(&(vm->errMsg[0]), "Stack overflow!");
                return;
            }
            vm->currCallInfo->pc = i;
            currFrame += FRAME_SIZE;

            decodeDerefArgA(instr, currFrame, currConst->content, &r1, &isConst1);
            decodePlainArgB(instr, &r2);
            decodePlainArgC(instr, &r3);
            decodePlainArgD(instr, &r4);

            OConstant constForFunc = (*(OConstant*)r1);
            OFunction* newFunc = constForFunc.value.func;
            currFunc = newFunc;
            currConst = currFunc->constants;
            vm->currCallInfo++;
            bool needReturnAddresses = (r4 >= 256);
            int numberExtraReturnAddresses = 0;

            *currFrame = needReturnAddresses ? r4 - 256 : 0; // This is the 0th slot in a stack frame. This is why we ignore 0 values in stack indices
            *(vm->currCallInfo) = (CallInfo){ .func = newFunc, .indFixedArg = r2,
                                 .returnAddress = r3, .needReturnAddresses = needReturnAddresses, .pc = 0, };
            printf("Called function %s\n", currFunc->name->content);
            i = -1;
            break;
        case OP_RETURN:
            // adjust pointers to previous frame
            // restore i to value saved in previous CallInfo + 1
            vm->currCallInfo--;
            currFunc = vm->currCallInfo->func;
            currConst = currFunc->constants;
            currFrame -= FRAME_SIZE;

            i = vm->currCallInfo->pc;
            printf("returned from call to pc = %d\n", i);
            break;
        default:
            printf("Unknown opcode %d\n", opCode);
        }        
        i++;
        // TODO check if we are in a non-toplevel function - in that case, execute a return routine.
        // TODO This is to save redundant OP_RETURN instructions at the ends of functions.
    }
}




