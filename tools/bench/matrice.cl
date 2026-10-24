def matmul(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
    for i in range(rows(a)) {
        for k in range(cols(a)) {
            let aik: float = a[i][k]
            for j in range(cols(b)) {
                c[i][j] = c[i][j] + aik * b[k][j]
            }
        }
    }
    return 0
}

def main() -> int {
    let n: int = 256
    let a: matrix[float] = matrix(n, n)
    let b: matrix[float] = matrix(n, n)
    let c: matrix[float] = matrix(n, n)
    for i in range(n) {
        for j in range(n) {
            a[i][j] = 1.0
            b[i][j] = 0.5
        }
    }
    for r in range(4) {
        matmul(a, b, c)
    }
    print(c[n - 1][n - 1])
    return 0
}
