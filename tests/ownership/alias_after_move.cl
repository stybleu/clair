def main() -> int {
    let a: list[int] = [10, 20]
    let b: list[int] = move(a)
    let alias: list[int] = b
    alias[0] = 99
    print(b[0])
    return 0
}
