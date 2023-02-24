package lexer


/** Must be the lowest value in the PunctuationToken enum */
const val firstPunctuationTokenType = 12
const val firstCoreFormTokenType = 18


data class Token(var tType: Int, var startByte: Int, var lenBytes: Int, var payload: Long)

data class TokenLite(var tType: Int, var payload: Long)

enum class FileType {
    executable,
    library,
    tests,
}
