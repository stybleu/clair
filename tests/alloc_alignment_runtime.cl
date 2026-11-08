def main() -> int {
    absolue {
        let p: ptr[float] = alloc(10, 48)
        print(p[0])
    }
    return 0
}
