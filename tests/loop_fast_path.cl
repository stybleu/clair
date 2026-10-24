def dot(a: list[float], b: list[float]) -> float {
    let s: float = 0.0
    for i in range(len(a)) {
        s = s + a[i] * b[i]
    }
    return s
}

def axpy(alpha: float, x: list[float], y: list[float]) -> int {
    for i in range(len(x)) {
        y[i] = alpha * x[i] + y[i]
    }
    return 0
}

def square(v: float) -> float {
    return v * v
}

def grow(v: list[int]) -> int {
    v.append(1)
    return len(v)
}

def main() -> int {
    let a: list[float] = [1.0, 2.0, 3.0, 4.0, 5.0]
    let b: list[float] = [0.5, 0.5, 0.5, 0.5, 0.5]
    print(dot(a, b))

    axpy(2.0, a, b)
    print(b[4])
    axpy(1.0, a, a)
    print(a[2])

    let v: list[int] = [10, 20, 30, 40, 50, 60]
    let n: int = len(v)
    let s: int = 0
    for i in range(n) {
        s = s + v[n - 1 - i] * (i + 1)
    }
    print(s)

    for i in range(1, n, 2) {
        v[i] = v[i] + v[i - 1]
    }
    print(v[5])
    for i in range(n - 1, -1, -1) {
        s = s - v[i]
    }
    print(s)

    let m: int = 3
    let t: int = 0
    for i in range(m) {
        m = m + 1
        t = t + v[i]
    }
    print(m)
    print(t)

    let flags: list[bool] = [true, false, true, true]
    let words: list[string] = ["a", "b", "c", "d"]
    let count: int = 0
    let joined: string = ""
    for i in range(len(flags)) {
        if flags[i] {
            count = count + 1
            joined = joined + words[i]
        }
        flags[i] = not flags[i]
    }
    print(count)
    print(joined)
    print(flags[1])

    let sq: list[float] = [1.0, 2.0, 3.0]
    for i in range(len(sq)) {
        sq[i] = square(sq[i])
    }
    print(sq[2])

    let g: list[int] = [1, 2, 3]
    let total: int = 0
    for i in range(3) {
        total = total + g[i] + grow(g)
    }
    print(total)
    print(len(g))

    let big: list[int] = [7, 8]
    for i in range(2) {
        print(big[i * 4611686018427387904 * 4])
    }

    let empty: list[int] = []
    for i in range(0) {
        print(empty[i])
    }
    for i in range(5, 2) {
        print(empty[i])
    }

    let mat: list[float] = [1.0, 2.0, 3.0, 4.0]
    let out: list[float] = [0.0, 0.0, 0.0, 0.0]
    let size: int = 2
    for i in range(size) {
        for j in range(size) {
            let acc: float = 0.0
            for k in range(size) {
                acc = acc + mat[i * size + k] * mat[k * size + j]
            }
            out[i * size + j] = acc
        }
    }
    print(out[0])
    print(out[3])

    print(100000 * 100000)
    print(-9223372036854775807 - 1)
    print(3 - -2)
    print(10 - (4 - 3))
    return 0
}
