(:f fibonacci Int(n Int) =
    (:if <= n 0 => return 0
         == n 1 => return 1)
    beforeLast = 0
    accum = 1
    swap = 0
    (:while (> k 0). k = - n 1
        swap := beforeLast
        beforeLast := accum
        accum += swap
        k -= 1)
    return accum)

(:f main() =
    fib = fibonacci 6
    print $fib)
