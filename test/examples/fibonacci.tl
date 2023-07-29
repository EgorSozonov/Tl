(:f fibonacci Int(n Int) =
    (:if <= n 1 => return 0
         == n 2 => return 1)
    beforeLast = 0
    accum = 1
    (:while (> k 0). k = - n 1
        temp = beforeLast
        beforeLast = accum
        accum += temp
        k -= 1)
    return accum)

(:f main() =
    fib = fibonacci 5
    print $fib)
