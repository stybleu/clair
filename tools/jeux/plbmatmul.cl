// PLB2 : matmul
// Produit de deux matrices n x n comme le programme C de PLB2 : b est d'abord transposee,
// puis chaque case est un produit scalaire d'une ligne de a et d'une ligne de b transposee.
// noyau: produit

def remplit(n: int) -> matrix[float] {
    let m: matrix[float] = matrix(n, n)
    let t: float = 1.0 / float(n) / float(n)
    for i in range(n) {
        for j in range(n) {
            m[i][j] = t * float(i - j) * float(i + j)
        }
    }
    return m
}

def produit(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
    let n: int = rows(a)
    let bt: matrix[float] = matrix(n, n)
    for i in range(n) {
        for j in range(n) {
            bt[j][i] = b[i][j]
        }
    }
    for i in range(n) {
        for j in range(n) {
            let s: float = 0.0
            for k in range(n) {
                s += a[i][k] * bt[j][k]
            }
            c[i][j] = s
        }
    }
    return 0
}

def main() -> int {
    let n: int = input_int()
    let a: matrix[float] = remplit(n)
    let b: matrix[float] = remplit(n)
    let c: matrix[float] = matrix(n, n)
    produit(a, b, c)
    print(format(c[n / 2][n / 2], "f"))
    return 0
}
