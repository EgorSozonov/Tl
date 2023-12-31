foo = {s String, flag Bool => Int;;
    if { flag => return #s }
    else { return s.getLength }

getLength = {s String => Int;;
    return s.foo(true)
}

main = {
    result = `Hello world`.foo(false)
    print(`Result is ` $result)
}
