fibonacci = def(n Int => Int :
    if( <=(n 0) : return 0
        ==(n 1) : return 1
    )
    beforeLast = 0
    accum = 1
    swap = 0
    while(k = -(n 1). >(k 0):
        swap := beforeLast
        beforeLast := accum
        accum += swap
        k -= 1
    )
    return accum
)

main = def(:
    fib = fibonacci(6)
    print(fib)
)
