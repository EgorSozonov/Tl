function foo(s, flag) {
    if (flag) {
        return s.length;
    } else {
        return getLength(s);
    }
}

function getLength(s) {
    return foo(s, true);
}

function main() {
    const result = foo(`Hello world`, false);
    print((`Result is ` + (result).toString()));
}
