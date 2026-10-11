def main() -> int {
    let a: list[int] = [10, 20]
    let alias: list[int] = a
    let b: list[int] = move(alias)
    print(a[0])
    return 0
}
