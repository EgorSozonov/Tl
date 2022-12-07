package codegen

import com.sun.tools.javap.AnnotationWriter
import javanator.JavaClass
import utils.UnsafeByteArray
import jdk.internal.org.objectweb.asm.ClassTooLargeException
import jdk.javadoc.internal.doclets.toolkit.FieldWriter
import jdk.javadoc.internal.doclets.toolkit.MethodWriter
import java.io.ByteArrayOutputStream


class Codegen {
    /**
     * The minorVersion and majorVersion fields of the JVMS ClassFile structure. minorVersion is
     * stored in the 16 most significant bits, and majorVersion in the 16 least significant bits.
     */
    private val version = 0

    fun codegen(theClass: JavaClass): UnsafeByteArray {
        val size = computeSizeClassFile(theClass)
        val result = writeClassFile(theClass, size)
        return result
    }

    /**
     * Computes the size of the ClassFile structure.
     * The magic field uses 4 bytes, 10 mandatory fields (minorVersion, majorVersion,
     * constantPoolCount, accessFlags, thisClass, superClass, interfacesCount, fieldsCount,
     * methodsCount and attributesCount) use 2 bytes each, and each interface uses 2 bytes too.
     */
    private fun computeSizeClassFile(theClass: JavaClass): Int {
        var result: Int = 24 + 2 * theClass.interfaces.size
//        var fieldsCount = 0
//        var fieldWriter: FieldWriter = firstField
//        while (fieldWriter != null) {
//            ++fieldsCount
//            result += fieldWriter.computeFieldInforesult()
//            fieldWriter = fieldWriter.fv as FieldWriter
//        }
//        var methodsCount = 0
//        var methodWriter: MethodWriter = firstMethod
//        while (methodWriter != null) {
//            ++methodsCount
//            result += methodWriter.computeMethodInforesult()
//            methodWriter = methodWriter.mv as MethodWriter
//        }
//
//        var attributesCount = 0
//        if (innerClasses != null) {
//            ++attributesCount
//            result += 8 + innerClasses.length
//            symbolTable.addConstantUtf8(Constants.INNER_CLASSES)
//        }
//        if (enclosingClassIndex !== 0) {
//            ++attributesCount
//            result += 10
//            symbolTable.addConstantUtf8(Constants.ENCLOSING_METHOD)
//        }
//        if (accessFlags and Opcodes.ACC_SYNTHETIC !== 0 && version and 0xFFFF < Opcodes.V1_5) {
//            ++attributesCount
//            result += 6
//            symbolTable.addConstantUtf8(Constants.SYNTHETIC)
//        }
//        if (signatureIndex !== 0) {
//            ++attributesCount
//            result += 8
//            symbolTable.addConstantUtf8(Constants.SIGNATURE)
//        }
//        if (sourceFileIndex !== 0) {
//            ++attributesCount
//            result += 8
//            symbolTable.addConstantUtf8(Constants.SOURCE_FILE)
//        }
//        if (debugExtension != null) {
//            ++attributesCount
//            result += 6 + debugExtension.length
//            symbolTable.addConstantUtf8(Constants.SOURCE_DEBUG_EXTENSION)
//        }
//        if (accessFlags and Opcodes.ACC_DEPRECATED !== 0) {
//            ++attributesCount
//            result += 6
//            symbolTable.addConstantUtf8(Constants.DEPRECATED)
//        }
//        if (lastRuntimeVisibleAnnotation != null) {
//            ++attributesCount
//            result += lastRuntimeVisibleAnnotation.computeAnnotationsresult(
//                Constants.RUNTIME_VISIBLE_ANNOTATIONS
//            )
//        }
//        if (lastRuntimeInvisibleAnnotation != null) {
//            ++attributesCount
//            result += lastRuntimeInvisibleAnnotation.computeAnnotationsresult(
//                Constants.RUNTIME_INVISIBLE_ANNOTATIONS
//            )
//        }
//        if (lastRuntimeVisibleTypeAnnotation != null) {
//            ++attributesCount
//            result += lastRuntimeVisibleTypeAnnotation.computeAnnotationsresult(
//                Constants.RUNTIME_VISIBLE_TYPE_ANNOTATIONS
//            )
//        }
//        if (lastRuntimeInvisibleTypeAnnotation != null) {
//            ++attributesCount
//            result += lastRuntimeInvisibleTypeAnnotation.computeAnnotationsresult(
//                Constants.RUNTIME_INVISIBLE_TYPE_ANNOTATIONS
//            )
//        }
//        if (symbolTable.computeBootstrapMethodsresult() > 0) {
//            ++attributesCount
//            result += symbolTable.computeBootstrapMethodsresult()
//        }
//        if (moduleWriter != null) {
//            attributesCount += moduleWriter.getAttributeCount()
//            result += moduleWriter.computeAttributesresult()
//        }
//        if (nestHostClassIndex !== 0) {
//            ++attributesCount
//            result += 8
//            symbolTable.addConstantUtf8(Constants.NEST_HOST)
//        }
//        if (nestMemberClasses != null) {
//            ++attributesCount
//            result += 8 + nestMemberClasses.length
//            symbolTable.addConstantUtf8(Constants.NEST_MEMBERS)
//        }
//        if (permittedSubclasses != null) {
//            ++attributesCount
//            result += 8 + permittedSubclasses.length
//            symbolTable.addConstantUtf8(Constants.PERMITTED_SUBCLASSES)
//        }
//        var recordComponentCount = 0
//        var recordresult = 0
//        if (accessFlags and Opcodes.ACC_RECORD !== 0 || firstRecordComponent != null) {
//            var recordComponentWriter: RecordComponentWriter = firstRecordComponent
//            while (recordComponentWriter != null) {
//                ++recordComponentCount
//                recordresult += recordComponentWriter.computeRecordComponentInforesult()
//                recordComponentWriter = recordComponentWriter.delegate as RecordComponentWriter
//            }
//            ++attributesCount
//            result += 8 + recordresult
//            symbolTable.addConstantUtf8(Constants.RECORD)
//        }
//        if (firstAttribute != null) {
//            attributesCount += firstAttribute.getAttributeCount()
//            result += firstAttribute.computeAttributesresult(symbolTable)
//        }
//        // IMPORTANT: this must be the last part of the ClassFile result computation, because the previous
//        // statements can add attribute names to the constant pool, thereby changing its result!
//        result += symbolTable.getConstantPoolLength()
//        val constantPoolCount: Int = symbolTable.getConstantPoolCount()
//        if (constantPoolCount > 0xFFFF) {
//            throw ClassTooLargeException(symbolTable.getClassName(), constantPoolCount)
//        }
        return result
    }

    /**
     * Allocates a ByteVector of the pre-calculated size and write its contents
     */
    private fun writeClassFile(theClass: JavaClass, size: Int): UnsafeByteArray {
        val result = UnsafeByteArray(size)
        result.addInt(0xCAFEBABE.toInt())
        result.addShort(0).addShort(61) // versions

        result.addShort(theClass.constantPool.size)
        for (con in theClass.constantPool) {

        }

        result.addShort(theClass.accessFlags).addShort(theClass.thisClass).addShort(theClass.superClass)

        val interfaceCount = theClass.interfaces.size/2

        result.addShort(interfaceCount)
        // TODO interfaces

        result.addShort(theClass.fields.size)
        // TODO fields

        val methodCount = theClass.methods.size
        result.addShort(methodCount)

        return result
    }
}

//Java class file can be divided into 10 basic sections e.g.
//
//Magic Number : 0xCAFEBABE
//Major and Minor class version or .class file
//Constant pool - constants for this class
//Access modifiers - to indicate whether the class is static or abstract
//This class - name of this class
//Super class - name of superclass
//Interfaces -
//Fields
//Methods
//Attributes
//



//4-byte magic number
//2-byte minor_version
//2-byte major_version

//2-byte constant_pool_count
//N-byte constant_pool[constant_pool_count-1]

//2-byte access_flags
//2-byte this_class
//2-byte super_class

//2-byte interfaces_count
//2-byte interfaces[interfaces_count]

//2-byte fields_count
//N-byte fields[fields_count]

//2-byte methods_count
//N-byte methods[methods_count]

//2-byte attributes_count
//N-byte attributes[attributes_count]