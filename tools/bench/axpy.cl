def axpy(alpha: float, x: list[float], y: list[float]) -> int {
    for i in range(len(x)) {
        y[i] = alpha * x[i] + y[i]
    }
    return 0
}

def main() -> int {
    let x: list[float] = []
    let y: list[float] = []
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
