def main() -> int {
    let a: list[int] = [10, 20]
    let alias: list[int] = a
    let b: list[int] = move(a)
    alias[0] = 99
    print(b[0])
    return 0
}
