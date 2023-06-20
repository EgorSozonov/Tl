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
extern const char errorTypeUnknownLastArg[];
extern const char errorTypeZeroArityOverload[];
extern const char errorTypeNoMatchingOverload[];
extern const char errorTypeWrongArgumentType[];

#define nodInt          0
#define nodLong         1
#define nodFloat        2
#define nodBool         3      // pl2 = value (1 or 0)
#define nodString       4
#define nodUnderscore   5
#define nodDocComment   6

#define nodId           7      // pl1 = index of binding, pl2 = index of name
#define nodCall         8      // pl1 = index of binding, pl2 = arity
#define nodBinding      9      // pl1 = index of binding
#define nodTypeId      10      // pl1 = index of binding
#define nodAnd         11
#define nodOr          12


// Punctuation (inner node)
#define nodScope       13       // (: This is resumable but trivially so, that's why it's not grouped with the others
#define nodExpr        14
#define nodAssignment  15       // pl1 = 1 if mutable assignment, 0 if immutable
#define nodReassign    16       // :=
#define nodMutation    17       // pl1 = like in topOperator. This is the "+=", operatorful type of mutations


// Single-shot core syntax forms
#define nodAlias       18       
#define nodAssert      19       
#define nodAssertDbg   20       
#define nodAwait       21       
#define nodBreak       22       
#define nodCatch       23       // "(catch e. e,print)"
#define nodContinue    24       
#define nodDefer       25       
#define nodEach        26       
#define nodEmbed       27       // noParen. Embed a text file as a string literal, or a binary resource file
#define nodExport      28       
#define nodExposePriv  29       
#define nodFnDef       30       // pl1 = index of binding
#define nodIface       31
#define nodLambda      32
#define nodMeta        33       
#define nodPackage     34       // for single-file packages
#define nodReturn      35       
#define nodStruct      36       
#define nodTry         37       
#define nodYield       38       
#define nodIfClause    39       // pl1 = number of tokens in the left side of the clause
#define nodElse        40
#define nodLoop        41       //
#define nodLoopCond    42

// Resumable core forms
#define nodIf          43       // paren
#define nodIfPr        44       // like if, but every branch is a value compared using custom predicate
#define nodImpl        45       
#define nodMatch       46       // pattern matching on sum type tag

