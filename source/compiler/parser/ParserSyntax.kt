package compiler.parser
import compiler.lexer.OperatorType
import compiler.lexer.operatorFunctionality


object ParserSyntax {
    fun builtInBindings(): ArrayList<FunctionBinding> {
        val result = ArrayList<FunctionBinding>(20)
        for (of in operatorFunctionality.filter { x -> x.first != "" }) {
            result.add(FunctionBinding(of.first, of.second, of.third))
        }
        return result
    }
}