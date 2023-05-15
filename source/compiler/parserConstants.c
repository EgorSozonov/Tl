const char errorBareAtom[]                    = "Malformed token stream (atoms and parentheses must not be bare)";
const char errorImportsNonUnique[]            = "Import names must be unique!";
const char errorLengthOverflow[]              = "AST nodes length overflow";
const char errorUnexpectedToken[]             = "Unexpected token";
const char errorInconsistentSpan[]            = "Inconsistent span length / structure of token scopes!";
const char errorCoreFormTooShort[]            = "Core syntax form too short";
const char errorCoreFormUnexpected[]          = "Unexpected core form";
const char errorCoreFormAssignment[]          = "A core form may not contain any assignments!";
const char errorCoreFormInappropriate[]       = "Inappropriate reserved word!";
const char errorIfLeft[]                      = "A left-hand clause in an if can only contain variables, boolean literals and expressions!";
const char errorIfRight[]                     = "A right-hand clause in an if can only contain atoms, expressions, scopes and some core forms!";
const char errorIfEmpty[]                     = "Empty `if` expression";
const char errorIfMalformed[]                 = "Malformed `if` expression, should look like (if pred. trueCase ::elseCase)";
const char errorFnNameAndParams[]             = "Function signature must look like this: `fn fnName ReturnType(x Type1 y Type2) (. body... )`";
const char errorFnMissingBody[]               = "Function definition must contain a body which must be a Scope immediately following its parameter list!";
const char errorDuplicateFunction[]           = "Duplicate function declaration: a function with same name and arity already exists in this scope!";
const char errorExpressionInfixNotSecond[]    = "An infix expression must have the infix operator in second position (not counting possible prefix operators)!";
const char errorExpressionError[]             = "Cannot parse expression!";
const char errorExpressionCannotContain[]     = "Expressions cannot contain scopes or statements!";
const char errorExpressionFunctionless[]      = "Functionless expression!";
const char errorExpressionHeadFormOperators[] = "Incorrect number of active operators at end of a head-function subexpression, should be exactly one!";
const char errorTypeDeclCannotContain[]       = "Type declarations may only contain types (like A), type params (like a), type constructors (like .List) and parentheses!";
const char errorTypeDeclError[]               = "Cannot parse type declaration!";
const char errorUnknownType[]                 = "Unknown type";
const char errorUnknownTypeFunction[]         = "Unknown type constructor";
const char errorOperatorWrongArity[]          = "Wrong number of arguments for operator!";
const char errorUnknownBinding[]              = "Unknown binding!";
const char errorUnknownFunction[]             = "Unknown function!";
const char errorIncorrectPrefixSequence[]     = "A prefix atom-expression must look like `!!a`, that is, only prefix operators followed by one ident or literal";
const char errorOperatorUsedInappropriately[] = "Operator used in an inappropriate location!";
const char errorAssignment[]                  = "Cannot parse assignment, it must look like `freshIdentifier` = `expression`";
const char errorAssignmentShadowing[]         = "Assignment error: existing identifier is being shadowed";
const char errorReturn[]                      = "Cannot parse return statement, it must look like `return ` {expression}";
const char errorScope[]                       = "A scope may consist only of expressions, assignments, function definitions and other scopes!";
const char errorLoopBreakOutside[]            = "The break keyword can only be used outside a loop scope!";

// temporary, delete it when the parser is finished
const char errorTemp[]                        = "Not implemented yet";
