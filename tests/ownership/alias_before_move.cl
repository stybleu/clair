def main() -> int {
    let a: list[int] = [10, 20]
    let alias: list[int] = a
    let b: list[int] = move(a)
    print(alias[0])
    return 0
}
