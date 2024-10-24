//{{{ Common

Arena* createArena();
void* allocateOnArena(size_t allocSize, Arena* ar);
void deleteArena(Arena* ar);


//~DEFINE_INTERNAL_LIST_HEADER(nodes, Node)
//~DEFINE_INTERNAL_LIST_HEADER(entities, Entity)
//~DEFINE_INTERNAL_LIST_HEADER(overloads, Int)
DEFINE_INTERNAL_LIST_HEADER(types, Int)
//~DEFINE_INTERNAL_LIST_HEADER(overloadIds, uint32_t)
//~DEFINE_INTERNAL_LIST_HEADER(imports, Entity)
Bool equal(String a, String b);

typedef struct {
    Int countTests;
    Int countPassed;
    Arena* a;
} TestContext;

String prepareInput(char const* content, Arena* a);
Compiler* lexicallyAnalyze(String input, Arena*);
void printLexer(Compiler* a);
int64_t longOfDoubleBits(double d);
void printIntArray(Int count, Arr(Int) arr);
void printIntArrayOff(Int startInd, Int count, Arr(Int) arr);
void initializeParser(Compiler* lx, Arena* a);
void addNode(Node node, SourceLoc loc, Compiler* cm);
Compiler* createLexer(String sourceCode, Arena* a);
Compiler* parse(Compiler* lx, Arena* a);
StandardText getStandardTextLength();
void typePrint(Int, Compiler*);
void pushIntokens(Token, Compiler*);
NameId nameOfStandard(Int strId);
void printRawOverload(Int listInd, Compiler* cm);
void printName(Int name, Compiler* cm);

//}}}
//{{{ Lexer

#ifdef LEXER_TEST

Int getStringDict(Byte* text, String strToSearch, StackInt* stringTable, StringDict* hm);

void printLexer(Compiler* restrict a);
Int equalityLexer(Compiler a, Compiler b);

extern char const errNonAscii[];
extern char const errPrematureEndOfInput[];
extern char const errUnrecognizedByte[];
extern char const errWordChunkStart[];
extern char const errWordCapitalizationOrder[];
extern char const errWordUnderscoresOnlyAtStart[];
extern char const errWordWrongAccessor[];
extern char const errWordLengthExceeded[];
extern char const errWordTilde[];
extern char const errWordFreeFloatingFieldAcc[];
extern char const errWordInMeta[];
extern char const errNumericEndUnderscore[];
extern char const errNumericWidthExceeded[];
extern char const errNumericBinWidthExceeded[];
extern char const errNumericFloatWidthExceeded[];
extern char const errNumericEmpty[];
extern char const errNumericMultipleDots[];
extern char const errNumericIntWidthExceeded[];
extern char const errPunctuationExtraOpening[];
extern char const errPunctuationExtraClosing[];

extern char const errPunctuationOnlyInMultiline[];
extern char const errPunctuationFnNotInStmt[];
extern char const errPunctuationUnmatched[];
extern char const errPunctuationScope[];

extern char const errOperatorUnknown[];
extern char const errOperatorAssignmentPunct[];
extern char const errToplevelAssignment[];
extern char const errOperatorTypeDeclPunct[];
extern char const errOperatorMutationInDef[];
extern char const errCoreNotInsideStmt[];
extern char const errCoreMisplacedElse[];
extern char const errCoreMissingParen[];
extern char const errIndentation[];

#endif
//}}}
//{{{ Parser

#ifdef PARSER_TEST

void importEntities(Arr(Entity) impts, Int countBindings, Compiler* cm);
void createCompiler(Compiler* lx, Arena* a);
void parseMain(Compiler* cm, Arena* a);
Int mergeType(Int startInd, Compiler* cm);
void printParser(Compiler* cm, Arena* a);
bool findOverload(Int typeId, Int ovInd, Compiler* restrict cm, Int* entityId);
Int addStringDict(Byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable, StringDict* hm);
Arr(Int) createOverloads(Compiler* cm);
void typeAddHeader(TypeHeader hdr, Compiler* restrict cm);

extern char const errBareAtom[];
extern char const errImportsNonUnique[];
extern char const errCannotMutateImmutable[];
extern char const errPrematureEndOfTokens[];
extern char const errUnexpectedToken[];
extern char const errInconsistentSpan[];
extern char const errCoreFormTooShort[];
extern char const errCoreFormUnexpected[];
extern char const errCoreFormAssignment[];
extern char const errCoreFormInappropriate[];
extern char const errIfLeft[];
extern char const errIfRight[];
extern char const errIfEmpty[];
extern char const errIfMalformed[];
extern char const errIfElseMustBeLast[];
extern char const errTypeDefCountNames[];
extern char const errFnNameAndParams[];
extern char const errFnDuplicateParams[];
extern char const errFnMissingBody[];
extern char const errLoopSyntaxError[];
extern char const errLoopNoCondition[];
extern char const errLoopEmptyStepBody[];
extern char const errBreakContinueTooComplex[];
extern char const errBreakContinueInvalidDepth[];
extern char const errDuplicateFunction[];
extern char const errExpressionError[];
extern char const errExpressionCannotContain[];
extern char const errExpressionFunctionless[];
extern char const errExpressionHeadFormOperators[];
extern char const errTypeDefCannotContain[];
extern char const errTypeDefError[];
extern char const errUnknownType[];
extern char const errUnknownTypeFunction[];
extern char const errOperatorWrongArity[];
extern char const errUnknownBinding[];
extern char const errUnknownFunction[];
extern char const errIncorrectPrefixSequence[];
extern char const errOperatorUsedInappropriately[];
extern char const errAssignment[];
extern char const errListDifferentEltTypes[];
extern char const errAssignmentShadowing[];
extern char const errAssignmentToplevelFn[];
extern char const errAssignmentLeftSide[];
extern char const errMutation[];
extern char const errReturn[];
extern char const errScope[];
extern char const errLoopBreakOutside[];
extern char const errTemp[];
extern char const errTypeUnknownFirstArg[];
extern char const errExpectedType[];
extern char const errUnexpectedType[];
extern char const errTypeZeroArityOverload[];
extern char const errTypeNoMatchingOverload[];
extern char const errTypeWrongArgumentType[];
extern char const errTypeWrongReturnType[];
extern char const errTypeMismatch[];
extern char const errTypeMustBeBool[];
extern char const errTypeTooManyParameters[];
extern char const errAssignmentAccessOnToplevel[];
extern char const errTypeOfNotList[];
extern char const errTypeOfListIndex[];

#endif
//}}}
//{{{ Utils

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

//}}}
//{{{ Codegen

#ifdef CODEGEN_TEST

testable Int getCoreLibSize();

#endif

//}}}
