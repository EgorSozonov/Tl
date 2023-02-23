package utils

class UnsafeByteArray(capacity: Int) {
    protected val content: ByteArray
    protected var ind: Int

    init {
        content = ByteArray(capacity)
        ind = 0
    }

    fun addShort(value: Int): UnsafeByteArray {
        content[ind] = (value ushr 8).toByte()
        content[ind + 1] = value.toByte()
        ind += 2
        return this
    }

    fun addInt(value: Int): UnsafeByteArray {
        content[ind] = (value ushr 8).toByte()
        content[ind + 1] = value.toByte()

        content[ind    ] = (value ushr 24).toByte()
        content[ind + 1] = (value ushr 16).toByte()
        content[ind + 2] = (value ushr 8).toByte()
        content[ind + 3] = value.toByte()

        ind += 4
        return this
    }
}