﻿#ifndef STRING_H
#define STRING_H
#include "Arena.h"


typedef struct {
    int length;
    char content[];
} String;

String* allocateString(Arena* ar, int length, char* content);
String* allocateFromSubstring(Arena* ar, char* content, int start, int length);

#endif
