DEFINE_STACK(RememberedToken)
DEFINE_STACK(int32_t)

#define initBucketSize 8

#define pop(X) _Generic((X), \
    StackRememberedToken*: popRememberedToken \
    )(X)

#define peek(X) _Generic((X), \
    StackRememberedToken*: peekRememberedToken \
    )(X)

#define push(A, X) _Generic((X), \
    StackRememberedToken*: pushRememberedToken, \
    Stackint32_t*: pushint32_t \
    )(A, X)
    
#define hasValues(X) _Generic((X), \
    StackRememberedToken*: hasValuesRememberedToken \
    )(X)   


typedef struct {
    Int length;
    Int indString; // index inside the stringTable
} StringValue;


struct Bucket {
    untt capAndLen;
    StringValue content[];
};


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


private Int hashCode(byte* start, Int len) {        
    Int result = 5381;

    byte* p = start;
    for (int i = 0; i < len; i++) {        
        result = ((result << 5) + result) + p[i]; /* hash * 33 + c */
    }

    return result;
}


private bool equalStringRefs(byte* text, Int a, Int b, Int len) {
    return memcmp(text + a, text + b, len) == 0;
}


private void addValueToBucket(Bucket** ptrToBucket, Int startByte, Int lenBytes, Int newIndString, Arena* a) {    
    Bucket* p = *ptrToBucket;                                        
    Int capacity = (p->capAndLen) >> 16;
    Int lenBucket = (p->capAndLen & 0xFFFF);
    if (lenBucket + 1 < capacity) {
        (*ptrToBucket)->content[lenBucket] = (StringValue){.length = lenBytes, .indString = newIndString};
        p->capAndLen = (capacity << 16) + lenBucket + 1;
    } else {
        // TODO handle the case where we've overflowing the 16 bits of capacity
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + 2*capacity*sizeof(StringValue), a);
        memcpy(newBucket->content, p->content, capacity*sizeof(StringValue));
        
        Arr(StringValue) newValues = (StringValue*)newBucket->content;                 
        newValues[capacity] = (StringValue){.indString = newIndString, .length = lenBytes};
        *ptrToBucket = newBucket;
        newBucket->capAndLen = ((2*capacity) << 16) + capacity + 1;      
    }
}


Int addStringStore(byte* text, Int startByte, Int lenBytes, Stackint32_t* stringTable, StringStore* hm) {
    Int hash = hashCode(text + startByte, lenBytes) % (hm->dictSize);
    Int newIndString;
    if (*(hm->dict + hash) == NULL) {
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (8 << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;
        
        newIndString = stringTable->length;
        push(startByte, stringTable);
        
        
        *firstElem = (StringValue){.length = lenBytes, .indString = newIndString };
        *(hm->dict + hash) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hash);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == lenBytes 
                && equalStringRefs(text, (*stringTable->content)[stringValues[i].indString], startByte, lenBytes)) { // key already present                
                return stringValues[i].indString;
            }
        }
        
        push(startByte, stringTable);
        newIndString = stringTable->length;
        addValueToBucket((hm->dict + hash), startByte, lenBytes, newIndString, hm->a);
    }
    return newIndString;
}
