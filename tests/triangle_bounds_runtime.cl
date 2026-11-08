def main() -> int {
    let xs: list[int] = [1, 2, 3]
    let s: int = 0
    for i in range(3) {
        for j in range(i + 1, 4) {
            s += xs[j]
        }
        print(s)
    }
    return 0
}
