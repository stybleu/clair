// c trop petite : le noyau n'est pas utilise, l'erreur arrive au meme moment qu'avant
def product(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
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
    let a: matrix[float] = matrix(2, 2)
    let b: matrix[float] = matrix(2, 3)
    let c: matrix[float] = matrix(2, 2)
    print(7)
    product(a, b, c)
    print(8)
    return 0
}
