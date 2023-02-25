package lexer




data class Token(var tType: Int, var startByte: Int, var lenBytes: Int, var payload: Long)

data class TokenLite(var tType: Int, var payload: Long)

enum class FileType {
    executable,
    library,
    tests,
}
