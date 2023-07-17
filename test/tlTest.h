Lexer* lexicallyAnalyze(String*, LanguageDefinition*, Arena*);
void printLexer(Lexer* a);
LanguageDefinition* buildLanguageDefinitions(Arena* a);
int64_t longOfDoubleBits(double d);
void printIntArray(Int count, Arr(Int) arr);
void printIntArrayOff(Int startInd, Int count, Arr(Int) arr);
Lexer* createLexer(String* inp, LanguageDefinition* langDef, Arena* ar);

#ifdef LEXER_TEST

Int getStringStore(byte* text, String* strToSearch, StackInt* stringTable, StringStore* hm);

void add(Token t, Lexer* lexer);

LanguageDefinition* buildLanguageDefinitions(Arena* a);
void printLexer(Lexer* a);
int equalityLexer(Lexer a, Lexer b);

extern const char errLengthOverflow[];
extern const char errNonAscii[];
extern const char errPrematureEndOfInput[];
extern const char errUnrecognizedByte[];
extern const char errWordChunkStart[];
extern const char errWordCapitalizationOrder[];
extern const char errWordUnderscoresOnlyAtStart[];
extern const char errWordWrongAccessor[];
extern const char errWordReservedWithDot[];
extern const char errNumericEndUnderscore[];
extern const char errNumericWidthExceeded[];
extern const char errNumericBinWidthExceeded[];
extern const char errNumericFloatWidthExceeded[];
extern const char errNumericEmpty[];
extern const char errNumericMultipleDots[];
extern const char errNumericIntWidthExceeded[];
extern const char errPunctuationExtraOpening[];
extern const char errPunctuationExtraClosing[];
extern const char errPunctuationOnlyInMultiline[];
extern const char errPunctuationUnmatched[];
extern const char errPunctuationWrongOpen[];
extern const char errPunctuationScope[];
extern const char errOperatorUnknown[];
extern const char errOperatorAssignmentPunct[];
extern const char errOperatorTypeDeclPunct[];
extern const char errOperatorMultipleAssignment[];
extern const char errOperatorMutableDef[];
extern const char errCoreNotInsideStmt[];
extern const char errCoreMisplacedArrow[];
extern const char errCoreMisplacedElse[];
extern const char errCoreMissingParen[];
extern const char errCoreNotAtSpanStart[];
extern const char errIndentation[];
extern const char errDocComment[];

#endif


#ifdef PARSER_TEST

void addNode(Node n, Compiler* cm);
void importEntities(Arr(EntityImport) impts, Int countBindings, Arr(Int) typeInds, Compiler* cm);
LanguageDefinition* buildLanguageDefinitions(Arena* a);
int equalityParser(Compiler a, Compiler b);
Compiler* createCompiler(Lexer* lx, Arena* a);
Compiler* parseMain(Lexer* lx, Compiler* cm, Arena* a);
Int mergeType(Int startInd, Int len, Compiler* cm);

extern const char errBareAtom[];
extern const char errImportsNonUnique[];
extern const char errLengthOverflow[];
extern const char errPrematureEndOfTokens[];
extern const char errUnexpectedToken[];
extern const char errInconsistentSpan[];
extern const char errCoreFormTooShort[];
extern const char errCoreFormUnexpected[];
extern const char errCoreFormAssignment[];
extern const char errCoreFormInappropriate[];
extern const char errIfLeft[];
extern const char errIfRight[];
extern const char errIfEmpty[];
extern const char errIfMalformed[];
extern const char errFnNameAndParams[];
extern const char errFnMissingBody[];
extern const char errLoopSyntaxError[];
extern const char errLoopHeader[];
extern const char errLoopEmptyBody[];
extern const char errBreakContinueTooComplex[];
extern const char errBreakContinueInvalidDepth[];
extern const char errDuplicateFunction[];
extern const char errExpressionInfixNotSecond[];
extern const char errExpressionError[];
extern const char errExpressionCannotContain[];
extern const char errExpressionFunctionless[];
extern const char errExpressionHeadFormOperators[];
extern const char errTypeDeclCannotContain[];
extern const char errTypeDeclError[];
extern const char errUnknownType[];
extern const char errUnknownTypeFunction[];
extern const char errOperatorWrongArity[];
extern const char errUnknownBinding[];
extern const char errUnknownFunction[];
extern const char errIncorrectPrefixSequence[];
extern const char errOperatorUsedInappropriately[];
extern const char errAssignment[];
extern const char errAssignmentShadowing[];
extern const char errMutation[];
extern const char errReturn[];
extern const char errScope[];
extern const char errLoopBreakOutside[];
extern const char errTemp[];

extern const char errTypeUnknownFirstArg[];
extern const char errTypeZeroArityOverload[];
extern const char errTypeNoMatchingOverload[];
extern const char errTypeWrongArgumentType[];
extern const char errTypeWrongReturnType[];
extern const char errTypeMismatch[];
extern const char errTypeMustBeBool[];

#endif


#ifdef TYPER_TEST

void addFunctionType(Int arity, Arr(Int) paramsAndReturn, Compiler* cm);
Int typeCheckResolveExpr(Int indExpr, Compiler* cm);
Arr(Int) createOverloads(Compiler* cm);
extern const char errTypeUnknownLastArg[];
extern const char errTypeZeroArityOverload[];
extern const char errTypeNoMatchingOverload[];
extern const char errTypeWrongArgumentType[];
Int findOverload(Int typeId, Int start, Int end, Entity* ent, Compiler* cm);

#endif


#ifdef UTILS_TEST

IntMap* createIntMap(int initSize, Arena* a);
void addIntMap(int key, int value, IntMap* hm);
bool hasKeyIntMap(int key, IntMap* hm);
ScopeStack* createScopeStack();
void pushScope(ScopeStack* scopeStack);
void addBinding(int nameId, int bindingId, Compiler* cm);
void popScopeFrame(Compiler* cm);

void sortOverloads(Int startInd, Int endInd, Arr(Int) overloads);
bool makeSureOverloadsUnique(Int startInd, Int endInd, Arr(Int) overloads);
Int overloadBinarySearch(Int typeIdToFind, Int startInd, Int endInd, Int* entityId, Arr(Int) overloads);


#endif
