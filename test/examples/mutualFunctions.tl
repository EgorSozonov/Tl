foo = {{s Str flag Bool -> Int:
    if { flag: return s$ }
    else { return getLength s }
}}

getLength = {{ s Str -> Int:
    return foo s true
}}

main = {{
    result = foo `Hello world` false;
    print `Result is ` result$
}}
