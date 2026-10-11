def main() -> int {
    let a: list[int] = [10, 20]
    let b: list[int] = move(a)
    print(b[0])
    return 0
}
