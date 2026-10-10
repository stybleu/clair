def calculate(a: int, b: int) -> int {
    let x: int = a * 2
    let y: int = b * 4
    return x + y
}

def main() -> int {
    let a: int = input_int()
    let b: int = input_int()
    print(calculate(a, b))
    return 0
}
