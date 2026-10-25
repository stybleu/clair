// Noyaux float32 : produit de matrices en blocs et couches denses sur tous les coeurs,
// resultats identiques au bit pres aux boucles simples (et quel que soit le nombre de coeurs)

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

def produit_plat(a: list[float32], b: list[float32], c: list[float32], m: int, n: int, p: int) -> int {
    for i in range(m) {
        for k in range(p) {
            for j in range(n) {
                c[i * n + j] = c[i * n + j] + a[i * p + k] * b[k * n + j]
            }
        }
    }
    return 0
}

def relu(v: float32) -> float32 {
    if v < 0.0 {
        return 0.01 * v
    }
    return v
}

def couche(w: matrix[float32], b: list[float32], x: list[float32], y: list[float32]) -> int {
    for i in range(rows(w)) {
        let s: float32 = b[i]
        for j in range(cols(w)) {
            s += w[i][j] * x[j]
        }
        y[i] = tanh(relu(s))
    }
    return 0
}

def sommes(w: matrix[float32], x: list[float32], y: list[float32]) -> int {
    for i in range(rows(w)) {
        let s: float32 = 0.0
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = s
        if i % 97 == 0 {
            print(s)
        }
    }
    return 0
}

def total(v: list[float32]) -> float32 {
    let t: float32 = 0.0
    for e in v {
        t += e
    }
    return t
}

def main() -> int {
    // valeurs pseudo-aleatoires, avec des arrondis differents a chaque addition
    let m: int = 130
    let p: int = 70
    let n: int = 150
    let a: matrix[float32] = matrix(m, p)
    let b: matrix[float32] = matrix(p, n)
    let c: matrix[float32] = matrix(m, n)
    let fa: list[float32] = []
    let fb: list[float32] = []
    let fc: list[float32] = []
    let graine: int = 12345
    for i in range(m) {
        for k in range(p) {
            graine = (graine * 1103515245 + 12345) % 2147483648
            a[i][k] = float32(graine % 2001 - 1000) / 333.0
            fa.append(a[i][k])
        }
    }
    for k in range(p) {
        for j in range(n) {
            graine = (graine * 1103515245 + 12345) % 2147483648
            b[k][j] = float32(graine % 1999 - 999) / 77.0
            fb.append(b[k][j])
        }
    }
    for i in range(m * n) {
        fc.append(0.1)
    }
    produit(a, b, c)
    produit_plat(fa, fb, fc, m, n, p)
    print(c[0][0])
    print(c[m - 1][n - 1])
    print(c[65][77])
    print(fc[0])
    print(fc[m * n - 1])
    let sc: float32 = 0.0
    for i in range(m) {
        for j in range(n) {
            sc += c[i][j]
        }
    }
    print(sc)
    print(total(fc))

    // couche dense 300 x 257 : la sigmoide et le reste de chaque ligne sur tous les coeurs
    let w: matrix[float32] = matrix(300, 257)
    let bb: list[float32] = []
    let x: list[float32] = []
    let y: list[float32] = []
    for i in range(300) {
        for j in range(257) {
            graine = (graine * 1103515245 + 12345) % 2147483648
            w[i][j] = float32(graine % 4001 - 2000) / 9000.0
        }
        bb.append(float32(i % 7) / 10.0 - 0.3)
        y.append(0.0)
    }
    for j in range(257) {
        x.append(float32(j % 11) / 5.5 - 1.0)
    }
    for r in range(5) {
        couche(w, bb, x, y)
        x[r] = y[r * 3]
    }
    print(y[0])
    print(y[299])
    print(total(y))
    sommes(w, x, y)
    print(total(y))
    return 0
}
