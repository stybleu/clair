def main() -> int {
    let xs: list[int] = [1, 2, 3]
    let lo: int = 0
    let hi: int = 3
    while lo < hi {
        print(xs[lo])
        xs[lo] = xs[hi]
        lo += 1
        hi -= 1
    }
    return 0
}
