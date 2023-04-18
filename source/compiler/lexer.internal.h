DEFINE_STACK(RememberedToken)

#define pop(X) _Generic((X), \
    StackRememberedToken*: popRememberedToken \
    )(X)

#define peek(X) _Generic((X), \
    StackRememberedToken*: peekRememberedToken \
    )(X)

#define push(A, X) _Generic((X), \
    StackRememberedToken*: pushRememberedToken \
    )(A, X)
    
#define hasValues(X) _Generic((X), \
    StackRememberedToken*: hasValuesRememberedToken \
    )(X)   
