package codegen

object CodegenConstants {
    // The ClassFile attribute names, in the order they are defined in
    // https://docs.oracle.com/javase/specs/jvms/se11/html/jvms-4.html#jvms-4.7-300.
    const val constantValue = "ConstantValue"
    const val code = "Code"
    const val stackMapTable = "StackMapTable"
    const val exceptions = "Exceptions"
    const val innerClasses = "InnerClasses"
    const val enclosingMethod = "EnclosingMethod"
    const val synthetic = "Synthetic"
    const val signature = "Signature"
    const val sourceFile = "SourceFile"
    const val sourceDebugExtension = "SourceDebugExtension"
    const val lineNumberTable = "LineNumberTable"
    const val localVariableTable = "LocalVariableTable"
    const val localVariableTypeTable = "LocalVariableTypeTable"
    const val deprecated = "Deprecated"
    const val runtimeVisibleAnnotations = "RuntimeVisibleAnnotations"
    const val runtimeInvisibleAnnotations = "RuntimeInvisibleAnnotations"
    const val runtimeVisibleParameterAnnotations = "RuntimeVisibleParameterAnnotations"
    const val runtimeInvisibleParameterAnnotations = "RuntimeInvisibleParameterAnnotations"
    const val runtimeVisibleTypeAnnotations = "RuntimeVisibleTypeAnnotations"
    const val runtimeInvisibleTypeAnnotations = "RuntimeInvisibleTypeAnnotations"
    const val annotationDefault = "AnnotationDefault"
    const val bootstrapMethods = "BootstrapMethods"
    const val methodParameters = "MethodParameters"
    const val module = "Module"
    const val modulePackages = "ModulePackages"
    const val moduleMainClass = "ModuleMainClass"
    const val nestHost = "NestHost"
    const val nestMembers = "NestMembers"
    const val permittedSubclasses = "PermittedSubclasses"
    const val record = "Record"

    // constant tags
//    CONSTANT_Utf8 	1
//    CONSTANT_Integer 	3
//    CONSTANT_Float 	4
//    CONSTANT_Long 	5
//    CONSTANT_Double 	6
//    CONSTANT_Class 	7
//    CONSTANT_String 	8
//    CONSTANT_Fieldref 	9
//    CONSTANT_Methodref 	10
//    CONSTANT_InterfaceMethodref 	11
//    CONSTANT_NameAndType 	12
//    CONSTANT_MethodHandle 	15
//    CONSTANT_MethodType 	16
//    CONSTANT_InvokeDynamic 	18


//    Class File Versions
//    JDK Version 	Bytecode Version
//    Java 1.0 	45.0
//    Java 1.1 	45.3
//    Java 1.2 	46.0
//    Java 1.3 	47.0
//    Java 1.4 	48.0
//    Java 5 	49.0
//    Java 6 	50.0
//    Java 7 	51.0
//    Java 8 	52.0
//    Java 9 	53.0
//    Java 10 	54.0
//    Java 11 	55.0
//    Java 12 	56.0
//    Java 13 	57.0
//    Java 14 	58.0
//    Java 15 	59.0
//    Java 16 	60.0
//    Java 17 	61.0
//    Java 18 	62.0
//    Java 19 	63.0
//    Java 20 	64.0
}