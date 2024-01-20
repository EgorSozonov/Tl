swap = ^{ E | i Int j Int arr L(E) => |
    tmp = arr_i
    arr_i = arr_j
    arr_j = tmp
} 

quickSort = ^{ [E] L(E) =>. arr.
    stack = l(L Int)
    i = 0
    j = #arr
    for: i < j;;
        if: arr_i < arr_j =>
            arr_i > arr_j =>
            else
} 

print = f: L(Int) =>. lst.
    printNoLn `[`
    for (i = 0; i < #lst;;
        printNoLn arr_i
        printNoLn ` `
    printNoLn `]`

main = f:
    leArray = a(5 1 2 4 3)
    quickSort leArray
    print leArray

