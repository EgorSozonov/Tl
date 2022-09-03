#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include "../utils/StackHeader.h"
#include "../utils/Arena.h"
#include "../utils/Casts.h"
#include "../utils/String.h"
#include <stdint.h>

typedef enum {
    OP_ADD,       // integer arithmetic
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_FLADD,     // floating-point arithmetic
    OP_FLSUB,
    OP_FLMUL,
    OP_FLDIV,
    OP_LOADI,     // Load int B into the stack at R[A]
    OP_PRINT,
    OP_READGLOB,
    OP_WRITEGLOB,
    OP_CALLPROLOGUE, // Preparation for a function call. A = index of first fixed arg, Bx = number of locals + fixed args
    OP_CALLRETADRESS, // Sets an additional return address for the future function call
    OP_CALLARG,       // Sets an argument (vararg or fixed) for the future function call    
    OP_CALL,      // R[A] is function id (in the constants list), B = number of locals + fixed args,
                  // C = index of first fixed arg, D = main return address as offset from new frame base, 
                  // E = whether we even need extra ret addresses, F = its lower 8 bits are the number of extra ret addresses
    OP_COPY,      // R[A] <- R[B]
    OP_NOOP,      // Does nothing
    OP_EXTRAARG,  // Indicates that is a double-length instruction; actual opcode will be in the second 32 bits
} OpCode;

// ======= Calling functions =======
// 8 index of first fixed arg
// 8 number of (locals + normal args)
// 8 function id
// 8 main return address (as offset)
// 1 need extra ret addresses?
// 8 return addresses

// Example call of function f x y = {z = x + 2*y; return z*x}:
// OP_CALLPROLOGUE 1 (3) # First fixed arg is at index 1 because index 0 is taken by the count extra ret addresses (0 in this case)
//                       # Bx = 3 because 2 fixed args plus one local
// OP_CALL         1 -3 0b0..0 # Index 1 for function id location is arbitrary, -1 means return value will be (childFrameBase - 3), and 0 extra ret values

typedef uint64_t OInstr;

typedef enum {
    intt,
    strr,
} ConstType;

typedef struct {
    ConstType tag;
    union {
        int num;
        String* str;
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

#define decodeOpcode(i)	 (cast(OpCode, ((i) & MASK_OPC)))
#define decodeOpcodeY(i)  cast(OpCode, ((i) & 0b0000000000000000000000000000000001111100000000000000000000000000)))
#define decodeOpcodeZ2(i) cast(OpCode, ((i) & 0b0000000000000000000000000000000001111110000000000000000000000000)))
#define decodeOpcodeZ3(i) cast(OpCode, ((i) & 0b0000000000000000000000000000000000000000000000001111110000000000)))
void decodeSlotArgA(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeSlotArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeSlotArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeSlotArgD(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeSlotArgE(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
void decodeSlotArgF(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);

int decodeSignedArgBx(uint64_t instr);

//void decodeLiteralArgA(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
//void decodeSlotArgB(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);
//void decodeSlotArgC(uint64_t instr, int64_t* callFrame, OConstant (*constants)[], /* output: */ int64_t* result, bool* isConstant);

uint64_t mkInstructionABC(OpCode opCode, unsigned int a, unsigned int b, unsigned int c);
uint64_t mkInstructionABx(OpCode opCode, unsigned int a, short bx);

#endif
