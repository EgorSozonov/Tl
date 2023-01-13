package parser


const val errorImportsNonUnique            = "Import names must be unique!"
const val errorLengthOverflow              = "AST nodes length overflow"
const val errorUnexpectedToken             = "Unexpected token"
const val errorInconsistentSpan            = "Inconsistent span length / structure of token scopes!"
const val errorCoreFormTooShort            = "Core syntax form too short"
const val errorCoreFormUnexpected          = "Unexpected core form"
const val errorCoreFormAssignment          = "A core form may not contain any assignments!"
const val errorCoreFormInappropriate       = "Inappropriate reserved word!"
const val errorIfLeft                      = "A left-hand clause in an if can only contain variables, boolean literals and expressions!"
const val errorIfRight                     = "A right-hand clause in an if can only contain expressions, scopes and some core forms!"
const val errorIfEmpty                     = "Empty \"if\" expression"
const val errorIfMalformed                 = "Malformed \"if\" expression, should look like (? pred -> trueCase : elseCase)"
const val errorFnNameAndParams             = "Function definition must start with more than one unique words: its name and parameters!"
const val errorFnMissingBody               = "Function definition must contain a body which must be a Scope immediately following its parameter list!"
const val errorDuplicateFunction           = "Duplicate function declaration: a function with same name and arity already exists in this scope!"
const val errorExpressionInfixNotSecond    = "An infix expression must have the infix operator in second position (not counting possible prefix operators)!"
const val errorExpressionError             = "Cannot parse expression!"
const val errorExpressionCannotContain     = "Expressions cannot contain scopes or statements!"
const val errorExpressionFunctionless      = "Functionless expression!"
const val errorExpressionHeadFormTooMany   = "Incorrect number operators in a head-function subexpression, should be just one!"
const val errorTypeDeclCannotContain       = "Type declarations may only contain types (like A), type params (like a), type constructors (like .List) and parentheses!"
const val errorTypeDeclError               = "Cannot parse type declaration!"
const val errorUnknownType                 = "Unknown type"
const val errorUnknownTypeFunction         = "Unknown type constructor"
const val errorOperatorWrongArity          = "Wrong number of arguments for operator!"
const val errorUnknownBinding              = "Unknown binding!"
const val errorUnknownFunction             = "Unknown function!"
const val errorIncorrectPrefixSequence     = "A prefix atom-expression must look like '!!a', that is, only prefix operators followed by one ident or literal"
const val errorOperatorUsedInappropriately = "Operator used in an inappropriate location!"
const val errorAssignment                  = "Cannot parse assignment, it must look like [freshIdentifier] = [expression]"
const val errorAssignmentShadowing         = "Assignment error: existing identifier is being shadowed"
const val errorScope                       = "A scope may consist only of expressions, assignments, function definitions and other scopes!"
const val errorLoopBreakOutside            = "The break keyword cannot be used outside a loop scope!"




val allowedInExpr = Parser.toAllowedSpans(false, false, false, false, true, true)
val allowedInIf = Parser.toAllowedSpans(false, false, false, true, false, true)
val allowedInIfRight = Parser.toAllowedSpans(true, false, false, true, true, true)