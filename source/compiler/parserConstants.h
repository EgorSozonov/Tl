extern const char errorBareAtom[];
extern const char errorImportsNonUnique[];
extern const char errorLengthOverflow[];
extern const char errorPrematureEndOfTokens[];
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
extern const char errorLoopSyntaxError[];
extern const char errorLoopHeader[];
extern const char errorLoopEmptyBody[];
extern const char errorBreakContinueTooComplex[];
extern const char errorBreakContinueInvalidDepth[];
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
#define nodBool         2      // pl2 = value (1 or 0)
#define nodString       3
#define nodUnderscore   4
#define nodDocComment   5

#define nodId           6      // pl1 = index of binding, pl2 = index of name
#define nodCall         7      // pl1 = index of binding, pl2 = arity
#define nodBinding      8      // pl1 = index of binding
#define nodTypeId       9      // pl1 = index of binding
#define nodAnd         10
#define nodOr          11


// Punctuation (inner node)
#define nodScope       12       // (: This is resumable but trivially so, that's why it's not grouped with the others
#define nodExpr        13
#define nodAssignment  14       // pl1 = 1 if mutable assignment, 0 if immutable
#define nodReassign    15       // :=
#define nodMutation    16       // pl1 = like in topOperator. This is the "+=", operatorful type of mutations


// Single-shot core syntax forms
#define nodAlias       17       
#define nodAssert      18       
#define nodAssertDbg   19       
#define nodAwait       20       
#define nodBreak       21       
#define nodCatch       22       // "(catch e. e,print)"
#define nodContinue    23       
#define nodDefer       24       
#define nodEach        25       
#define nodEmbed       26       // noParen. Embed a text file as a string literal, or a binary resource file
#define nodExport      27       
#define nodExposePriv  28       
#define nodFnDef       29       // pl1 = index of binding
#define nodIface       30
#define nodLambda      31
#define nodMeta        32       
#define nodPackage     33       // for single-file packages
#define nodReturn      34       
#define nodStruct      35       
#define nodTry         36       
#define nodYield       37       
#define nodIfClause    38       // pl1 = number of tokens in the left side of the clause
#define nodElse        39
#define nodLoop        40       //
#define nodLoopCond    41

// Resumable core forms
#define nodIf          42       // paren
#define nodIfPr        43       // like if, but every branch is a value compared using custom predicate
#define nodImpl        44       
#define nodMatch       45       // pattern matching on sum type tag




// Entity flavors
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
