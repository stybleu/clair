def main() -> int {
    let xs: list[float] = [1.0]
    absolue {
        let p: ptr[float] = address(xs)
        xs.append(2.0)
        print(p[0])
    }
    return 0
}
