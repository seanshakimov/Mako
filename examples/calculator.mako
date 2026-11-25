// ╔═══════════════════════════════════════╗
// ║  Calculator — Functions & operators    ║
// ╚═══════════════════════════════════════╝

fn add(a, b) {
    return a + b;
}

fn multiply(a, b) {
    return a * b;
}

fn power(base, exp) {
    let result = 1;
    let i = 0;
    while (i < exp) {
        result = result * base;
        i = i + 1;
    }
    return result;
}

fn factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

// Demo calculations
print(add(100, 200));        // 300
print(multiply(7, 8));       // 56
print(power(2, 10));         // 1024
print(factorial(10));        // 3628800

// Complex expression
let result = add(multiply(3, 4), power(2, 5));
print(result);               // 44
