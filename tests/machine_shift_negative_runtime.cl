def main() -> int {
    let x: i8 = 1
    let n: int = 7
    print(u8(x << n))
    n -= 8
    print(x << n)
    return 0
}
