foo = def(s String flag Bool => Int:
    if( flag : return #s
          else return getLength(s)
    )
)

getLength = def(s String => Int:
    return foo(s true)
)

main = def(:
    result = foo(`Hello world` false)
    print(+(`Result is ` $result))
)
