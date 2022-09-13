package utils

class ByteBuffer(initSize: Int) {
    var buffer: ByteArray
        private set
    var ind: Int
        private set

    init {
        buffer = ByteArray(initSize.coerceAtLeast(4))
        ind = 0
    }

    fun add(b: Byte) {
        if (ind < buffer.size) {
            buffer[ind] = b
            ind += 1
        } else {
            val newBuffer = ByteArray(buffer.size * 2)
            buffer.copyInto(newBuffer, 0, 0, buffer.size)
            newBuffer[buffer.size] = b
            buffer = newBuffer
            ind = buffer.size + 1
        }
    }

    fun clear() {
        ind = 0
    }
}