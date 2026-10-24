def main() -> int {
    let m: matrix[float] = matrix(2, 2)
    for i in range(3) {
        print(i)
        m[i][0] = 1.0
    }
    return 0
}
