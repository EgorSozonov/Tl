#ifndef STACK_H
#define STACK_H


#define DEFINE_STACK(T)                                                                  \
    Stack##T * createStack##T (int initCapacity, Arena* a) {                             \
        int capacity = initCapacity < 4 ? 4 : initCapacity;                              \
        Stack##T * result = allocateOnArena(sizeof(Stack##T), a);                        \
        result->capacity = capacity;                                                     \
        result->length = 0;                                                              \
        result->arena = a;                                                               \
        T (* arr)[] = allocateOnArena(capacity*sizeof(T), a);                            \
        result->content = arr;                                                           \
        return result;                                                                   \
    }                                                                                    \
    bool hasValues ## T (Stack ## T * st) {                                              \
        return st->length > 0;                                                           \
    }                                                                                    \
    T pop##T (Stack ## T * st) {                                                         \
        st->length--;                                                                    \
        return (*st->content)[st->length];                                               \
    }                                                                                    \
    T peek##T(Stack##T * st) {                                                           \
        return (*st->content)[st->length - 1];                                           \
    }                                                                                    \
    void push##T (T newItem, Stack ## T * st) {                                          \
        if (st->length < st->capacity) {                                                 \
            memcpy((T*)(st->content) + (st->length), &newItem, sizeof(T));               \
        } else {                                                                         \
            T (* newContent)[] = allocateOnArena(2*(st->capacity)*sizeof(T), st->arena); \
            memcpy(newContent, st->content, st->length*sizeof(T));                       \
            memcpy((T*)(newContent) + (st->length), &newItem, sizeof(T));                \
            st->capacity *= 2;                                                           \
            st->content = (T(*)[])newContent;                                            \
        }                                                                                \
        st->length++;                                                                    \
    }                                                                                    \
    void clear##T (Stack##T * st) {                                                      \
        st->length = 0;                                                                  \
    }                                                                                    \

#endif
