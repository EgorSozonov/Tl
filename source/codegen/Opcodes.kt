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

    const val accessPublic = 0x0001       // class, field, method
    const val accessPrivate = 0x0002      // class, field, method
    const val accessProtected = 0x0004    // class, field, method
    const val accessStatic = 0x0008       // field, method
    const val accessFinal = 0x0010        // class, field, method, parameter
    const val accessSuper = 0x0020        // class
    const val accessSynchronized = 0x0020 // method
    const val accessOpen = 0x0020         // module
    const val accessTransitive = 0x0020   // module requires
    const val accessVolatile = 0x0040     // field
    const val accessBridge = 0x0040       // method
    const val accessStaticPhase = 0x0040  // module requires
    const val accessVarargs = 0x0080      // method
    const val accessTransient = 0x0080    // field
    const val accessNative = 0x0100       // method
    const val accessInterface = 0x0200    // class
    const val accessAbstract = 0x0400     // class, method
    const val accessStrict = 0x0800       // method
    const val accessSynthetic = 0x1000    // class, field, method, parameter, module *
    const val accessAnnotation = 0x2000   // class
    const val accessEnum = 0x4000         // class(?) field inner
    const val accessMandated = 0x8000     // field, method, parameter, module, module *
    const val accessModule = 0x8000       // class

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

    const val handleGetField = 1
    const val handleGetStatic = 2
    const val handlePutField = 3
    const val handlePutStatic = 4
    const val handleInvokeVirtual = 5
    const val handleInvokeStatic = 6
    const val handleInvokeSpecial = 7
    const val handleNewInvokeSpecial = 8
    const val handleInvokeInterface = 9



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
    const val dConst0      =  14
    const val dConst1      =  15
    const val biPush       =  16
    const val siPush       =  17
    const val ldc          =  18
    const val iLoad        =  21
    const val lLoad        =  22
    const val fLoad        =  23
    const val dLoad        =  24
    const val aLoad        =  25
    const val iaLoad       =  46
    const val laLoad       =  47
    const val faLoad       =  48
    const val daLoad       =  49
    const val aaLoad       =  50
    const val baLoad       =  51
    const val caLoad       =  52
    const val saLoad       =  53
    const val iStore       =  54
    const val lStore       =  55
    const val fStore       =  56
    const val dStore       =  57
    const val aStore       =  58
    const val iaStore      =  79
    const val laStore      =  80
    const val faStore      =  81
    const val daStore      =  82
    const val aaStore      =  83
    const val baStore      =  84
    const val caStore      =  85
    const val saStore      =  86
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



    const val ldcW = 19
    const val ldc2W = 20
    const val iLoad0 = 26
    const val iLoad1 = 27
    const val iLoad2 = 28
    const val iLoad3 = 29
    const val lLoad0 = 30
    const val lLoad1 = 31
    const val lLoad2 = 32
    const val lLoad3 = 33
    const val fLoad0 = 34
    const val fLoad1 = 35
    const val fLoad2 = 36
    const val fLoad3 = 37
    const val dLoad0 = 38
    const val dLoad1 = 39
    const val dLoad2 = 40
    const val dLoad3 = 41
    const val ALOAD_0 = 42
    const val ALOAD_1 = 43
    const val ALOAD_2 = 44
    const val ALOAD_3 = 45
    const val ISTORE_0 = 59
    const val ISTORE_1 = 60
    const val ISTORE_2 = 61
    const val ISTORE_3 = 62
    const val lStore0 = 63
    const val lStore1 = 64
    const val lStore2 = 65
    const val lStore3 = 66
    const val fStore0 = 67
    const val fStore1 = 68
    const val fStore2 = 69
    const val fStore3 = 70
    const val dStore0 = 71
    const val dStore1 = 72
    const val dStore2 = 73
    const val dStore3 = 74
    const val aStore0 = 75
    const val aStore1 = 76
    const val aStore2 = 77
    const val aStore3 = 78
    const val wide = 196
    const val gotoW = 200
    const val jsrW = 201
}