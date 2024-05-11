Arena* mkArena();
void* allocateOnArena(size_t allocSize, Arena* ar);
void deleteArena(Arena* ar);

DEFINE_INTERNAL_LIST_HEADER(nodes, Node)
DEFINE_INTERNAL_LIST_HEADER(entities, Entity)
DEFINE_INTERNAL_LIST_HEADER(overloads, Int)
DEFINE_INTERNAL_LIST_HEADER(types, Int)
DEFINE_INTERNAL_LIST_HEADER(overloadIds, uint32_t)
DEFINE_INTERNAL_LIST_HEADER(imports, EntityImport)


void buildLanguageDefinitions();
String* prepareInput(const char* content, Arena* a);
Compiler* lexicallyAnalyze(String* input, Compiler*, Arena*);
void printLexer(Compiler* a);
int64_t longOfDoubleBits(double d);
void printIntArray(Int count, Arr(Int) arr);
void printIntArrayOff(Int startInd, Int count, Arr(Int) arr);
void initializeParser(Compiler* lx, Compiler* proto, Arena* a);
void addNode(Node node, SourceLoc loc, Compiler* cm);
Compiler* parse(Compiler* lx, Compiler* proto, Arena* a);
Compiler* createProtoCompiler(Arena* a);
StandardText getStandardTextLength();
void typePrint(Int, Compiler*);
void pushIntokens(Token, Compiler*);
Int stToNameId(Int a);
Unt nameOfStandard(Int strId);
void printRawOverload(Int listInd, Compiler* cm);
void printName(Int name, Compiler* cm);

#ifdef LEXER_TEST

Int getStringDict(Byte* text, String* strToSearch, StackInt* stringTable, StringDict* hm);

Compiler* createLexerFromProto(String* sourceCode, Compiler* proto, Arena* a);
void printLexer(Compiler* a);
int equalityLexer(Compiler a, Compiler b);

extern const char errNonAscii[];
extern const char errPrematureEndOfInput[];
extern const char errUnrecognizedByte[];
extern const char errWordChunkStart[];
extern const char errWordCapitalizationOrder[];
extern const char errWordUnderscoresOnlyAtStart[];
extern const char errWordWrongAccessor[];
extern const char errWordLengthExceeded[];
extern const char errWordTilde[];
extern const char errWordFreeFloatingFieldAcc[];
extern const char errWordInMeta[];
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
extern const char errPunctuationParamList[];
extern const char errPunctuationUnmatched[];
extern const char errPunctuationWrongCall[];
extern const char errPunctuationScope[];
extern const char errOperatorUnknown[];
extern const char errOperatorAssignmentPunct[];
extern const char errOperatorTypeDefPunct[];
extern const char errOperatorMutableDef[];
extern const char errCoreNotInsideStmt[];
extern const char errCoreMisplacedElse[];
extern const char errCoreMissingParen[];
extern const char errIndentation[];

#endif


#ifdef PARSER_TEST

void importEntities(Arr(EntityImport) impts, Int countBindings, Compiler* cm);
void createCompiler(Compiler* lx, Arena* a);
void parseMain(Compiler* cm, Arena* a);
Int mergeType(Int startInd, Compiler* cm);
void printParser(Compiler* cm, Arena* a);
bool findOverload(Int typeId, Int ovInd, Int* entityId, Compiler* cm);
Int addStringDict(Byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable, StringDict* hm);
Arr(Int) createOverloads(Compiler* cm);
void typeAddHeader(TypeHeader hdr, Compiler* cm);

extern const char errBareAtom[];
extern const char errImportsNonUnique[];
extern const char errCannotMutateImmutable[];
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
extern const char errIfElseMustBeLast[];
extern const char errTypeDefCountNames[];
extern const char errFnNameAndParams[];
extern const char errFnDuplicateParams[];
extern const char errFnMissingBody[];
extern const char errLoopSyntaxError[];
extern const char errLoopNoCondition[];
extern const char errLoopEmptyStepBody[];
extern const char errBreakContinueTooComplex[];
extern const char errBreakContinueInvalidDepth[];
extern const char errDuplicateFunction[];
extern const char errExpressionError[];
extern const char errExpressionCannotContain[];
extern const char errExpressionFunctionless[];
extern const char errExpressionHeadFormOperators[];
extern const char errTypeDefCannotContain[];
extern const char errTypeDefError[];
extern const char errUnknownType[];
extern const char errUnknownTypeFunction[];
extern const char errOperatorWrongArity[];
extern const char errUnknownBinding[];
extern const char errUnknownFunction[];
extern const char errIncorrectPrefixSequence[];
extern const char errOperatorUsedInappropriately[];
extern const char errAssignment[];
extern const char errListDifferentEltTypes[];
extern const char errAssignmentShadowing[];
extern const char errAssignmentToplevelFn[];
extern const char errAssignmentLeftSide[];
extern const char errMutation[];
extern const char errReturn[];
extern const char errScope[];
extern const char errLoopBreakOutside[];
extern const char errTemp[];

extern const char errTypeUnknownFirstArg[];
extern const char errExpectedType[];
extern const char errUnexpectedType[];
extern const char errTypeZeroArityOverload[];
extern const char errTypeNoMatchingOverload[];
extern const char errTypeWrongArgumentType[];
extern const char errTypeWrongReturnType[];
extern const char errTypeMismatch[];
extern const char errTypeMustBeBool[];
extern const char errTypeTooManyParameters[];
extern const char errAssignmentAccessOnToplevel[];
extern const char errTypeOfNotList[];
extern const char errTypeOfListIndex[];

#endif


#ifdef UTILS_TEST

IntMap* createIntMap(int initSize, Arena* a);
void addIntMap(int key, int value, IntMap* hm);
bool hasKeyIntMap(int key, IntMap* hm);
ScopeStack* createScopeStack();
void pushScope(ScopeStack* scopeStack);
void addBinding(int nameId, int bindingId, Compiler* cm);
void popScopeFrame(Compiler* cm);

MultiList* createMultiList(Arena* a);
Int addMultiList(Int newKey, Int newVal, Int listInd, MultiList* ml);
Int listAddMultiList(Int newKey, Int newVal, MultiList* ml);
Int searchMultiList(Int searchKey, Int listInd, MultiList* ml);

void sortPairsDisjoint(Int startInd, Int endInd, Arr(Int) arr);
void sortPairs(Int startInd, Int endInd, Arr(Int) arr);
bool verifyUniquenessPairsDisjoint(Int startInd, Int endInd, Arr(Int) arr);
bool makeSureOverloadsUnique(Int startInd, Int endInd, Arr(Int) overloads);
Int overloadBinarySearch(Int typeIdToFind, Int startInd, Int endInd, Int* entityId, Arr(Int) overloads);


#endif
