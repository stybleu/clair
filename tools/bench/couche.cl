// couche dense d'un reseau de neurones : 256 entrees -> 256 sorties, sigmoide, 4000 fois
// noyau: couche
def sigmoid(v: float) -> float {
    return 1.0 / (1.0 + exp(-v))
}

def couche(w: matrix[float], b: list[float], x: list[float], y: list[float]) -> int {
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = sigmoid(s)
    }
    return 0
}

def main() -> int {
    let n: int = 256
    let w: matrix[float] = matrix(n, n)
    let b: list[float] = []
    let x: list[float] = []
    let y: list[float] = []
    for i in range(n) {
        for j in range(n) {
            w[i][j] = float((i * 7 + j * 3) % 17) / 17.0 - 0.5
        }
        b.append(float(i % 5) / 10.0)
        x.append(float(i % 3) / 3.0)
        y.append(0.0)
    }
    for r in range(4000) {
        couche(w, b, x, y)
        x[r % n] = y[(r * 7) % n]
    }
    print(y[0] + y[n - 1])
    return 0
}
