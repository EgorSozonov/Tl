fizzBuzz = {{
    for { i = 1; i < 101; i += 1:
        worker i
    }
}}

worker = {{ i Int ->:
    if { i % 3 == 0 && (i % 5 == 0): print `FizzBuzz` }
    ei { i % 3 == 0: print `Fizz` }
    ei { i % 5 == 0: print `Buzz` }
    else { print i }
}}

main = {{
    (fizzBuzz)
}}
