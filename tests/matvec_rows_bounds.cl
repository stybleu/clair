// x trop court : les 4 lignes ne sont pas utilisees, l'erreur arrive au meme moment qu'avant
def couche(w: matrix[float], b: list[float], x: list[float], y: list[float]) -> int {
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = s
        print(i)
    }
    return 0
}

def main() -> int {
    let w: matrix[float] = matrix(5, 3)
    couche(w, [0.0, 0.0, 0.0, 0.0, 0.0], [1.0, 2.0], [0.0, 0.0, 0.0, 0.0, 0.0])
    return 0
}
