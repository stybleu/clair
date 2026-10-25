// V0.43 : boucles triangulaires et boucles a deux bouts, prouvees une seule fois
def inverse(xs: list[int], lo: int, hi: int) -> int {
    while lo < hi {
        let t: int = xs[lo]
        xs[lo] = xs[hi]
        xs[hi] = t
        lo += 1
        hi -= 1
    }
    return 0
}

def main() -> int {
    let xs: list[int] = [1, 2, 3, 4, 5, 6, 7]
    inverse(xs, 0, len(xs) - 1)
    inverse(xs, 2, 4)
    for v in xs {
        print(v)
    }
    let n: int = len(xs)
    let paires: int = 0
    for i in range(n) {
        for j in range(i + 1, n) {
            if xs[i] > xs[j] {
                paires += 1
            }
        }
    }
    print(paires)
    let m: matrix[float] = matrix(4, 4)
    for i in range(4) {
        for j in range(i, 4) {
            m[i][j] = float(i * 4 + j)
            m[j][i] = m[i][j]
        }
    }
    print(m[3][1])
    return 0
}
