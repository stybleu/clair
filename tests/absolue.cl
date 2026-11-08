// V0.41 : absolue { } -- la mémoire sans vérification, comme en C

absolue def somme(p: ptr[float], n: int) -> float {
    let s: float = 0.0
    for i in range(n) {
        s += p[i]
    }
    return s
}

absolue def axpy(a: float32, x: ptr[float32], y: ptr[float32], n: int) -> int {
    for i in range(n) {
        y[i] += a * x[i]
    }
    return 0
}

absolue def produit(a: ptr[float], b: ptr[float], c: ptr[float], n: int) -> int {
    for i in range(n) {
        for k in range(n) {
            for j in range(n) {
                c[i * n + j] += a[i * n + k] * b[k * n + j]
            }
        }
    }
    return 0
}

absolue def couche(w: ptr[float32], x: ptr[float32], y: ptr[float32], r: int, c: int) -> int {
    for i in range(r) {
        let s: float32 = 0.0
        for j in range(c) {
            s += w[i * c + j] * x[j]
        }
        y[i] = s
    }
    return 0
}

absolue def nouveau(n: int) -> ptr[int] {
    let p: ptr[int] = alloc(n)
    for i in range(n) {
        p[i] = i * 10
    }
    return p
}

def main() -> int {
    // address() d'une liste : lire, écrire, avancer
    let xs: list[float] = [1.5, 2.5, 3.0, 4.0]
    absolue {
        let p: ptr[float] = address(xs)
        p[0] = 10.0
        print(somme(p, 4))
        let q: ptr[float] = p + 2
        print(q[1])
        print(q - 2 == p)
        print(p != q)
    }
    print(xs[0])

    // address() d'une matrice : les rangées se suivent
    let m: matrix[int] = matrix(3, 4)
    absolue {
        let mp: ptr[int] = address(m)
        for i in range(12) {
            mp[i] = i * i
        }
        let o: ptr[u8] = bytes(mp)
        print(o[8])
        print(o[9])
    }
    print(m[2][3])
    print(m[1][0])

    // alloc, fill, copy, free
    absolue {
        let a: ptr[float32] = alloc(1000, 64)
        fill(a, 0.5, 1000)
        let total: float32 = 0.0
        for i in range(1000) {
            total += a[i]
        }
        print(total)
        let b: ptr[float32] = alloc(1000)
        print(b[999])
        copy(b, a, 1000)
        print(b[999])
        copy(a + 1, a, 999)
        a[1] = 7.0
        copy(a, a + 1, 3)
        print(a[0])
        free(a)
        free(b)
        let vide: ptr[float] = alloc(0)
        free(vide)
    }

    // entiers machine
    absolue {
        let o: ptr[u8] = alloc(16)
        for i in range(16) {
            o[i] = u8(i * 20)
        }
        o[3] += u8(100)
        print(o[3])
        print(o[15] + u8(1))
        let k: u32 = u32(5)
        print(o[k])
        let w: ptr[i64] = alloc(3)
        w[1] = i64(-5)
        print(w[1] * i64(3))
        free(o)
        free(w)
        let p: ptr[int] = nouveau(5)
        print(p[4])
        free(p)
    }

    // axpy et boucles en forme de noyaux : C ordinaire, mêmes résultats que Python
    let n: int = 70
    let a: list[float] = []
    let b: list[float] = []
    let c: list[float] = []
    seed(7)
    for i in range(n * n) {
        a.append(random())
        b.append(random())
        c.append(0.0)
    }
    let w: matrix[float32] = matrix(33, 41)
    let x: list[float32] = []
    let y: list[float32] = []
    for i in range(33) {
        for j in range(41) {
            w[i][j] = gauss(0.0, 1.0)
        }
        y.append(0.0)
    }
    for j in range(41) {
        x.append(gauss(0.0, 1.0))
    }
    absolue {
        produit(address(a), address(b), address(c), n)
        couche(address(w), address(x), address(y), 33, 41)
        axpy(0.5, address(x), address(x), 41)
    }
    let t: float = 0.0
    for v in c {
        t += v
    }
    print(t)
    print(c[n * n - 1])
    print(y[0])
    print(y[32])
    print(x[0])
    return 0
}
