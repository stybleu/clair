def total(v: list[int]) -> int {
    let s: int = 0
    let i: int = 0
    while i < len(v) {
        s = s + v[i]
        i = i + 1
    }
    return s
}

def scale(v: list[float], k: float) -> int {
    let i: int = 0
    while i < len(v) {
        v[i] = v[i] * k
        i = i + 1
    }
    return i
}

def main() -> int {
    let v: list[int] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    print(total(v))

    let f: list[float] = [1.0, 2.0, 3.0]
    print(scale(f, 2.5))
    print(f[2])

    let i: int = 2
    let odd: int = 0
    while i < 10 {
        if v[i] / 2 * 2 == v[i] {
            i = i + 1
            continue
        }
        odd = odd + v[i]
        i = i + 1
    }
    print(odd)

    let j: int = 0
    let first_big: int = -1
    while j < len(v) {
        if v[j] > 6 {
            first_big = j
            break
        }
        j = j + 1
    }
    print(first_big)
    print(j)

    let n: int = 3
    let k: int = 0
    let acc: int = 0
    while k < n {
        acc = acc + v[k]
        if k == 0 {
            n = 5
        }
        k = k + 1
    }
    print(acc)

    let m: int = 0
    let back: int = 0
    while m < 5 {
        back = back + v[9 - m]
        m = m + 1
    }
    print(back)

    let mat: matrix[float] = matrix(2, 3)
    let c: int = 0
    while c < 3 {
        mat[1][c] = 1.5 * c
        c = c + 1
    }
    print(mat[1][2])
    return 0
}
