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

    for i in range(0, 1024) {
        values.append(i)
    }

    print(sum_int(values))
    return 0
}
