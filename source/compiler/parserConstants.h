extern const char errorBareAtom[];
extern const char errorImportsNonUnique[];
extern const char errorLengthOverflow[];
extern const char errorUnexpectedToken[];
extern const char errorInconsistentSpan[];
extern const char errorCoreFormTooShort[];
extern const char errorCoreFormUnexpected[];
extern const char errorCoreFormAssignment[];
extern const char errorCoreFormInappropriate[];
extern const char errorIfLeft[];
extern const char errorIfRight[];
extern const char errorIfEmpty[];
extern const char errorIfMalformed[];
extern const char errorFnNameAndParams[];
extern const char errorFnMissingBody[];
extern const char errorDuplicateFunction[];
extern const char errorExpressionInfixNotSecond[];
extern const char errorExpressionError[];
extern const char errorExpressionCannotContain[];
extern const char errorExpressionFunctionless[];
extern const char errorExpressionHeadFormOperators[];
extern const char errorTypeDeclCannotContain[];
extern const char errorTypeDeclError[];
extern const char errorUnknownType[];
extern const char errorUnknownTypeFunction[];
extern const char errorOperatorWrongArity[];
extern const char errorUnknownBinding[];
extern const char errorUnknownFunction[];
extern const char errorIncorrectPrefixSequence[];
extern const char errorOperatorUsedInappropriately[];
extern const char errorAssignment[];
extern const char errorAssignmentShadowing[];
extern const char errorReturn[];
extern const char errorScope[];
extern const char errorLoopBreakOutside[];
extern const char errorTemp[];


#define nodInt          0
#define nodFloat        1
#define nodBool         2      // payload2 = value (1 or 0)
#define nodString       3
#define nodUnderscore   4
#define nodDocComment   5

#define nodId           6      // payload1 = index of binding, payload2 = index of name
#define nodCall         7      // payload1 = index of binding, payload2 = arity
#define nodBinding      8      // payload1 = index of binding
#define nodTypeId       9      // payload1 = index of binding
#define nodAnnotation  10      // "@annotation"
#define nodAnd         11
#define nodOr          12


// Punctuation (inner node)
#define nodScope       13       // (: This is resumable but trivially so, that's why it's not grouped with the others
#define nodExpr        14
#define nodAccessor    15
#define nodAssignment  16       // payload1 = 1 if mutable assignment, 0 if immutable    // 400
#define nodReassign    17       // :=
#define nodMutation    18       // payload1 = like in topOperator. This is the "+=", operatorful type of mutations


// Single-shot core syntax forms
#define nodAlias       19       // noParen   // 400
#define nodAssert      20       // noParen   // 300
#define nodAssertDbg   21       // noParen   // 300
#define nodAwait       22       // noParen   // 300
#define nodBreak       23       // noParen   // 300
#define nodCatch       24       // paren "(catch e. e,print)"  // 500
#define nodContinue    25       // noParen   // 300
#define nodDefer       26       // noParen   // 300
#define nodEmbed       27       // noParen. Embed a text file as a string literal, or a binary resource file // 200
#define nodExport      28       // paren     // 600
#define nodExposePriv  29       // paren     // 600
#define nodFnDef       30       // payload1 = index of binding
#define nodIf          31       // paren    // 200/500
#define nodIface       32       // 400
#define nodLambda      33       // 500
#define nodLambda1     34       // 500
#define nodLambda2     35       // 500
#define nodLambda3     36       // 500
#define nodPackage     37       // 500 // for single-file packages
#define nodReturn      38       // 300
#define nodStruct      39       // 400
#define nodTry         40       // 500
#define nodYield       41       // 300

// Resumable core forms

#define nodIfPr        42       // like if, but every branch is a value compared using custom predicate  // 200/500
#define nodIfClause    43       // paren    // 200/500
#define nodImpl        44       // paren // 400
#define nodMatch       45       // pattern matching on sum type tag  // 200/500
#define nodLoop        46       // 



// Binding flavors
#define bndMut          0
#define bndImmut        1
#define bndCallable     2       // functions
#define bndType         3       // types



// Must correspond to the order in insertBuiltinBindings
#define builtInt        0
#define builtFloat      1
#define builtString     2
#define builtBool       3
// must be the count of built-in constants above
#define countBuiltins   4
