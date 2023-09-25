fizzBuzz = f:
    i = 1
    for: ; i < 101;
        worker i
        i += 1

worker = f:Int =>. i.
    if: i % 3 == 0 &&
            i % 5 == 0 => print `FizzBuzz`
         i % 3 == 0   => print `Fizz`
         i % 5 == 0   => print `Buzz`
                    else print i


main = f:
    fizzBuzz _
