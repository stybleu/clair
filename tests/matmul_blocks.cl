// Produit de matrices en blocs : resultat identique au bit pres a la triple boucle

def fill(m: matrix[float], seed: int) -> int {
    for i in range(rows(m)) {
        for j in range(cols(m)) {
            m[i][j] = float((i * 7 + j * 3 + seed) % 11) / 7.0 + 0.1
        }
    }
    return 0
}

// reconnue : remplacee par le noyau en blocs
def product(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
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

// meme calcul, non reconnu (une instruction de plus) : la triple boucle simple
def product_plain(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
    let steps: int = 0
    for i in range(rows(a)) {
        for k in range(cols(a)) {
            let aik: float = a[i][k]
            for j in range(cols(b)) {
                c[i][j] = c[i][j] + aik * b[k][j]
                steps = steps + 1
            }
        }
    }
    return steps
}

def differences(x: matrix[float], y: matrix[float]) -> int {
    let d: int = 0
    for i in range(rows(x)) {
        for j in range(cols(x)) {
            if x[i][j] != y[i][j] {
                d = d + 1
            }
        }
    }
    return d
}

def check(m: int, k: int, n: int) -> int {
    let a: matrix[float] = matrix(m, k)
    let b: matrix[float] = matrix(k, n)
    let c1: matrix[float] = matrix(m, n)
    let c2: matrix[float] = matrix(m, n)
    fill(a, 1)
    fill(b, 5)
    fill(c1, 2)
    fill(c2, 2)
    product(a, b, c1)
    product_plain(a, b, c2)
    return differences(c1, c2)
}

// listes a plat, sans variable aik, operandes dans l'autre ordre
def product_list(a: list[float], b: list[float], c: list[float], m: int, k: int, n: int) -> int {
    for i in range(m) {
        for p in range(k) {
            for j in range(n) {
                c[i * n + j] = b[p * n + j] * a[i * k + p] + c[i * n + j]
            }
        }
    }
    return 0
}

def product_list_plain(a: list[float], b: list[float], c: list[float], m: int, k: int, n: int) -> int {
    let steps: int = 0
    for i in range(m) {
        for p in range(k) {
            for j in range(n) {
                c[i * n + j] = b[p * n + j] * a[i * k + p] + c[i * n + j]
                steps = steps + 1
            }
        }
    }
    return steps
}

def make(count: int, seed: int) -> list[float] {
    let out: list[float] = []
    for i in range(count) {
        out.append(float((i * 5 + seed) % 13) / 9.0 + 0.3)
    }
    return out
}

def check_list(m: int, k: int, n: int) -> int {
    let a: list[float] = make(m * k, 1)
    let b: list[float] = make(k * n, 4)
    let c1: list[float] = make(m * n, 2)
    let c2: list[float] = make(m * n, 2)
    product_list(a, b, c1, m, k, n)
    product_list_plain(a, b, c2, m, k, n)
    let d: int = 0
    for i in range(m * n) {
        if c1[i] != c2[i] {
            d = d + 1
        }
    }
    return d
}

def main() -> int {
    print(check(1, 1, 1))
    print(check(4, 8, 8))
    print(check(13, 11, 9))
    print(check(37, 150, 70))
    print(check(64, 300, 33))
    print(check_list(5, 7, 3))
    print(check_list(29, 140, 41))

    // c est aussi a : le noyau n'est pas utilise, resultat de la triple boucle
    let s: matrix[float] = matrix(3, 3)
    let t: matrix[float] = matrix(3, 3)
    fill(s, 3)
    fill(t, 4)
    let s2: matrix[float] = matrix(3, 3)
    fill(s2, 3)
    product(s, t, s)
    product_plain(s2, t, s2)
    print(differences(s, s2))
    print(s[2][2])

    // dimension nulle : rien ne change
    let e: matrix[float] = matrix(0, 4)
    let f: matrix[float] = matrix(4, 2)
    let g: matrix[float] = matrix(0, 2)
    product(e, f, g)
    print(rows(g))
    return 0
}
