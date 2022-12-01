package codegen

object Opcodes {
    const val sourceDeprecated = 0x100

    // Java ClassFile versions (the minor version is stored in the 16 most significant bits, and the
    // major version in the 16 least significant bits).

    const val v1_1 = (3 shl 16) or 45
    const val v1_2 = (0 shl 16) or 46
    const val v1_3 = (0 shl 16) or 47
    const val v1_4 = (0 shl 16) or 48
    const val v1_5 = (0 shl 16) or 49
    const val v1_6 = (0 shl 16) or 50
    const val v1_7 = (0 shl 16) or 51
    const val v1_8 = (0 shl 16) or 52
    const val v9   = (0 shl 16) or 53
    const val v10  = (0 shl 16) or 54
    const val v11  = (0 shl 16) or 55
    const val v12  = (0 shl 16) or 56
    const val v13  = (0 shl 16) or 57
    const val v14  = (0 shl 16) or 58
    const val v15  = (0 shl 16) or 59
    const val v16  = (0 shl 16) or 60
    const val v17  = (0 shl 16) or 61
    const val v18  = (0 shl 16) or 62
    const val v19  = (0 shl 16) or 63
    const val v20  = (0 shl 16) or 64

    /**
     * Version flag indicating that the class is using 'preview' features.
     *
     * <p>{@code version & V_PREVIEW == V_PREVIEW} tests if a version is flagged with {@code
     * V_PREVIEW}.
     */
    const val V_PREVIEW = 0xFFFF0000

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



    // The JVM opcode values
    // See https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-6.html.

    const val nop          =   0
    const val aConstNull   =   1
    const val iConstM1     =   2
    const val iConst0      =   3
    const val iConst1      =   4
    const val iConst2      =   5
    const val iConst3      =   6
    const val iConst4      =   7
    const val iConst5      =   8
    const val lConst0      =   9
    const val lConst1      =  10
    const val fConst0      =  11
    const val fConst1      =  12
    const val fConst2      =  13
    const val DCONST_0     =  14
    const val DCONST_1     =  15
    const val BIPUSH       =  16
    const val SIPUSH       =  17
    const val LDC          =  18
    const val ILOAD        =  21
    const val LLOAD        =  22
    const val FLOAD        =  23
    const val DLOAD        =  24
    const val ALOAD        =  25
    const val IALOAD       =  46
    const val LALOAD       =  47
    const val FALOAD       =  48
    const val DALOAD       =  49
    const val AALOAD       =  50
    const val BALOAD       =  51
    const val CALOAD       =  52
    const val SALOAD       =  53
    const val ISTORE       =  54
    const val LSTORE       =  55
    const val FSTORE       =  56
    const val DSTORE       =  57
    const val ASTORE       =  58
    const val IASTORE      =  79
    const val LASTORE      =  80
    const val fAStore      =  81
    const val dAStore      =  82
    const val aAStore      =  83
    const val bAStore      =  84
    const val cAStore      =  85
    const val sAStore      =  86
    const val pop          =  87
    const val pop2         =  88
    const val dup          =  89
    const val dupX1        =  90
    const val dupX2        =  91
    const val dup2         =  92
    const val dup2X1       =  93
    const val dup2X2       =  94
    const val swap         =  95
    const val iAdd         =  96
    const val lAdd         =  97
    const val fAdd         =  98
    const val dAdd         =  99
    const val iSub         = 100
    const val lSub         = 101
    const val fSub         = 102
    const val dSub         = 103
    const val iMul         = 104
    const val lMul         = 105
    const val fMul         = 106
    const val dMul         = 107
    const val iDiv         = 108
    const val lDiv         = 109
    const val fDiv         = 110
    const val dDiv         = 111
    const val iRem         = 112
    const val lRem         = 113
    const val fRem         = 114
    const val dRem         = 115
    const val iNeg         = 116
    const val lNeg         = 117
    const val fNeg         = 118
    const val dNeg         = 119
    const val iShL         = 120
    const val lShL         = 121
    const val iShR         = 122
    const val lShR         = 123
    const val iUShR        = 124
    const val lUShR        = 125
    const val iAnd         = 126
    const val lAnd         = 127
    const val iOr          = 128
    const val lOr          = 129
    const val iXOr         = 130
    const val lXOr         = 131
    const val iInc         = 132
    const val i2l          = 133
    const val i2f          = 134
    const val i2d          = 135
    const val l2i          = 136
    const val l2f          = 137
    const val l2d          = 138
    const val f2i          = 139
    const val f2l          = 140
    const val f2d          = 141
    const val d2i          = 142
    const val d2l          = 143
    const val d2f          = 144
    const val i2b          = 145
    const val i2c          = 146
    const val i2s          = 147
    const val lCmp         = 148
    const val fCmpL        = 149
    const val fCmpG        = 150
    const val dCmpL        = 151
    const val dCmpG        = 152
    const val ifEq         = 153
    const val ifNE         = 154
    const val ifLT         = 155
    const val ifGE         = 156
    const val ifGT         = 157
    const val ifLE         = 158
    const val ifICmpEq     = 159
    const val ifICmpNE     = 160
    const val ifICmpLT     = 161
    const val ifICmpGE     = 162
    const val ifICmpGT     = 163
    const val ifICmpLE     = 164
    const val ifAcmpEQ     = 165
    const val ifACmpNE     = 166
    const val goto         = 167
    const val jsr          = 168
    const val ret          = 169
    const val tableSwitch  = 170
    const val lookupSwitch = 171
    const val iReturn      = 172
    const val lReturn      = 173
    const val fReturn      = 174
    const val dReturn      = 175
    const val aReturn      = 176
    const val returnn      = 177
    const val getStatic    = 178
    const val putStatic    = 179
    const val getField     = 180
    const val putField     = 181
    const val invokeVirt   = 182
    const val invokeSpec   = 183
    const val invokeStatic = 184
    const val invokeIface  = 185
    const val invokeDyn    = 186
    const val new          = 187
    const val newArray     = 188
    const val aNewArray    = 189
    const val arrayLength  = 190
    const val aThrow       = 191
    const val checkCast    = 192
    const val instanceOf   = 193
    const val monitorEnter = 194
    const val monitorExit  = 195
    const val multiANewArr = 197
    const val ifNull       = 198
    const val ifNonnull    = 199
}