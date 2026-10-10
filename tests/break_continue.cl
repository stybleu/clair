def main() -> int {
    let i: int = 0

    while i < 10 {
        i = i + 1

        if i == 2 {
            continue
        }

        if i == 5 {
            break
        }

        print(i)
    }

    for j in range(0, 7) {
        if j == 1 {
            continue
        }

        if j == 5 {
            break
        }

        print(j)
    }

    for outer in range(0, 3) {
        for inner in range(0, 4) {
            if inner == 2 {
                break
            }

            print(outer * 10 + inner)
        }
    }

    return 0
}
