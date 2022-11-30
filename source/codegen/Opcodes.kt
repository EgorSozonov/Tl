package codegen

object Opcodes {
    const val ASM4 = (4 shl 16) or (0 shl 8)
    const val ASM5 = (5 shl 16) or (0 shl 8)
    const val ASM6 = (6 shl 16) or (0 shl 8)
    const val ASM7 = (7 shl 16) or (0 shl 8)
    const val ASM8 = (8 shl 16) or (0 shl 8)
    const val ASM9 = (9 shl 16) or (0 shl 8)


    const val sourceDeprecated = 0x100

    // Java ClassFile versions (the minor version is stored in the 16 most significant bits, and the
    // major version in the 16 least significant bits).

    const val V1_1 = (3 shl 16) or 45
    const val V1_2 = (0 shl 16) or 46
    const val V1_3 = (0 shl 16) or 47
    const val V1_4 = (0 shl 16) or 48
    const val V1_5 = (0 shl 16) or 49
    const val V1_6 = (0 shl 16) or 50
    const val V1_7 = (0 shl 16) or 51
    const val V1_8 = (0 shl 16) or 52
    const val V9   = (0 shl 16) or 53
    const val V10  = (0 shl 16) or 54
    const val V11  = (0 shl 16) or 55
    const val V12  = (0 shl 16) or 56
    const val V13  = (0 shl 16) or 57
    const val V14  = (0 shl 16) or 58
    const val V15  = (0 shl 16) or 59
    const val V16  = (0 shl 16) or 60
    const val V17  = (0 shl 16) or 61
    const val V18  = (0 shl 16) or 62
    const val V19  = (0 shl 16) or 63
    const val V20  = (0 shl 16) or 64

    /**
     * Version flag indicating that the class is using 'preview' features.
     *
     * <p>{@code version & V_PREVIEW == V_PREVIEW} tests if a version is flagged with {@code
     * V_PREVIEW}.
     */
    const val V_PREVIEW = 0xFFFF0000;

    // JVM Access flags

    const val accessPublic = 0x0001; // class, field, method
    const val accessPrivate = 0x0002; // class, field, method
    const val accessProtected = 0x0004; // class, field, method
    const val accessStatic = 0x0008; // field, method
    const val ACC_FINAL = 0x0010; // class, field, method, parameter
    const val ACC_SUPER = 0x0020; // class
    const val ACC_SYNCHRONIZED = 0x0020; // method
    const val ACC_OPEN = 0x0020; // module
    const val ACC_TRANSITIVE = 0x0020; // module requires
    const val ACC_VOLATILE = 0x0040; // field
    const val ACC_BRIDGE = 0x0040; // method
    const val ACC_STATIC_PHASE = 0x0040; // module requires
    const val ACC_VARARGS = 0x0080; // method
    const val ACC_TRANSIENT = 0x0080; // field
    const val ACC_NATIVE = 0x0100; // method
    const val ACC_INTERFACE = 0x0200; // class
    const val ACC_ABSTRACT = 0x0400; // class, method
    const val ACC_STRICT = 0x0800; // method
    const val ACC_SYNTHETIC = 0x1000; // class, field, method, parameter, module *
    const val ACC_ANNOTATION = 0x2000; // class
    const val ACC_ENUM = 0x4000; // class(?) field inner
    const val ACC_MANDATED = 0x8000; // field, method, parameter, module, module *
    const val ACC_MODULE = 0x8000; // class

    // Primitive types for the NEWARRAY instruction.
    // See https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-6.html#jvms-6.5.newarray.

    const val primitiveBool = 4
    const val primitiveChar = 5
    const val primitiveFloat = 6
    const val primitiveDouble = 7
    const val primitiveByte = 8
    const val primitiveShort = 9
    const val primitiveInt = 10
    const val primitiveLong = 11

    // Possible values for the reference_kind field of CONSTANT_MethodHandle_info structures.
    // See https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-4.html#jvms-4.4.8.

    const val handleGetField = 1;
    const val H_GETSTATIC = 2;
    const val H_PUTFIELD = 3;
    const val H_PUTSTATIC = 4;
    const val H_INVOKEVIRTUAL = 5;
    const val H_INVOKESTATIC = 6;
    const val H_INVOKESPECIAL = 7;
    const val H_NEWINVOKESPECIAL = 8;
    const val H_INVOKEINTERFACE = 9;


    /** An expanded frame. */
    const val F_NEW = -1;

    /** A compressed frame with complete frame data. */
    const val F_FULL = 0;

    /**
     * A compressed frame where locals are the same as the locals in the previous frame, except that
     * additional 1-3 locals are defined, and with an empty stack.
     */
    const val F_APPEND = 1;

    /**
     * A compressed frame where locals are the same as the locals in the previous frame, except that
     * the last 1-3 locals are absent and with an empty stack.
     */
    const val F_CHOP = 2;

    /**
     * A compressed frame with exactly the same locals as the previous frame and with an empty stack.
     */
    const val F_SAME = 3;

    /**
     * A compressed frame with exactly the same locals as the previous frame and with a single value
     * on the stack.
     */
    const val F_SAME1 = 4;

    // Standard stack map frame element types

    const val TOP = Frame.ITEM_TOP;
    const val INTEGER = Frame.ITEM_INTEGER;
    const val FLOAT = Frame.ITEM_FLOAT;
    const val DOUBLE = Frame.ITEM_DOUBLE;
    const val LONG = Frame.ITEM_LONG;
    const val NULL = Frame.ITEM_NULL;
    const val UNINITIALIZED_THIS = Frame.ITEM_UNINITIALIZED_THIS;

    // The JVM opcode values
    // See https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-6.html.

    const val NOP = 0
    const val ACONST_NULL = 1
    const val ICONST_M1 = 2
    const val ICONST_0 = 3
    const val ICONST_1 = 4
    const val ICONST_2 = 5
    const val ICONST_3 = 6
    const val ICONST_4 = 7
    const val ICONST_5 = 8
    const val LCONST_0 = 9
    const val LCONST_1 = 10
    const val FCONST_0 = 11
    const val FCONST_1 = 12
    const val FCONST_2 = 13
    const val DCONST_0 = 14
    const val DCONST_1 = 15
    const val BIPUSH = 16
    const val SIPUSH = 17
    const val LDC = 18
    const val ILOAD = 21
    const val LLOAD = 22
    const val FLOAD = 23
    const val DLOAD = 24
    const val ALOAD = 25
    const val IALOAD = 46
    const val LALOAD = 47
    const val FALOAD = 48
    const val DALOAD = 49
    const val AALOAD = 50
    const val BALOAD = 51
    const val CALOAD = 52
    const val SALOAD = 53
    const val ISTORE = 54
    const val LSTORE = 55
    const val FSTORE = 56
    const val DSTORE = 57
    const val ASTORE = 58
    const val IASTORE = 79
    const val LASTORE = 80
    const val FASTORE = 81
    const val DASTORE = 82
    const val AASTORE = 83
    const val BASTORE = 84
    const val CASTORE = 85
    const val SASTORE = 86
    const val POP = 87
    const val POP2 = 88
    const val DUP = 89
    const val DUP_X1 = 90
    const val DUP_X2 = 91
    const val DUP2 = 92
    const val DUP2_X1 = 93
    const val DUP2_X2 = 94
    const val SWAP = 95
    const val IADD = 96
    const val LADD = 97
    const val FADD = 98
    const val DADD = 99
    const val ISUB = 100
    const val LSUB = 101
    const val FSUB = 102
    const val DSUB = 103
    const val IMUL = 104
    const val LMUL = 105
    const val FMUL = 106
    const val DMUL = 107
    const val IDIV = 108
    const val LDIV = 109
    const val FDIV = 110
    const val DDIV = 111
    const val IREM = 112
    const val LREM = 113
    const val FREM = 114
    const val DREM = 115
    const val INEG = 116
    const val LNEG = 117
    const val FNEG = 118
    const val DNEG = 119
    const val ISHL = 120
    const val LSHL = 121
    const val ISHR = 122
    const val LSHR = 123
    const val IUSHR = 124
    const val LUSHR = 125
    const val IAND = 126
    const val LAND = 127
    const val IOR = 128
    const val LOR = 129
    const val IXOR = 130
    const val LXOR = 131
    const val IINC = 132
    const val I2L = 133
    const val I2F = 134
    const val I2D = 135
    const val L2I = 136
    const val L2F = 137
    const val L2D = 138
    const val F2I = 139
    const val F2L = 140
    const val F2D = 141
    const val D2I = 142
    const val D2L = 143
    const val D2F = 144
    const val I2B = 145
    const val I2C = 146
    const val I2S = 147
    const val LCMP = 148
    const val FCMPL = 149
    const val FCMPG = 150
    const val DCMPL = 151
    const val DCMPG = 152
    const val IFEQ = 153
    const val IFNE = 154
    const val IFLT = 155
    const val IFGE = 156
    const val IFGT = 157
    const val IFLE = 158
    const val IF_ICMPEQ = 159
    const val IF_ICMPNE = 160
    const val IF_ICMPLT = 161
    const val IF_ICMPGE = 162
    const val IF_ICMPGT = 163
    const val IF_ICMPLE = 164
    const val IF_ACMPEQ = 165
    const val IF_ACMPNE = 166
    const val GOTO = 167
    const val JSR = 168
    const val RET = 169
    const val TABLESWITCH = 170
    const val LOOKUPSWITCH = 171
    const val IRETURN = 172
    const val LRETURN = 173
    const val FRETURN = 174;
    const val DRETURN = 175
    const val ARETURN = 176
    const val RETURN = 177
    const val GETSTATIC = 178
    const val PUTSTATIC = 179
    const val GETFIELD = 180
    const val PUTFIELD = 181
    const val INVOKEVIRTUAL = 182
    const val INVOKESPECIAL = 183
    const val INVOKESTATIC = 184
    const val INVOKEINTERFACE = 185
    const val INVOKEDYNAMIC = 186
    const val NEW = 187
    const val NEWARRAY = 188
    const val ANEWARRAY = 189
    const val ARRAYLENGTH = 190
    const val ATHROW = 191
    const val CHECKCAST = 192
    const val INSTANCEOF = 193
    const val MONITORENTER = 194
    const val MONITOREXIT = 195
    const val MULTIANEWARRAY = 197
    const val IFNULL = 198
    const val IFNONNULL = 199
}