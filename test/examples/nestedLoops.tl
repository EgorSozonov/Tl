nestedLoops = ^{
    for x = 0; x < 2; { 
        for y = 0; y < 2; {
            for z = 0; a = x + y; z < 2 {
                x++
                y++
                z++
                `z = ` .print $z `; y = ` $y `; x = ` $x
                if x > 0 { break 3 }
            }
        }
    }
}

main = ^{
    nestedLoops()
}
