def main() -> int {
    absolue {
        let p: ptr[float] = alloc(4)
        p[1] = 2.5
        print(p[1])
        copy(p, p + 1, 0 - 1)
    }
    return 0
}
