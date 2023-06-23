#ifdef ALL_TESTS

typedef struct Arena Arena;
typedef struct String String;
#define private static
#define Arr(T) T*
#define Int int32_t
#define StackInt Stackint32_t
#define byte unsigned char
#define untt uint32_t

//DEFINE_STACK_HEADER(int32_t)

#endif


#ifdef LEXER_TEST

typedef struct LanguageDefinition LanguageDefinition;
typedef struct Lexer Lexer;
typedef struct Token Token;
Int getStringStore(byte* text, String* strToSearch, StackInt* stringTable, StringStore* hm);

Lexer* createLexer(String* inp, LanguageDefinition* langDef, Arena* ar);
void add(Token t, Lexer* lexer);

LanguageDefinition* buildLanguageDefinitions(Arena* a);
Lexer* lexicallyAnalyze(String*, LanguageDefinition*, Arena*);

#endif


#ifdef PARSER_TEST

#endif


#ifdef TYPER_TEST

#endif
