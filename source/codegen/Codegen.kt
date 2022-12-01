package codegen

import com.sun.tools.javap.AnnotationWriter
import jdk.internal.org.objectweb.asm.ClassTooLargeException
import jdk.javadoc.internal.doclets.toolkit.FieldWriter
import jdk.javadoc.internal.doclets.toolkit.MethodWriter
import parser.Parser
import java.io.ByteArrayOutputStream


class Codegen {
    /**
     * The minorVersion and majorVersion fields of the JVMS ClassFile structure. minorVersion is
     * stored in the 16 most significant bits, and majorVersion in the 16 least significant bits.
     */
    private val version = 0

    fun codegen(pr: Parser): ByteArray {
        val size = computeSizeClassFile()
        val result = writeClassFile(size)
        return result
    }

    /**
     * Computes the size of the ClassFile structure.
     * The magic field uses 4 bytes, 10 mandatory fields (minorVersion, majorVersion,
     * constantPoolCount, accessFlags, thisClass, superClass, interfacesCount, fieldsCount,
     * methodsCount and attributesCount) use 2 bytes each, and each interface uses 2 bytes too.
     */
    private fun computeSizeClassFile(): Int {
        // First step: compute the size in bytes of the ClassFile structure.
        // The magic field uses 4 bytes, 10 mandatory fields (minorVersion, majorVersion,
        // constantPoolCount, accessFlags, thisClass, superClass, interfacesCount, fieldsCount,
        // methodsCount and attributesCount) use 2 bytes each, and each interface uses 2 bytes too.
        var result: Int = 24 + 2 * interfaceCount
        var fieldsCount = 0
        var fieldWriter: FieldWriter = firstField
        while (fieldWriter != null) {
            ++fieldsCount
            result += fieldWriter.computeFieldInforesult()
            fieldWriter = fieldWriter.fv as FieldWriter
        }
        var methodsCount = 0
        var methodWriter: MethodWriter = firstMethod
        while (methodWriter != null) {
            ++methodsCount
            result += methodWriter.computeMethodInforesult()
            methodWriter = methodWriter.mv as MethodWriter
        }

        var attributesCount = 0
        if (innerClasses != null) {
            ++attributesCount
            result += 8 + innerClasses.length
            symbolTable.addConstantUtf8(Constants.INNER_CLASSES)
        }
        if (enclosingClassIndex !== 0) {
            ++attributesCount
            result += 10
            symbolTable.addConstantUtf8(Constants.ENCLOSING_METHOD)
        }
        if (accessFlags and Opcodes.ACC_SYNTHETIC !== 0 && version and 0xFFFF < Opcodes.V1_5) {
            ++attributesCount
            result += 6
            symbolTable.addConstantUtf8(Constants.SYNTHETIC)
        }
        if (signatureIndex !== 0) {
            ++attributesCount
            result += 8
            symbolTable.addConstantUtf8(Constants.SIGNATURE)
        }
        if (sourceFileIndex !== 0) {
            ++attributesCount
            result += 8
            symbolTable.addConstantUtf8(Constants.SOURCE_FILE)
        }
        if (debugExtension != null) {
            ++attributesCount
            result += 6 + debugExtension.length
            symbolTable.addConstantUtf8(Constants.SOURCE_DEBUG_EXTENSION)
        }
        if (accessFlags and Opcodes.ACC_DEPRECATED !== 0) {
            ++attributesCount
            result += 6
            symbolTable.addConstantUtf8(Constants.DEPRECATED)
        }
        if (lastRuntimeVisibleAnnotation != null) {
            ++attributesCount
            result += lastRuntimeVisibleAnnotation.computeAnnotationsresult(
                Constants.RUNTIME_VISIBLE_ANNOTATIONS
            )
        }
        if (lastRuntimeInvisibleAnnotation != null) {
            ++attributesCount
            result += lastRuntimeInvisibleAnnotation.computeAnnotationsresult(
                Constants.RUNTIME_INVISIBLE_ANNOTATIONS
            )
        }
        if (lastRuntimeVisibleTypeAnnotation != null) {
            ++attributesCount
            result += lastRuntimeVisibleTypeAnnotation.computeAnnotationsresult(
                Constants.RUNTIME_VISIBLE_TYPE_ANNOTATIONS
            )
        }
        if (lastRuntimeInvisibleTypeAnnotation != null) {
            ++attributesCount
            result += lastRuntimeInvisibleTypeAnnotation.computeAnnotationsresult(
                Constants.RUNTIME_INVISIBLE_TYPE_ANNOTATIONS
            )
        }
        if (symbolTable.computeBootstrapMethodsresult() > 0) {
            ++attributesCount
            result += symbolTable.computeBootstrapMethodsresult()
        }
        if (moduleWriter != null) {
            attributesCount += moduleWriter.getAttributeCount()
            result += moduleWriter.computeAttributesresult()
        }
        if (nestHostClassIndex !== 0) {
            ++attributesCount
            result += 8
            symbolTable.addConstantUtf8(Constants.NEST_HOST)
        }
        if (nestMemberClasses != null) {
            ++attributesCount
            result += 8 + nestMemberClasses.length
            symbolTable.addConstantUtf8(Constants.NEST_MEMBERS)
        }
        if (permittedSubclasses != null) {
            ++attributesCount
            result += 8 + permittedSubclasses.length
            symbolTable.addConstantUtf8(Constants.PERMITTED_SUBCLASSES)
        }
        var recordComponentCount = 0
        var recordresult = 0
        if (accessFlags and Opcodes.ACC_RECORD !== 0 || firstRecordComponent != null) {
            var recordComponentWriter: RecordComponentWriter = firstRecordComponent
            while (recordComponentWriter != null) {
                ++recordComponentCount
                recordresult += recordComponentWriter.computeRecordComponentInforesult()
                recordComponentWriter = recordComponentWriter.delegate as RecordComponentWriter
            }
            ++attributesCount
            result += 8 + recordresult
            symbolTable.addConstantUtf8(Constants.RECORD)
        }
        if (firstAttribute != null) {
            attributesCount += firstAttribute.getAttributeCount()
            result += firstAttribute.computeAttributesresult(symbolTable)
        }
        // IMPORTANT: this must be the last part of the ClassFile result computation, because the previous
        // statements can add attribute names to the constant pool, thereby changing its result!
        result += symbolTable.getConstantPoolLength()
        val constantPoolCount: Int = symbolTable.getConstantPoolCount()
        if (constantPoolCount > 0xFFFF) {
            throw ClassTooLargeException(symbolTable.getClassName(), constantPoolCount)
        }
        return result
    }

    /**
     * Allocates a ByteVector of the pre-calculated size and write its contents
     */
    private fun writeClassFile(size: Int): ByteArray {
        val result = ByteArrayOutputStream(size)
        result.write(0xCAFEBABE.toInt())

        result.write(version)
        symbolTable.putConstantPool(result)
        val mask = if (version and 0xFFFF < Opcodes.V1_5) Opcodes.ACC_SYNTHETIC else 0
        result.putShort(accessFlags and mask.inv()).putShort(thisClass).putShort(superClass)
        result.putShort(interfaceCount)
        for (i in 0 until interfaceCount) {
            result.putShort(interfaces.get(i))
        }
        result.putShort(fieldsCount)
        fieldWriter = firstField
        while (fieldWriter != null) {
            fieldWriter.putFieldInfo(result)
            fieldWriter = fieldWriter.fv as FieldWriter
        }
        result.putShort(methodsCount)
        var hasFrames = false
        var hasAsmInstructions = false
        methodWriter = firstMethod
        while (methodWriter != null) {
            hasFrames = hasFrames or methodWriter.hasFrames()
            hasAsmInstructions = hasAsmInstructions or methodWriter.hasAsmInstructions()
            methodWriter.putMethodInfo(result)
            methodWriter = methodWriter.mv as MethodWriter
        }

        result.putShort(attributesCount)
        if (innerClasses != null) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.INNER_CLASSES))
                .putInt(innerClasses.length + 2)
                .putShort(numberOfInnerClasses)
                .putByteArray(innerClasses.data, 0, innerClasses.length)
        }
        if (enclosingClassIndex !== 0) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.ENCLOSING_METHOD))
                .putInt(4)
                .putShort(enclosingClassIndex)
                .putShort(enclosingMethodIndex)
        }
        if (accessFlags and Opcodes.ACC_SYNTHETIC !== 0 && version and 0xFFFF < Opcodes.V1_5) {
            result.putShort(symbolTable.addConstantUtf8(Constants.SYNTHETIC)).putInt(0)
        }
        if (signatureIndex !== 0) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.SIGNATURE))
                .putInt(2)
                .putShort(signatureIndex)
        }
        if (sourceFileIndex !== 0) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.SOURCE_FILE))
                .putInt(2)
                .putShort(sourceFileIndex)
        }
        if (debugExtension != null) {
            val length: Int = debugExtension.length
            result
                .putShort(symbolTable.addConstantUtf8(Constants.SOURCE_DEBUG_EXTENSION))
                .putInt(length)
                .putByteArray(debugExtension.data, 0, length)
        }
        if (accessFlags and Opcodes.ACC_DEPRECATED !== 0) {
            result.putShort(symbolTable.addConstantUtf8(Constants.DEPRECATED)).putInt(0)
        }
        AnnotationWriter.putAnnotations(
            symbolTable,
            lastRuntimeVisibleAnnotation,
            lastRuntimeInvisibleAnnotation,
            lastRuntimeVisibleTypeAnnotation,
            lastRuntimeInvisibleTypeAnnotation,
            result
        )
        symbolTable.putBootstrapMethods(result)
        if (moduleWriter != null) {
            moduleWriter.putAttributes(result)
        }
        if (nestHostClassIndex !== 0) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.NEST_HOST))
                .putInt(2)
                .putShort(nestHostClassIndex)
        }
        if (nestMemberClasses != null) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.NEST_MEMBERS))
                .putInt(nestMemberClasses.length + 2)
                .putShort(numberOfNestMemberClasses)
                .putByteArray(nestMemberClasses.data, 0, nestMemberClasses.length)
        }
        if (permittedSubclasses != null) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.PERMITTED_SUBCLASSES))
                .putInt(permittedSubclasses.length + 2)
                .putShort(numberOfPermittedSubclasses)
                .putByteArray(permittedSubclasses.data, 0, permittedSubclasses.length)
        }
        if (accessFlags and Opcodes.ACC_RECORD !== 0 || firstRecordComponent != null) {
            result
                .putShort(symbolTable.addConstantUtf8(Constants.RECORD))
                .putInt(recordSize + 2)
                .putShort(recordComponentCount)
            var recordComponentWriter: RecordComponentWriter = firstRecordComponent
            while (recordComponentWriter != null) {
                recordComponentWriter.putRecordComponentInfo(result)
                recordComponentWriter = recordComponentWriter.delegate as RecordComponentWriter
            }
        }
        if (firstAttribute != null) {
            firstAttribute.putAttributes(symbolTable, result)
        }
        return result.toByteArray()
    }
}