def main() -> int {
    for i in range(5) {
        print(i)
    }

    let sum: int = 0
    for i in range(2, 6) {
        sum = sum + i
    }
    print(sum)

    for i in range(5, 0, -2) {
        print(i)
    }

    for i in range(1 + 1, 7, 2) {
        print(i)
    }

    let nested: int = 0
    for i in range(2) {
        for j in range(3) {
            nested = nested + 1
        }
    }
    print(nested)

    return 0
}
