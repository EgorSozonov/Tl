nestedLoops = f:
    for: x = 0; x < 2; ;
        for: y = 0; y < 2;
            while(z = 0. a = +(x y). <(z 2):
                x += 1
                y += 1
                z += 1
                print(+(+(+(`z = ` ;z) +(`; y = ` ;y)) +(`; x = ` ;x)))
                if(>(x 0) : break 3)

main = f:
    nestedLoops _
