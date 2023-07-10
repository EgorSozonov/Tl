Lexer* lexicallyAnalyze(String*, LanguageDefinition*, Arena*);
void printLexer(Lexer* a);
LanguageDefinition* buildLanguageDefinitions(Arena* a);

#ifdef LEXER_TEST

Int getStringStore(byte* text, String* strToSearch, StackInt* stringTable, StringStore* hm);

Lexer* createLexer(String* inp, LanguageDefinition* langDef, Arena* ar);
void add(Token t, Lexer* lexer);

LanguageDefinition* buildLanguageDefinitions(Arena* a);

int64_t longOfDoubleBits(double d);
void printLexer(Lexer* a);
int equalityLexer(Lexer a, Lexer b);


extern const char errorLengthOverflow[];
extern const char errorNonAscii[];
extern const char errorPrematureEndOfInput[];
extern const char errorUnrecognizedByte[];
extern const char errorWordChunkStart[];
extern const char errorWordCapitalizationOrder[];
extern const char errorWordUnderscoresOnlyAtStart[];
extern const char errorWordWrongAccessor[];
extern const char errorWordReservedWithDot[];
extern const char errorNumericEndUnderscore[];
extern const char errorNumericWidthExceeded[];
extern const char errorNumericBinWidthExceeded[];
extern const char errorNumericFloatWidthExceeded[];
extern const char errorNumericEmpty[];
extern const char errorNumericMultipleDots[];
extern const char errorNumericIntWidthExceeded[];
extern const char errorPunctuationExtraOpening[];
extern const char errorPunctuationExtraClosing[];
extern const char errorPunctuationOnlyInMultiline[];
extern const char errorPunctuationUnmatched[];
extern const char errorPunctuationWrongOpen[];
extern const char errorPunctuationScope[];
extern const char errorOperatorUnknown[];
extern const char errorOperatorAssignmentPunct[];
extern const char errorOperatorTypeDeclPunct[];
extern const char errorOperatorMultipleAssignment[];
extern const char errorOperatorMutableDef[];
extern const char errorCoreNotInsideStmt[];
extern const char errorCoreMisplacedArrow[];
extern const char errorCoreMisplacedElse[];
extern const char errorCoreMissingParen[];
extern const char errorCoreNotAtSpanStart[];
extern const char errorIndentation[];
extern const char errorDocComment[];

#endif


#ifdef PARSER_TEST

void addNode(Node n, Compiler* cm);
void importEntities(Arr(EntityImport) impts, Int countBindings, Arr(Int) typeInds, Compiler* cm);
LanguageDefinition* buildLanguageDefinitions(Arena* a);
int equalityParser(Compiler a, Compiler b);
Compiler* createCompiler(Lexer* lx, Arena* a);
Compiler* parseWithCompiler(Lexer* lx, Compiler* cm, Arena* a);
Int addType(Int startBt, Int lenBts, Compiler* cm);

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

#endif


#ifdef TYPER_TEST

void addFunctionType(Int arity, Arr(Int) paramsAndReturn, Compiler* cm);
Int typeCheckResolveExpr(Int indExpr, Compiler* cm);
Arr(Int) createOverloads(Compiler* cm);
extern const char errorTypeUnknownLastArg[];
extern const char errorTypeZeroArityOverload[];
extern const char errorTypeNoMatchingOverload[];
extern const char errorTypeWrongArgumentType[];
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
