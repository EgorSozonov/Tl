package javanator

import parser.Parser

class Javanator {
    fun javanate(pa: Parser): ArrayList<JavaClass> {
        return ArrayList()
    }

    /**
     * Since all functions in v0.1 take only Ints as params and return an Int,
     * writing descriptors for them is easy.
     */
    fun descriptorForFunc(arity: Int): String {
        return "(" + ("I".repeat(arity)) + ")" + "I"
    }

    fun getEntrypointClassName(): Pair<String, String> {
        return Pair("com/company/Main", "Lcom/company/Main;")
    }

    fun writeConstantPoolBase() {

    }
}