package parser
import lexer.operatorFunctionality


object ParserSyntax {
    fun builtInBindings(): ArrayList<FunctionBinding> {
        val result = ArrayList<FunctionBinding>(20)
        for (of in operatorFunctionality.filter { x -> x.first != "" }) {
            result.add(FunctionBinding(of.first, of.second, of.third))
        }
        // This must always be the first after the built-ins
        result.add(FunctionBinding("__entrypoint", funcPrecedence, 0))

        return result
    }
}