def main() -> int {
    let a: list[int] = [10, 20]
    let alias: list[int] = a
    a = [30, 40]
    let b: list[int] = move(a)
    print(alias[0])
    print(b[0])
    return 0
}
