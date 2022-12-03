package javanator

data class JavaClass(
    val name: String,
    val magic: Int,
    val minorVersion: Int,
    val majorVersion: Int,
    val constantPool: Array<CpInfo>,
    val accessFlags: Int,
    val thisClass: Int,
    val superClass: Int,
    val interfaces: ByteArray,
    val fields: Array<FieldInfo>,
    val methods: Array<MethodInfo>,
    val attributes: Array<AttributeInfo>,
)

data class CpInfo(val tag: Byte, val info: ByteArray)
data class FieldInfo(val accessFlags: Int, val nameIndex: Int, val descriptorIndex: Int,
                     val attributes: Array<AttributeInfo>)
data class MethodInfo(val accessFlags: Int, val nameIndex: Int, val descriptorIndex: Int,
                      val attributes: Array<AttributeInfo>)
data class AttributeInfo(val attributeNameIndex: Int, val info: ByteArray)
