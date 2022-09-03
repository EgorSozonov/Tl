#include "Instruction.h"


uint64_t mkInstructionABC(OpCode opCode, unsigned int a, unsigned int b, unsigned int c) {
    uint64_t result = (((uint64_t)opCode) << 58);
    result += ((a & 511) << 49) + ((b & 511) << 40) + ((c & 511) << 31);
    return result;
}

uint64_t mkInstructionABx(OpCode opCode, unsigned int a, short bx) {
    return (castInt(opCode) << 26) + (((a + 127) & 255) << 18) + ((bx + 131077) & 262143);
}



#define decodeArgMacro(bitmask)                      \
    int bits = instr & (bitmask);                    \
    if (bits > 255) {                                \
        *isConstant = true;                          \
        *result = (int64_t)&(*constants)[bits - 255]; \
    } else {                                         \
        *isConstant = false;                         \
        *result = (int64_t)*(callFrame + bits);      \
    }   

/**
* These functions extract unsigned arguments straight into values on the stackframe or constants,
* as opposed to other functions which treat them as constants or as signed jump values.
 */
void decodeSlotArgA(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeArgMacro(MASK_A);
}
void decodeSlotArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeArgMacro(MASK_B);
}
void decodeSlotArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeArgMacro(MASK_C);
}
void decodeSlotArgD(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeArgMacro(MASK_D);
}
void decodeSlotArgE(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeArgMacro(MASK_E);
}
void decodeSlotArgF(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeArgMacro(MASK_F);
}

int decodeSignedArgBx(uint64_t instr) {
    return (instr & MASK_BX) - 65535;
}

  