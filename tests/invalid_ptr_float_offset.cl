def main() -> int {
    let xs: list[float] = [1.0, 2.0]
    absolue {
        let p: ptr[float] = address(xs) + 1.5
        print(p[0])
    }
    return 0
}
