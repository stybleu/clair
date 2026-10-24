Class Dense {
    w: matrix[float]
    b: list[float]

    def forward(x: list[float], y: list[float]) -> int {
        for i in range(rows(self.w)) {
            let s: float = self.b[i]
            let w: matrix[float] = self.w
            for j in range(cols(w)) {
                s = s + w[i][j] * x[j]
            }
            y[i] = 1.0 / (1.0 + exp(-s))
        }
        return 0
    }
}

def matmul(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
    for i in range(rows(a)) {
        for k in range(cols(a)) {
            let aik: float = a[i][k]
            for j in range(cols(b)) {
                c[i][j] = c[i][j] + aik * b[k][j]
            }
        }
    }
    return 0
}

def trace(m: matrix[int]) -> int {
    let t: int = 0
    for i in range(rows(m)) {
        t = t + m[i][i]
    }
    return t
}

def main() -> int {
    let m: matrix[float] = matrix(2, 3)
    print(rows(m))
    print(cols(m))
    print(m[1][2])
    m[0][1] = 2.5
    m[1][2] = 4
    print(m[0][1] + m[1][2])

    let a: matrix[float] = matrix(2, 2)
    let b: matrix[float] = matrix(2, 2)
    let c: matrix[float] = matrix(2, 2)
    a[0][0] = 1.0
    a[0][1] = 2.0
    a[1][0] = 3.0
    a[1][1] = 4.0
    for i in range(2) {
        for j in range(2) {
            b[i][j] = 1.0 * (i + j)
        }
    }
    matmul(a, b, c)
    print(c[0][0])
    print(c[0][1])
    print(c[1][0])
    print(c[1][1])

    let id: matrix[int] = matrix(3, 3)
    for i in range(3) {
        id[i][i] = i + 1
    }
    print(trace(id))

    let layer: Dense = Dense(matrix(2, 3), [0.0, -1.0])

    let w: matrix[float] = layer.w
    w[1][2] = 2.0
    w[0][0] = 1.0
    let x: list[float] = [1.0, 1.0, 1.0]
    let y: list[float] = [0.0, 0.0]
    layer.forward(x, y)
    print(y[0])
    print(y[1])

    let alias: matrix[float] = m
    alias[0][0] = 9.0
    print(m[0][0])
    let empty: matrix[float] = matrix(0, 5)
    print(rows(empty))
    print(cols(empty))
    return 0
}
