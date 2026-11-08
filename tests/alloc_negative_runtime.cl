def main() -> int {
    let n: int = 0 - 3
    print(1)
    absolue {
        let p: ptr[int] = alloc(n)
        print(p[0])
    }
    return 0
}
