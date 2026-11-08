
def main() -> int {
    let values: list[int] = []

    for j in range(16) {
        values.append(j)
    }

    let i: int = 0

    while i < len(values) {
        values[i] = values[i] + 1

        if i == 0 {
            values.append(77)
        }

        i = i + 1
    }

    print(i)
    print(len(values))
    print(values[16])
    return 0
}
