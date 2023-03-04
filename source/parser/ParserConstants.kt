package parser

import lexer.*

data class BuiltinOperator(val name: String, val arity: Int)

data class Import(val name: String, val nativeName: String, val arity: Int)

class FunctionCall(var nameStringId: Int, var precedence: Int, var arity: Int, var maxArity: Int,
                   /** Used to distinguish concrete type constructors from type parameters that are constructors */
                   var isCapitalized: Boolean,
                   var startByte: Int)

const val nodInt = tokInt
const val nodFloat = tokFloat
const val nodBool = tokBool // payload2 = 1 or 0
const val nodString = tokString
const val nodUnderscore = tokUnderscore
const val nodDocComment = tokDocComment

const val nodId = 6                 // payload2 = index in the identifiers table of AST
const val nodFunc = 7               // payload1 = arity, negated if it's an operator. payload2 = index in the identifiers table
const val nodBinding = 8            // payload2 = index in the identifiers table
const val nodTypeId = 9             // payload1 = 1 if it's a type identifier (uppercase), 0 if it's a param. payload2 = index in the identifiers table
const val nodTypeFunc = 10          // payload1 = 1 bit isOperator, 1 bit is a concrete type (uppercase), 30 bits arity
const val nodAnnotation = 11        // index in the annotations table of parser
const val nodFnDefPlaceholder = 12  // payload1 = index in the 'functions' table of AST

// Spans that are still atomic. I.e. they are parsed in one go, they're not re-entrant, and contain 0 or 1 sub-spans
const val nodTypeDecl = 13
const val nodBreak = 14
const val nodStmtAssignment = 15
const val nodLambda = 16
const val nodReturn = 17
const val nodFunctionDef = 18       // payload1 = index in the 'functions' table of AST
const val nodIf = 19            // payload1 = number of clauses
const val nodFor = 20

// True spans
const val nodExpr = 21
const val nodScope = 22             // payload1 = 1 if this scope is the right-hand side of an assignment
const val nodIfClause = 23
const val nodForClause = 24



/** Must be the lowest value of the Span AST node types above */
const val firstSpanASTType = nodScope


/** Order must agree with the tok... constants above */
val nodeNames = arrayOf("Int", "Flo", "Bool", "String", "_", "Comm", "Ident", "Func", "Binding", "Type",
    "TypeCons", "Annot", "Scope", "Expr", "FunDef", "FunDef=>", "Assign", "Return", "TypeDecl", "If", "IfClause",
    "For", "Break")



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
const val errorExpressionHeadFormOperators = "Incorrect number of active operators at end of a head-function subexpression, should be exactly one!"
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
