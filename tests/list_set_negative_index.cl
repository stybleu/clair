def main() -> int {
    let values: list[int] = [1]
    values[-1] = 9
    print(values[0])
    values[-2] = 1
    return 0
}
