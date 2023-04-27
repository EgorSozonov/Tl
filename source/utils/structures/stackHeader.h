#ifndef STACK_HEADER_H
#define STACK_HEADER_H
#include <stdbool.h>

#define DEFINE_STACK_HEADER(T)                                  \
    typedef struct {                                            \
        Int capacity;                                           \
        Int length;                                             \
        Arena* arena;                                           \
        T (* content)[];                                        \
    } Stack##T;                                                 \
    Stack ## T * createStack ## T (Int initCapacity, Arena* ar);\
    bool hasValues ## T (Stack ## T * st);                      \
    T pop ## T (Stack ## T * st);                               \
    T peek ## T(Stack ## T * st);                               \
    void push ## T (T newItem, Stack ## T * st);                \
    void clear ## T (Stack ## T * st);


#endif
