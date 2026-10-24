def main() -> int {
    let v: list[int] = [1, 2, 3, 4]
    for k in range(3) {
        print(k)
        for j in range(2) {
            v[k * 2 + j] = 0
        }
    }
    return 0
}
