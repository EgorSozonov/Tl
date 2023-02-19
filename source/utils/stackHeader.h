#ifndef STACK_HEADER_H
#define STACK_HEADER_H

#include "stdbool.h"
#define DEFINE_STACK_HEADER(T)                                  \
    typedef struct {                                            \
        int capacity;                                           \
        int length;                                             \
        Arena* arena;                                           \
        T (* content)[];                                        \
    } Stack ## T;                                               \
    Stack ## T * createStack ## T (Arena* ar, int initCapacity);\
    bool hasValues ## T (Stack ## T * st);                      \
    T pop ## T (Stack ## T * st);                               \
    T peek ## T(Stack ## T * st);                               \
    void push ## T (Stack ## T * st, T newItem);                \
    void clear ## T (Stack ## T * st);


#endif
