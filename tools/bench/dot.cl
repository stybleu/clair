def dot(a: list[float], b: list[float]) -> float {
    let s: float = 0.0
    for i in range(len(a)) {
        s = s + a[i] * b[i]
    }
    return s
}

def main() -> int {
    let a: list[float] = []
    let b: list[float] = []
    for i in range(4096) {
        a.append(0.5)
        b.append(2.0)
    }
    let total: float = 0.0
    for r in range(100000) {
        total = total + dot(a, b)
    }
    print(total)
    return 0
}
