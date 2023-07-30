(:f fizzBuzz () =
    i = 1
    (:while (< i 101)
        worker i
        i += 1))

(:f worker(i Int) =
    (:if (and (== (% i 3) 0)
             (== (% i 5) 0)) => print `FizzBuzz`
         == (% i 3) 0        => print `Fizz`
         == (% i 5) 0        => print `Buzz`
                            else print $i))
                            
(:f main() =
    (fizzBuzz))
