package codegen
import parser.Parser
import java.io.ByteArrayOutputStream

class Codegen {
    fun codegen(pr: Parser): ByteArray {
        val wr = ByteArrayOutputStream()

        wr.write(byteArrayOf(12, 10, 15, 14, 11, 10, 11, 14), 0, 4) // CAFEBABE
        wr.write(0xCAFEBABE.toInt())

        return wr.toByteArray()
    }
}