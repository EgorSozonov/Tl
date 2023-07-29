function fibonacci(n) {
    if (n <= 0) {
        return 0;
    } else if (n === 1) {
        return 1;
    }
    let beforeLast = 0;
    let accum = 1;
    let swap = 0;
    {
    let k = n - 1;
    while (k > 0) {
        swap = beforeLast;
        beforeLast = accum;
        accum = accum + swap;
        k = k - 1;
    }}
    return accum;
}

function main() {
    const fib = fibonacci(5);
    print((fib).toString());
}
