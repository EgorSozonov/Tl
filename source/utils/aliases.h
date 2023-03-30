#ifndef ALIASES_H
#define ALIASES_H


#define private static
#define byte unsigned char
#define Arr(T) T*
#define uint int

#define LOWER24BITS 0x00FFFFFF
#define LOWER26BITS 0x03FFFFFF
#define LOWER32BITS 0x00000000FFFFFFFF
#define THIRTYFIRSTBIT 0x40000000
#define MAXTOKENLEN = 67108864 // 2**26

#ifdef DEBUG
#define DBG(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)
#else
#define DBG(fmt, ...) // empty
#endif                                

#endif
