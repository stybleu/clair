def main() -> int {
    let v: list[int] = [1, 2, 3, 4]
    for i in range(6) {
        print(i)
        v[i] = v[i] * 2
    }
    return 0
}
