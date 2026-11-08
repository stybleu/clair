// The Computer Language Benchmarks Game : spectral-norm
// Plus grande valeur propre de la matrice infinie A(i, j) = 1 / ((i + j)(i + j + 1) / 2 + i + 1).
// noyau: produit_a

def a(i: int, j: int) -> float {
    return 1.0 / float((i + j) * (i + j + 1) / 2 + i + 1)
}

def produit_a(u: list[float], v: list[float]) -> int {
    let n: int = len(u)
    for i in range(n) {
        let s: float = 0.0
        for j in range(n) {
            s += a(i, j) * u[j]
        }
        v[i] = s
    }
    return 0
}

def produit_at(u: list[float], v: list[float]) -> int {
    let n: int = len(u)
    for i in range(n) {
        let s: float = 0.0
        for j in range(n) {
            s += a(j, i) * u[j]
        }
        v[i] = s
    }
    return 0
}

def produit_ata(u: list[float], v: list[float], w: list[float]) -> int {
    produit_a(u, w)
    produit_at(w, v)
    return 0
}

def main() -> int {
    let n: int = input_int()
    let u: list[float] = []
    let v: list[float] = []
    let w: list[float] = []
    for i in range(n) {
        u.append(1.0)
        v.append(0.0)
        w.append(0.0)
    }
    for k in range(10) {
        produit_ata(u, v, w)
        produit_ata(v, u, w)
    }
    let vbv: float = 0.0
    let vv: float = 0.0
    for i in range(n) {
        vbv += u[i] * v[i]
        vv += v[i] * v[i]
    }
    print(format(sqrt(vbv / vv), ".9f"))
    return 0
}
