#include "Instruction.h"


uint64_t mkInstructionABC(OpCode opCode, unsigned int a, unsigned int b, unsigned int c) {
    uint64_t result = (((uint64_t)opCode) << 58);    
    result += ((uint64_t)(a & 511) << 49) + (((uint64_t)(b & 511)) << 40) + (((uint64_t)(c & 511)) << 31);
    return result;
}

uint64_t mkInstructionABCD(OpCode opCode, unsigned int a, unsigned int b, unsigned int c, unsigned int d) {
    uint64_t result = mkInstructionABC(opCode, a, b, c);
    result += ((uint64_t)(d & 511) << 22);
    return result;
}

uint64_t mkInstructionABCDEF(OpCode opCode, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f) {
    uint64_t result = mkInstructionABC(opCode, a, b, c);
    result += ((uint64_t)(d & 511) << 22) + (((uint64_t)(e & 511)) << 13) + (((uint64_t)(f & 511)) << 4);
    return result;
}

uint64_t mkInstructionABx(OpCode opCode, unsigned int a, short bx) {
    return (castInt(opCode) << 26) + (((a + 127) & 255) << 18) + ((bx + 131077) & 262143);
}



#define decodeDerefArgMacro(bitmask, shiftLen)        \
    int bits = (instr & (bitmask)) >> shiftLen;       \
    if (bits > 255) {                                 \
        *isConstant = true;                           \
        *result = (int64_t)&(*constants)[bits - 256]; \
    } else {                                          \
        *isConstant = false;                          \
        *result = (int64_t)*(callFrame + bits);       \
    }

#define decodeIntArgMacro(bitmask, shiftLen)                     \
    int bits = (instr & (bitmask)) >> shiftLen;                  \
    if (bits > 255) {                                            \
        *result = (int64_t)((*constants)[bits - 256].value.num); \
    } else {                                                     \
        *result = (int64_t)*(callFrame + bits);                  \
    }

#define decodePlainArgMacro(bitmask, shiftLen)        \
        int bits = (instr & (bitmask)) >> shiftLen;   \
        *result = bits;                               \

/**
 * These functions extract unsigned arguments and dereference them to return either values from the stackframe or pointers to constants.
 */
void decodeDerefArgA(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeDerefArgMacro(MASK_A, 49);
}
void decodeDerefArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeDerefArgMacro(MASK_B, 40);
}
void decodeDerefArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeDerefArgMacro(MASK_C, 31);
}
void decodeDerefArgD(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeDerefArgMacro(MASK_D, 22);
}
void decodeDerefArgE(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeDerefArgMacro(MASK_E, 13);
}
void decodeDerefArgF(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant){
    decodeDerefArgMacro(MASK_F, 4);
}

void decodeIntArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result){
    decodeIntArgMacro(MASK_B, 40);
}

void decodeIntArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result){
    decodeIntArgMacro(MASK_C, 31);
}

/**
 * These functions extract unsigned arguments into plain unsigned numbers.
 */
void decodePlainArgA(uint64_t instr, /* output: */ int64_t* result){
    decodePlainArgMacro(MASK_A, 49);
}

void decodePlainArgB(uint64_t instr, /* output: */ int64_t* result){
    decodePlainArgMacro(MASK_B, 40);
}

void decodePlainArgC(uint64_t instr, /* output: */ int64_t* result){
    decodePlainArgMacro(MASK_C, 31);
}

void decodePlainArgD(uint64_t instr, /* output: */ int64_t* result){
    decodePlainArgMacro(MASK_D, 22);
}

int decodeSignedArgBx(uint64_t instr) {
    return (instr & MASK_BX) - 65535;
}

  
