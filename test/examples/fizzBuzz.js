function fizzBuzz() {
    for (i = 1; i < 101;) {
        if (((i % 3) === 0) && ((i % 5) === 0)) {
            print(`FizzBuzz`);
        } else if ((i % 3) === 0) {
            print(`Fizz`);
        } else if ((i % 5) === 0) {
            print(`Buzz`);
        } else {
            print((i).toString());
        }
        ++(i);
    }}

function main() {
    fizzBuzz();
}
