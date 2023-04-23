DEFINE_STACK(ParseFrame)

#define pop(X) _Generic((X), \
    StackParseFrame*: popParseFrame \
    )(X)

#define peek(X) _Generic((X), \
    StackParseFrame*: peekParseFrame \
    )(X)

#define push(A, X) _Generic((X), \
    StackParseFrame*: pushParseFrame \
    )(A, X)
    
#define hasValues(X) _Generic((X), \
    StackParseFrame*: hasValuesParseFrame \
    )(X)   
