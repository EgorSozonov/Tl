swap = fn([E] i Int j Int arr L(E):
    tmp = arr@i
    arr@i = arr@j
    arr@j = tmp
)

quickSort = fn([E] arr L(E):
    stack = L(Int) () 
    i = 0
    j = #arr
    while(<(i j): 
        if(<(arr@i arr@j):
            >(arr@i arr@j):
            else
)))

print = fn(lst L(Int):
    printNoLn(`[`)
    while(i = 0. <(i #lst):
        printNoLn(arr@i)
        printNoLn(` `)
    )
    printNoLn(`]`)
)

main = fn(:
    leArray = (5 1 2 4 3)
    quickSort(leArray)
    print(leArray)
)
