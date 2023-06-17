DEFINE_STACK(BtToken)
DEFINE_STACK(int32_t)

#define initBucketSize 8

#define pop(X) _Generic((X), \
    StackBtToken*: popBtToken \
    )(X)

#define peek(X) _Generic((X), \
    StackBtToken*: peekBtToken \
    )(X)

#define push(A, X) _Generic((X), \
    StackBtToken*: pushBtToken, \
    Stackint32_t*: pushint32_t \
    )(A, X)
    
#define hasValues(X) _Generic((X), \
    StackBtToken*: hasValuesBtToken \
    )(X)   


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


private untt hashCode(byte* start, Int len) {        
    untt result = 5381;

    byte* p = start;
    for (int i = 0; i < len; i++) {        
        result = ((result << 5) + result) + p[i]; /* hash*33 + c */
    }

    return result;
}


private bool equalStringRefs(byte* text, Int a, Int b, Int len) {
    return memcmp(text + a, text + b, len) == 0;
}


private void addValueToBucket(Bucket** ptrToBucket, Int startBt, Int lenBts, Int newIndString, Arena* a) {    
    Bucket* p = *ptrToBucket;                                        
    Int capacity = (p->capAndLen) >> 16;
    Int lenBucket = (p->capAndLen & 0xFFFF);
    if (lenBucket + 1 < capacity) {
        (*ptrToBucket)->content[lenBucket] = (StringValue){.length = lenBts, .indString = newIndString};
        p->capAndLen = (capacity << 16) + lenBucket + 1;
    } else {
        // TODO handle the case where we've overflowing the 16 bits of capacity
        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + 2*capacity*sizeof(StringValue), a);
        memcpy(newBucket->content, p->content, capacity*sizeof(StringValue));
        
        Arr(StringValue) newValues = (StringValue*)newBucket->content;                 
        newValues[capacity] = (StringValue){.indString = newIndString, .length = lenBts};
        *ptrToBucket = newBucket;
        newBucket->capAndLen = ((2*capacity) << 16) + capacity + 1;      
    }
}


Int addStringStore(byte* text, Int startBt, Int lenBts, Stackint32_t* stringTable, StringStore* hm) {
    Int hash = hashCode(text + startBt, lenBts) % (hm->dictSize);
    Int newIndString;
    if (*(hm->dict + hash) == NULL) {

        Bucket* newBucket = allocateOnArena(sizeof(Bucket) + initBucketSize*sizeof(StringValue), hm->a);
        newBucket->capAndLen = (8 << 16) + 1; // left u16 = capacity, right u16 = length
        StringValue* firstElem = (StringValue*)newBucket->content;
        
        newIndString = stringTable->length;
        push(startBt, stringTable);
                
        *firstElem = (StringValue){.length = lenBts, .indString = newIndString };
        *(hm->dict + hash) = newBucket;
    } else {
        Bucket* p = *(hm->dict + hash);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == lenBts 
                && equalStringRefs(text, (*stringTable->content)[stringValues[i].indString], startBt, lenBts)) { // key already present                
                return stringValues[i].indString;
            }
        }
        
        push(startBt, stringTable);
        newIndString = stringTable->length;
        addValueToBucket((hm->dict + hash), startBt, lenBts, newIndString, hm->a);
    }
    return newIndString;
}

/** Returns the index of a string within the string table, or -1 if it's not present */
Int getStringStore(byte* text, String* strToSearch, Stackint32_t* stringTable, StringStore* hm) {
    Int lenBts = strToSearch->length;
    Int hash = hashCode(strToSearch->content, lenBts) % (hm->dictSize);    
    Int newIndString;
    if (*(hm->dict + hash) == NULL) {
        return -1;
    } else {
        Bucket* p = *(hm->dict + hash);
        int lenBucket = (p->capAndLen & 0xFFFF);
        Arr(StringValue) stringValues = (StringValue*)p->content;
        for (int i = 0; i < lenBucket; i++) {
            if (stringValues[i].length == lenBts
                && memcmp(strToSearch->content, text + (*stringTable->content)[stringValues[i].indString], lenBts) == 0) {
                return stringValues[i].indString;
            }
        }
        
        return -1;
    }
}
