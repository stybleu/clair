def main() -> int {
    print(3)
    let m: matrix[int] = load_csv("tests/donnees/bancal.csv", "int")
    print(rows(m))
    return 0
}
