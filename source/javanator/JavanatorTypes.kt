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

data class ConstantClass(val tag: Int, val nameIndex: Int)
data class ConstantFieldRef(val tag: Int, val classIndex: Int, val nameTypeIndex: Int)
data class ConstantMethodRef(val tag: Int, val classIndex: Int, val nameTypeIndex: Int)
data class ConstantInterfaceMethodRef(val tag: Int, val classIndex: Int, val nameTypeIndex: Int)
data class ConstantString(val tag: Int, val stringIndex: Int)
data class ConstantInt(val tag: Int, val lowBytes: Int)
data class ConstantShortFloat(val tag: Int, val lowBytes: Int)
data class ConstantLong(val tag: Int, val highBytes: Int, val lowBytes: Int)
data class ConstantFloat(val tag: Int, val highBytes: Int, val lowBytes: Int)
data class ConstantNameType(val tag: Int, val nameIndex: Int, val descriptorIndex: Int)
data class ConstantUtf8(val tag: Int, val content: ByteArray)
data class ConstantMethodHandle(val tag: Int, val referenceKind: Int, val referenceIndex: Int)
data class ConstantMethodType(val tag: Int, val descriptorIndex: Int)
data class ConstantInvokeDynamic(val tag: Int, val bootStrapMethodIndex: Int, val nameAndTypeIndex: Int)
