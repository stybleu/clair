def main() -> int {
    let values: list[int] = []
    for i in range(0, 100) {
        values.append(i)
    }
    print(len(values))
    print(values[0])
    print(values[50])
    print(values[99])
    return 0
}
