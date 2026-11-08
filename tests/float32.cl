// float32 : nombres a virgule sur 32 bits, comme en IA (NumPy, PyTorch)
// memes resultats que NumPy en float32, deux fois moins de memoire, 4 nombres par instruction NEON

Class Couche {
    w: matrix[float32]
    b: list[float32]
}

def remplir(n: int, graine: int) -> list[float32] {
    let out: list[float32] = []
    for i in range(n) {
        out.append(float32((i * 7 + graine) % 13) / 9.0 - 0.6)
    }
    return out
}

def norme(v: list[float32]) -> float32 {
    let s: float32 = 0.0
    for x in v {
        s += x * x
    }
    return sqrt(s)
}

def axpy(a: float32, x: list[float32], y: list[float32]) -> int {
    for i in range(len(x)) {
        y[i] += a * x[i]
    }
    return 0
}

def produit(a: matrix[float32], b: matrix[float32], c: matrix[float32]) -> int {
    for i in range(rows(a)) {
        for k in range(cols(a)) {
            let aik: float32 = a[i][k]
            for j in range(cols(b)) {
                c[i][j] += aik * b[k][j]
            }
        }
    }
    return 0
}

def avant(c: Couche, x: list[float32], y: list[float32]) -> int {
    for i in range(rows(c.w)) {
        let s: float32 = c.b[i]
        for j in range(cols(c.w)) {
            s += c.w[i][j] * x[j]
        }
        y[i] = max(s, 0.0)
    }
    return 0
}

def main() -> int {
    // calculs : un nombre ecrit dans le programme prend le type float32 a cote d'un float32
    let a: float32 = 0.1
    let b: float32 = 0.2
    print(a + b)
    print(a * 3)
    print(a / 3.0)
    print(a == 0.1)
    print(a < 0.1)
    let d: float = 0.1
    print(a + d)
    print(float(a))
    print(float32(1) / float32(3))
    print(float32(d) == a)
    let n: int = 7
    print(a * n)
    print(int(a * 1000.0))
    print(-a)
    print(abs(-b))
    print(min(a, b))
    print(max(a, 0.5))
    print(sqrt(b))
    print(floor(b * 10.0 + 0.5))
    print(pow(b, 3.0))
    let grand: float32 = 16777217
    print(grand)
    print(grand + 1.0)
    let petit: float32 = 1e-3
    print(petit)
    let x7: float32 = 1.0
    x7 /= 7.0
    print(x7)

    // listes, boucles rapides, tranches, enumerate
    let x: list[float32] = remplir(37, 1)
    let y: list[float32] = remplir(37, 2)
    axpy(0.25, x, y)
    let t: float32 = 0.0
    for v in y {
        t += v
    }
    print(t)
    print(y[36])
    print(norme(x[::3]))
    for i, v in enumerate(y[:3]) {
        print(float32(i) + v)
    }

    // matrices et couche dense
    let ma: matrix[float32] = matrix(5, 7)
    let mb: matrix[float32] = matrix(7, 6)
    let mc: matrix[float32] = matrix(5, 6)
    for i in range(5) {
        for j in range(7) {
            ma[i][j] = float32(i * 3 + j) * 0.1
            mb[j][i] = float32(i + j * 2) * 0.3 - 1.0
        }
    }
    produit(ma, mb, mc)
    print(mc[4][5])
    print(mc[0][0])
    let couche: Couche = Couche(ma, remplir(5, 3))
    let h: list[float32] = remplir(5, 0)
    avant(couche, remplir(7, 4), h)
    for v in h {
        print(v)
    }
    return 0
}
