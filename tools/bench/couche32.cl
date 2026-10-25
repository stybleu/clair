// couche dense en float32 : 256 entrees -> 256 sorties, sigmoide, 16000 fois
// noyau: couche
def sigmoid(v: float32) -> float32 {
    return 1.0 / (1.0 + exp(-v))
}

def couche(w: matrix[float32], b: list[float32], x: list[float32], y: list[float32]) -> int {
    for i in range(rows(w)) {
        let s: float32 = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = sigmoid(s)
    }
    return 0
}

def main() -> int {
    let n: int = 256
    let w: matrix[float32] = matrix(n, n)
    let b: list[float32] = []
    let x: list[float32] = []
    let y: list[float32] = []
    for i in range(n) {
        for j in range(n) {
            w[i][j] = float32((i * 7 + j * 3) % 17) / 17.0 - 0.5
        }
        b.append(float32(i % 5) / 10.0)
        x.append(float32(i % 3) / 3.0)
        y.append(0.0)
    }
    for r in range(16000) {
        couche(w, b, x, y)
        x[r % n] = y[(r * 7) % n]
    }
    print(y[0] + y[n - 1])
    return 0
}
