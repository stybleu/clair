// axpy en float32 : 4 nombres par instruction NEON au lieu de 2
def axpy(alpha: float32, x: list[float32], y: list[float32]) -> int {
    for i in range(len(x)) {
        y[i] = alpha * x[i] + y[i]
    }
    return 0
}

def main() -> int {
    let x: list[float32] = []
    let y: list[float32] = []
    for i in range(4096) {
        x.append(1.0)
        y.append(0.0)
    }
    for r in range(400000) {
        axpy(0.000001, x, y)
    }
    print(y[4095])
    return 0
}
