foo = f: String, Bool => Int
    s, flag
    if: flag => return #s
        else    return getLength s

getLength = f: String => Int.
    s.
    return foo s true

main = f:
    result = foo `Hello world` false
    print $ concat $ `Result is ` ;result
