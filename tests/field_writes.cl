Class Layer {
    w: matrix[float]
    b: list[float]
    seen: list[int]

    def init(scale: float) -> int {
        for i in range(rows(self.w)) {
            for j in range(cols(self.w)) {
                self.w[i][j] = scale * (i + j)
            }
            self.b[i] = 0.5
        }
        return 0
    }

    def forward(x: list[float], y: list[float]) -> int {
        for i in range(rows(self.w)) {
            let s: float = self.b[i]
            for j in range(cols(self.w)) {
                s = s + self.w[i][j] * x[j]
            }
            y[i] = s
            self.seen.append(i)
        }
        return 0
    }

    def scale_bias(k: float) -> int {
        for i in range(len(self.b)) {
            self.b[i] = self.b[i] * k
        }
        return 0
    }
}

Class Net {
    first: Layer
    name: string
}

def swap_weights(l: Layer, other: matrix[float]) -> int {
    l.w = other
    return 0
}

def main() -> int {
    let l: Layer = Layer(matrix(2, 3), [0.0, 0.0], [])
    l.init(1.0)
    print(l.w[1][2])
    print(l.b[1])

    let x: list[float] = [1.0, 2.0, 3.0]
    let y: list[float] = [0.0, 0.0]
    l.forward(x, y)
    print(y[0])
    print(y[1])
    print(len(l.seen))

    l.scale_bias(4.0)
    print(l.b[0])

    let net: Net = Net(l, "demo")
    net.first.b[1] = 7.0
    net.first.w[0][0] = 9.0
    net.first.seen.append(42)
    print(l.b[1])
    print(l.w[0][0])
    print(l.seen[2])

    let other: matrix[float] = matrix(2, 3)
    let total: float = 0.0
    for j in range(3) {
        total = total + l.w[1][j]
        if j == 1 {
            swap_weights(l, other)
        }
    }
    print(total)
    print(l.w[1][2])

    for j in range(3) {
        l.w[0][j] = 1.0
        l.w = other
    }
    print(other[0][2])
    return 0
}
