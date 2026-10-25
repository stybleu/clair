// produit de matrices en float32 : 4 nombres par instruction NEON au lieu de 2
def matmul(a: list[float32], b: list[float32], c: list[float32], n: int) -> int {
    for i in range(n) {
        for k in range(n) {
            let aik: float32 = a[i * n + k]
            for j in range(n) {
                c[i * n + j] = c[i * n + j] + aik * b[k * n + j]
            }
        }
    }
    return 0
}

def main() -> int {
    let n: int = 256
    let a: list[float32] = []
    let b: list[float32] = []
    let c: list[float32] = []
    for i in range(n * n) {
        a.append(1.0)
        b.append(0.5)
        c.append(0.0)
    }
    for r in range(128) {
        matmul(a, b, c, n)
    }
    print(c[n * n - 1])
    return 0
}
