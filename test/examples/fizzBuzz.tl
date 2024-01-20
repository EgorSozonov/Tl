fizzBuzz = {
    for i = 1; i < 101; i++ (
        i .worker
    )
}

worker = { i Int => |
    if i % 3 == 0 && i % 5 == 0 => `FizzBuzz`.print)
    ei i % 3 == 0 => `Fizz`.print)
    ei i % 5 == 0 => `Buzz`.print)
    else { i .print }
}

main = {
    .fizzBuzz
}
