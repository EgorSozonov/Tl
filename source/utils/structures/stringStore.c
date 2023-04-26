#include "stringStore.h"
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "stack.h"
extern jmp_buf excBuf;

#define initBucketSize 8
DEFINE_STACK(StringRef)

typedef struct {
    intt length;
    intt indString; // index inside the stringTable
} StringValue;

typedef struct {
    untt capAndLen;
    StringValue content[];
} Bucket;


struct StringStore {
    Arr(Bucket*) dict;
    int dictSize;
    int length;    
    Arena* a;
};


StringStore* createStringStore(int initSize, Arena* a) {
    StringStore* result = allocateOnArena(sizeof(StringStore), a);
    int realInitSize = (initSize >= initBucketSize && initSize < 2048) ? initSize : (initSize >= initBucketSize ? 2048 : initBucketSize);
    Arr(Bucket*) dict = allocateOnArena(sizeof(Bucket*)*realInitSize, a);
    
    result->a = a;
    
    Arr(Bucket*) d = dict;

    for (int i = 0; i < realInitSize; i++) {
        d[i] = NULL;
    }
    result->dictSize = realInitSize;
    result->dict = dict;

    return result;
}

private intt hashCode(byte* start, intt len) {        
    intt result = 5381;

    byte* p = start;
    for (int i = 0; i < len; i++) {        
        result = ((result << 5) + result) + p[i]; /* hash * 33 + c */
    }

    return result;
}

private bool equalStringRefs(const byte* text, intt a, intt b, intt len) {
    return memcmp(text + a, text + b, len) == 0;
}


private void addValueToBucket(Bucket* p, intt startByte, intt lenBytes, intt newIndString, 
                                Arr(Bucket*)* ptrToBucket) {        
    int capacity = (p->capAndLen) >> 16;
    int lenBucket = (p->capAndLen & 0xFFFF);
    if (lenBucket + 1 < capacity) {
        *(*ptrToBucket + lenBucket) = (StringValue){.length = lenBytes, .indString = newIndString};
        p->capAndLen = (capacity << 16) + lenBucket + 1;
    } else {
        // TODO handle the case where we've overflowing the 16 bits of capacity
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + 2*capacity*sizeof(StringValue), hm->a);
        memcpy(newBucket->content, p->content, capacity*sizeof(StringValue));
        
        Arr(StringValue) newValues = (StringValue*)newBucket->content;                 
        newValues[capacity] = (StringValue){.indString = newIndString, .length = lenBytes};
        *ptrToBucket = newBucket;
        newBucket->capAndLen = ((2*capacity) << 16) + capacity + 1;      
    }
}

intt addStringStore(const byte* text, intt startByte, intt len, StackStringRef* stringTable, StringStore* hm) {
    int hash = hashCode(text + startByte, lenBytes) % (hm->dictSize);
    int keyLen = key->length;
    intt newIndString;
    if (*(hm->dict + hash) == NULL) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (8 << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;
        
        pushStringRef((StringRef){.startByte = startByte, .length = lenBytes});
        newIndString = stringTable->length;
        
        *firstElem = (StringValue){.length = lenBytes, .indString = newIndString };
        *(hm->dict + hash) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hash);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == lenBytes 
                && equalStringRefs(text, stringValues[i].indString, startByte, lenBytes)) { // key already present                
                return stringValues[i].indString;
            }
        }
        pushStringRef((StringRef){.startByte = startByte, .length = lenBytes});
        newIndString = stringTable->length;
        addValueToBucket(p, startByte, lenBytes, newIndString, (hm->dict + hash));
    }
    return newIndString;
}

