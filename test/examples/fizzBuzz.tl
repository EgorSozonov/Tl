(:f fizzBuzz () =
    i = 1
    (:while (< i 101)
        (:if (and (== (% i 3) 0)
                  (== (% i 5) 0)) => print `FizzBuzz`
             (== (% i 3) 0)       => print `Fizz`
             (== (% i 5) 0)       => print `Buzz`
                                else print $i)
        ++i))

(:f main() =
    (fizzBuzz))
