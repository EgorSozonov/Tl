typedef struct { // :String
    const char* cont;
    int32_t len;
} String;

void
eyrRunFile(String filename);

void
eyrRun(String sourceCode);
