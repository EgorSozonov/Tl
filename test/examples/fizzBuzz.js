function fizzBuzz() {
    let i = 1;
    while (i < 101) {
        worker(i);
        i = i + 1;
    }
}

function worker(i) {
    if (((i % 3) === 0) && ((i % 5) === 0)) {
        print(`FizzBuzz`);
    } else if ((i % 3) === 0) {
        print(`Fizz`);
    } else if ((i % 5) === 0) {
        print(`Buzz`);
    } else {
        print((i).toString());
    }
}

function main() {
    fizzBuzz();
}
