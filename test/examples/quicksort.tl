swap = {{ E: i Int j Int arr L(E):
    tmp = arr@i;
    arr@i = arr@j;
    arr@j = tmp
}}

quickSort = {{ E: arr L(E):
    stack [L Int] = L();
    i = 0;
    j = arr##;
    for{ i = 0; j~ = arr##; i < j:
        if{ arr@i < (arr@j): }
        ei{ arr@i > (arr@j): }
        else{} 
    } 
}}

print = {{ lst L(Int) =>
    printNoLn `[`;
    for (i = 0; i < #lst;;
        printNoLn arr@i
        printNoLn ` `
    printNoLn `]`
}}

main = {{ 
    leArray = A(5 1 2 4 3);
    quickSort leArray;
    print leArray;
}}

