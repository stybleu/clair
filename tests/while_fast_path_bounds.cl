def main() -> int {
    let v: list[int] = [1, 2, 3]
    let i: int = -1
    while i < 3 {
        print(i)
        print(v[i + 1])
        v[i - 3] = 0
        i = i + 1
    }
    return 0
}
