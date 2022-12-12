package parser

import lexer.CHUNKSZ
import lexer.LOWER27BITS
import lexer.LOWER32BITS
import lexer.operatorFunctionality
import java.lang.StringBuilder


/**
 * AST consists of a functions array, a types array a string array, a numerical constants array (?).
 */
class AST {
    private val functions = ASTChunk()
    var currChunk: ASTChunk                                    // Last array of tokens
        private set
    private val functionsIndex = ArrayList<Int>(50)
    private val bindings = ArrayList<String>(50)
    var funcNextInd: Int                                             // Next ind inside the current token array
        private set
    var funcTotalNodes: Int
        private set
    val indFirstFunction: Int

    private val unknownFunctions: HashMap<String, ArrayList<UnknownFunLocation>>

    /**
     * Finds the top-level punctuation opener by its index, and sets its node length.
     * Called when the parsing of punctuation is finished.
     */
    fun setPunctuationLength(nodeInd: Int) {
        var curr = functions
        var j = nodeInd * 4
        while (j >= CHUNKSZ) {
            curr = curr.next!!
            j -= CHUNKSZ
        }

        curr.nodes[j + 3] = funcTotalNodes - nodeInd - 1
    }


    /** Append a regular AST node */
    fun appendNode(tType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[funcNextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[funcNextInd + 1] = startByte
        currChunk.nodes[funcNextInd + 2] = payload1
        currChunk.nodes[funcNextInd + 3] = payload2
        bump()
    }


    fun appendNode(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[funcNextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[funcNextInd + 1] = startByte
        currChunk.nodes[funcNextInd + 2] = 0
        currChunk.nodes[funcNextInd + 3] = lenTokens
        bump()
    }


    fun appendNode(nType: PunctuationAST, lenTokens: Int, startByte: Int, lenBytes: Int) {
        ensureSpaceForNode()
        currChunk.nodes[funcNextInd    ] = (nType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[funcNextInd + 1] = startByte
        currChunk.nodes[funcNextInd + 2] = 0
        currChunk.nodes[funcNextInd + 3] = lenTokens
        bump()
    }

    /** Append a punctuation AST node */
    fun appendNode(startByte: Int, lenBytes: Int, tType: PunctuationAST) {
        ensureSpaceForNode()
        currChunk.nodes[funcNextInd    ] = (tType.internalVal.toInt() shl 27) + lenBytes
        currChunk.nodes[funcNextInd + 1] = startByte
        bump()
    }

    /** The programmatic/builder method for allocating a function binding */
    fun buildBinding(binding: Binding): Parser {
        this.bindings.add(binding)
        return this
    }

    /** The programmatic/builder method for allocating a function binding */
    fun buildFBinding(fBinding: FunctionBinding): Parser {
        this.functionBindings.add(fBinding)
        return this
    }

    /** The programmatic/builder method for inserting all non-builtin function bindings into top scope */
    fun buildInsertBindingsIntoScope(): Parser {
        if (currFnDef.subscopes.isEmpty()) {
            currFnDef.subscopes.add(LexicalScope())
        }
        val topScope = currFnDef.subscopes.peek()
        for (id in 0 until this.bindings.size) {
            val name = this.bindings[id].name
            if (!topScope.bindings.containsKey(name)) {
                topScope.bindings[name]  = id
            }
        }
        for (id in 0 until this.functionBindings.size) {
            val name = this.functionBindings[id].name
            if (topScope.functions.containsKey(name)) {
                topScope.functions[name]!!.add(id)
            } else {
                topScope.functions[name] = arrayListOf(id)
            }
        }
        return this
    }


    fun buildNode(nType: RegularAST, payload1: Int, payload2: Int, startByte: Int, lenBytes: Int): Parser {
        appendNode(nType, payload1, payload2, startByte, lenBytes)
        return this
    }


    fun buildNode(payload: Double, startByte: Int, lenBytes: Int): Parser {
        val payloadAsLong = payload.toBits()
        appendNode(RegularAST.litFloat, (payloadAsLong ushr 32).toInt(), (payloadAsLong and LOWER32BITS).toInt(), startByte, lenBytes)
        return this
    }


    fun buildNode(nType: FrameAST, lenTokens: Int, startByte: Int, lenBytes: Int): Parser {
        appendNode(nType, lenTokens, startByte, lenBytes)
        return this
    }


    fun buildNode(nType: PunctuationAST, lenTokens: Int, startByte: Int, lenBytes: Int): Parser {
        appendNode(nType, lenTokens, startByte, lenBytes)
        return this
    }


    fun buildUnknownFunc(name: String, arity: Int, startByte: Int) {
        if (unknownFunctions.containsKey(name)) {
            unknownFunctions[name]!!.add(UnknownFunLocation(funcTotalNodes, arity))
        } else {
            unknownFunctions[name] = arrayListOf(UnknownFunLocation(funcTotalNodes, arity))
        }
        appendNode(RegularAST.idFunc, 0, -1, startByte, name.length)
    }


    private fun bump() {
        funcNextInd += 4
        funcTotalNodes++
    }


    private fun ensureSpaceForNode() {
        if (funcNextInd < (CHUNKSZ - 4)) return

        val newChunk = ASTChunk()
        currChunk.next = newChunk
        currChunk = newChunk
        funcNextInd = 0
    }


    init {
        currChunk = functions
        funcNextInd = 0
        funcTotalNodes = 0
        functionBindings = builtInBindings()
        indFirstFunction = functionBindings.size
    }
    
    companion object {
        fun builtInBindings(): ArrayList<FunctionBinding> {
            val result = ArrayList<FunctionBinding>(20)
            for (of in operatorFunctionality.filter { x -> x.first != "" }) {
                result.add(FunctionBinding(of.first, of.second, of.third))
            }
            // This must always be the first after the built-ins
            result.add(FunctionBinding("__entrypoint", funcPrecedence, 0))

            return result
        }

        /**
         * Equality comparison for ASTs.
         */
        fun equality(a: AST, b: AST): Boolean {
            if (a.funcTotalNodes != b.funcTotalNodes) {
                return false
            }
            var currA: ASTChunk? = a.functions
            var currB: ASTChunk? = b.functions
            while (currA != null) {
                if (currB == null) {
                    return false
                }
                val len = if (currA == a.currChunk) { a.funcNextInd } else { CHUNKSZ }
                for (i in 0 until len) {
                    if (currA.nodes[i] != currB.nodes[i]) {
                        return false
                    }
                }
                currA = currA.next
                currB = currB.next
            }
            if (a.unknownFunctions.size != b.unknownFunctions.size) {
                return false
            }
            for ((key, v) in a.unknownFunctions.entries) {
                if (!b.unknownFunctions.containsKey(key) || b.unknownFunctions[key]!!.size != v.size) {
                    return false
                }
                for (j in 0 until v.size) {
                    if (v[j] != b.unknownFunctions[key]!![j]) {
                        return false
                    }
                }
            }

            if (a.bindings.size != b.bindings.size) {
                return false
            }
            for (i in 0 until a.bindings.size) {
                if (a.bindings[i] != b.bindings[i]) {
                    return false
                }
            }

            return true
        }


        /**
         * Pretty printer function for debugging purposes
         */
        fun printSideBySide(a: AST, b: AST, result: StringBuilder): String {
            val result = StringBuilder()
            result.appendLine("Result | Expected")
            result.appendLine("${if (a.wasError) {a.errMsg} else { "OK" }} | ${if (b.wasError) {b.errMsg} else { "OK" }}")
            result.appendLine("astType [startByte lenBytes] (payload/lenTokens)")
            var currA: ASTChunk? = a.functions
            var currB: ASTChunk? = b.functions
            while (true) {
                if (currA != null) {
                    if (currB != null) {
                        val lenA = if (currA == a.currChunk) { a.funcNextInd } else { CHUNKSZ }
                        val lenB = if (currB == b.currChunk) { b.funcNextInd } else { CHUNKSZ }
                        val len = lenA.coerceAtMost(lenB)
                        for (i in 0 until len step 4) {
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
                        val len = if (currA == a.currChunk) { a.funcNextInd } else { CHUNKSZ }
                        for (i in 0 until len step 4) {
                            printNode(currA, i, result)
                            result.appendLine(" | ")
                        }
                    }
                    currA = currA.next
                } else if (currB != null) {
                    val len = if (currB == b.currChunk) { b.funcNextInd } else { CHUNKSZ }
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


        private fun printNode(chunk: ASTChunk, ind: Int, wr: StringBuilder) {
            val startByte = chunk.nodes[ind + 1]
            val lenBytes = chunk.nodes[ind] and LOWER27BITS
            val typeBits = (chunk.nodes[ind] ushr 27).toByte()
            if (typeBits <= 6) {
                val regType = RegularAST.values().firstOrNull { it.internalVal == typeBits }
                if (regType != RegularAST.litFloat) {
                    val payload: Long = (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                    wr.append("$regType [${startByte} ${lenBytes}] $payload")
                } else {
                    val payload: Double = Double.fromBits(
                        (chunk.nodes[ind + 2].toLong() shl 32) + chunk.nodes[ind + 3].toLong()
                    )
                    wr.append("$regType [${startByte} ${lenBytes}] $payload")
                }
            } else if (typeBits <= 13) {
                val punctType = FrameAST.values().firstOrNull { it.internalVal == typeBits }
                val lenTokens = chunk.nodes[ind + 3]
                wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
            } else {
                val punctType = PunctuationAST.values().firstOrNull { it.internalVal == typeBits }
                val lenTokens = chunk.nodes[ind + 3]
                wr.append("$punctType [${startByte} ${lenBytes}] $lenTokens")
            }
        }
    }
}
