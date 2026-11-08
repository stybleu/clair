def grow(values: list[int]) -> int {
    for i in range(0, 4096) {
        values.append(i)
    }
    return len(values)
}

def sum_int(values: list[int]) -> int {
    let total: int = 0
    let i: int = 0

    while i < len(values) {
        total = total + values[i]
        i = i + 1
    }
    return total
}

def main() -> int {
    let values: list[int] = []
    let alias: list[int] = values

    print(grow(alias))
    print(sum_int(values))

    alias[4095] = 10000

    print(values[4095])
    print(sum_int(alias))

    values.append(7)

    print(alias[4096])
    print(len(values))
    return 0
}
