package parser
import language.operatorDefinitions
import lexer.CHUNKSZ
import utils.*
import java.lang.StringBuilder

/**
 * AST consists of a functions array, a types array a string array, a numerical constants array (?).
 */
class AST {
/**
 * The unique string storage for identifier names
 */
val identifiers = ArrayList<String>(50)

/** 4-int32 elements of [
 *     (nameId: ind in 'identifiers') i32
 *     (arity) u8
 *     (typeId) u24
 *     (nativeNameId: ind in 'identifiers') i32
 *     (bodyId: ind in 'functionBodies') i32
 * ]  */
val functions = FourIntList()

/** 4-int32 elements of [
 *     (type) u5
 *     (lenBytes) i27
 *     (startByte) i32
 *     (payload1) i32
 *     (payload2) i32
 * ]
 * A functionBody starts with the list of identIds of the parameter names.
 * Then come the statements, expressions, scopes of the body.
 * */
val functionBodies = FourIntList()

/** 4-int32 elements of [
 *     (nameId: ind in 'identifiers') i32
 *     (?) i32
 *     (?) i32
 *     (bodyId: ind in 'functionBodies') i32
 * ]  */
val types = FourIntList()


fun getFunc(funcId: Int): FunctionSignature {
    val ind = funcId*4
    val arity = functions.c[ind + 1]
    val typeId = functions.c[ind + 2]
    return FunctionSignature(functions.c[ind], arity,
                             typeId, functions.c[ind + 3])
}

fun appendSpan(nType: Int, lenTokens: Int, startByte: Int, lenBytes: Int) {
    functionBodies.add(nType shl 26 + startByte, lenBytes, 0, lenTokens)
}

fun appendFnDefPlaceholder(funcId: Int) {
    functionBodies.add(nodFnDefPlaceholder shl 26, 0, funcId, 0)
}

fun currNodeType(): Int {
    return functionBodies.c[functionBodies.ind] ushr 26
}

/**
 * Copies the nodes from the functionDef's scratch space to the AST proper.
 * This happens only after the functionDef is complete.
 */
fun storeFreshFunction(freshlyMintedFn: FunctionDef) {
    var src = freshlyMintedFn
    var tgt = functionBodies

    val srcLast = freshlyMintedFn
    val srcIndLast = freshlyMintedFn.totalNodes
    val bodyInd = functionBodies.ind

    if (functionBodies.ind + freshlyMintedFn.currInd >= functionBodies.cap) {
        val newCapacity = Math.max(functionBodies.ind + freshlyMintedFn.currInd, 2*functionBodies.cap)
        val newArr = IntArray(newCapacity)
        functionBodies.c.copyInto(newArr, 0, 0, functionBodies.ind)
        functionBodies.c = newArr
        functionBodies.cap = newCapacity
    }

    // setting the token length of the function body because it's only known now
    freshlyMintedFn.nodes[3] = freshlyMintedFn.totalNodes - 1

    freshlyMintedFn.nodes.copyInto(functionBodies.c, bodyInd, 0, freshlyMintedFn.currInd)
    functions.c[freshlyMintedFn.funcId*4 + 3] = bodyInd
}

fun getString(stringId: Int): String {
    return identifiers[stringId]
}

fun getPayload(): Long {
    return (functionBodies.c[functionBodies.ind + 2].toLong() shl 32) +
            (functionBodies.c[functionBodies.ind + 3].toLong() and LOWER32BITS)
}


companion object {
    /**
     * Equality comparison for ASTs.
     */
    fun diffInd(a: AST, b: AST): Int? {
        val sizeA = a.functionBodies.ind
        val sizeB = b.functionBodies.ind

        var nodeInd = 0
        if (!equalityFuncs(a, b)) return 0

        val len = Math.min(sizeA, sizeB)
        for (i in 0 until len) {
            if (a.functionBodies.c[i] != b.functionBodies.c[i]) {
                return nodeInd
            }
            nodeInd++
        }
        if (sizeA != sizeB) return len

        if (a.identifiers.size != b.identifiers.size) {
            return 0
        }
        for (i in 0 until a.identifiers.size) {
            if (a.identifiers[i] != b.identifiers[i]) {
                return 0
            }
        }

        return null
    }

    private fun equalityFuncs(a: AST, b: AST): Boolean {
        if (a.functions.ind != b.functions.ind) return false
        for (i in 0 until a.functions.ind) {
            if (a.functions.c[i] != b.functions.c[i]) {
                return false
            }
        }
        return true
    }

    /**
     * Pretty printer function for debugging purposes
     */
    fun printSideBySide(a: AST, b: AST, indDiff: Int, result: StringBuilder): String {
        print("Diff: ")
        printNode(a.functionBodies, indDiff, result)
        println()

        val len = Math.min(a.functionBodies.ind, b.functionBodies.ind)
        for (i in 0 until len step 4) {
            result.append((i/4).toString() + " ")
            printNode(currA, i, result)
            result.append(" | ")
            printNode(currB, i, result)
            result.appendLine("")
        }
        for (i in len until lenA step 4) {
            printNode(currA, i, result)
            result.appendLine(" | ")
        }
        for (i in len until lenB step 4) {
            result.append(" | ")
            printNode(currB, i, result)
            result.appendLine("")
        }

        return result.toString()
    }

    /**
     * Pretty printer function for debugging purposes
     */
    fun print(a: AST, result: StringBuilder): String {
        for (i in 0 until a.functionBodies.ind step 4) {
            result.append((i/4 + 1).toString() + " ")
            printNode(a.functionBodies, i, result)
            result.appendLine("")
        }

        return result.toString()
    }

    private fun printNode(chunk: FourIntList, ind: Int, wr: StringBuilder) {
        val startByte = chunk.c[ind + 1]
        val lenBytes = chunk.c[ind] and LOWER26BITS
        val typeBits = chunk.c[ind] ushr 26

        val nodeName = nodeNames[typeBits]
        if (typeBits < firstSpanASTType) {
            if (typeBits == nodFunc) {
                val funcId: Int = chunk.c[ind + 3]
                val arity = chunk.c[ind + 2]
                if (arity >= 0) {
                    wr.append("funcId $funcId $arity-ary [${startByte} ${lenBytes}]")
                } else {
                    wr.append("${operatorDefinitions[funcId].name} ${-arity}-ary [${startByte} ${lenBytes}]")
                }

            } else if (typeBits == nodTypeFunc) {
                val payload1 = chunk.c[ind + 2]
                val payload2 = chunk.c[ind + 3]
                val payl1Str = if (payload1 ushr 31 > 0) {"operator "} else {""} +
                        (if (payload1 and THIRTYFIRSTBIT > 0) {"concreteType "} else {"typeVar "}) +
                        "arity ${payload1 and LOWER26BITS}"
                wr.append("$nodeName $payload2 [${startByte} ${lenBytes}] $payl1Str")
            } else if (typeBits == nodFloat) {
                val payload: Double = Double.fromBits(
                    (chunk.c[ind + 2].toLong() shl 32) + chunk.c[ind + 3].toLong()
                )
                wr.append("$nodeName $payload [${startByte} ${lenBytes}]")
            } else {
                val payload: Long = (chunk.c[ind + 2].toLong() shl 32) + (chunk.c[ind + 3].toLong() and LOWER32BITS)
                wr.append("$nodeName $payload [${startByte} ${lenBytes}]")
            }
        } else {
            val lenNodes = chunk.c[ind + 3]
            wr.append("$nodeName len=$lenNodes [${startByte} ${lenBytes}] ")
        }
    }
}


}
