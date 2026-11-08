def main() -> int {
    print(1)
    let m: matrix[float] = load_csv("tests/donnees/absent.csv")
    print(rows(m))
    return 0
}
