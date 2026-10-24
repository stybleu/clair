def matmul_list(a: list[float], b: list[float], c: list[float], n: int) -> int {
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

def transpose(m: matrix[int], t: matrix[int]) -> int {
    for i in range(rows(m)) {
        for j in range(cols(m)) {
            t[j][i] = m[i][j]
        }
    }
    return 0
}

def main() -> int {
    let n: int = 3
    let a: list[float] = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]
    let b: list[float] = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    let c: list[float] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    matmul_list(a, b, c, n)
    print(c[5])

    let m: matrix[int] = matrix(2, 3)
    let t: matrix[int] = matrix(3, 2)
    for i in range(2) {
        for j in range(3) {
            m[i][j] = 10 * i + j
        }
    }
    transpose(m, t)
    print(t[2][1])

    let tri: float = 0.0
    for k in range(4) {
        for j in range(k) {
            tri = tri + a[k * 2 + j]
        }
    }
    print(tri)

    let v: list[int] = [1, 2, 3]
    let none: int = 0
    for k in range(5) {
        for j in range(0) {
            none = none + v[k * 100 + j]
        }
    }
    print(none)

    let quad: int = 0
    for k in range(3) {
        for j in range(3) {
            quad = quad + v[k * j / 2]
        }
    }
    print(quad)

    let shift: int = 0
    let s: int = 0
    for k in range(3) {
        s = s + v[k]
        for j in range(2) {
            shift = shift + v[j + s / 6]
        }
    }
    print(shift)

    let stop_at: int = 0
    for k in range(10) {
        if k == 2 {
            break
        }
        for j in range(3) {
            stop_at = stop_at + v[j] * (k + 1)
        }
    }
    print(stop_at)
    return 0
}
