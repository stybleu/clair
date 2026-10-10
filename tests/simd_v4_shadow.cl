def check_shadow(n: int) -> int {
    let values: list[int] = []

    for j in range(n) {
        values.append(j)
    }

    let alias: list[int] = values
    let i: int = 0

    while i < len(values) {
        values[i] = values[i] + 1
        i = i + 1
    }

    if n >= 16 {
        let i: int = 0

        while i < len(alias) {
            alias[i] = alias[i] + 1
            i = i + 1
        }

        print(i)
    } else {
        print(-1)
    }

    print(i)
    print(values[0])
    print(alias[n - 1])

    return 0
}

def main() -> int {
    let result: int = check_shadow(15)
    result = check_shadow(16)
    result = check_shadow(17)
    result = check_shadow(256)
    return 0
}
