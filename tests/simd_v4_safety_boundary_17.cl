
def main() -> int {
    let values: list[int] = []

    for j in range(17) {
        values.append(j)
    }

    values[0] = 9223372036854775807
    values[1] = -9223372036854775807 - 1

    let i: int = 1

    while i < len(values) {
        values[i] = values[i] + 1
        i = i + 1
    }

    print(i)
    print(values[0])
    print(values[1])
    print(values[17 - 1])
    return 0
}
