def main() -> int {
    print(2)
    let m: matrix[float] = load_csv("tests/donnees/mauvais.csv")
    print(rows(m))
    return 0
}
