(:f nestedLoops() =
    (:while (< x 2). x = 0
        (:while (< y 2). y = 0
            (:while (< z 2). z = 0. a = + x y
                x += 1
                y += 1
                z += 1
                print (+ (+ (+ `z = ` $z) (+ `; y = ` $y)) (+ `; x = ` $x))
                (:if (> x 0) => break 3)))))

(:f main() =
    (nestedLoops))
