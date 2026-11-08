def add(a: int, b: int) -> int {
    return a + b
}

def multiply(a: int, b: int) -> int {
    return a * b
}

def main() -> int {
    let result: int = add(10, 20)
    let nested: int = multiply(add(2, 5), 6)
    print(result)
    print(nested)
    return 0
}
