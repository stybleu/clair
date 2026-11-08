def sigmoid(x: float) -> float {
    return 1.0 / (1.0 + exp(-x))
}

def softmax(v: list[float], out: list[float]) -> int {
    let m: float = v[0]
    for i in range(len(v)) {
        m = max(m, v[i])
    }
    let total: float = 0.0
    for i in range(len(v)) {
        out[i] = exp(v[i] - m)
        total = total + out[i]
    }
    for i in range(len(v)) {
        out[i] = out[i] / total
    }
    return 0
}

def roots(x: list[float], y: list[float]) -> int {
    for i in range(len(x)) {
        y[i] = sqrt(x[i])
    }
    return 0
}

def main() -> int {
    print(sqrt(16.0))
    print(sqrt(2))
    print(pow(2.0, 10))
    print(floor(-2.5))
    print(ceil(2.1))
    print(abs(-7))
    print(abs(-2.5))
    print(min(3, 9))
    print(max(3, 9.5))
    print(int(7.9))
    print(int(-7.9))
    print(float(7) / 2)
    print(int(true))
    print(17 % 5)
    print(-17 % 5)
    let a: int = 17
    let b: int = -5
    print(a % b)
    print(7.5 % 2)
    print(1e3)
    print(2.5e-3 * 1000)
    print(sigmoid(0.0))
    let v: list[float] = [1.0, 2.0, 3.0]
    let o: list[float] = [0.0, 0.0, 0.0]
    softmax(v, o)
    print(o[2])
    print(tanh(0.0))
    print(log(exp(2.0)))
    print(100000 * 100000 % 7)
    let n: int = 10
    let evens: int = 0
    for i in range(n) {
        if i % 2 == 0 {
            evens = evens + 1
        }
    }
    print(evens)
    let x: list[float] = [1.0, 4.0, 9.0, 16.0]
    let y: list[float] = [0.0, 0.0, 0.0, 0.0]
    roots(x, y)
    print(y[3])
    return 0
}
