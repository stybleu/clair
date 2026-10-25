// La suite de chaque ligne ecrit dans une liste trop courte : l'erreur arrive au meme
// moment que dans la boucle simple (ligne 3), meme si la couche est reconnue.

def couche(w: matrix[float], x: list[float], y: list[float]) -> int {
    for i in range(rows(w)) {
        let s: float = 0.0
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = max(s, 0.0)
    }
    return 0
}

def main() -> int {
    let w: matrix[float] = matrix(5, 4)
    let x: list[float] = [1.0, 2.0, 3.0, 4.0]
    let y: list[float] = [0.0, 0.0, 0.0]
    print(1)
    couche(w, x, y)
    print(2)
    return 0
}
