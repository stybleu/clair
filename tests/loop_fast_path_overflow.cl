def main() -> int {
    let v: list[int] = [5, 6]
    for i in range(3) {
        print(i)
        print(v[i * 4611686018427387904])
    }
    return 0
}
