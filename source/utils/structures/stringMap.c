#include "stringMap.h"
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
extern jmp_buf excBuf;

#define initBucketSize 8


typedef struct {
    int length;
    int value;
    String* string;
} StringValue;


typedef struct {
    untt capAndLen;
    StringValue content[];
} ValueList;


struct StringMap {
    Arr(ValueList*) dict;
    int dictSize;
    int length;    
    Arena* a;
};


StringMap* createStringMap(int initSize, Arena* a) {
    StringMap* result = allocateOnArena(sizeof(StringMap), a);
    int realInitSize = (initSize >= initBucketSize && initSize < 2048) ? initSize : (initSize >= initBucketSize ? 2048 : initBucketSize);
    Arr(ValueList*) dict = allocateOnArena(sizeof(ValueList*)*realInitSize, a);
    
    result->a = a;
    
    Arr(ValueList*) d = dict;

    for (int i = 0; i < realInitSize; i++) {
        d[i] = NULL;
    }
    result->dictSize = realInitSize;
    result->dict = dict;

    return result;
}

private intt hashCode(String* s) {        
    intt result = 5381;
    int c;

    byte* p = (byte*)s->content;
    const intt len = s->length;
    for (int i = 0; i < len; i++) {        
        result = ((result << 5) + result) + p[i]; /* hash * 33 + c */
    }

    return result;
}


void addStringMap(String* key, int value, StringMap* hm) {
    int hash = hashCode(key) % (hm->dictSize);
    int keyLen = key->length;
    if (*(hm->dict + hash) == NULL) {
        ValueList* newBucket = allocateOnArena(sizeof(ValueList) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (8 << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;
        *firstElem = (StringValue){.length = keyLen, .value = value, .string = key};
        *(hm->dict + hash) = newBucket;
    } else {
        ValueList* p = *(hm->dict + hash);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == keyLen && equal(stringValues[i].string, key)) { // key already present                
                longjmp(excBuf, 1);
            }
        }
        int capacity = (p->capAndLen) >> 16;
        if (lenBucket + 1 < capacity) {
            stringValues[lenBucket] = (StringValue){.length = key->length, .value = value, .string = key};
            p->capAndLen = (capacity << 16) + lenBucket + 1;
        } else {
            // TODO handle the case where we've overflowing the 16 bits of capacity
            ValueList* newBucket = allocateOnArena(sizeof(ValueList) + 2*capacity*sizeof(StringValue), hm->a);
            memcpy(newBucket->content, p->content, capacity*sizeof(StringValue));
            
            Arr(StringValue) newValues = (StringValue*)newBucket->content;                 
            newValues[capacity] = (StringValue){.length = key->length, .value = value, .string = key};
            *(hm->dict + hash) = newBucket;
            newBucket->capAndLen = ((2*capacity) << 16) + capacity + 1;      
        }
    }
}


bool hasKeyStringMap(String* key, StringMap* hm) {   
    int hash = hashCode(key) % (hm->dictSize);
    int keyLen = key->length;
    if (*(hm->dict + hash) == NULL) {
        return false;
    }
    ValueList* p = *(hm->dict + hash);
    int lenBucket = (p->capAndLen & 0xFFFF);
    Arr(StringValue) stringValues = (StringValue*)p->content;
    for (int i = 0; i < lenBucket; i++) {
        if (stringValues[i].length == keyLen && equal(stringValues[i].string, key)) { // key already present                
            return true;
        }
    }
    return false;    
}

/** Returns 1 if key is not found, 0 if found (then the value is written into the result argument) */
int getStringMap(String* key, int* result, StringMap* hm) {
    int hash = hashCode(key) % (hm->dictSize);
    int keyLen = key->length;
    if (*(hm->dict + hash) == NULL) {
        return 1;
    } 

    ValueList* p = *(hm->dict + hash);
    int lenBucket = (p->capAndLen & 0xFFFF);
    Arr(StringValue) stringValues = (StringValue*)p->content;
    for (int i = 0; i < lenBucket; i++) {
        if (stringValues[i].length == keyLen && equal(stringValues[i].string, key)) {                                
            *result = stringValues[i].value;
            return 0;
        }
    }
    
    return 1;
}


/** Throws an exception when key is absent */
int getUnsafeStringMap(String* key, StringMap* hm) {
    int hash = hashCode(key) % (hm->dictSize);
    int keyLen = key->length;
    if (*(hm->dict + hash) == NULL) {
        longjmp(excBuf, 1);
    } 

    ValueList* p = *(hm->dict + hash);
    int lenBucket = (p->capAndLen & 0xFFFF);
    Arr(StringValue) stringValues = (StringValue*)p->content;
    for (int i = 0; i < lenBucket; i++) {
        if (stringValues[i].length == keyLen && equal(stringValues[i].string, key)) { // key already present                                
            return stringValues[i].value; 
        }
    }
    
    longjmp(excBuf, 1);
}
