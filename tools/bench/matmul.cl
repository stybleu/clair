def matmul(a: list[float], b: list[float], c: list[float], n: int) -> int {
    for i in range(n) {
        for k in range(n) {
            let aik: float = a[i * n + k]
            for j in range(n) {
                c[i * n + j] = c[i * n + j] + aik * b[k * n + j]
            }
        }
    }
    return 0
}

def main() -> int {
    let n: int = 256
    let a: list[float] = []
    let b: list[float] = []
    let c: list[float] = []
    for i in range(n * n) {
        a.append(1.0)
        b.append(0.5)
        c.append(0.0)
    }
    for r in range(4) {
        matmul(a, b, c, n)
    }
    print(c[n * n - 1])
    return 0
}
