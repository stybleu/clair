def sum_to(n: int) -> int {
    let i: int = 1
    let total: int = 0

    while i <= n {
        total = total + i
        i = i + 1
    }

    return total
}

def main() -> int {
    print(sum_to(5))

    let x: int = 3
    while x > 0 {
        print(x)
        x = x - 1
    }

    let untouched: int = 9
    while untouched < 0 {
        untouched = 100
    }
    print(untouched)

    return 0
}
