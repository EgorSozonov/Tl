#ifndef STACK_H
#define STACK_H


#define DEFINE_STACK(T)                                                                  \
    Stack ## T * mkStack ## T (Arena* ar, int initCapacity) {                            \
        int capacity = initCapacity < 4 ? 4 : initCapacity;                              \
        Stack ## T * result = allocateOnArena(ar, sizeof(Stack ## T));                   \
        result->capacity = capacity;                                                     \
        result->length = 0;                                                              \
        result->arena = ar;                                                              \
        T (* arr)[] = allocateOnArena(ar, capacity*sizeof(T));                           \
        result->content = arr;                                                           \
        return result;                                                                   \
    }                                                                                    \
    bool hasValues ## T (Stack ## T * st) {                                              \
        return st->length > 0;                                                           \
    }                                                                                    \
    T pop ## T (Stack ## T * st) {                                                       \
        st->length--;                                                                    \
        return (*st->content)[st->length];                                               \
    }                                                                                    \
    void push ## T (Stack ## T * st, T newItem) {                                        \
        if (st->length < st->capacity) {                                                 \
            memcpy((T*)(st->content) + (st->length), &newItem, sizeof(T));               \
            st->length++;                                                                \
        } else {                                                                         \
            T (* newContent)[] = allocateOnArena(st->arena, 2*(st->capacity)*sizeof(T)); \
            memcpy(newContent, st->content, st->length*sizeof(T));                       \
            memcpy((T*)(newContent) + (st->length), &newItem, sizeof(T));                \
            st->capacity *= 2;                                                           \
            st->length++;                                                                \
            st->content = (T(*)[])newContent;                                            \
        }                                                                                \
    }                                                                                    \
    void clear ## T (Stack ## T * st) {                                                  \
        st->length = 0;                                                                  \
    }                                                                                    \

#endif
