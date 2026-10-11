def main() -> int {
    let a: list[int] = [10, 20]
    let b: list[int] = move(a)
    a.append(99)
    return 0
}
