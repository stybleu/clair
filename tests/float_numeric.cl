def average(a: float, b: float) -> float {
    return (a + b) / 2.0
}

def widen(x: int) -> float {
    return x
}

def main() -> int {
    let a: float = 10.5
    let b: float = 4.5
    print(average(a, b))

    let mixed: float = 2 + 0.5
    print(mixed)

    let promoted: float = 7
    print(promoted)

    print(widen(9))
    print(3.0 * 2)
    print(5 / 2.0)
    print(-1.5)

    if 2.5 > 2 and 4.0 == 4 {
        print(1)
    }

    return 0
}
