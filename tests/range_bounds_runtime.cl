def main() -> int {
    let xs: list[int] = [1, 2, 3]
    let n: int = 4
    let k: int = 0
    let s: int = 0
    while k >= 0 {
        s += xs[k]
        print(s)
        if k < n - 1 {
            k += 1
        } else {
            k = -1
        }
    }
    return 0
}
