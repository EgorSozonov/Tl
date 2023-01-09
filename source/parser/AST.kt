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
val functions = FourIntStorage()

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
val functionBodies = FourIntStorage()

/** 4-int32 elements of [
 *     (nameId: ind in 'identifiers') i32
 *     (?) i32
 *     (?) i32
 *     (bodyId: ind in 'functionBodies') i32
 * ]  */
val types = FourIntStorage()


fun getFunc(funcId: Int): FunctionSignature {
    functions.seek(funcId)
    val arity = functions.currChunk.nodes[functions.currInd + 1]
    val typeId = functions.currChunk.nodes[functions.currInd + 2]
    return FunctionSignature(functions.currChunk.nodes[functions.currInd], arity,
                             typeId, functions.currChunk.nodes[functions.currInd + 3])
}

fun appendSpan(nType: SpanAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
    functionBodies.ensureSpaceForNode()
    functionBodies.currChunk.nodes[functionBodies.nextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
    functionBodies.currChunk.nodes[functionBodies.nextInd + 1] = startByte
    functionBodies.currChunk.nodes[functionBodies.nextInd + 2] = 0
    functionBodies.currChunk.nodes[functionBodies.nextInd + 3] = lenTokens
    functionBodies.bump()
}

fun appendFnDefPlaceholder(funcId: Int) {
    functionBodies.ensureSpaceForNode()
    functionBodies.currChunk.nodes[functionBodies.nextInd    ] = (SpanAST.fnDefPlaceholder.internalVal.toInt() shl 27)
    functionBodies.currChunk.nodes[functionBodies.nextInd + 2] = funcId
    functionBodies.bump()
}



fun currNodeType(): Int {
    return functionBodies.currChunk.nodes[functionBodies.currInd] ushr 27
}

/**
 * Copies the nodes from the functionDef's scratch space to the AST proper.
 * This happens only after the functionDef is complete.
 */
fun storeFreshFunction(freshlyMintedFn: FunctionDef) {
    var src = freshlyMintedFn.firstChunk
    var tgt = functionBodies.currChunk

    val srcLast = freshlyMintedFn.currChunk
    val srcIndLast = freshlyMintedFn.nextInd
    val bodyInd = functionBodies.totalNodes

    // setting the token length of the function body because it's only known now
    freshlyMintedFn.firstChunk.nodes[3] = freshlyMintedFn.totalNodes - 1

    while (src != srcLast) {
        val spaceInTgt = (CHUNKSZ - functionBodies.nextInd)
        if (SCRATCHSZ < spaceInTgt) {
            src.nodes.copyInto(tgt.nodes, functionBodies.nextInd, 0)
            functionBodies.nextInd += SCRATCHSZ
        } else {
            val toCopy = SCRATCHSZ.coerceAtMost(spaceInTgt)
            src.nodes.copyInto(tgt.nodes, functionBodies.nextInd, 0, toCopy)
            tgt = tgt.next!!
            src.nodes.copyInto(tgt.nodes, 0, toCopy)
            functionBodies.nextInd = SCRATCHSZ - toCopy
        }
        src = src.next!!
    }

    val spaceInTgt = (CHUNKSZ - functionBodies.nextInd)
    if (srcIndLast < spaceInTgt) {
        src.nodes.copyInto(tgt.nodes, functionBodies.nextInd, 0)
        functionBodies.nextInd += srcIndLast
    } else {
        val toCopy = srcIndLast.coerceAtMost(spaceInTgt)
        src.nodes.copyInto(tgt.nodes, functionBodies.nextInd, 0, toCopy)
        functionBodies.currChunk = functionBodies.currChunk.next!!
        src.nodes.copyInto(tgt.nodes, 0, toCopy)
        functionBodies.nextInd = srcIndLast - toCopy
    }
    functionBodies.totalNodes += freshlyMintedFn.totalNodes

    functions.setFourth(freshlyMintedFn.funcId, bodyInd)
}

fun getString(stringId: Int): String {
    return identifiers[stringId]
}

fun getPayload(): Long {
    return (functionBodies.currChunk.nodes[functionBodies.currInd + 2].toLong() shl 32) +
            (functionBodies.currChunk.nodes[functionBodies.currInd + 3].toLong() and LOWER32BITS)
}


init {
}

companion object {
    /**
     * Equality comparison for ASTs.
     */
    fun diffInd(a: AST, b: AST): Int? {
        if (a.functionBodies.totalNodes != b.functionBodies.totalNodes
            || a.functions.totalNodes != b.functions.totalNodes) {
            return 0
        }
        var nodeInd = 0
        if (!equalityFuncs(a, b)) return 0
        var currA: ASTChunk? = a.functionBodies.firstChunk
        var currB: ASTChunk? = b.functionBodies.firstChunk
        while (currA != null) {
            if (currB == null) {
                return 0
            }
            val len = if (currA == a.functionBodies.currChunk) { a.functionBodies.nextInd } else { CHUNKSZ }
            for (i in 0 until len) {
                if (currA.nodes[i] != currB.nodes[i]) {
                    return nodeInd
                }
                nodeInd++
            }
            currA = currA.next
            currB = currB.next
        }
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
        var currA: ASTChunk? = a.functions.firstChunk
        var currB: ASTChunk? = b.functions.firstChunk
        while (currA != null) {
            if (currB == null) {
                return false
            }
            val len = if (currA == a.functions.currChunk) { a.functionBodies.nextInd } else { CHUNKSZ }
            for (i in 0 until len) {
                if (currA.nodes[i] != currB.nodes[i]) {
                    return false
                }
            }
            currA = currA.next
            currB = currB.next
        }
        return true
    }

    /**
     * Pretty printer function for debugging purposes
     */
    fun printSideBySide(a: AST, b: AST, indDiff: Int, result: StringBuilder): String {
        var currA: ASTChunk? = a.functionBodies.firstChunk
        var currB: ASTChunk? = b.functionBodies.firstChunk
        a.functionBodies.seek(indDiff)
        print("Diff: ")
        printNode(a.functionBodies.currChunk, a.functionBodies.currInd, result)
        println()
        a.functionBodies.seek(0)
        while (true) {
            if (currA != null) {
                if (currB != null) {
                    val lenA = if (currA == a.functionBodies.currChunk) { a.functionBodies.nextInd } else { CHUNKSZ }
                    val lenB = if (currB == b.functionBodies.currChunk) { b.functionBodies.nextInd } else { CHUNKSZ }
                    val len = lenA.coerceAtMost(lenB)
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
                    currB = currB.next
                } else {
                    val len = if (currA == a.functionBodies.currChunk) { a.functionBodies.nextInd } else { CHUNKSZ }
                    for (i in 0 until len step 4) {
                        printNode(currA, i, result)
                        result.appendLine(" | ")
                    }
                }
                currA = currA.next
            } else if (currB != null) {
                val len = if (currB == b.functionBodies.currChunk) { b.functionBodies.nextInd } else { CHUNKSZ }
                for (i in 0 until len step 4) {
                    result.append(" | ")
                    printNode(currB, i, result)
                    result.appendLine("")
                }
                currB = currB.next
            } else {
                break
            }


        }
        return result.toString()
    }

    /**
     * Pretty printer function for debugging purposes
     */
    fun print(a: AST, result: StringBuilder): String {
        var currA: ASTChunk? = a.functionBodies.firstChunk
        while (true) {
            if (currA != null) {
                val lenA = if (currA == a.functionBodies.currChunk) { a.functionBodies.nextInd } else { CHUNKSZ }
                for (i in 0 until lenA step 4) {
                    result.append((i/4 + 1).toString() + " ")
                    printNode(currA, i, result)
                    result.appendLine("")
                }
                currA = currA.next
            } else {
                break
            }
        }
        return result.toString()
    }

    private fun printNode(chunk: ASTChunk, ind: Int, wr: StringBuilder) {
        val startByte = chunk.nodes[ind + 1]
        val lenBytes = chunk.nodes[ind] and LOWER27BITS
        val typeBits = (chunk.nodes[ind] ushr 27).toByte()
        if (typeBits < firstSpanASTType) {
            val regType = RegularAST.values().firstOrNull { it.internalVal == typeBits }
            if (regType == RegularAST.idFunc) {
                val funcId: Int = chunk.nodes[ind + 3]
                val arity = chunk.nodes[ind + 2]
                if (arity >= 0) {
                    wr.append("funcId $funcId $arity-ary [${startByte} ${lenBytes}]")
                } else {
                    wr.append("${operatorDefinitions[funcId].name} ${-arity}-ary [${startByte} ${lenBytes}]")
                }

            } else if (regType == RegularAST.typeFunc) {
                val payload1 = chunk.nodes[ind + 2]
                val payload2 = chunk.nodes[ind + 3]
                val payl1Str = if (payload1 ushr 31 > 0) {"operator "} else {""} +
                        (if (payload1 and THIRTYFIRSTBIT > 0) {"concreteType "} else {"typeVar "}) +
                        "arity ${payload1 and LOWER27BITS}"
                wr.append("$regType $payload2 [${startByte} ${lenBytes}] $payl1Str")
            } else if (regType == RegularAST.litFloat) {
                val payload: Double = Double.fromBits(
                    (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                )
                wr.append("$regType $payload [${startByte} ${lenBytes}]")
            } else {
                val payload: Long = (chunk.nodes[ind + 2].toLong() shl 32) + (chunk.nodes[ind + 3].toLong() and LOWER32BITS)
                wr.append("$regType $payload [${startByte} ${lenBytes}]")
            }
        } else {
            val spanType = SpanAST.values().firstOrNull { it.internalVal == typeBits }
            val lenNodes = chunk.nodes[ind + 3]
            wr.append("$spanType len=$lenNodes [${startByte} ${lenBytes}] ")
        }
    }
}


}
