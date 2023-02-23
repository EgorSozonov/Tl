package utils

import lexer.CHUNKSZ
import lexer.RegularToken
import parser.ASTChunk
import parser.RegularAST
import parser.SpanAST
const val INIT_LIST_SIZE = 1000
class FourIntList {
    private var cap = INIT_LIST_SIZE
    var ind = 0
    var c = IntArray(INIT_LIST_SIZE)

    fun add(i1: Int, i2: Int, i3: Int, i4: Int) {
        ind++
        if (ind == cap) {
            val newArray = IntArray(2*cap)
            c.copyInto(newArray, 0, cap)
            cap *= 2
        }
        c[ind    ] = i1
        c[ind + 1] = i2
        c[ind + 2] = i3
        c[ind + 3] = i4
    }
}