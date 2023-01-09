package codegen

object StackMap {


    /** An expanded frame */
    const val fNew = -1;

    /** A compressed frame with complete frame data */
    const val fFull = 0

    /**
     * A compressed frame where locals are the same as the locals in the previous frame, except that
     * additional 1-3 locals are defined, and with an empty stack.
     */
    const val fAppend = 1

    /**
     * A compressed frame where locals are the same as the locals in the previous frame, except that
     * the last 1-3 locals are absent and with an empty stack.
     */
    const val fChop = 2

    /**
     * A compressed frame with exactly the same locals as the previous frame and with an empty stack.
     */
    const val fSame = 3

    /**
     * A compressed frame with exactly the same locals as the previous frame, with a single value
     * on the stack.
     */
    const val fSame1 = 4
    
    const val sameFrame = 0
    const val sameLocals1StackItemFrame = 64
    const val reserved = 128
    const val sameLocals1StackItemFrameExtended = 247
    const val chopFrame = 248
    const val sameFrameExtended = 251
    const val appendFrame = 252
    const val fullFrame = 255

    const val itemTop = 0
    const val itemInteger = 1
    const val itemFloat = 2
    const val itemDouble = 3
    const val itemLong = 4
    const val itemNull = 5
    const val itemUninitializedThis = 6
    const val itemObject = 7
    const val itemUninitialized = 8


    // The size and offset in bits of each field of an abstract type.

    // The size and offset in bits of each field of an abstract type.
    private const val dimSize = 6
    private const val kindSize = 4
    private const val flagsSize = 2
    private const val valueSize = 32 - dimSize - kindSize - flagsSize

    private const val dimShift = kindSize + flagsSize + valueSize
    private const val kindShift = flagsSize + valueSize
    private const val flagsShift = valueSize

    // Bitmasks to get each field of an abstract type.

    // Bitmasks to get each field of an abstract type.
    private const val dimMask = (1 shl dimSize) - 1 shl dimShift
    private const val KIND_MASK = (1 shl kindSize) - 1 shl kindShift
    private const val VALUE_MASK = (1 shl valueSize) - 1

    // Constants to manipulate the DIM field of an abstract type.

    // Constants to manipulate the DIM field of an abstract type.
    /** The constant to be added to an abstract type to get one with one more array dimension.  */
    private const val arrayOf = +1 shl dimShift

    /** The constant to be added to an abstract type to get one with one less array dimension.  */
    private const val elementOf = -1 shl dimShift

    // Possible values for the KIND field of an abstract type.

    // Possible values for the KIND field of an abstract type.
    private const val CONSTANT_KIND = 1 shl kindShift
    private const val REFERENCE_KIND = 2 shl kindShift
    private const val UNINITIALIZED_KIND = 3 shl kindShift
    private const val LOCAL_KIND = 4 shl kindShift
    private const val STACK_KIND = 5 shl kindShift

    // Possible flags for the FLAGS field of an abstract type.

    // Possible flags for the FLAGS field of an abstract type.
    /**
     * A flag used for LOCAL_KIND and STACK_KIND abstract types, indicating that if the resolved,
     * concrete type is LONG or DOUBLE, TOP should be used instead (because the value has been
     * partially overridden with an xSTORE instruction).
     */
    private const val topIfLongOrDoubleFlag = 1 shl flagsShift



}