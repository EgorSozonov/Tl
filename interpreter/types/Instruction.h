#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include "../../utils/StackHeader.h"
#include "../../utils/Arena.h"
#include "../../utils/Casts.h"
#include "../../utils/String.h"
#include <stdint.h>

typedef enum {
    OP_ADD,       // R[A] = R[B] + R[C}
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_ADDFL,     // floating-point arithmetic
    OP_SUBFL,
    OP_MULFL,
    OP_DIVFL,
    OP_LOADI,     // Load int B into the stack at R[A]
    OP_PRINT,
    OP_READGLOB,
    OP_WRITEGLOB,
    OP_CALLRETADRESS, // Sets an additional return address for the future function call
    OP_CALLARG,       // Sets an argument (vararg or fixed) for the future function call    
    OP_CALL,      // A is function index (in the constants list), B = index of first fixed arg, C = main return address as offset from new frame base,
                  // D = whether we need extra ret addresses (its lower 8 bits are the number of extra ret addresses)
    OP_RETURN,    // Return from function call
    OP_COPY,      // R[A] <- R[B]
    OP_NOOP,      // Does nothing
    OP_EXTRAARG,  // Indicates that is a double-length instruction; actual opcode will be in the second 32 bits
} OpCode;

// Register/constant addresses are distinguished by the highest bit (1 means constant).
// If it's a register address, then 0 value is ignored by most instructions, since position 0 in the stack frame
// is for a specific purpose (number of extra return addresses).


typedef uint64_t OInstr;

typedef enum {
    intt,
    strr,
    funcc,
} ConstType;

struct OFunction;
typedef struct {
    ConstType tag;
    union {
        int64_t num;
        String* str;
        struct OFunction* func;
    } value;
} OConstant;
DEFINE_STACK_HEADER(OConstant)

/**
 *  The list of instructions must have an even number of elements.
 *  Two-element instructions are aligned at even numbers.
 *  Hence, the interpreter will ingest instructions by two (two 32-bit instrs = 64 bit).
 *  In order to facilitate this, the OP_NOOP exists.
 */
DEFINE_STACK_HEADER(OInstr)



enum InstructionMode {modeABC, modeABx, modeAx};

/**
 * Instruction layouts:
 */

// Standard layout
// [Opcode 6] [A 9] [B 9] [C 9] [D 9] [E 9] [F 9] [_ 4]
#define MASK_OPC  0b1111110000000000000000000000000000000000000000000000000000000000
#define MASK_A    0b0000001111111110000000000000000000000000000000000000000000000000
#define MASK_B    0b0000000000000001111111110000000000000000000000000000000000000000
#define MASK_C    0b0000000000000000000000001111111110000000000000000000000000000000
#define MASK_D    0b0000000000000000000000000000000001111111110000000000000000000000
#define MASK_E    0b0000000000000000000000000000000000000000001111111110000000000000
#define MASK_F    0b0000000000000000000000000000000000000000000000000001111111110000

// For extended operand lengths
// [Opcode 6] [Ax 17] [Bx 17] [Cx 17] [_ 7]
#define MASK_AX   0b0000001111111111111111100000000000000000000000000000000000000000
#define MASK_BX   0b0000000000000000000000011111111111111111000000000000000000000000
#define MASK_CX   0b0000000000000000000000000000000000000000111111111111111110000000

// For two operations with one instruction
// [Opcode 6] [A 9] [B 9] [C 9] [Opcode 4] [Dy 9] [Ey 9] [Fy 9]
#define MASK_OPY  0b0000000000000000000000000000000001111100000000000000000000000000
#define MASK_DY   0b0000000000000000000000000000000000000011111111000000000000000000
#define MASK_EY   0b0000000000000000000000000000000000000000000000111111111000000000
#define MASK_FY   0b0000000000000000000000000000000000000000000000000000000111111111

// For up to three arithmetic operations with one instruction
// [Opcode 6] [A 9] [B 9] [C 9] [?Opcode 6] [Dz 9] [?Opcode 6] [Ez 9] [_ 1]
#define MASK_OPZ2 0b0000000000000000000000000000000001111110000000000000000000000000
#define MASK_OPZ3 0b0000000000000000000000000000000000000000000000001111110000000000
#define MASK_DZ   0b0000000000000000000000000000000000000001111111110000000000000000
#define MASK_EZ   0b0000000000000000000000000000000000000000000000000000001111111111

#define decodeOpcode(i)	 (cast(OpCode, (((i) & MASK_OPC) >> 58)))
#define decodeOpcodeY(i)  cast(OpCode, (((i) & MASK_OPY) >> 27))
#define decodeOpcodeZ2(i) cast(OpCode, (((i) & MASK_OPZ2) >> 25))
#define decodeOpcodeZ3(i) cast(OpCode, (((i) & MASK_OPZ3) >> 10)))
void decodeDerefArgA(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeDerefArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeDerefArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeDerefArgD(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeDerefArgE(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeDerefArgF(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeIntArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result);
void decodeIntArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result);
void decodePlainArgA(uint64_t instr, /* output: */ int64_t* result);
void decodePlainArgB(uint64_t instr, /* output: */ int64_t* result);
void decodePlainArgC(uint64_t instr, /* output: */ int64_t* result);
void decodePlainArgD(uint64_t instr, /* output: */ int64_t* result);

int decodeSignedArgBx(uint64_t instr);

//void decodeLiteralArgA(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
//void decodeSlotArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
//void decodeSlotArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);

uint64_t mkInstructionABC(OpCode opCode, unsigned int a, unsigned int b, unsigned int c);
uint64_t mkInstructionABCD(OpCode opCode, unsigned int a, unsigned int b, unsigned int c, unsigned int d);
uint64_t mkInstructionABCDEF(OpCode opCode, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f);
uint64_t mkInstructionABx(OpCode opCode, unsigned int a, short bx);

#endif
